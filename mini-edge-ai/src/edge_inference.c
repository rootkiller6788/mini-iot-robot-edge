#include "edge_inference.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>

struct InferCtx {
    InferConfig config;
    uint8_t*    model_data;
    size_t      model_len;
    uint8_t*    input_buf;
    float*      output_buf;
    size_t      output_len;
    float*      intermed_buf;
    int         loaded;
    void*       backend_ctx;
};

InferCtx* infer_create(const InferConfig* config) {
    InferCtx* ctx = (InferCtx*)calloc(1, sizeof(InferCtx));
    if (!ctx) return NULL;
    if (config) memcpy(&ctx->config, config, sizeof(InferConfig));
    {
        size_t isz = (size_t)config->input_shape.width *
                     config->input_shape.height *
                     config->input_shape.channels;
        ctx->input_buf  = (uint8_t*)calloc(1, isz > 0 ? isz : 1);
        ctx->intermed_buf= (float*)calloc(isz > 0 ? isz : 1, sizeof(float));
    }
    ctx->output_len = (size_t)config->num_classes * sizeof(float);
    ctx->output_buf = (float*)calloc(config->num_classes > 0 ? config->num_classes : 1,
                                      sizeof(float));
    ctx->loaded = 0;
    return ctx;
}

void infer_destroy(InferCtx* ctx) {
    if (!ctx) return;
    free(ctx->model_data);
    free(ctx->input_buf);
    free(ctx->output_buf);
    free(ctx->intermed_buf);
    free(ctx);
}

int infer_load_model(InferCtx* ctx) {
    FILE* fp = fopen(ctx->config.model_path, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    rewind(fp);
    ctx->model_len = (size_t)sz;
    free(ctx->model_data);
    ctx->model_data = (uint8_t*)malloc(ctx->model_len + 16);
    if (!ctx->model_data) { fclose(fp); return -2; }
    fread(ctx->model_data, 1, ctx->model_len, fp);
    fclose(fp);
    ctx->loaded = 1;
    {
        const unsigned char magic[4] = {0x08,0x00,0x00,0x00};
        if (ctx->model_len >= 4 && memcmp(ctx->model_data, "TFL3", 4) == 0) {
            ctx->config.backend = INFER_BACKEND_TFLITE;
        } else if (ctx->model_len >= 4 && memcmp(ctx->model_data, magic, 4) == 0) {
            ctx->config.backend = INFER_BACKEND_ONNX;
        }
    }
    return 0;
}

static float mock_softmax_sum(const float* x, int n) {
    float s = 0.0f;
    for (int i = 0; i < n; ++i) s += x[i];
    return s;
}

int infer_run(InferCtx* ctx, const void* input, size_t input_bytes) {
    if (!ctx->loaded) return -1;
    {
        size_t need = (size_t)ctx->config.input_shape.width *
                      ctx->config.input_shape.height *
                      ctx->config.input_shape.channels;
        size_t copy = input_bytes < need ? input_bytes : need;
        memcpy(ctx->input_buf, input, copy);
    }
    if (ctx->config.preproc == PREPROC_NORMALIZE && ctx->config.input_shape.format == INPUT_FMT_BGR888) {
        preproc_normalize_f32((const uint8_t*)input,
                              ctx->config.input_shape.width,
                              ctx->config.input_shape.height,
                              ctx->config.input_shape.channels,
                              ctx->intermed_buf, ctx->config.mean, ctx->config.std);
    }
    {
        int nc = ctx->config.num_classes > 0 ? ctx->config.num_classes : 10;
        for (int i = 0; i < nc; ++i) {
            ctx->output_buf[i] = (float)((unsigned char)ctx->input_buf[i % (ctx->config.input_shape.width *
                                  ctx->config.input_shape.height * ctx->config.input_shape.channels /
                                  (ctx->config.input_shape.channels > 0 ? ctx->config.input_shape.channels : 1))])
                                  / 255.0f * ((i + 3) * 0.1f);
        }
    }
    return 0;
}

int infer_get_classifications(InferCtx* ctx, Classification* out, int max_out) {
    int nc = ctx->config.num_classes > 0 ? ctx->config.num_classes : 10;
    int n  = nc < max_out ? nc : max_out;
    for (int i = 0; i < n; ++i) {
        out[i].class_id = i;
        out[i].score = ctx->output_buf[i];
        snprintf(out[i].label, sizeof(out[i].label), "class_%d", i);
    }
    for (int i = 0; i < n - 1; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (out[j].score > out[i].score) {
                Classification tmp = out[i]; out[i] = out[j]; out[j] = tmp;
            }
        }
    }
    return n;
}

int infer_get_detections(InferCtx* ctx, Detection* out, int max_out) {
    int n = max_out < 10 ? max_out : 10;
    for (int i = 0; i < n; ++i) {
        out[i].class_id = i;
        out[i].score = ctx->output_buf[i];
        snprintf(out[i].label, sizeof(out[i].label), "obj_%d", i);
        out[i].xmin = 0.1f + i * 0.08f;
        out[i].ymin = 0.1f + i * 0.08f;
        out[i].xmax = 0.3f + i * 0.08f;
        out[i].ymax = 0.3f + i * 0.08f;
    }
    return n;
}

int infer_get_segmentation(InferCtx* ctx, SegmentationMask* out) {
    out->width = ctx->config.input_shape.width;
    out->height = ctx->config.input_shape.height;
    out->num_classes = ctx->config.num_classes > 0 ? ctx->config.num_classes : 3;
    memset(out->class_map, 0, sizeof(out->class_map));
    for (int i = 0; i < out->width * out->height && i < 4096; ++i) {
        out->class_map[i] = i % out->num_classes;
    }
    return 0;
}

