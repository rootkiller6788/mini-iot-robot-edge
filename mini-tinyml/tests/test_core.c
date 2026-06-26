/*
 * test_core.c - Core Unit Tests for mini-tinyml
 *
 * Tests all 5 modules: anomaly_detect.h, model_compress.h, ondevice_learn.h,
 *   tflite_micro.h, wake_word.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "anomaly_detect.h"
#include "model_compress.h"
#include "ondevice_learn.h"
#include "tflite_micro.h"
#include "wake_word.h"

/* ---- test harness ---- */
static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s ... ", name); } while(0)
#define PASS()     do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg)  do { printf("FAIL: %s\n", msg); return 1; } while(0)
#define CHECK(cond, msg) if (!(cond)) FAIL(msg)

/* ================================================================
 *  anomaly_detect.h tests
 * ================================================================ */

static int test_anomaly_detector_create_init(void) {
    TEST("anomaly_detector_create / init");
    AnomalyDetector *det = anomaly_detector_create(ANOMALY_DETECT_METHOD_STATISTICAL);
    CHECK(det != NULL, "anomaly_detector_create returned NULL");
    AnomalyDetectStatus st = anomaly_detector_init(det);
    CHECK(st == ANOMALY_DETECT_STATUS_OK, "anomaly_detector_init failed");
    CHECK(det->is_initialized == true, "detector not initialized");
    anomaly_detector_free(det);
    PASS();
    return 0;
}

static int test_statistical_anomaly_fit_detect(void) {
    TEST("statistical_anomaly_create / fit / is_anomaly");
    float data[100 * 4];
    for (int i = 0; i < 100 * 4; i++) data[i] = (float)((i % 10) * 0.5f);

    StatisticalAnomaly *stat = statistical_anomaly_create(3.0f, 50.0f);
    CHECK(stat != NULL, "statistical_anomaly_create returned NULL");
    AnomalyDetectStatus st = statistical_anomaly_fit(stat, data, 100, 4);
    CHECK(st == ANOMALY_DETECT_STATUS_OK, "statistical_anomaly_fit failed");

    /* Normal point should NOT be anomaly */
    float normal[4] = { 1.0f, 1.5f, 2.0f, 2.5f };
    bool is_anom = statistical_anomaly_is_anomaly(stat, normal, 4);
    /* With z_threshold=3.0, normal data within range should not trigger */
    CHECK(is_anom == false, "normal data incorrectly flagged as anomaly");

    statistical_anomaly_free(stat);
    PASS();
    return 0;
}

static int test_one_class_svm_create_train(void) {
    TEST("one_class_svm_create / train / decision");
    float data[50 * 4];
    for (int i = 0; i < 50 * 4; i++) data[i] = (float)((i % 10) * 0.1f);

    OneClassSVM *svm = one_class_svm_create(4, 0.5f, 0.1f);
    CHECK(svm != NULL, "one_class_svm_create returned NULL");
    AnomalyDetectStatus st = one_class_svm_train(svm, data, 50, 4);
    CHECK(svm->is_trained == true, "SVM not trained after train call");

    float feat[4] = { 0.5f, 0.6f, 0.7f, 0.8f };
    float dec = one_class_svm_decision(svm, feat);
    CHECK(dec > -1000.0f, "SVM decision returned invalid value");

    one_class_svm_free(svm);
    (void)st;
    PASS();
    return 0;
}

/* ================================================================
 *  model_compress.h tests
 * ================================================================ */

static int test_model_prune_magnitude(void) {
    TEST("model_prune_magnitude / compute_sparsity");
    float weights[256];
    for (int i = 0; i < 256; i++) weights[i] = (float)(i * 0.005f) - 0.5f;

    ModelWeightBuffer *buf = model_weight_buffer_create(weights, 256);
    CHECK(buf != NULL, "model_weight_buffer_create returned NULL");
    CHECK(buf->count == 256, "buffer count mismatch");

    float sparsity = model_prune_compute_sparsity(weights, 256, 0.1f);
    CHECK(sparsity >= 0.0f && sparsity <= 1.0f, "sparsity out of range");

    ModelPruneBuffer *pruned = model_prune_magnitude(buf, 0.3f);
    CHECK(pruned != NULL, "model_prune_magnitude returned NULL");

    int nonzero = model_prune_count_nonzero(weights, 256, 0.1f);
    CHECK(nonzero >= 0, "prune_count_nonzero returned negative");

    model_prune_buffer_free(pruned);
    model_weight_buffer_free(buf);
    PASS();
    return 0;
}

