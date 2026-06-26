#include "inference_opt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

/* Portable aligned allocation (C99; C11 has aligned_alloc) */
static void* aligned_malloc(size_t alignment, size_t size) {
    void* raw = malloc(size + alignment + sizeof(void*));
    if (!raw) return NULL;
    uintptr_t addr = (uintptr_t)raw + sizeof(void*);
    uintptr_t mis  = addr % alignment;
    if (mis > 0) addr += alignment - mis;
    void* aligned = (void*)addr;
    ((void**)aligned)[-1] = raw;
    return aligned;
}

static void aligned_free(void* ptr) {
    if (ptr) free(((void**)ptr)[-1]);
}

struct OptCtx {
    FusedOp    fused_ops[MAX_FUSED_OPS];
    int        num_fused;
    TensorArena arena;
    int        initialized;
};

OptCtx* opt_create(void) {
    OptCtx* ctx = (OptCtx*)calloc(1, sizeof(OptCtx));
    if (!ctx) return NULL;
    ctx->num_fused = 0;
    ctx->initialized = 1;
    return ctx;
}

void opt_destroy(OptCtx* ctx) {
    if (!ctx) return;
    for (int i = 0; i < ctx->num_fused; ++i) {
        free(ctx->fused_ops[i].fused_weights);
        free(ctx->fused_ops[i].fused_bias);
    }
    free(ctx);
}

int opt_fuse_operators(OptCtx* ctx, FusedOp* ops, int* num_ops) {
    if (!ctx || !ops || !num_ops) return -1;
    int n = *num_ops;
    if (n > MAX_FUSED_OPS) n = MAX_FUSED_OPS;
    ctx->num_fused = n;
    for (int i = 0; i < n; ++i) {
        memcpy(&ctx->fused_ops[i], &ops[i], sizeof(FusedOp));
        int kc = ops[i].out_channels * ops[i].in_channels *
                 ops[i].kernel_size * ops[i].kernel_size;
        if (!ctx->fused_ops[i].fused_weights) {
            ctx->fused_ops[i].fused_weights = (float*)calloc(kc > 0 ? kc : 1, sizeof(float));
        }
    }
    return 0;
}

int opt_fuse_conv_bn_relu(const float* conv_w, const float* conv_b,
                           const float* bn_gamma, const float* bn_beta,
                           const float* bn_mean, const float* bn_var,
                           float eps, int out_ch, int in_ch, int ksize,
                           float* fused_w, float* fused_b) {
    if (!conv_w || !conv_b || !bn_gamma || !bn_beta ||
        !bn_mean || !bn_var || !fused_w || !fused_b) return -1;
    int w_size = out_ch * in_ch * ksize * ksize;
    memcpy(fused_w, conv_w, (size_t)w_size * sizeof(float));
    for (int oc = 0; oc < out_ch; ++oc) {
        float denom = sqrtf(bn_var[oc] + eps);
        float scale = bn_gamma[oc] / denom;
        float offset = bn_beta[oc] - (bn_mean[oc] * bn_gamma[oc]) / denom;
        fused_b[oc] = conv_b[oc] * scale + offset;
        if (offset <= 0.0f) fused_b[oc] = 0.0f;
        for (int idx = 0; idx < in_ch * ksize * ksize; ++idx) {
            fused_w[oc * in_ch * ksize * ksize + idx] *= scale;
        }
    }
    return 0;
}

TensorArena* arena_create(size_t size) {
    TensorArena* arena = (TensorArena*)calloc(1, sizeof(TensorArena));
    if (!arena) return NULL;
    arena->base = (uint8_t*)aligned_malloc(64, size);
    if (!arena->base) { free(arena); return NULL; }
    arena->total_size = size;
    arena->used = 0;
    arena->num_allocations = 0;
    return arena;
}

void arena_destroy(TensorArena* arena) {
    if (!arena) return;
    aligned_free(arena->base);
    free(arena);
}

void* arena_alloc(TensorArena* arena, size_t size) {
    if (!arena || arena->used + size > arena->total_size) return NULL;
    size_t aligned = (size + 63) & ~(size_t)63;
    if (arena->used + aligned > arena->total_size) return NULL;
    void* ptr = arena->base + arena->used;
    arena->used += aligned;
    arena->num_allocations++;
    return ptr;
}

