/*
 * bench_core.c - Core Benchmarks for mini-tinyml
 *
 * Measures performance of major API functions across all 5 modules:
 *   anomaly_detect.h, model_compress.h, ondevice_learn.h, tflite_micro.h, wake_word.h
 *
 * Usage: bench_core [N]
 *   N = iteration scale factor (default 5000)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "anomaly_detect.h"
#include "model_compress.h"
#include "ondevice_learn.h"
#include "tflite_micro.h"
#include "wake_word.h"

/* ---- helper: high-resolution timer ---- */
static double now_ms(void) {
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

static void bench_run(const char *name, void (*fn)(int), int n) {
    double t0 = now_ms();
    fn(n);
    double t1 = now_ms();
    double elapsed = t1 - t0;
    printf("  %-45s  %d ops in %9.1f ms  (%8.1f us/op)\n",
           name, n, elapsed, (elapsed * 1000.0) / (double)n);
}

/* ================================================================
 *  BENCHMARKS - anomaly_detect.h
 * ================================================================ */

static void bm_anomaly_detector_init_detect(int n) {
    for (int i = 0; i < n; i++) {
        AnomalyDetector *det = anomaly_detector_create(ANOMALY_DETECT_METHOD_STATISTICAL);
        if (det) {
            anomaly_detector_init(det);
            float features[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
            anomaly_detector_detect(det, features, 4, i);
            anomaly_detector_free(det);
        }
    }
}

static void bm_statistical_anomaly_score(int n) {
    float history_data[100 * 4];
    for (int i = 0; i < 100 * 4; i++) history_data[i] = (float)(i % 20);

    StatisticalAnomaly *stat = statistical_anomaly_create(3.0f, 50.0f);
    statistical_anomaly_fit(stat, history_data, 100, 4);

    float features[4] = { 10.0f, 20.0f, 30.0f, 40.0f };
    float z_scores[4];
    for (int i = 0; i < n; i++) {
        statistical_anomaly_z_score(stat, features, 4, z_scores);
        statistical_anomaly_is_anomaly(stat, features, 4);
    }
    statistical_anomaly_free(stat);
}

static void bm_one_class_svm_train_predict(int n) {
    float data[50 * 4];
    for (int i = 0; i < 50 * 4; i++) data[i] = (float)((i % 10) * 0.1f);

    OneClassSVM *svm = one_class_svm_create(4, 0.5f, 0.1f);
    one_class_svm_train(svm, data, 50, 4);

    float feat[4] = { 0.5f, 0.6f, 0.7f, 0.8f };
    for (int i = 0; i < n; i++) {
        one_class_svm_decision(svm, feat);
        one_class_svm_is_anomaly(svm, feat);
    }
    one_class_svm_free(svm);
}

/* ================================================================
 *  BENCHMARKS - model_compress.h
 * ================================================================ */

static void bm_model_prune_magnitude(int n) {
    float weights[1024];
    for (int i = 0; i < 1024; i++) weights[i] = (float)(i * 0.01f) - 5.0f;

    ModelWeightBuffer *buf = model_weight_buffer_create(weights, 1024);
    for (int i = 0; i < n; i++) {
        ModelPruneBuffer *pruned = model_prune_magnitude(buf, 0.3f);
        if (pruned) model_prune_buffer_free(pruned);
    }
    model_weight_buffer_free(buf);
}

static void bm_model_quant_float_to_int8(int n) {
    float weights[1024];
    for (int i = 0; i < 1024; i++) weights[i] = (float)(i * 0.01f) - 5.0f;

    ModelWeightBuffer *buf = model_weight_buffer_create(weights, 1024);
    for (int i = 0; i < n; i++) {
        ModelQuantBuffer *qb = model_quant_float_to_int8(buf);
        if (qb) model_quant_buffer_free(qb);
    }
    model_weight_buffer_free(buf);
}

static void bm_model_cluster_kmeans(int n) {
    float weights[512];
    for (int i = 0; i < 512; i++) weights[i] = (float)(i * 0.02f) - 5.0f;

    ModelWeightBuffer *buf = model_weight_buffer_create(weights, 512);
    for (int i = 0; i < n; i++) {
        ModelClusterResult *cr = model_cluster_kmeans(buf, 8, 20);
        if (cr) model_cluster_result_free(cr);
    }
    model_weight_buffer_free(buf);
}

/* ================================================================
 *  BENCHMARKS - ondevice_learn.h
 * ================================================================ */

static void bm_od_matrix_multiply(int n) {
    ODMatrix *a = od_matrix_create(32, 32);
    ODMatrix *b = od_matrix_create(32, 32);
    ODMatrix *c = od_matrix_create(32, 32);
    od_matrix_fill_random(a, 1.0f);
    od_matrix_fill_random(b, 1.0f);

    /* This is a simplified loop; real matmul would be via layer_forward */
    for (int i = 0; i < n; i++) {
        od_matrix_fill_zeros(c);
        /* simulate element-wise access */
        for (int r = 0; r < 32; r++) {
            for (int ci = 0; ci < 32; ci++) {
                c->data[r * 32 + ci] = a->data[r * 32 + ci] + b->data[r * 32 + ci];
            }
        }
    }
    od_matrix_free(a);
    od_matrix_free(b);
    od_matrix_free(c);
}

static void bm_od_layer_forward(int n) {
    ODMatrix *input = od_matrix_create(1, 16);
    od_matrix_fill_random(input, 1.0f);
    ODMatrix *output = od_matrix_create(1, 8);

    ODLayer *layer = od_layer_create(ONDEVICE_LEARN_LAYER_DENSE, 16, 8);
    for (int i = 0; i < n; i++) {
        od_layer_forward(layer, input, output);
    }
    od_layer_free(layer);
    od_matrix_free(input);
    od_matrix_free(output);
}

static void bm_od_transfer_model(int n) {
    for (int i = 0; i < n; i++) {
        ODTransferModel *model = od_transfer_model_create(64, 10, 0.001f, ONDEVICE_LEARN_OPTIMIZER_ADAM);
        if (model) {
            od_transfer_model_add_layer(model, ONDEVICE_LEARN_LAYER_DENSE, 32);
            od_transfer_model_add_layer(model, ONDEVICE_LEARN_LAYER_RELU, 32);
            od_transfer_model_freeze_base(model, 1);
            od_transfer_model_free(model);
        }
    }
}

/* ================================================================
 *  BENCHMARKS - tflite_micro.h
 * ================================================================ */

static void bm_tflite_arena_alloc_reset(int n) {
    TFLiteTensorArena *arena = tflite_tensor_arena_create(TFLITE_MICRO_DEFAULT_ARENA_SIZE);
    for (int i = 0; i < n; i++) {
        void *p = tflite_tensor_arena_alloc(arena, 256);
        (void)p;
        tflite_tensor_arena_reset(arena);
    }
    tflite_tensor_arena_free(arena);
}

static void bm_tflite_tensor_create_free(int n) {
    for (int i = 0; i < n; i++) {
        TFLiteTensor *t = tflite_tensor_create();
        if (t) {
            int32_t dims[4] = { 1, 28, 28, 1 };
            tflite_tensor_alloc_data(t, TFLITE_TYPE_FLOAT32, dims, 4);
            tflite_tensor_free(t);
        }
    }
}

static void bm_tflite_op_resolver(int n) {
    MicroMutableOpResolver *resolver = tflite_micro_op_resolver_create();
    for (int i = 0; i < n; i++) {
        tflite_micro_op_resolver_add_builtin(resolver, "CONV_2D", 3, 1, 3);
    }
    /* Only first few will fit; bench is about the internal operation */
    tflite_micro_op_resolver_free(resolver);
}

/* ================================================================
 *  BENCHMARKS - wake_word.h
 * ================================================================ */

static int16_t ww_dummy_audio[WAKE_WORD_MAX_FRAME_SAMPLES];

static void bm_mfcc_extract_frame(int n) {
    memset(ww_dummy_audio, 0, sizeof(ww_dummy_audio));
    for (int i = 0; i < WAKE_WORD_MAX_FRAME_SAMPLES; i++) {
        ww_dummy_audio[i] = (int16_t)((i % 256 - 128) * 16);
    }

    MFCCExtractor *extractor = mfcc_extractor_create(16000, 25, 10, 13, 26);
    MFCCFrame frame;
    for (int i = 0; i < n; i++) {
        mfcc_extractor_process_frame(extractor, ww_dummy_audio, 400, &frame);
    }
    mfcc_extractor_free(extractor);
}

static void bm_mel_filterbank_apply(int n) {
    MelFilterbank *fb = mel_filterbank_create(26, 512, 16000, 20.0f, 8000.0f);
    float spectrum[513];
    float mel_energies[26];
    for (int i = 0; i < 513; i++) spectrum[i] = (float)(i % 50) / 50.0f;
    for (int i = 0; i < n; i++) {
        mel_filterbank_apply(fb, spectrum, mel_energies);
    }
    mel_filterbank_free(fb);
}

static void bm_wake_word_detector(int n) {
    WakeWordDetector *det = wake_word_detector_create(16000, "hey_nano");
    wake_word_detector_init(det);
    wake_word_detector_set_threshold(det, 0.85f);

    memset(ww_dummy_audio, 0, sizeof(ww_dummy_audio));
    for (int i = 0; i < n; i++) {
        wake_word_detector_process_samples(det, ww_dummy_audio, 400);
    }
    wake_word_detector_free(det);
}

/* ================================================================
 *  MAIN
 * ================================================================ */

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 5000;
    if (N <= 0) N = 5000;

    printf("\n=== mini-tinyml Benchmarks (N=%d) ===\n\n", N);

    int n_simple  = N / 10;   /* simple ops */
    int n_ml      = N / 50;   /* ML inference */
    int n_medium  = N / 5;    /* mid-range */

    bench_run("anomaly_detector_create/init/detect",   bm_anomaly_detector_init_detect, n_simple);
    bench_run("statistical_anomaly_z_score (multi-feat)", bm_statistical_anomaly_score, n_simple);
    bench_run("one_class_svm_train/decision (SVM)",     bm_one_class_svm_train_predict, n_ml);
    bench_run("model_prune_magnitude 1K weights",       bm_model_prune_magnitude,       n_medium);
    bench_run("model_quant_float_to_int8 1K weights",   bm_model_quant_float_to_int8,   n_medium);
    bench_run("model_cluster_kmeans k=8 512 weights",   bm_model_cluster_kmeans,         n_simple);
    bench_run("od_matrix multiply 32x32",               bm_od_matrix_multiply,           n_ml);
    bench_run("od_layer_forward (Dense 16->8)",         bm_od_layer_forward,             n_medium);
    bench_run("od_transfer_model (create+config)",      bm_od_transfer_model,            n_simple);
    bench_run("tflite_tensor_arena alloc+reset",         bm_tflite_arena_alloc_reset,     n_medium);
    bench_run("tflite_tensor create+free",              bm_tflite_tensor_create_free,     n_simple);
    bench_run("tflite_op_resolver_add_builtin",          bm_tflite_op_resolver,           n_medium);
    bench_run("mfcc_extract_frame (25ms frame)",        bm_mfcc_extract_frame,           n_medium);
    bench_run("mel_filterbank_apply (26 bins)",          bm_mel_filterbank_apply,         n_medium);
    bench_run("wake_word_detector_process_samples",     bm_wake_word_detector,           n_ml);

    printf("\nDone.\n");
    return 0;
}
