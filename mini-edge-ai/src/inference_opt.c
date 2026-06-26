#include "inference_opt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

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
    arena->base = (uint8_t*)aligned_alloc(64, size);
    if (!arena->base) { free(arena); return NULL; }
    arena->total_size = size;
    arena->used = 0;
    arena->num_allocations = 0;
    return arena;
}

void arena_destroy(TensorArena* arena) {
    if (!arena) return;
    free(arena->base);
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

static void parallel_worker(void* arg, int start, int end) {
    (void)arg; (void)start; (void)end;
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
    int chunk = (total + pool->num_threads - 1) / pool->num_threads;
    for (int t = 0; t < pool->num_threads; ++t) {
        int s = start + t * chunk;
        int e = s + chunk;
        if (e > end) e = end;
        if (s < end) fn(arg, s, e);
    }
    return 0;
}