void arena_reset(TensorArena* arena) {
    if (!arena) return;
    arena->used = 0;
    arena->num_allocations = 0;
}

size_t arena_available(const TensorArena* arena) {
    if (!arena) return 0;
    return arena->total_size - arena->used;
}

int winograd_f23_transform_input(const float* in, int in_c, int in_h, int in_w,
                                  float* out) {
    if (!in || !out) return -1;
    int tile_h = (in_h - 2) / 2, tile_w = (in_w - 2) / 2;
    int num_tiles = tile_h * tile_w;
    const float Bt[6][4] = {{ 1, 0, 0, 0}, { 0, 1,-1, 1}, {-1, 1, 1, 0},
                            { 0, 0, 0, 1}, { 1, 1, 1, 0}, { 0, 0, 0, 1}};
    for (int c = 0; c < in_c; ++c) {
        for (int t = 0; t < num_tiles; ++t) {
            int ty = t / tile_w, tx = t % tile_w;
            float tile[4][4];
            for (int dy = 0; dy < 4; ++dy)
                for (int dx = 0; dx < 4; ++dx)
                    tile[dy][dx] = in[c * in_h * in_w + (ty*2+dy)*in_w + (tx*2+dx)];
            float tmp[6][4], res[6][6];
            for (int i = 0; i < 6; ++i) for (int j = 0; j < 4; ++j) {
                tmp[i][j] = 0; for (int k = 0; k < 4; ++k) tmp[i][j] += Bt[i][k]*tile[k][j];
            }
            for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) {
                res[i][j] = 0; for (int k = 0; k < 4; ++k) res[i][j] += tmp[i][k]*Bt[j][k];
            }
            int base = c * num_tiles * 36 + t * 36;
            for (int i = 0; i < 6; ++i) for (int j = 0; j < 6; ++j) out[base + i*6 + j] = res[i][j];
        }
    }
    return 0;
}

int winograd_f23_transform_kernel(const float* kernel, int out_c, int in_c,
                                   int k_h, int k_w, float* out) {
    (void)k_h; (void)k_w;
    if (!kernel || !out) return -1;
    const float G[4][3] = {{ 1, 0, 0}, { 0.5f, 0.5f, 0.5f},
                            { 0.5f,-0.5f, 0.5f}, { 0, 0, 1}};
    for (int oc = 0; oc < out_c; ++oc) {
        for (int ic = 0; ic < in_c; ++ic) {
            float src[3][3];
            for (int ky = 0; ky < 3; ++ky)
                for (int kx = 0; kx < 3; ++kx)
                    src[ky][kx] = kernel[oc*in_c*9 + ic*9 + ky*3 + kx];
            float tmp[4][3];
            for (int i = 0; i < 4; ++i) for (int j = 0; j < 3; ++j) {
                tmp[i][j] = 0; for (int k = 0; k < 3; ++k) tmp[i][j] += G[i][k]*src[k][j];
            }
            float res[4][4];
            for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) {
                res[i][j] = 0; for (int k = 0; k < 3; ++k) res[i][j] += tmp[i][k]*G[j][k];
            }
            int base = (oc * in_c + ic) * 16;
            for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) out[base + i*4 + j] = res[i][j];
        }
    }
    return 0;
}

int winograd_f23_transform_output(const float* in, int out_c, int out_h, int out_w,
                                   float* out) {
    if (!in || !out) return -1;
    (void)out_c; (void)out_h; (void)out_w;
    return 0;
}

int winograd_f23_conv(const float* A, const float* B, int tiles,
                       int in_ch, int out_ch, const float* G, float* C) {
    (void)G;
    if (!A || !B || !C) return -1;
    for (int t = 0; t < tiles; ++t) {
        for (int oc = 0; oc < out_ch; ++oc) {
            for (int ic = 0; ic < in_ch; ++ic) {
                for (int i = 0; i < 36; ++i) {
                    C[t * out_ch * 36 + oc * 36 + i] +=
                        A[t * in_ch * 36 + ic * 36 + i] *
                        B[(oc * in_ch + ic) * 16 + (i / 6) * 4 + (i % 6)];
                }
            }
        }
    }
    return 0;
}

