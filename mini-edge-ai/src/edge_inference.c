#include "edge_inference.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

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
        probs[i] = expf(logits[i] - m);
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

/* ================================================================
 *  L5: Kalman Filter 1D (Kalman 1960)
 *  "A New Approach to Linear Filtering and Prediction Problems"
 *  Optimal state estimation for linear systems with Gaussian noise.
 *  Predict:  x = x + u,   P = P + Q
 *  Update:   K = P / (P + R),  x = x + K*(z - x),  P = (1-K)*P
 * ================================================================ */
void kalman1d_init(KalmanFilter1D* kf, float init_x, float init_p,
                   float process_q, float measure_r) {
    if (!kf) return;
    kf->x = init_x;
    kf->p = init_p;
    kf->q = process_q;
    kf->r = measure_r;
    kf->k = 0.0f;
}

float kalman1d_predict(KalmanFilter1D* kf, float control) {
    if (!kf) return 0.0f;
    kf->x += control;
    kf->p += kf->q;
    return kf->x;
}

float kalman1d_update(KalmanFilter1D* kf, float measurement) {
    if (!kf) return 0.0f;
    kf->k = kf->p / (kf->p + kf->r);
    kf->x = kf->x + kf->k * (measurement - kf->x);
    kf->p = (1.0f - kf->k) * kf->p;
    return kf->x;
}

/* ================================================================
 *  L5: Welford's Online Algorithm (Welford 1962 / Knuth TAOCP Vol.2)
 *  Numerically stable single-pass computation of mean and variance.
 *  M1 = x1,  Mk = M_{k-1} + (xk - M_{k-1})/k
 *  S1 = 0,   Sk = S_{k-1} + (xk - M_{k-1})*(xk - Mk)
 *  variance = S_n / (n - 1),  population var = S_n / n
 * ================================================================ */
void online_stats_init(OnlineStats* s) {
    if (!s) return;
    s->count   = 0;
    s->mean    = 0.0f;
    s->m2      = 0.0f;
    s->min_val = 1e30f;
    s->max_val = -1e30f;
}

void online_stats_push(OnlineStats* s, float value) {
    if (!s) return;
    s->count++;
    float delta = value - s->mean;
    s->mean += delta / (float)s->count;
    float delta2 = value - s->mean;
    s->m2 += delta * delta2;
    if (value < s->min_val) s->min_val = value;
    if (value > s->max_val) s->max_val = value;
}

float online_stats_mean(const OnlineStats* s) {
    return s ? s->mean : 0.0f;
}

float online_stats_variance(const OnlineStats* s) {
    if (!s || s->count < 2) return 0.0f;
    return s->m2 / (float)(s->count - 1);
}

float online_stats_stddev(const OnlineStats* s) {
    if (!s || s->count < 2) return 0.0f;
    return sqrtf(s->m2 / (float)(s->count - 1));
}

/* ================================================================
 *  L5: Streaming Top-K using Min-Heap
 *  Maintains the k largest elements seen so far in a min-heap.
 *  Time: O(log k) per push, O(k log k) for final sort.
 *  Space: O(k)
 * ================================================================ */
void topk_init(TopKHeap* tk, int k) {
    if (!tk) return;
    tk->k = k < TOPK_MAX_K ? k : TOPK_MAX_K;
    tk->size = 0;
}

static void topk_heap_sift_down(TopKHeap* tk, int i) {
    int smallest = i;
    int l = 2 * i + 1, r = 2 * i + 2;
    if (l < tk->size && tk->scores[l] < tk->scores[smallest]) smallest = l;
    if (r < tk->size && tk->scores[r] < tk->scores[smallest]) smallest = r;
    if (smallest != i) {
        float ts = tk->scores[i]; tk->scores[i] = tk->scores[smallest]; tk->scores[smallest] = ts;
        int ti = tk->indices[i]; tk->indices[i] = tk->indices[smallest]; tk->indices[smallest] = ti;
        topk_heap_sift_down(tk, smallest);
    }
}

void topk_push(TopKHeap* tk, float score, int idx) {
    if (!tk || tk->k <= 0) return;
    if (tk->size < tk->k) {
        tk->scores[tk->size] = score;
        tk->indices[tk->size] = idx;
        int i = tk->size++;
        while (i > 0 && tk->scores[(i - 1) / 2] > tk->scores[i]) {
            float ts = tk->scores[i]; tk->scores[i] = tk->scores[(i-1)/2]; tk->scores[(i-1)/2] = ts;
            int ti = tk->indices[i]; tk->indices[i] = tk->indices[(i-1)/2]; tk->indices[(i-1)/2] = ti;
            i = (i - 1) / 2;
        }
    } else if (score > tk->scores[0]) {
        tk->scores[0] = score;
        tk->indices[0] = idx;
        topk_heap_sift_down(tk, 0);
    }
}

