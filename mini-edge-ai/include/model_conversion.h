#ifndef MODEL_CONVERSION_H
#define MODEL_CONVERSION_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_OPS 256
#define MAX_CONVERT_PATH 512

typedef enum {
    SRC_FRAMEWORK_PYTORCH,
    SRC_FRAMEWORK_TENSORFLOW,
    SRC_FRAMEWORK_KERAS,
    SRC_FRAMEWORK_CAFFE,
    SRC_FRAMEWORK_COUNT
} SrcFramework;

typedef enum {
    DST_FRAMEWORK_ONNX,
    DST_FRAMEWORK_TFLITE,
    DST_FRAMEWORK_OPENVINO_IR,
    DST_FRAMEWORK_RKNN,
    DST_FRAMEWORK_COUNT
} DstFramework;

typedef enum {
    QUANT_NONE,
    QUANT_INT8_POST_TRAINING,
    QUANT_INT8_QAT,
    QUANT_FP16,
    QUANT_DYNAMIC_RANGE,
    QUANT_COUNT
} QuantMode;

typedef enum {
    OPT_FOLD_BN,
    OPT_FUSE_CONV_RELU,
    OPT_FUSE_CONV_BN_RELU,
    OPT_ELIM_DROPOUT,
    OPT_ELIM_IDENTITY,
    OPT_COUNT
} GraphOpt;

typedef struct {
    char name[64];
    int  supported;
    int  alternative_op_index;
} OpCheckResult;

typedef struct {
    char input_path[MAX_CONVERT_PATH];
    char output_path[MAX_CONVERT_PATH];
    SrcFramework src;
    DstFramework dst;
    QuantMode quant;
    GraphOpt graph_opts[8];
    int num_graph_opts;
    int input_w, input_h;
    int input_channels;
    float quant_scale;
    int32_t quant_zero_point;
} ConvertConfig;

typedef struct ConvertCtx ConvertCtx;

ConvertCtx*   convert_create(const ConvertConfig* config);
void          convert_destroy(ConvertCtx* ctx);
int           convert_run(ConvertCtx* ctx);
int           convert_check_ops(const ConvertCtx* ctx, OpCheckResult* results, int max_ops);

int           convert_calibrate_int8(const ConvertCtx* ctx, const char* rep_dataset,
                                     int num_samples);
int           convert_get_log(ConvertCtx* ctx, char* buf, size_t max_len);

int           graph_opt_fold_batchnorm(float* weights, float* bias,
                                       const float* bn_gamma, const float* bn_beta,
                                       const float* bn_mean, const float* bn_var,
                                       float eps, int channels);
int           graph_fuse_conv_relu_check(int conv_kernel, int relu_slope);

/* ── L2: Operator Compatibility Matrix ──
   Maps source framework ops to destination framework support.
   Helps users determine if a model can be converted. ── */
typedef struct {
    const char* op_name;
    int         src_supported[256];
    int         dst_supported[256];
    int         num_src;
    int         num_dst;
} OpCompatMatrix;

int           op_compat_init(OpCompatMatrix* mat);
int           op_compat_check_conversion(const OpCompatMatrix* mat,
                                          const char* op_name,
                                          SrcFramework src, DstFramework dst);

/* ── L5: Channel Pruning (Li et al. 2016, "Pruning Filters for Efficient ConvNets") ──
   Prunes channels with smallest L1-norm of filter weights.
   For each output channel oc: L1 = Σ |W[oc,:,:,:]|
   Removes the least important channels and their corresponding
   filters in the next layer. ── */
typedef struct {
    float*  importance;    /* L1 norm per channel */
    int*    keep_mask;     /* 1 = keep, 0 = prune */
    int     num_channels;
    float   pruned_ratio;
} ChannelPrunePlan;

void          channel_prune_init(ChannelPrunePlan* plan, int num_channels);
void          channel_prune_destroy(ChannelPrunePlan* plan);
int           channel_prune_compute_l1(const float* weights, int out_ch,
                                        int in_ch, int kh, int kw,
                                        ChannelPrunePlan* plan);
int           channel_prune_select(const ChannelPrunePlan* plan,
                                    float prune_ratio, int* keep_mask);
int           channel_prune_apply(const float* weights, const int* keep_mask,
                                   int out_ch_orig, int in_ch, int kh, int kw,
                                   float* pruned_weights, int* out_ch_new);

/* ── L2: FLOPs & Parameter Counting ── */
uint64_t      count_params_conv2d(int in_ch, int out_ch, int kh, int kw,
                                   int has_bias);
uint64_t      count_flops_conv2d(int in_ch, int out_ch, int kh, int kw,
                                  int out_h, int out_w);
uint64_t      count_params_fully_connected(int in_dim, int out_dim, int has_bias);
uint64_t      count_total_params(const uint64_t* layer_params, int n_layers);

/* ── L5: Magnitude-Based Weight Pruning (Han et al. 2015, NIPS) ── */
int           magnitude_prune_weights(float* weights, int n, float prune_ratio);
int           magnitude_prune_create_mask(const float* weights, int n,
                                           float prune_ratio, uint8_t* mask);
float         compute_sparsity(const float* weights, int n, float eps);
float         block_sparsity_4x1(const float* weights, int n);

/* ── L5: SQLNR — Signal-to-Quantization-Noise Ratio ──
   SQNR(dB) = 10 * log10(σ²_signal / σ²_noise)
   For uniform quantization: σ²_noise ≈ Δ²/12 where Δ = range/2^bits
   Guides bit-width selection for mixed-precision. ── */
float         compute_sqnr_uniform(const float* data, int n, int bits);
int           assign_bitwidth_sqnr(const float* layer_weights, int n_layers,
                                    const int* layer_sizes, float target_sqnr,
                                    int* bitwidths);

#ifdef __cplusplus
}
#endif

#endif