int quantize_int8_asym(const float* src, int n, float scale, int32_t zp,
                        int8_t* dst) {
    if (!src || !dst || n <= 0 || scale <= 0.0f) return -1;
    for (int i = 0; i < n; ++i) {
        float v = src[i] / scale + (float)zp;
        if (v > 127.0f) v = 127.0f;
        if (v < -128.0f) v = -128.0f;
        dst[i] = (int8_t)(v + 0.5f);
    }
    return 0;
}

int dequantize_int8_asym(const int8_t* src, int n, float scale, int32_t zp,
                          float* dst) {
    if (!src || !dst || n <= 0) return -1;
    for (int i = 0; i < n; ++i) dst[i] = ((float)src[i] - (float)zp) * scale;
    return 0;
}

void quant_calc_scale_zp(const float* data, int n, float* scale, int32_t* zp) {
    if (!data || n <= 0) { *scale = 1.0f; *zp = 0; return; }
    float mn = data[0], mx = data[0];
    for (int i = 1; i < n; ++i) { if (data[i] < mn) mn = data[i]; if (data[i] > mx) mx = data[i]; }
    float range = mx > mn ? mx - mn : 1.0f;
    *scale = range / 255.0f;
    *zp = (int32_t)(-mn / *scale);
    if (*zp < 0) *zp = 0; else if (*zp > 255) *zp = 255;
}

int quant_matmul_int8(const int8_t* A, const int8_t* B, int32_t* C,
                       int M, int N, int K) {
    if (!A || !B || !C) return -1;
    for (int i = 0; i < M; ++i) {
        for (int j = 0; j < N; ++j) {
            int32_t sum = 0;
            for (int k = 0; k < K; ++k) sum += (int32_t)A[i*K+k] * (int32_t)B[k*N+j];
            C[i*N+j] = sum;
        }
    }
    return 0;
}

int quant_requantize(const int32_t* src, int n, float scale, int32_t zp,
                      int8_t* dst) {
    if (!src || !dst || n <= 0 || scale <= 0.0f) return -1;
    for (int i = 0; i < n; ++i) {
        float v = (float)src[i] * scale + (float)zp;
        if (v > 127.0f) v = 127.0f;
        if (v < -128.0f) v = -128.0f;
        dst[i] = (int8_t)(v + 0.5f);
    }
    return 0;
}

ThreadPool* thread_pool_create(int n) {
    if (n <= 0 || n > MAX_THREADS) return NULL;
    ThreadPool* pool = (ThreadPool*)calloc(1, sizeof(ThreadPool));
    if (!pool) return NULL;
    pool->num_threads = n;
    pool->active = 1;
    for (int i = 0; i < n; ++i) pool->thread_ids[i] = i;
    return pool;
}

void thread_pool_destroy(ThreadPool* pool) {
    if (!pool) return;
    pool->active = 0;
    free(pool);
}

int thread_pool_parallel_for(ThreadPool* pool, void (*fn)(void*, int, int),
                              void* arg, int start, int end) {
    if (!pool || !fn) return -1;
    int total = end - start;
    if (total <= 0) return 0;
    int chunk = (total + pool->num_threads - 1) / pool->num_threads;
    for (int t = 0; t < pool->num_threads; ++t) {
        int s = start + t * chunk;
        int e = s + chunk;
        if (e > end) e = end;
        if (s < end) fn(arg, s, e);
    }
    return 0;
}

/* ================================================================
 *  L5: GEMM Micro-Kernel 6×16 (GotoBLAS-style)
 *
 *  Computes C += A × B where A is M×K, B is K×N, C is M×N.
 *  Register blocking: 6 rows of C × 16 columns of B.
 *  Inner loop accumulates over K dimension.
 *
 *  Reference: Goto & van de Geijn (2008), "Anatomy of
 *  High-Performance Matrix Multiplication", ACM TOMS.
 * ================================================================ */
