#include "edge_inference.h"
#include "inference_opt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MFCC_BINS    40
#define MFCC_FRAMES  49
#define SAMPLE_RATE  16000
#define AUDIO_LEN_MS 1000
#define AUDIO_SAMPLES (SAMPLE_RATE * AUDIO_LEN_MS / 1000)
#define KEYWORD_COUNT 12

static const char* keywords[KEYWORD_COUNT] = {
    "silence", "unknown", "yes", "no", "up", "down", "left", "right",
    "on", "off", "stop", "go"
};

typedef struct {
    float mfcc[MFCC_BINS * MFCC_FRAMES];
} AudioFeatures;

static int extract_mfcc(const int16_t* samples, int num_samples,
                         AudioFeatures* features) {
    if (num_samples < AUDIO_SAMPLES) return -1;
    if (MFCC_BINS * MFCC_FRAMES < 1) return -1;
    for (int f = 0; f < MFCC_FRAMES; ++f) {
        for (int b = 0; b < MFCC_BINS; ++b) {
            float val = 0.0f;
            for (int s = 0; s < 40; ++s) {
                int idx = f * 512 + s;
                if (idx >= 0 && idx < num_samples) {
                    val += (float)samples[idx] * 0.001f *
                           (float)((b + 1) * (s + 1)) / 20480.0f;
                }
            }
            features->mfcc[f * MFCC_BINS + b] = val;
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    int16_t* audio = (int16_t*)calloc((size_t)AUDIO_SAMPLES, sizeof(int16_t));
    if (!audio) { fprintf(stderr, "OOM\n"); return 1; }
    for (int i = 0; i < AUDIO_SAMPLES; ++i)
        audio[i] = (int16_t)((int)(32767.0f *
            (float)((i * 7 + 13) % 1024) / 1024.0f - 16383.5f));

    InferConfig config;
    memset(&config, 0, sizeof(config));
    strncpy(config.model_path, "kw_spotting.tflite", MAX_MODEL_PATH - 1);
    config.model_type = MODEL_TYPE_CLASSIFICATION;
    config.backend = INFER_BACKEND_TFLITE;
    config.input_shape.width = MFCC_BINS;
    config.input_shape.height = MFCC_FRAMES;
    config.input_shape.channels = 1;
    config.input_shape.format = INPUT_FMT_FLOAT32;
    config.preproc = PREPROC_NONE;
    config.postproc = POSTPROC_SOFTMAX;
    config.num_classes = KEYWORD_COUNT;
    config.num_threads = 2;

    InferCtx* ctx = infer_create(&config);
    if (!ctx) { free(audio); fprintf(stderr, "Failed to create context\n"); return 1; }
    fprintf(stdout, "mini-edge-ai: Keyword Spotting example\n");
    fprintf(stdout, "  model=%s, sr=%d Hz, window=%d ms, mfcc=%dx%d\n",
            config.model_path, SAMPLE_RATE, AUDIO_LEN_MS, MFCC_BINS, MFCC_FRAMES);
    AudioFeatures feats;
    memset(&feats, 0, sizeof(feats));
    if (extract_mfcc(audio, AUDIO_SAMPLES, &feats) == 0) {
        infer_set_input_tensor(ctx, feats.mfcc,
                               sizeof(feats.mfcc));
        infer_run(ctx, NULL, 0);
    }
    Classification results[KEYWORD_COUNT];
    int n = infer_get_classifications(ctx, results, KEYWORD_COUNT);
    float probs[KEYWORD_COUNT];
    float raw[KEYWORD_COUNT];
    for (int i = 0; i < n; ++i) raw[i] = results[i].score;
    postproc_softmax_f32(raw, probs, n);
    int best = postproc_argmax_f32(probs, n);
    fprintf(stdout, "Keyword predictions:\n");
    for (int i = 0; i < 5 && i < n; ++i) {
        int idx = results[i].class_id;
        fprintf(stdout, "  %d: %-10s score=%.4f\n", i + 1,
                keywords[idx < KEYWORD_COUNT ? idx : 1], probs[idx]);
    }
    fprintf(stdout, "Detected: \"%s\" (conf=%.3f)\n",
            keywords[best < KEYWORD_COUNT ? best : 1], probs[best]);
    infer_destroy(ctx);
    free(audio);
    return 0;
}
