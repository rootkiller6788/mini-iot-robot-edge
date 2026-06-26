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

#ifdef __cplusplus
}
#endif

#endif
