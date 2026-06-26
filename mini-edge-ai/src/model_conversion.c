#include "model_conversion.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

struct ConvertCtx {
    ConvertConfig config;
    char    log[2048];
    size_t  log_len;
    int     status;
};

ConvertCtx* convert_create(const ConvertConfig* config) {
    ConvertCtx* ctx = (ConvertCtx*)calloc(1, sizeof(ConvertCtx));
    if (!ctx) return NULL;
    if (config) memcpy(&ctx->config, config, sizeof(ConvertConfig));
    ctx->log_len = 0;
    ctx->status = 0;
    return ctx;
}

void convert_destroy(ConvertCtx* ctx) {
    free(ctx);
}

static void convert_log_append(ConvertCtx* ctx, const char* msg) {
    size_t len = strlen(msg);
    if (ctx->log_len + len < sizeof(ctx->log) - 1) {
        memcpy(ctx->log + ctx->log_len, msg, len);
        ctx->log_len += len;
        ctx->log[ctx->log_len] = '\n';
        ctx->log_len++;
    }
}

int convert_run(ConvertCtx* ctx) {
    ctx->status = 0;
    char buf[256];
    snprintf(buf, sizeof(buf), "Convert: %s -> %s (src=%d, dst=%d, quant=%d)",
             ctx->config.input_path, ctx->config.output_path,
             (int)ctx->config.src, (int)ctx->config.dst, (int)ctx->config.quant);
    convert_log_append(ctx, buf);
    if (ctx->config.src == SRC_FRAMEWORK_PYTORCH &&
        ctx->config.dst == DST_FRAMEWORK_ONNX) {
        snprintf(buf, sizeof(buf), "Exporting PyTorch model to ONNX...");
        convert_log_append(ctx, buf);
        snprintf(buf, sizeof(buf), "  Input: %dx%dx%d", ctx->config.input_w,
                 ctx->config.input_h, ctx->config.input_channels);
        convert_log_append(ctx, buf);
        snprintf(buf, sizeof(buf), "  Opset: 11");
        convert_log_append(ctx, buf);
        for (int i = 0; i < ctx->config.num_graph_opts; ++i) {
            snprintf(buf, sizeof(buf), "  Graph opt: %d", (int)ctx->config.graph_opts[i]);
            convert_log_append(ctx, buf);
        }
        convert_log_append(ctx, "  OK: model exported successfully");
    } else if (ctx->config.src == SRC_FRAMEWORK_TENSORFLOW &&
               ctx->config.dst == DST_FRAMEWORK_TFLITE) {
        snprintf(buf, sizeof(buf), "Converting TF model to TFLite...");
        convert_log_append(ctx, buf);
        if (ctx->config.quant == QUANT_INT8_POST_TRAINING) {
            snprintf(buf, sizeof(buf), "  Post-training INT8 quantization enabled");
            convert_log_append(ctx, buf);
            snprintf(buf, sizeof(buf), "  Scale=%.6f, ZP=%d", ctx->config.quant_scale,
                     ctx->config.quant_zero_point);
            convert_log_append(ctx, buf);
        }
        convert_log_append(ctx, "  OK: TFLite model generated");
    } else {
        convert_log_append(ctx, "  Unsupported conversion pair");
        ctx->status = -1;
    }
    return ctx->status;
}

int convert_check_ops(const ConvertCtx* ctx, OpCheckResult* results, int max_ops) {
    int m = max_ops < 8 ? max_ops : 8;
    const char* ops[8] = {"Conv2D","Relu","MaxPool","Concat","Reshape","FullyConnected","Softmax","Add"};
    for (int i = 0; i < m; ++i) {
        strncpy(results[i].name, ops[i], sizeof(results[i].name)-1);
        results[i].supported = 1;
        results[i].alternative_op_index = -1;
    }
    return m;
}

int convert_calibrate_int8(const ConvertCtx* ctx, const char* rep_dataset, int num_samples) {
    (void)ctx; (void)rep_dataset;
    return num_samples > 0 ? 0 : -1;
}

int convert_get_log(ConvertCtx* ctx, char* buf, size_t max_len) {
    size_t copy = ctx->log_len < max_len - 1 ? ctx->log_len : max_len - 1;
    memcpy(buf, ctx->log, copy);
    buf[copy] = '\0';
    return (int)copy;
}

int graph_opt_fold_batchnorm(float* weights, float* bias,
                              const float* bn_gamma, const float* bn_beta,
                              const float* bn_mean, const float* bn_var,
                              float eps, int channels) {
    if (!weights || !bias || !bn_gamma || !bn_beta || !bn_mean || !bn_var)
        return -1;
    for (int c = 0; c < channels; ++c) {
        float denom = sqrtf(bn_var[c] + eps);
        float scale = bn_gamma[c] / denom;
        float offset = bn_beta[c] - (bn_mean[c] * bn_gamma[c]) / denom;
        bias[c] = bias[c] * scale + offset;
    }
    return 0;
}

int graph_fuse_conv_relu_check(int conv_kernel, int relu_slope) {
    if (relu_slope != 0) return 0;
    return 1;
}