int gemm_micro_6x16(int M, int N, int K,
                     const float* A, int lda,
                     const float* B, int ldb,
                     float* C, int ldc) {
    if (!A || !B || !C || M <= 0 || N <= 0 || K <= 0) return -1;
    for (int i = 0; i < M; i += 6) {
        int mr = (M - i) < 6 ? (M - i) : 6;
        for (int j = 0; j < N; j += 16) {
            int nr = (N - j) < 16 ? (N - j) : 16;
            /* Accumulate C[i:i+mr, j:j+nr] += A[i:i+mr, :] × B[:, j:j+nr] */
            for (int k = 0; k < K; ++k) {
                for (int ii = 0; ii < mr; ++ii) {
                    float aik = A[(i+ii) * lda + k];
                    for (int jj = 0; jj < nr; ++jj) {
                        C[(i+ii) * ldc + (j+jj)] += aik * B[k * ldb + (j+jj)];
                    }
                }
            }
        }
    }
    return 0;
}

/* ================================================================
 *  L5: Cache-Blocked GEMM (Loop Tiling)
 *
 *  Splits M×N×K into smaller tiles that fit in L1/L2 cache.
 *  Reduces cache misses by temporal reuse.
 *  Block sizes are tunable based on cache size.
 *
 *  Reference: Lam, Rothberg & Wolf (1991), "The Cache Performance
 *  and Optimizations of Blocked Algorithms", ASPLOS.
 * ================================================================ */
int gemm_blocked(int M, int N, int K,
                 const float* A, int lda,
                 const float* B, int ldb,
                 float* C, int ldc,
                 int block_m, int block_n, int block_k) {
    if (!A || !B || !C || M <= 0 || N <= 0 || K <= 0) return -1;
    int bm = block_m > 0 ? block_m : 64;
    int bn = block_n > 0 ? block_n : 64;
    int bk = block_k > 0 ? block_k : 64;
    for (int i = 0; i < M; i += bm) {
        int mr = (M - i) < bm ? (M - i) : bm;
        for (int j = 0; j < N; j += bn) {
            int nr = (N - j) < bn ? (N - j) : bn;
            for (int k = 0; k < K; k += bk) {
                int kr = (K - k) < bk ? (K - k) : bk;
                gemm_micro_6x16(mr, nr, kr,
                                &A[i * lda + k], lda,
                                &B[k * ldb + j], ldb,
                                &C[i * ldc + j], ldc);
            }
        }
    }
    return 0;
}

/* ================================================================
 *  L5: Depthwise Separable Convolution (MobileNetV1)
 *
 *  Depthwise: 1 filter per input channel, spatial only.
 *  Pointwise: 1×1 conv to combine channels.
 *  Total FLOPs: D_K²×C×H×W + C×C_out×H×W
 *  vs Standard: D_K²×C×C_out×H×W
 *
 *  Reduction ratio ≈ 1/C_out + 1/D_K² ≈ 1/8 to 1/9.
 *  Reference: Howard et al. (2017), "MobileNets", arXiv:1704.04861
 * ================================================================ */
int depthwise_conv2d_3x3(const float* input, int h, int w, int c,
                          const float* kernel, const float* bias,
                          int stride, float* output) {
    if (!input || !kernel || !output || h <= 2 || w <= 2 || c <= 0 || stride <= 0)
        return -1;
    int out_h = (h - 3) / stride + 1;
    int out_w = (w - 3) / stride + 1;
    for (int ci = 0; ci < c; ++ci) {
        const float* kin = kernel + ci * 9;
        float b = bias ? bias[ci] : 0.0f;
        for (int oy = 0; oy < out_h; ++oy) {
            for (int ox = 0; ox < out_w; ++ox) {
                float sum = b;
                for (int ky = 0; ky < 3; ++ky) {
                    for (int kx = 0; kx < 3; ++kx) {
                        int iy = oy * stride + ky;
                        int ix = ox * stride + kx;
                        sum += input[(iy * w + ix) * c + ci]
                               * kin[ky * 3 + kx];
                    }
                }
                output[(oy * out_w + ox) * c + ci] = sum;
            }
        }
    }
    return 0;
}

int pointwise_conv2d_1x1(const float* input, int h, int w, int in_c,
                          int out_c, const float* kernel,
                          const float* bias, float* output) {
    if (!input || !kernel || !output || h <= 0 || w <= 0 || in_c <= 0 || out_c <= 0)
        return -1;
    for (int oc = 0; oc < out_c; ++oc) {
        float b = bias ? bias[oc] : 0.0f;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float sum = b;
                for (int ic = 0; ic < in_c; ++ic) {
                    sum += input[(y * w + x) * in_c + ic]
                           * kernel[oc * in_c + ic];
                }
                output[(y * w + x) * out_c + oc] = sum;
            }
        }
    }
    return 0;
}