static int test_model_quantize_int8(void) {
    TEST("model_quant_float_to_int8 / dequant");
    float weights[128];
    for (int i = 0; i < 128; i++) weights[i] = (float)(i * 0.01f) - 0.64f;

    ModelWeightBuffer *buf = model_weight_buffer_create(weights, 128);
    ModelQuantBuffer *qb = model_quant_float_to_int8(buf);
    CHECK(qb != NULL, "model_quant_float_to_int8 returned NULL");
    CHECK(qb->count == 128, "quant buffer count mismatch");

    float abs_err = model_quant_absolute_error(buf, qb);
    CHECK(abs_err >= 0.0f, "absolute error negative");

    float *dequant = model_quant_dequant_int8(qb);
    CHECK(dequant != NULL, "model_quant_dequant_int8 returned NULL");

    free(dequant);
    model_quant_buffer_free(qb);
    model_weight_buffer_free(buf);
    PASS();
    return 0;
}

static int test_model_compress_pipeline(void) {
    TEST("model_compress_pipeline (quantize)");
    float weights[256];
    for (int i = 0; i < 256; i++) weights[i] = (float)(i * 0.005f) - 0.5f;

    ModelWeightBuffer *buf = model_weight_buffer_create(weights, 256);
    ModelCompressionStats *stats = model_compress_pipeline(buf, MODEL_COMPRESS_METHOD_QUANTIZE);
    CHECK(stats != NULL, "model_compress_pipeline returned NULL");
    CHECK(stats->compression_ratio > 0.0f, "compression ratio is zero");

    model_compression_stats_free(stats);
    model_weight_buffer_free(buf);
    PASS();
    return 0;
}

/* ================================================================
 *  ondevice_learn.h tests
 * ================================================================ */

static int test_od_matrix_create_fill(void) {
    TEST("od_matrix_create / fill_random / fill_zeros");
    ODMatrix *mat = od_matrix_create(16, 8);
    CHECK(mat != NULL, "od_matrix_create returned NULL");
    CHECK(mat->rows == 16, "rows mismatch");
    CHECK(mat->cols == 8, "cols mismatch");

    od_matrix_fill_random(mat, 0.5f);
    int has_nonzero = 0;
    for (int i = 0; i < 16 * 8; i++) {
        if (fabsf(mat->data[i]) > 0.001f) { has_nonzero = 1; break; }
    }
    CHECK(has_nonzero, "fill_random left all zeros");

    od_matrix_fill_zeros(mat);
    int all_zero = 1;
    for (int i = 0; i < 16 * 8; i++) {
        if (fabsf(mat->data[i]) > 0.001f) { all_zero = 0; break; }
    }
    CHECK(all_zero, "fill_zeros did not zero all elements");

    od_matrix_free(mat);
    PASS();
    return 0;
}

static int test_od_layer_create_forward(void) {
    TEST("od_layer_create / forward / freeze");
    ODMatrix *input = od_matrix_create(1, 8);
    od_matrix_fill_random(input, 1.0f);
    ODMatrix *output = od_matrix_create(1, 4);

    ODLayer *layer = od_layer_create(ONDEVICE_LEARN_LAYER_DENSE, 8, 4);
    CHECK(layer != NULL, "od_layer_create returned NULL");
    CHECK(layer->input_size == 8, "input_size mismatch");
    CHECK(layer->output_size == 4, "output_size mismatch");

    od_layer_forward(layer, input, output);
    CHECK(output->data != NULL, "output data NULL after forward");

    bool frozen_before = od_layer_is_frozen(layer);
    CHECK(frozen_before == false, "layer frozen before freeze call");

    od_layer_freeze(layer);
    bool frozen_after = od_layer_is_frozen(layer);
    CHECK(frozen_after == true, "layer not frozen after freeze call");

    od_layer_unfreeze(layer);
    CHECK(od_layer_is_frozen(layer) == false, "layer still frozen after unfreeze");

    od_layer_free(layer);
    od_matrix_free(input);
    od_matrix_free(output);
    PASS();
    return 0;
}