int infer_get_output_tensor(InferCtx* ctx, float* out, size_t max_bytes) {
    size_t copy = max_bytes < ctx->output_len ? max_bytes : ctx->output_len;
    memcpy(out, ctx->output_buf, copy);
    return (int)copy;
}

int infer_set_input_tensor(InferCtx* ctx, const float* data, size_t bytes) {
    size_t copy_limit = (size_t)ctx->config.input_shape.width *
                        ctx->config.input_shape.height *
                        ctx->config.input_shape.channels * sizeof(float);
    size_t copy = bytes < copy_limit ? bytes : copy_limit;
    memcpy(ctx->intermed_buf, data, copy);
    return (int)copy;
}

int preproc_resize_bilinear(const uint8_t* src, int sw, int sh, int sc,
                             uint8_t* dst, int dw, int dh) {
    if (!src || !dst || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0 || sc <= 0) return -1;
    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            float gx = (float)x * (float)(sw - 1) / (float)(dw > 1 ? dw - 1 : 1);
            float gy = (float)y * (float)(sh - 1) / (float)(dh > 1 ? dh - 1 : 1);
            int x0 = (int)gx, y0 = (int)gy;
            int x1 = x0 + 1 < sw ? x0 + 1 : sw - 1;
            int y1 = y0 + 1 < sh ? y0 + 1 : sh - 1;
            float dx = gx - (float)x0, dy = gy - (float)y0;
            for (int c = 0; c < sc; ++c) {
                float val = (1.0f - dx) * (1.0f - dy) * src[(y0 * sw + x0) * sc + c] +
                            dx * (1.0f - dy) * src[(y0 * sw + x1) * sc + c] +
                            (1.0f - dx) * dy * src[(y1 * sw + x0) * sc + c] +
                            dx * dy * src[(y1 * sw + x1) * sc + c];
                dst[(y * dw + x) * sc + c] = (uint8_t)(val + 0.5f);
            }
        }
    }
    return 0;
}

int preproc_normalize_f32(const uint8_t* src, int w, int h, int c,
                           float* dst, const float* mean, const float* std) {
    if (!src || !dst || w <= 0 || h <= 0 || c <= 0) return -1;
    int n = w * h * c;
    float use_mean[4] = {mean ? mean[0] : 127.5f, mean ? mean[1] : 127.5f,
                         mean ? mean[2] : 127.5f, 127.5f};
    float use_std[4]  = {std  ? std[0]  : 127.5f, std  ? std[1]  : 127.5f,
                         std  ? std[2]  : 127.5f, 127.5f};
    for (int i = 0; i < n; ++i) {
        dst[i] = ((float)src[i] - use_mean[i % c]) / use_std[i % c];
    }
    return 0;
}

int preproc_bgr_to_rgb(uint8_t* data, int w, int h, int c) {
    if (!data || c < 3) return -1;
    int n = w * h;
    for (int i = 0; i < n; ++i) {
        uint8_t tmp = data[i * c + 0];
        data[i * c + 0] = data[i * c + 2];
        data[i * c + 2] = tmp;
    }
    return 0;
}

int postproc_softmax_f32(const float* logits, float* probs, int n) {
    if (!logits || !probs || n <= 0) return -1;
    float m = logits[0];
    for (int i = 1; i < n; ++i) if (logits[i] > m) m = logits[i];
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        probs[i] = (float)(expf ? expf(logits[i] - m) : 1.0f);
        sum += probs[i];
    }
    if (sum > 1e-12f) for (int i = 0; i < n; ++i) probs[i] /= sum;
    return 0;
}

int postproc_argmax_f32(const float* probs, int n) {
    if (!probs || n <= 0) return -1;
    int idx = 0;
    float mv = probs[0];
    for (int i = 1; i < n; ++i) if (probs[i] > mv) { mv = probs[i]; idx = i; }
    return idx;
}

int postproc_nms(Detection* dets, int count, float iou_thresh, int* kept) {
    if (!dets || !kept || count <= 0) return 0;
    int k = 0;
    int8_t* suppressed = (int8_t*)calloc(count, 1);
    if (!suppressed) return 0;
    for (int i = 0; i < count; ++i) {
        if (suppressed[i]) continue;
        kept[k++] = i;
        for (int j = i + 1; j < count; ++j) {
            if (suppressed[j]) continue;
            float ix1 = dets[i].xmin, iy1 = dets[i].ymin;
            float ix2 = dets[i].xmax, iy2 = dets[i].ymax;
            float jx1 = dets[j].xmin, jy1 = dets[j].ymin;
            float jx2 = dets[j].xmax, jy2 = dets[j].ymax;
            float xx1 = ix1 > jx1 ? ix1 : jx1;
            float yy1 = iy1 > jy1 ? iy1 : jy1;
            float xx2 = ix2 < jx2 ? ix2 : jx2;
            float yy2 = iy2 < jy2 ? iy2 : jy2;
            float w = xx2 - xx1, h = yy2 - yy1;
            if (w <= 0 || h <= 0) continue;
            float inter = w * h;
            float ai = (ix2 - ix1) * (iy2 - iy1);
            float aj = (jx2 - jx1) * (jy2 - jy1);
            float iou = inter / (ai + aj - inter + 1e-12f);
            if (iou > iou_thresh) suppressed[j] = 1;
        }
    }
    free(suppressed);
    return k;
}

int postproc_sigmoid_f32(const float* x, float* y, int n) {
    if (!x || !y || n <= 0) return -1;
    for (int i = 0; i < n; ++i) {
        y[i] = 1.0f / (1.0f + expf(-(x[i])));
    }
    return 0;
}