/* ================================================================
 *  L5: Huffman Coding (Huffman 1952)
 *  "A Method for the Construction of Minimum-Redundancy Codes"
 *  Proc. IRE, 40(9):1098-1101.
 *
 *  Builds optimal prefix code from symbol frequencies.
 *  Used in Deep Compression (Han et al. 2016) for neural network
 *  weight compression, achieving 3-5× storage reduction.
 * ================================================================ */
typedef struct {
    int      left, right;    /* children; -1 = leaf */
    uint32_t freq;
    int      symbol;         /* valid iff leaf */
} HuffmanNode;

int huffman_build_tree(const uint32_t* freqs, int n_symbols,
                        HuffmanEncoder* enc) {
    if (!freqs || !enc || n_symbols <= 0 || n_symbols > HUFFMAN_MAX_SYMBOLS)
        return -1;
    int n_nodes = 2 * n_symbols - 1;
    HuffmanNode* nodes = (HuffmanNode*)calloc((size_t)n_nodes, sizeof(HuffmanNode));
    if (!nodes) return -1;
    /* Initialize leaf nodes */
    for (int i = 0; i < n_symbols; ++i) {
        nodes[i].symbol = i;
        nodes[i].freq   = freqs[i] > 0 ? freqs[i] : 1;
        nodes[i].left   = -1;
        nodes[i].right  = -1;
    }
    /* Build tree: repeatedly merge two lowest-frequency nodes */
    for (int i = n_symbols; i < n_nodes; ++i) {
        /* Find two smallest */
        int min1 = -1, min2 = -1;
        uint32_t f1 = UINT32_MAX, f2 = UINT32_MAX;
        for (int j = 0; j < i; ++j) {
            if (nodes[j].freq == 0) continue;
            if (nodes[j].freq < f1) {
                f2 = f1; min2 = min1;
                f1 = nodes[j].freq; min1 = j;
            } else if (nodes[j].freq < f2) {
                f2 = nodes[j].freq; min2 = j;
            }
        }
        if (min1 < 0 || min2 < 0) { free(nodes); return -1; }
        nodes[i].freq  = nodes[min1].freq + nodes[min2].freq;
        nodes[i].left  = min1;
        nodes[i].right = min2;
        nodes[i].symbol = -1;
        nodes[min1].freq = 0;
        nodes[min2].freq = 0;
    }
    /* Traverse tree to assign codes */
    int root = n_nodes - 1;
    enc->num_symbols = n_symbols;
    enc->total_bits = 0;
    for (int i = 0; i < n_symbols; ++i) {
        enc->codes[i].symbol = i;
        enc->codes[i].code = 0;
        enc->codes[i].code_len = 0;
        /* Trace path from root to leaf i using DFS */
        uint32_t code = 0;
        int depth = 0;
        int stack_n = 0;
        int stack_nodes[512];
        int stack_depth[512];
        uint32_t stack_code[512];
        stack_nodes[0] = root;
        stack_depth[0] = 0;
        stack_code[0] = 0;
        stack_n = 1;
        while (stack_n > 0) {
            stack_n--;
            int node = stack_nodes[stack_n];
            int d = stack_depth[stack_n];
            uint32_t c = stack_code[stack_n];
            if (nodes[node].left < 0) {
                /* Leaf */
                if (nodes[node].symbol == i) {
                    code = c;
                    depth = d;
                    break;
                }
            } else {
                stack_nodes[stack_n] = nodes[node].right;
                stack_depth[stack_n] = d + 1;
                stack_code[stack_n] = (c << 1) | 1;
                stack_n++;
                stack_nodes[stack_n] = nodes[node].left;
                stack_depth[stack_n] = d + 1;
                stack_code[stack_n] = (c << 1) | 0;
                stack_n++;
            }
        }
        enc->codes[i].code = code;
        enc->codes[i].code_len = depth > 0 ? depth : 8;
        enc->total_bits += (size_t)enc->codes[i].code_len * freqs[i];
    }
    free(nodes);
    return 0;
}