static int test_ondevice_learner_predict(void) {
    TEST("ondevice_learner_create / init / predict");
    OnDeviceLearner *learner = ondevice_learner_create(ONDEVICE_LEARN_METHOD_TRANSFER);
    CHECK(learner != NULL, "ondevice_learner_create returned NULL");

    OnDeviceLearnStatus st = ondevice_learner_init(learner, 8, 3);
    CHECK(st == ONDEVICE_LEARN_STATUS_OK, "ondevice_learner_init failed");

    float input[8];
    float output[3];
    for (int i = 0; i < 8; i++) input[i] = 0.1f * (float)i;
    st = ondevice_learner_predict(learner, input, 8, output, 3);
    CHECK(st == ONDEVICE_LEARN_STATUS_OK, "ondevice_learner_predict failed");

    ondevice_learner_free(learner);
    PASS();
    return 0;
}

/* ================================================================
 *  tflite_micro.h tests
 * ================================================================ */

static int test_tflite_tensor_arena(void) {
    TEST("tflite_tensor_arena create / alloc / reset");
    TFLiteTensorArena *arena = tflite_tensor_arena_create(4096);
    CHECK(arena != NULL, "tflite_tensor_arena_create returned NULL");
    CHECK(arena->arena_size == 4096, "arena size mismatch");

    size_t avail = tflite_tensor_arena_available(arena);
    CHECK(avail > 0, "arena available is zero");

    void *p = tflite_tensor_arena_alloc(arena, 256);
    CHECK(p != NULL, "tflite_tensor_arena_alloc returned NULL");

    size_t used = tflite_tensor_arena_used(arena);
    CHECK(used >= 256, "arena used count too low");

    tflite_tensor_arena_reset(arena);
    size_t after_reset = tflite_tensor_arena_available(arena);
    CHECK(after_reset == 4096, "arena not fully freed after reset");

    tflite_tensor_arena_free(arena);
    PASS();
    return 0;
}

static int test_tflite_tensor_create_data(void) {
    TEST("tflite_tensor_create / alloc_data / element_count");
    TFLiteTensor *t = tflite_tensor_create();
    CHECK(t != NULL, "tflite_tensor_create returned NULL");

    int32_t dims[4] = { 1, 3, 32, 32 };
    TFLiteStatus st = tflite_tensor_alloc_data(t, TFLITE_TYPE_FLOAT32, dims, 4);
    CHECK(st == TFLITE_STATUS_OK, "tflite_tensor_alloc_data failed");

    size_t count = tflite_tensor_element_count(t);
    CHECK(count == (size_t)(1 * 3 * 32 * 32), "element_count mismatch");

    float src[3072];
    for (int i = 0; i < 3072; i++) src[i] = (float)i * 0.001f;
    st = tflite_tensor_copy_float_data(t, src, 3072);
    CHECK(st == TFLITE_STATUS_OK, "copy_float_data failed");

    float out[5];
    st = tflite_tensor_get_float(t, out, 5);
    CHECK(st == TFLITE_STATUS_OK, "get_float failed");
    CHECK(fabsf(out[0] - 0.0f) < 0.01f, "first tensor element mismatch");

    tflite_tensor_free(t);
    PASS();
    return 0;
}

static int test_tflite_op_resolver_create(void) {
    TEST("tflite_micro_op_resolver create / add_builtin");
    MicroMutableOpResolver *resolver = tflite_micro_op_resolver_create();
    CHECK(resolver != NULL, "op_resolver_create returned NULL");

    TFLiteOpResolveStatus rs = tflite_micro_op_resolver_add_builtin(
        resolver, "FULLY_CONNECTED", 9, 1, 5);
    CHECK(rs == TFLITE_OP_RESOLVE_OK, "add_builtin FULLY_CONNECTED failed");

    rs = tflite_micro_op_resolver_add_builtin(
        resolver, "SOFTMAX", 25, 1, 3);
    CHECK(rs == TFLITE_OP_RESOLVE_OK, "add_builtin SOFTMAX failed");

    TFLiteOpEntry entry;
    rs = tflite_micro_op_resolver_resolve(resolver, 9, 2, &entry);
    CHECK(rs == TFLITE_OP_RESOLVE_OK, "resolve FULLY_CONNECTED failed");

    tflite_micro_op_resolver_free(resolver);
    PASS();
    return 0;
}

