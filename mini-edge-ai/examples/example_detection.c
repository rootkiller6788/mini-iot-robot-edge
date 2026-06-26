#include "edge_inference.h"
#include "inference_opt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char* coco_labels[] = {
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck",
    "boat", "traffic_light", "fire_hydrant", "stop_sign", "parking_meter", "bench",
    "bird", "cat", "dog", "horse", "sheep", "cow"
};
#define NUM_COCO (sizeof(coco_labels) / sizeof(coco_labels[0]))

int main(int argc, char* argv[]) {
    int img_w = 300, img_h = 300, img_c = 3;
    size_t img_size = (size_t)img_w * img_h * img_c;
    uint8_t* img_data = (uint8_t*)calloc(1, img_size);
    if (!img_data) { fprintf(stderr, "OOM\n"); return 1; }
    for (size_t i = 0; i < img_size; ++i) img_data[i] = (uint8_t)((i * 53 + 31) % 256);

    InferConfig config;
    memset(&config, 0, sizeof(config));
    strncpy(config.model_path, "ssd_mobilenet_v2.onnx", MAX_MODEL_PATH - 1);
    config.model_type = MODEL_TYPE_DETECTION;
    config.backend = INFER_BACKEND_ONNX;
    config.input_shape.width = img_w;
    config.input_shape.height = img_h;
    config.input_shape.channels = img_c;
    config.input_shape.format = INPUT_FMT_RGB888;
    config.preproc = PREPROC_RESIZE;
    config.postproc = POSTPROC_NMS;
    config.num_classes = (int)NUM_COCO;
    config.num_threads = 4;
    config.use_nhwc = 1;

    InferCtx* ctx = infer_create(&config);
    if (!ctx) { free(img_data); fprintf(stderr, "Failed to create context\n"); return 1; }
    fprintf(stdout, "mini-edge-ai: Detection example (SSD-MobileNet v2)\n");
    fprintf(stdout, "  model=%s, backend=%d, input=%dx%dx%d, classes=%d\n",
            config.model_path, (int)config.backend, img_w, img_h, img_c, config.num_classes);
    {
        uint8_t* resized = (uint8_t*)malloc(img_size);
        preproc_resize_bilinear(img_data, 640, 480, img_c, resized, img_w, img_h);
        preproc_bgr_to_rgb(resized, img_w, img_h, img_c);
        infer_run(ctx, resized, img_size);
        free(resized);
    }
    Detection dets[MAX_DETECTIONS];
    int nd = infer_get_detections(ctx, dets, MAX_DETECTIONS);
    float scores[MAX_DETECTIONS];
    for (int i = 0; i < nd; ++i) {
        float* raw = (float*)dets;
        postproc_sigmoid_f32(raw + i * 6, scores + i, 1);
    }
    int kept[MAX_DETECTIONS];
    int nk = postproc_nms(dets, nd, 0.45f, kept);
    fprintf(stdout, "Detections (raw=%d, after NMS=%d):\n", nd, nk);
    for (int i = 0; i < nk && i < 10; ++i) {
        int idx = kept[i];
        int cid = dets[idx].class_id;
        const char* label = cid < (int)NUM_COCO ? coco_labels[cid] : "unknown";
        fprintf(stdout, "  %s: bbox=(%.2f,%.2f,%.2f,%.2f) conf=%.3f\n",
                label, dets[idx].xmin, dets[idx].ymin,
                dets[idx].xmax, dets[idx].ymax, dets[idx].score);
    }
    fprintf(stdout, "  * First detection: xmin=%.3f ymin=%.3f xmax=%.3f ymax=%.3f\n",
            dets[0].xmin, dets[0].ymin, dets[0].xmax, dets[0].ymax);
    infer_destroy(ctx);
    free(img_data);
    return 0;
}
