#ifndef INFERENCE_OPT_H
#define INFERENCE_OPT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TENSOR_ARENA_SIZE (512 * 1024)
#define MAX_FUSED_OPS     64
#define MAX_THREADS       16
#define TILE_M            6
#define TILE_N            16
#define TILE_K            8

typedef enum {
    FUSE_CONV_BN_RELU,
    FUSE_CONV_BIAS_RELU,
    FUSE_CONV_BN,
    FUSE_FC_RELU,
    FUSE_COUNT
} FuseType;

typedef enum {
    CONV_WINOGRAD_F6x6_3x3,
    CONV_WINOGRAD_F4x4_3x3,
    CONV_WINOGRAD_F2x2_3x3,
    CONV_IM2COL,
    CONV_GEMM,
    CONV_COUNT
} ConvAlgo;

typedef enum {
    QUANT_ASYMMETRIC,
    QUANT_SYMMETRIC,
    QUANT_PER_CHANNEL,
    QUANT_SCHEME_COUNT
} QuantScheme;

typedef struct {
    float   scale;
    int32_t zero_point;
    int8_t* data;
    size_t  num_elements;
} QuantizedTensor;

typedef struct {
    uint8_t* base;
    size_t   total_size;
    size_t   used;
    int      num_allocations;
} TensorArena;

typedef struct {
    int        input_idx;
    int        output_idx;
    FuseType   type;
    float*     fused_weights;
    float*     fused_bias;
    int        kernel_size;
    int        out_channels;
    int        in_channels;
} FusedOp;

typedef struct {
    int      num_threads;
    int      thread_ids[MAX_THREADS];
    uint8_t  active;
} ThreadPool;

typedef struct {
    ConvAlgo  algo;
    int       input_w, input_h;
    int       output_w, output_h;
    int       kernel_size;
    int       stride;
    int       padding;
    int       in_channels;
    int       out_channels;
} WinogradPlan;

typedef struct OptCtx OptCtx;

OptCtx*        opt_create(void);
void           opt_destroy(OptCtx* ctx);

int            opt_fuse_operators(OptCtx* ctx, FusedOp* ops, int* num_ops);
int            opt_fuse_conv_bn_relu(const float* conv_w, const float* conv_b,
                                     const float* bn_gamma, const float* bn_beta,
                                     const float* bn_mean, const float* bn_var,
                                     float eps, int out_ch, int in_ch, int ksize,
                                     float* fused_w, float* fused_b);

TensorArena*   arena_create(size_t size);
void           arena_destroy(TensorArena* arena);
void*          arena_alloc(TensorArena* arena, size_t size);
void           arena_reset(TensorArena* arena);
size_t         arena_available(const TensorArena* arena);

int            winograd_f23_transform_input(const float* in, int in_c, int in_h, int in_w,
                                            float* out);
int            winograd_f23_transform_kernel(const float* kernel, int out_c, int in_c,
                                             int k_h, int k_w, float* out);
int            winograd_f23_transform_output(const float* in, int out_c, int out_h, int out_w,
                                             float* out);
int            winograd_f23_conv(const float* A, const float* B, int tiles,
                                 int in_ch, int out_ch, const float* G, float* C);

int            quantize_int8_asym(const float* src, int n, float scale, int32_t zp,
                                  int8_t* dst);
int            dequantize_int8_asym(const int8_t* src, int n, float scale, int32_t zp,
                                    float* dst);
void           quant_calc_scale_zp(const float* data, int n, float* scale, int32_t* zp);
int            quant_matmul_int8(const int8_t* A, const int8_t* B, int32_t* C,
                                 int M, int N, int K);
int            quant_requantize(const int32_t* src, int n, float scale, int32_t zp,
                                int8_t* dst);

ThreadPool*    thread_pool_create(int n);
void           thread_pool_destroy(ThreadPool* pool);
int            thread_pool_parallel_for(ThreadPool* pool, void (*fn)(void*, int, int),
                                        void* arg, int start, int end);

/* ── L5: GEMM Micro-Kernel (GotoBLAS 2008 / BLIS framework) ──
   C[M×N] += A[M×K] × B[K×N]
   Optimized 6×16 register-blocked kernel with loop unrolling.
   Simulates register-level tiling for cache efficiency. ── */
int            gemm_micro_6x16(int M, int N, int K,
                               const float* A, int lda,
                               const float* B, int ldb,
                               float* C, int ldc);

/* ── L5: Cache-Blocked GEMM with Loop Tiling ── */
int            gemm_blocked(int M, int N, int K,
                            const float* A, int lda,
                            const float* B, int ldb,
                            float* C, int ldc,
                            int block_m, int block_n, int block_k);

/* ── L5: Depthwise Separable Convolution (Howard et al. 2017) ──
   MobileNetV1: Splits standard conv into depthwise + pointwise.
   Reduces computation from D_K²*M*N*D_F² to D_K²*M*D_F² + M*N*D_F². ── */
int            depthwise_conv2d_3x3(const float* input, int h, int w, int c,
                                    const float* kernel, const float* bias,
                                    int stride, float* output);
int            pointwise_conv2d_1x1(const float* input, int h, int w, int in_c,
                                     int out_c, const float* kernel,
                                     const float* bias, float* output);

/* ── L5: Huffman Coding for Weight Compression (Huffman 1952) ──
   Variable-length prefix coding. Optimal for known symbol frequencies.
   Used in Deep Compression (Han et al. 2016, ICLR). ── */
#define HUFFMAN_MAX_SYMBOLS 256
typedef struct {
    int      symbol;
    uint32_t code;
    int      code_len;
} HuffmanCode;

typedef struct {
    HuffmanCode codes[HUFFMAN_MAX_SYMBOLS];
    int         num_symbols;
    size_t      total_bits;
} HuffmanEncoder;

int            huffman_build_tree(const uint32_t* freqs, int n_symbols,
                                  HuffmanEncoder* enc);
int            huffman_encode(const HuffmanEncoder* enc, const int* symbols,
                              int n, uint8_t* bitstream, size_t* bitstream_bytes);
int            huffman_decode(const HuffmanEncoder* enc, const uint8_t* bitstream,
                              size_t bitstream_bits, int* symbols, int max_symbols);

/* ── L5: Sparse-Dense Matrix Multiplication (CSR format) ──
   C[M×N] += A_csr[M×K] × B_dense[K×N]
   CSR: values[], col_ind[], row_ptr[]
   Exploits sparsity for speedup proportional to density. ── */
int            sparse_dense_matmul_csr(const float* values, const int* col_ind,
                                        const int* row_ptr, int M, int K,
                                        const float* B, int N,
                                        float* C);

/* ── L5: Per-Channel Quantization ──
   Each output channel has its own scale and zero-point.
   More accurate than per-tensor quantization for convolutions. ── */
int            per_channel_quantize(const float* src, int n, int channels,
                                     const float* scales, const int32_t* zps,
                                     int8_t* dst);
int            per_channel_dequantize(const int8_t* src, int n, int channels,
                                       const float* scales, const int32_t* zps,
                                       float* dst);

/* ── L5: Im2Col + GEMM Convolution (Chellapilla et al. 2006) ──
   Transforms convolution into matrix multiplication. ── */
int            im2col(const float* input, int h, int w, int c,
                      int kh, int kw, int stride, int pad,
                      float* col);
int            col2im_gradient(const float* col, int h, int w, int c,
                               int kh, int kw, int stride, int pad,
                               float* grad_input);

#ifdef __cplusplus
}
#endif

#endif
