#ifndef EDGE_INFERENCE_H
#define EDGE_INFERENCE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MODEL_PATH 256
#define MAX_CLASSES    1000
#define MAX_DETECTIONS 100
#define MAX_SEG_CLASSES 50

typedef enum {
    INFER_BACKEND_TFLITE,
    INFER_BACKEND_ONNX,
    INFER_BACKEND_OPENVINO,
    INFER_BACKEND_NATIVE,
    INFER_BACKEND_COUNT
} InferBackend;

typedef enum {
    INPUT_FMT_BGR888,
    INPUT_FMT_RGB888,
    INPUT_FMT_GRAY8,
    INPUT_FMT_FLOAT32,
    INPUT_FMT_INT8,
    INPUT_FMT_COUNT
} InputFormat;

typedef enum {
    MODEL_TYPE_CLASSIFICATION,
    MODEL_TYPE_DETECTION,
    MODEL_TYPE_SEGMENTATION,
    MODEL_TYPE_REGRESSION,
    MODEL_TYPE_COUNT
} ModelType;

typedef enum {
    PREPROC_NONE,
    PREPROC_RESIZE,
    PREPROC_NORMALIZE,
    PREPROC_MEAN_STD,
    PREPROC_COUNT
} PreprocMode;

typedef enum {
    POSTPROC_SOFTMAX,
    POSTPROC_SIGMOID,
    POSTPROC_NMS,
    POSTPROC_ARGMAX,
    POSTPROC_COUNT
} PostprocMode;

typedef struct {
    int width;
    int height;
    int channels;
    InputFormat format;
} InputShape;

typedef struct {
    int class_id;
    char label[64];
    float score;
} Classification;

typedef struct {
    int class_id;
    char label[64];
    float score;
    float xmin, ymin, xmax, ymax;
} Detection;

typedef struct {
    int width;
    int height;
    int num_classes;
    int32_t class_map[4096];
} SegmentationMask;

typedef struct {
    char model_path[MAX_MODEL_PATH];
    ModelType model_type;
    InferBackend backend;
    InputShape input_shape;
    PreprocMode preproc;
    PostprocMode postproc;
    float mean[4];
    float std[4];
    int num_classes;
    int num_threads;
    uint8_t use_nhwc;
} InferConfig;

typedef struct InferCtx InferCtx;

InferCtx*     infer_create(const InferConfig* config);
void          infer_destroy(InferCtx* ctx);
int           infer_load_model(InferCtx* ctx);
int           infer_run(InferCtx* ctx, const void* input, size_t input_bytes);
int           infer_get_classifications(InferCtx* ctx, Classification* out, int max_out);
int           infer_get_detections(InferCtx* ctx, Detection* out, int max_out);
int           infer_get_segmentation(InferCtx* ctx, SegmentationMask* out);
int           infer_get_output_tensor(InferCtx* ctx, float* out, size_t max_bytes);
int           infer_set_input_tensor(InferCtx* ctx, const float* data, size_t bytes);

int           preproc_resize_bilinear(const uint8_t* src, int sw, int sh, int sc,
                                      uint8_t* dst, int dw, int dh);
int           preproc_normalize_f32(const uint8_t* src, int w, int h, int c,
                                    float* dst, const float* mean, const float* std);
int           preproc_bgr_to_rgb(uint8_t* data, int w, int h, int c);

int           postproc_softmax_f32(const float* logits, float* probs, int n);
int           postproc_argmax_f32(const float* probs, int n);
int           postproc_nms(Detection* dets, int count, float iou_thresh, int* kept);
int           postproc_sigmoid_f32(const float* x, float* y, int n);

/* ── L5: Kalman Filter (Kalman 1960) — optimal linear quadratic estimation ── */
typedef struct {
    float x;        /* state estimate */
    float p;        /* error covariance */
    float q;        /* process noise covariance */
    float r;        /* measurement noise covariance */
    float k;        /* Kalman gain */
} KalmanFilter1D;

void          kalman1d_init(KalmanFilter1D* kf, float init_x, float init_p,
                            float process_q, float measure_r);
float         kalman1d_predict(KalmanFilter1D* kf, float control);
float         kalman1d_update(KalmanFilter1D* kf, float measurement);

/* ── L5: Welford's online algorithm (Welford 1962 / Knuth TAOCP Vol.2) ── */
typedef struct {
    int     count;
    float   mean;
    float   m2;       /* sum of squared differences from current mean */
    float   min_val;
    float   max_val;
} OnlineStats;

void          online_stats_init(OnlineStats* s);
void          online_stats_push(OnlineStats* s, float value);
float         online_stats_mean(const OnlineStats* s);
float         online_stats_variance(const OnlineStats* s);
float         online_stats_stddev(const OnlineStats* s);

/* ── L5: Streaming Top-K via Min-Heap ── */
#define TOPK_MAX_K 100
typedef struct {
    float   scores[TOPK_MAX_K];
    int     indices[TOPK_MAX_K];
    int     k;
    int     size;
} TopKHeap;

void          topk_init(TopKHeap* tk, int k);
void          topk_push(TopKHeap* tk, float score, int idx);
int           topk_get_sorted(TopKHeap* tk, float* scores, int* indices);

/* ── L5: Temperature Scaling (Guo et al. 2017, "On Calibration of Modern NNs") ── */
float         temperature_scale_logits(float* logits, int n, float temperature);
int           temperature_scale_find_optimal(const float* logits, const int* labels,
                                              int n_samples, int n_classes,
                                              float* temp_out, int max_iter);

/* ── L4: Mahalanobis Distance (Mahalanobis 1936) — multivariate anomaly detection ── */
typedef struct {
    int      dim;
    float*   mean;
    float*   inv_cov;    /* inverse covariance, flattened row-major */
    int      n_samples;
} MahalanobisCtx;

MahalanobisCtx* mahalanobis_create(int dim);
void            mahalanobis_destroy(MahalanobisCtx* ctx);
int             mahalanobis_fit(MahalanobisCtx* ctx, const float* data, int n_samples);
float           mahalanobis_distance(const MahalanobisCtx* ctx, const float* sample);
int             mahalanobis_is_anomaly(const MahalanobisCtx* ctx, const float* sample,
                                       float threshold);

/* ── L4: Pearson's Chi-Squared Goodness-of-Fit Test (Pearson 1900) ── */
float         chi_square_test(const int* observed, const float* expected_prob,
                              int n_bins, int n_total);

/* ── L5: Ensemble Prediction via Weighted Average ── */
int           ensemble_predict_classification(float** model_logits, int n_models,
                                              int n_classes, float* weights,
                                              float* ensemble_probs);

#ifdef __cplusplus
}
#endif

#endif