/* ================================================================
 *  wake_word.h tests
 * ================================================================ */

static int test_mfcc_extractor(void) {
    TEST("mfcc_extractor create / process_frame");
    MFCCExtractor *ext = mfcc_extractor_create(16000, 25, 10, 13, 26);
    CHECK(ext != NULL, "mfcc_extractor_create returned NULL");
    CHECK(ext->is_initialized == true, "extractor not initialized");

    int16_t audio[400];
    for (int i = 0; i < 400; i++) audio[i] = (int16_t)((i % 256 - 128) * 8);

    MFCCFrame frame;
    WakeWordStatus st = mfcc_extractor_process_frame(ext, audio, 400, &frame);
    CHECK(st == WAKE_WORD_STATUS_OK, "mfcc_process_frame failed");
    CHECK(frame.is_speech == false, "silent audio flagged as speech");

    mfcc_extractor_free(ext);
    PASS();
    return 0;
}

static int test_mel_filterbank(void) {
    TEST("mel_filterbank create / apply");
    MelFilterbank *fb = mel_filterbank_create(26, 512, 16000, 20.0f, 8000.0f);
    CHECK(fb != NULL, "mel_filterbank_create returned NULL");
    CHECK(fb->is_initialized == true, "filterbank not initialized");
    CHECK(fb->num_filters == 26, "num_filters mismatch");

    float spectrum[513];
    float mel_energies[26];
    for (int i = 0; i < 513; i++) spectrum[i] = (float)i * 0.001f;

    mel_filterbank_apply(fb, spectrum, mel_energies);
    CHECK(mel_energies[0] >= 0.0f, "mel energy negative");

    /* Test mel scale conversions */
    float mel = mel_hz_to_mel(1000.0f);
    CHECK(mel > 0.0f, "hz_to_mel returned non-positive");
    float hz = mel_mel_to_hz(mel);
    CHECK(fabsf(hz - 1000.0f) < 10.0f, "mel roundtrip precision issue");

    mel_filterbank_free(fb);
    PASS();
    return 0;
}

static int test_wake_word_detector(void) {
    TEST("wake_word_detector create / init / set_threshold");
    WakeWordDetector *det = wake_word_detector_create(16000, "hey_nano");
    CHECK(det != NULL, "wake_word_detector_create returned NULL");

    WakeWordStatus st = wake_word_detector_init(det);
    CHECK(st == WAKE_WORD_STATUS_OK, "wake_word_detector_init failed");
    CHECK(det->is_initialized == true, "detector not initialized");
    CHECK(strcmp(det->target_keyword, "hey_nano") == 0, "target_keyword mismatch");

    wake_word_detector_set_threshold(det, 0.90f);
    CHECK(det->probability_threshold > 0.89f, "threshold not set");

    WakeWordState state = wake_word_detector_get_state(det);
    CHECK(state == WAKE_WORD_STATE_IDLE, "initial state not IDLE");

    /* Process some silence */
    int16_t silence[400];
    memset(silence, 0, sizeof(silence));
    st = wake_word_detector_process_samples(det, silence, 400);
    CHECK(st == WAKE_WORD_STATUS_OK, "process_samples failed on silence");

    bool awake = wake_word_detector_is_awake(det);
    CHECK(awake == false, "silence triggered wake word");

    wake_word_detector_free(det);
    PASS();
    return 0;
}

/* ================================================================
 *  MAIN
 * ================================================================ */

int main(void) {
    printf("\n=== mini-tinyml Unit Tests ===\n\n");

    /* anomaly_detect.h */
    test_anomaly_detector_create_init();
    test_statistical_anomaly_fit_detect();
    test_one_class_svm_create_train();

    /* model_compress.h */
    test_model_prune_magnitude();
    test_model_quantize_int8();
    test_model_compress_pipeline();

    /* ondevice_learn.h */
    test_od_matrix_create_fill();
    test_od_layer_create_forward();
    test_ondevice_learner_predict();

    /* tflite_micro.h */
    test_tflite_tensor_arena();
    test_tflite_tensor_create_data();
    test_tflite_op_resolver_create();

    /* wake_word.h */
    test_mfcc_extractor();
    test_mel_filterbank();
    test_wake_word_detector();

    printf("\n%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
