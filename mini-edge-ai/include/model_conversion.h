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

#ifdef __cplusplus
}
#endif

#endif