int huffman_encode(const HuffmanEncoder* enc, const int* symbols,
                    int n, uint8_t* bitstream, size_t* bitstream_bytes) {
    if (!enc || !symbols || !bitstream || !bitstream_bytes || n <= 0) return -1;
    size_t bit_pos = 0;
    for (int i = 0; i < n; ++i) {
        int sym = symbols[i];
        if (sym < 0 || sym >= enc->num_symbols) continue;
        const HuffmanCode* hc = &enc->codes[sym];
        for (int b = hc->code_len - 1; b >= 0; --b) {
            int bit = (hc->code >> b) & 1;
            size_t byte_off = bit_pos / 8;
            int    bit_off  = (int)(7 - (bit_pos % 8));
            if (bit) bitstream[byte_off] |= (uint8_t)(1 << bit_off);
            bit_pos++;
        }
    }
    *bitstream_bytes = (bit_pos + 7) / 8;
    return 0;
}

int huffman_decode(const HuffmanEncoder* enc, const uint8_t* bitstream,
                    size_t bitstream_bits, int* symbols, int max_symbols) {
    if (!enc || !bitstream || !symbols || max_symbols <= 0) return -1;
    /* Build lookup by longest code: simple search over codes */
    uint32_t buffer = 0;
    int bits_in_buffer = 0;
    int out_idx = 0;
    size_t bits_read = 0;
    int max_code_len = 0;
    for (int i = 0; i < enc->num_symbols; ++i)
        if (enc->codes[i].code_len > max_code_len)
            max_code_len = enc->codes[i].code_len;
    while (bits_read < bitstream_bits && out_idx < max_symbols) {
        /* Fill buffer */
        while (bits_in_buffer < max_code_len && bits_read < bitstream_bits) {
            size_t byte_off = bits_read / 8;
            int    bit_off  = (int)(7 - (bits_read % 8));
            buffer = (buffer << 1) | ((bitstream[byte_off] >> bit_off) & 1);
            bits_in_buffer++;
            bits_read++;
        }
        /* Try to match */
        int matched = 0;
        for (int i = 0; i < enc->num_symbols; ++i) {
            if (enc->codes[i].code_len == 0) continue;
            if (enc->codes[i].code_len <= bits_in_buffer) {
                int shift = bits_in_buffer - enc->codes[i].code_len;
                uint32_t mask = (uint32_t)((1ULL << enc->codes[i].code_len) - 1);
                uint32_t prefix = (buffer >> shift) & mask;
                if (prefix == enc->codes[i].code) {
                    symbols[out_idx++] = enc->codes[i].symbol;
                    bits_in_buffer -= enc->codes[i].code_len;
                    buffer &= (uint32_t)((1ULL << bits_in_buffer) - 1);
                    matched = 1;
                    break;
                }
            }
        }
        if (!matched) break;
    }
    return out_idx;
}

/* ================================================================
 *  L5: Sparse-Dense Matrix Multiplication (CSR format)
 *
 *  CSR: Compressed Sparse Row storage.
 *  For each row i: columns in [col_ind[row_ptr[i] .. row_ptr[i+1]-1]]
 *  Only non-zero values participate in computation.
 *  Speedup ≈ 1/density for very sparse matrices.
 * ================================================================ */
int sparse_dense_matmul_csr(const float* values, const int* col_ind,
                             const int* row_ptr, int M, int K,
                             const float* B, int N,
                             float* C) {
    if (!values || !col_ind || !row_ptr || !B || !C ||
        M <= 0 || K <= 0 || N <= 0) return -1;
    for (int i = 0; i < M; ++i) {
        for (int j = 0; j < N; ++j) {
            float sum = 0.0f;
            for (int k = row_ptr[i]; k < row_ptr[i + 1]; ++k) {
                int col = col_ind[k];
                if (col >= 0 && col < K)
                    sum += values[k] * B[col * N + j];
            }
            C[i * N + j] = sum;
        }
    }
    return 0;
}

/* ================================================================
 *  L5: Per-Channel Quantization
 *
 *  Unlike per-tensor quantization which uses a single (scale, zp)
 *  for the entire tensor, per-channel assigns unique (scale, zp)
 *  to each channel (output channel for weights, for convolutions).
 *  This better handles varying ranges across channels.
 *
 *  Reference: Krishnamoorthi (2018), "Quantizing deep
 *  convolutional networks for efficient inference", arXiv.
 * ================================================================ */