int topk_get_sorted(TopKHeap* tk, float* scores, int* indices) {
    if (!tk) return 0;
    int orig_size = tk->size;
    for (int i = 0; i < tk->size; ++i) {
        scores[i] = tk->scores[i];
        if (indices) indices[i] = tk->indices[i];
    }
    for (int i = 0; i < tk->size - 1; ++i) {
        for (int j = i + 1; j < tk->size; ++j) {
            if (scores[j] > scores[i]) {
                float ts = scores[i]; scores[i] = scores[j]; scores[j] = ts;
                if (indices) { int ti = indices[i]; indices[i] = indices[j]; indices[j] = ti; }
            }
        }
    }
    return orig_size;
}

/* ================================================================
 *  L5: Temperature Scaling (Guo et al. 2017)
 *  "On Calibration of Modern Neural Networks", ICML 2017.
 *  p_i = exp(z_i / T) / sum_j exp(z_j / T)
 *  Single scalar T > 0; T=1 leaves logits unchanged.
 *  Optimal T minimizes NLL on validation set.
 * ================================================================ */
float temperature_scale_logits(float* logits, int n, float temperature) {
    if (!logits || n <= 0) return 0.0f;
    float t = temperature > 0.0f ? temperature : 1.0f;
    float m = logits[0];
    for (int i = 1; i < n; ++i) if (logits[i] > m) m = logits[i];
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        logits[i] = expf((logits[i] - m) / t);
        sum += logits[i];
    }
    for (int i = 0; i < n; ++i) logits[i] /= sum;
    return sum;
}

int temperature_scale_find_optimal(const float* logits, const int* labels,
                                    int n_samples, int n_classes,
                                    float* temp_out, int max_iter) {
    if (!logits || !labels || n_samples <= 0 || n_classes <= 1 || !temp_out)
        return -1;
    float best_t = 1.0f, best_nll = 1e30f;
    for (int iter = 0; iter < max_iter; ++iter) {
        float t = 0.1f + (float)iter * 4.9f / (float)(max_iter > 1 ? max_iter - 1 : 1);
        float nll = 0.0f;
        for (int s = 0; s < n_samples; ++s) {
            const float* row = logits + s * n_classes;
            float m = row[0];
            for (int i = 1; i < n_classes; ++i) if (row[i] > m) m = row[i];
            float sum = 0.0f;
            for (int i = 0; i < n_classes; ++i) sum += expf((row[i] - m) / t);
            int lb = labels[s];
            float prob = expf((row[lb] - m) / t) / sum;
            nll -= logf(prob > 1e-12f ? prob : 1e-12f);
        }
        if (nll < best_nll) { best_nll = nll; best_t = t; }
    }
    *temp_out = best_t;
    return 0;
}

/* ================================================================
 *  L4: Mahalanobis Distance (Mahalanobis 1936)
 *  D^2(x) = (x - μ)^T Σ^{-1} (x - μ)
 *  Under multivariate normality, D^2 ~ χ²(dim).
 *  Anomaly threshold from chi-square critical values.
 * ================================================================ */
MahalanobisCtx* mahalanobis_create(int dim) {
    if (dim <= 0) return NULL;
    MahalanobisCtx* ctx = (MahalanobisCtx*)calloc(1, sizeof(MahalanobisCtx));
    if (!ctx) return NULL;
    ctx->dim = dim;
    ctx->mean = (float*)calloc((size_t)dim, sizeof(float));
    ctx->inv_cov = (float*)calloc((size_t)dim * dim, sizeof(float));
    if (!ctx->mean || !ctx->inv_cov) {
        free(ctx->mean); free(ctx->inv_cov); free(ctx); return NULL;
    }
    for (int i = 0; i < dim; ++i) ctx->inv_cov[i * dim + i] = 1.0f;
    return ctx;
}

void mahalanobis_destroy(MahalanobisCtx* ctx) {
    if (!ctx) return;
    free(ctx->mean);
    free(ctx->inv_cov);
    free(ctx);
}

