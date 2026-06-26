#include "edge_inference.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char* imagenet_labels[] = {
    "tench", "goldfish", "great_white_shark", "tiger_shark", "hammerhead",
    "electric_ray", "stingray", "cock", "hen", "ostrich"
};
#define NUM_LABELS (sizeof(imagenet_labels) / sizeof(imagenet_labels[0]))

int main(int argc, char* argv[]) {
    int img_w = 224, img_h = 224, img_c = 3;
    size_t img_size = (size_t)img_w * img_h * img_c;
    uint8_t* img_data = (uint8_t*)calloc(1, img_size);
    if (!img_data) { fprintf(stderr, "OOM\n"); return 1; }
    for (size_t i = 0; i < img_size; ++i) img_data[i] = (uint8_t)((i * 37 + 17) % 256);

    InferConfig config;
    memset(&config, 0, sizeof(config));
    strncpy(config.model_path, "mobilenet_v2.tflite", MAX_MODEL_PATH - 1);
    config.model_type = MODEL_TYPE_CLASSIFICATION;
    config.backend = INFER_BACKEND_TFLITE;
    config.input_shape.width = img_w;
    config.input_shape.height = img_h;
    config.input_shape.channels = img_c;
    config.input_shape.format = INPUT_FMT_BGR888;
    config.preproc = PREPROC_NORMALIZE;
    config.postproc = POSTPROC_SOFTMAX;
    config.num_classes = (int)NUM_LABELS;
    config.num_threads = 4;
    {
        config.mean[0] = 103.94f; config.mean[1] = 116.78f; config.mean[2] = 123.68f;
        config.std[0] = 57.38f; config.std[1] = 57.12f; config.std[2] = 58.40f;
    }

    InferCtx* ctx = infer_create(&config);
    if (!ctx) { free(img_data); fprintf(stderr, "Failed to create context\n"); return 1; }
    fprintf(stdout, "mini-edge-ai: Classification example (MobileNet v2)\n");
    fprintf(stdout, "  model=%s, backend=%d, input=%dx%dx%d\n",
            config.model_path, (int)config.backend, img_w, img_h, img_c);
    {
        uint8_t* resized = (uint8_t*)malloc(img_size);
        preproc_resize_bilinear(img_data, 320, 240, img_c, resized, img_w, img_h);
        float* norm = (float*)malloc(img_size * sizeof(float));
        preproc_normalize_f32(resized, img_w, img_h, img_c, norm, config.mean, config.std);
        infer_set_input_tensor(ctx, norm, img_size * sizeof(float));
        infer_run(ctx, NULL, 0);
        free(resized);
        free(norm);
    }
    Classification results[NUM_LABELS];
    int n = infer_get_classifications(ctx, results, (int)NUM_LABELS);
    float probs[NUM_LABELS];
    float logits[NUM_LABELS];
    for (int i = 0; i < n; ++i) logits[i] = results[i].score;
    postproc_softmax_f32(logits, probs, n);
    int top = postproc_argmax_f32(probs, n);
    fprintf(stdout, "Top-5 predictions:\n");
    for (int i = 0; i < 5 && i < n; ++i) {
        fprintf(stdout, "  %2d: %s (score=%.4f)\n", i + 1,
                results[i].label, probs[results[i].class_id]);
    }
    fprintf(stdout, "Best class: %s (class_id=%d)\n",
            imagenet_labels[top % NUM_LABELS], top);
    infer_destroy(ctx);
    free(img_data);
    return 0;
}