int per_channel_quantize(const float* src, int n, int channels,
                          const float* scales, const int32_t* zps,
                          int8_t* dst) {
    if (!src || !dst || !scales || !zps || n <= 0 || channels <= 0) return -1;
    int per_ch = n / channels;
    for (int ch = 0; ch < channels; ++ch) {
        float sc = scales[ch];
        int32_t zp = zps[ch];
        for (int i = 0; i < per_ch; ++i) {
            int idx = ch * per_ch + i;
            float v = src[idx] / sc + (float)zp;
            if (v > 127.0f) v = 127.0f;
            if (v < -128.0f) v = -128.0f;
            dst[idx] = (int8_t)(v + 0.5f);
        }
    }
    return 0;
}

int per_channel_dequantize(const int8_t* src, int n, int channels,
                            const float* scales, const int32_t* zps,
                            float* dst) {
    if (!src || !dst || !scales || !zps || n <= 0 || channels <= 0) return -1;
    int per_ch = n / channels;
    for (int ch = 0; ch < channels; ++ch) {
        float sc = scales[ch];
        int32_t zp = zps[ch];
        for (int i = 0; i < per_ch; ++i) {
            int idx = ch * per_ch + i;
            dst[idx] = ((float)src[idx] - (float)zp) * sc;
        }
    }
    return 0;
}

/* ================================================================
 *  L5: Im2Col Transformation (Chellapilla et al. 2006)
 *
 *  Converts convolution to matrix multiplication:
 *    conv2d(input, kernel) = GEMM(kernel, im2col(input))
 *  Unrolls input patches into columns of a matrix.
 *  Each column is a KH×KW×C patch at one output position.
 * ================================================================ */
int im2col(const float* input, int h, int w, int c,
            int kh, int kw, int stride, int pad,
            float* col) {
    if (!input || !col || h <= 0 || w <= 0 || c <= 0 ||
        kh <= 0 || kw <= 0 || stride <= 0) return -1;
    int out_h = (h + 2 * pad - kh) / stride + 1;
    int out_w = (w + 2 * pad - kw) / stride + 1;
    int patch_size = kh * kw * c;
    for (int oy = 0; oy < out_h; ++oy) {
        for (int ox = 0; ox < out_w; ++ox) {
            int col_offset = (oy * out_w + ox) * patch_size;
            for (int ky = 0; ky < kh; ++ky) {
                for (int kx = 0; kx < kw; ++kx) {
                    int iy = oy * stride + ky - pad;
                    int ix = ox * stride + kx - pad;
                    int in_offset = (ky * kw + kx) * c;
                    for (int ci = 0; ci < c; ++ci) {
                        if (iy >= 0 && iy < h && ix >= 0 && ix < w)
                            col[col_offset + in_offset + ci] =
                                input[(iy * w + ix) * c + ci];
                        else
                            col[col_offset + in_offset + ci] = 0.0f;
                    }
                }
            }
        }
    }
    return 0;
}

int col2im_gradient(const float* col, int h, int w, int c,
                     int kh, int kw, int stride, int pad,
                     float* grad_input) {
    if (!col || !grad_input || h <= 0 || w <= 0 || c <= 0 ||
        kh <= 0 || kw <= 0 || stride <= 0) return -1;
    int out_h = (h + 2 * pad - kh) / stride + 1;
    int out_w = (w + 2 * pad - kw) / stride + 1;
    int patch_size = kh * kw * c;
    int total = h * w * c;
    for (int i = 0; i < total; ++i) grad_input[i] = 0.0f;
    for (int oy = 0; oy < out_h; ++oy) {
        for (int ox = 0; ox < out_w; ++ox) {
            int col_offset = (oy * out_w + ox) * patch_size;
            for (int ky = 0; ky < kh; ++ky) {
                for (int kx = 0; kx < kw; ++kx) {
                    int iy = oy * stride + ky - pad;
                    int ix = ox * stride + kx - pad;
                    if (iy >= 0 && iy < h && ix >= 0 && ix < w) {
                        int in_offset = (ky * kw + kx) * c;
                        for (int ci = 0; ci < c; ++ci) {
                            grad_input[(iy * w + ix) * c + ci] +=
                                col[col_offset + in_offset + ci];
                        }
                    }
                }
            }
        }
    }
    return 0;
}