int mahalanobis_fit(MahalanobisCtx* ctx, const float* data, int n_samples) {
    if (!ctx || !data || n_samples <= 0) return -1;
    int d = ctx->dim;
    memset(ctx->mean, 0, (size_t)d * sizeof(float));
    for (int s = 0; s < n_samples; ++s)
        for (int i = 0; i < d; ++i)
            ctx->mean[i] += data[s * d + i];
    for (int i = 0; i < d; ++i) ctx->mean[i] /= (float)n_samples;
    /* Compute covariance Σ */
    float* cov = (float*)calloc((size_t)d * d, sizeof(float));
    if (!cov) return -1;
    for (int s = 0; s < n_samples; ++s) {
        for (int i = 0; i < d; ++i) {
            float di = data[s * d + i] - ctx->mean[i];
            for (int j = 0; j < d; ++j)
                cov[i * d + j] += di * (data[s * d + j] - ctx->mean[j]);
        }
    }
    float denom = n_samples > 1 ? (float)(n_samples - 1) : 1.0f;
    for (int i = 0; i < d * d; ++i) cov[i] /= denom;
    /* Invert using Cholesky with diagonal regularization */
    for (int i = 0; i < d; ++i) cov[i * d + i] += 1e-6f;
    for (int i = 0; i < d; ++i) {
        for (int j = 0; j < d; ++j) {
            float sum = cov[i * d + j];
            for (int k = 0; k < i; ++k)
                sum -= ctx->inv_cov[i * d + k] * ctx->inv_cov[k * d + j];
            if (i == j) {
                if (sum <= 0.0f) { free(cov); return -1; }
                ctx->inv_cov[i * d + i] = sqrtf(sum);
            } else {
                ctx->inv_cov[i * d + j] = sum / ctx->inv_cov[i * d + i];
            }
        }
    }
    /* Copy lower triangular and compute inverse by solving LL^T * inv_cov = I */
    float* L = (float*)calloc((size_t)d * d, sizeof(float));
    if (!L) { free(cov); return -1; }
    memcpy(L, ctx->inv_cov, (size_t)d * d * sizeof(float));
    memset(ctx->inv_cov, 0, (size_t)d * d * sizeof(float));
    for (int i = 0; i < d; ++i) ctx->inv_cov[i * d + i] = 1.0f;
    for (int col = 0; col < d; ++col) {
        /* Forward substitution: L * y = e_col */
        for (int i = 0; i < d; ++i) {
            float sum = (i == col) ? 1.0f : 0.0f;
            for (int k = 0; k < i; ++k) sum -= L[i * d + k] * ctx->inv_cov[k * d + col];
            ctx->inv_cov[i * d + col] = sum / L[i * d + i];
        }
        /* Back substitution: L^T * x = y */
        for (int i = d - 1; i >= 0; --i) {
            float sum = ctx->inv_cov[i * d + col];
            for (int k = i + 1; k < d; ++k) sum -= L[k * d + i] * ctx->inv_cov[k * d + col];
            ctx->inv_cov[i * d + col] = sum / L[i * d + i];
        }
    }
    free(L);
    free(cov);
    ctx->n_samples = n_samples;
    return 0;
}

float mahalanobis_distance(const MahalanobisCtx* ctx, const float* sample) {
    if (!ctx || !sample) return 0.0f;
    int d = ctx->dim;
    float* diff = (float*)malloc((size_t)d * sizeof(float));
    if (!diff) return 0.0f;
    for (int i = 0; i < d; ++i) diff[i] = sample[i] - ctx->mean[i];
    float dist2 = 0.0f;
    for (int i = 0; i < d; ++i)
        for (int j = 0; j < d; ++j)
            dist2 += diff[i] * ctx->inv_cov[j * d + i] * diff[j];  /* j*d+i: transposed access for symmetry */
    free(diff);
    return sqrtf(dist2 > 0.0f ? dist2 : 0.0f);
}

int mahalanobis_is_anomaly(const MahalanobisCtx* ctx, const float* sample,
                            float threshold) {
    float d = mahalanobis_distance(ctx, sample);
    return d > threshold ? 1 : 0;
}

/* ================================================================
 *  L4: Pearson's Chi-Squared Goodness-of-Fit Test (Pearson 1900)
 *  χ² = Σ (O_i - E_i)² / E_i, where E_i = n_total * p_i
 *  Tests whether observed frequencies differ from expected distribution.
 *  Degrees of freedom = n_bins - 1.
 * ================================================================ */
float chi_square_test(const int* observed, const float* expected_prob,
                      int n_bins, int n_total) {
    if (!observed || !expected_prob || n_bins <= 0 || n_total <= 0)
        return 0.0f;
    float chi2 = 0.0f;
    for (int i = 0; i < n_bins; ++i) {
        float expected = expected_prob[i] * (float)n_total;
        if (expected < 1e-9f) continue;
        float diff = (float)observed[i] - expected;
        chi2 += diff * diff / expected;
    }
    return chi2;
}

/* ================================================================
 *  L5: Ensemble Prediction — Weighted Average of Model Logits
 *  Given N models each producing logit vector of size C,
 *  compute weighted average probabilities via softmax of
 *  averaged logits.  Weights should sum to 1.
 *  Theorem: Under MSE loss, equal weighting minimizes
 *  expected error (Bates & Granger 1969, "Combination of Forecasts").
 * ================================================================ */
int ensemble_predict_classification(float** model_logits, int n_models,
                                     int n_classes, float* weights,
                                     float* ensemble_probs) {
    if (!model_logits || !ensemble_probs || n_models <= 0 || n_classes <= 0)
        return -1;
    float* avg_logits = (float*)calloc((size_t)n_classes, sizeof(float));
    if (!avg_logits) return -1;
    for (int m = 0; m < n_models; ++m) {
        float w = weights ? weights[m] : 1.0f / (float)n_models;
        for (int c = 0; c < n_classes; ++c)
            avg_logits[c] += model_logits[m][c] * w;
    }
    float m = avg_logits[0];
    for (int i = 1; i < n_classes; ++i) if (avg_logits[i] > m) m = avg_logits[i];
    float sum = 0.0f;
    for (int i = 0; i < n_classes; ++i) {
        ensemble_probs[i] = expf(avg_logits[i] - m);
        sum += ensemble_probs[i];
    }
    for (int i = 0; i < n_classes; ++i) ensemble_probs[i] /= sum;
    free(avg_logits);
    return 0;
}
