/*
 * demo_full.c - Full Demonstration of mini-tinyml
 *
 * Walks through all 5 sub-modules:
 *   anomaly_detect.h, model_compress.h, ondevice_learn.h, tflite_micro.h, wake_word.h
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

int main(void) {
    printf("\n");
    printf("******************************************************************\n");
    printf("*                                                                *\n");
    printf("*       MINI-TINYML  --  Full Feature Demonstration              *\n");
    printf("*  Anomaly | Compress | On-Device Learn | TFLite | Wake Word     *\n");
    printf("*                                                                *\n");
    printf("******************************************************************\n");
    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 1 -- anomaly_detect.h  (Anomaly Detection Pipeline)
     * --------------------------------------------------------------- */
    printf("--- Section 1: Anomaly Detection ---\n\n");

    /* Statistical anomaly detector */
    float history_data[200 * 3];
    for (int i = 0; i < 200; i++) {
        history_data[i * 3 + 0] = 25.0f + (float)(i % 10) * 0.5f;   /* temp */
        history_data[i * 3 + 1] = 60.0f + (float)(i % 20) * 0.5f;   /* humidity */
        history_data[i * 3 + 2] = 3.3f + (float)(i % 5) * 0.05f;    /* voltage */
    }

    AnomalyDetector *det = anomaly_detector_create(ANOMALY_DETECT_METHOD_STATISTICAL);
    printf("[OK] anomaly_detector_create() -- STATISTICAL method\n");

    anomaly_detector_init(det);
    anomaly_detector_learn_normal(det, history_data, 200, 3);
    printf("[OK] anomaly_detector_init/learn_normal() -- fitted on 200 samples, 3 features\n");

    /* Detect a normal point */
    float normal_point[3] = { 27.0f, 65.0f, 3.35f };
    AnomalyDetection result = anomaly_detector_detect(det, normal_point, 3, 1000);
    printf("[OK] anomaly_detector_detect(normal) -- score=%.3f, threshold=%.3f, anomaly=%s\n",
           result.score, result.threshold, result.is_anomaly ? "YES" : "NO");

    /* Detect an anomalous point */
    float anomaly_point[3] = { 50.0f, 5.0f, 1.5f };
    result = anomaly_detector_detect(det, anomaly_point, 3, 2000);
    printf("[OK] anomaly_detector_detect(anomaly) -- score=%.3f, severity=%s\n",
           result.score, anomaly_detector_severity_string(result.severity));

    anomaly_detector_free(det);

    /* One-Class SVM */
    float svm_data[100 * 4];
    for (int i = 0; i < 100 * 4; i++) svm_data[i] = ((float)(i % 40) / 40.0f) * 2.0f;

    OneClassSVM *svm = one_class_svm_create(4, 0.5f, 0.1f);
    one_class_svm_train(svm, svm_data, 100, 4);
    printf("[OK] one_class_svm_create/train() -- 4 features, 100 samples, %d SVs\n",
           svm->num_support_vectors);

    float svm_normal[4] = { 0.5f, 0.8f, 1.2f, 1.5f };
    bool svm_anom = one_class_svm_is_anomaly(svm, svm_normal);
    float svm_score = one_class_svm_decision(svm, svm_normal);
    printf("[OK] one_class_svm_is_anomaly(normal) -- decision=%.3f, anomaly=%s\n",
           svm_score, svm_anom ? "YES" : "NO");
    one_class_svm_free(svm);

    /* ---------------------------------------------------------------
     *  SECTION 2 -- model_compress.h  (Model Compression)
     * --------------------------------------------------------------- */
    printf("\n--- Section 2: Model Compression ---\n\n");

    /* Create sample weight data (simulated neural net weights) */
    float weights[2048];
    for (int i = 0; i < 2048; i++) {
        weights[i] = ((float)(i % 100) / 100.0f) - 0.5f;
    }

    ModelWeightBuffer *buf = model_weight_buffer_create(weights, 2048);
    printf("[OK] model_weight_buffer_create() -- %d weights\n", buf->count);
    model_weight_buffer_stats(buf);
    printf("[OK] model_weight_buffer_stats() -- min=%.3f max=%.3f mean=%.3f\n",
           buf->min_val, buf->max_val, buf->mean);

    /* Quantization */
    ModelQuantBuffer *qb = model_quant_float_to_int8(buf);
    printf("[OK] model_quant_float_to_int8() -- %d values, scale=%.4f zp=%d\n",
           qb->count, qb->params.scale, qb->params.zero_point);

    float q_err = model_quant_absolute_error(buf, qb);
    printf("[OK] model_quant_absolute_error() -- %.6f per element\n", q_err);

    /* Dequantize */
    float *dequant = model_quant_dequant_int8(qb);
    printf("[OK] model_quant_dequant_int8() -- reconstructed %d floats\n", qb->count);
    free(dequant);
    model_quant_buffer_free(qb);

    /* Pruning */
    float sparsity = model_prune_compute_sparsity(weights, 2048, 0.05f);
    printf("[OK] model_prune_compute_sparsity() -- %.1f%% below threshold=0.05\n",
           sparsity * 100.0f);

    ModelPruneBuffer *pruned = model_prune_magnitude(buf, 0.3f);
    int nonzero = model_prune_count_nonzero(weights, 2048, 0.05f);
    printf("[OK] model_prune_magnitude() -- target=30%%, %d weights nonzero\n", nonzero);
    model_prune_buffer_free(pruned);

    /* Compression pipeline */
    ModelCompressionStats *stats = model_compress_pipeline(buf, MODEL_COMPRESS_METHOD_QUANTIZE);
    model_compression_stats_print(stats);
    printf("[OK] model_compress_pipeline(QUANTIZE) -- ratio=%.2fx, reduction=%.1f%%\n",
           stats->compression_ratio, stats->size_reduction_percent);
    model_compression_stats_free(stats);

    model_weight_buffer_free(buf);

    /* ---------------------------------------------------------------
     *  SECTION 3 -- ondevice_learn.h  (On-Device Learning)
     * --------------------------------------------------------------- */
    printf("\n--- Section 3: On-Device Learning ---\n\n");

    /* Matrix operations */
    ODMatrix *mat_a = od_matrix_create(4, 8);
    ODMatrix *mat_b = od_matrix_create(8, 4);
    od_matrix_fill_random(mat_a, 0.5f);
    od_matrix_fill_random(mat_b, 0.5f);
    printf("[OK] od_matrix_create/fill_random() -- A(%dx%d) B(%dx%d)\n",
           mat_a->rows, mat_a->cols, mat_b->rows, mat_b->cols);

    float grad_norm = od_matrix_gradient_norm(mat_a);
    printf("[OK] od_matrix_gradient_norm() -- ||A|| = %.4f\n", grad_norm);

    ODMatrix *mat_c = od_matrix_create(4, 4);
    od_matrix_fill_zeros(mat_c);
    od_matrix_clip(mat_a, -0.5f, 0.5f);
    printf("[OK] od_matrix_fill_zeros/clip() -- clip range [-0.5, 0.5]\n");

    /* Layer operations */
    ODLayer *dense = od_layer_create(ONDEVICE_LEARN_LAYER_DENSE, 8, 4);
    od_layer_forward(dense, mat_a, mat_c);
    printf("[OK] od_layer_create(DENSE 8->4) / forward() -- output %dx%d\n",
           mat_c->rows, mat_c->cols);

    ODLayer *relu = od_layer_create(ONDEVICE_LEARN_LAYER_RELU, 4, 4);
    od_layer_forward(relu, mat_c, mat_c);
    printf("[OK] od_layer_create(RELU 4->4) / forward()\n");

    od_layer_freeze(dense);
    printf("[OK] od_layer_freeze() -- layer is_frozen=%s\n",
           od_layer_is_frozen(dense) ? "true" : "false");

    /* Transfer model */
    ODTransferModel *model = od_transfer_model_create(64, 10, 0.001f, ONDEVICE_LEARN_OPTIMIZER_ADAM);
    od_transfer_model_add_layer(model, ONDEVICE_LEARN_LAYER_DENSE, 32);
    od_transfer_model_add_layer(model, ONDEVICE_LEARN_LAYER_RELU, 32);
    od_transfer_model_freeze_base(model, 1);
    int frozen = od_transfer_model_frozen_layer_count(model);
    printf("[OK] od_transfer_model_create/add_layer/freeze() -- %d frozen layers\n", frozen);

    /* Predict using transfer model */
    float feature_input[10] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };
    float predictions[64];
    float labels[64] = {0};
    OnDeviceLearnStatus ol_st;

    /* Train classifier head with synthetic data */
    float train_features[5 * 10];
    float train_labels[5 * 5];
    memset(train_features, 0, sizeof(train_features));
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 10; j++) train_features[i * 10 + j] = (float)j * 0.1f;
        for (int j = 0; j < 5; j++) train_labels[i * 5 + j] = 0.0f;
    }

    ol_st = od_transfer_model_train_classifier_head(model, train_features, train_labels, 5, 10, 5, 2);
    printf("[OK] od_transfer_model_train_classifier_head() -- status=%d\n", ol_st);

    ol_st = od_transfer_model_predict(model, feature_input, 10, predictions, 5);
    printf("[OK] od_transfer_model_predict() -- status=%d\n", ol_st);

    od_transfer_model_free(model);

    /* On-device learner wrapper */
    OnDeviceLearner *learner = ondevice_learner_create(ONDEVICE_LEARN_METHOD_TRANSFER);
    ondevice_learner_init(learner, 8, 3);
    printf("[OK] ondevice_learner_create/init() -- TRANSFER, input=8, output=3\n");

    float lr_input[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    float lr_output[3];
    ol_st = ondevice_learner_predict(learner, lr_input, 8, lr_output, 3);
    printf("[OK] ondevice_learner_predict() -- status=%d, out=[%.2f, %.2f, %.2f]\n",
           ol_st, lr_output[0], lr_output[1], lr_output[2]);

    ondevice_learner_free(learner);

    /* Cleanup matrix ops */
    od_matrix_free(mat_a);
    od_matrix_free(mat_b);
    od_matrix_free(mat_c);
    od_layer_free(dense);
    od_layer_free(relu);

    /* ---------------------------------------------------------------
     *  SECTION 4 -- tflite_micro.h  (TFLite Micro Runtime)
     * --------------------------------------------------------------- */
    printf("\n--- Section 4: TFLite Micro Runtime ---\n\n");

    /* Tensor arena */
    TFLiteTensorArena *arena = tflite_tensor_arena_create(TFLITE_MICRO_DEFAULT_ARENA_SIZE);
    printf("[OK] tflite_tensor_arena_create() -- size=%zu bytes\n", arena->arena_size);

    void *p1 = tflite_tensor_arena_alloc(arena, 1024);
    void *p2 = tflite_tensor_arena_alloc(arena, 2048);
    printf("[OK] tflite_tensor_arena_alloc() -- 2 allocations, used=%zu available=%zu\n",
           tflite_tensor_arena_used(arena), tflite_tensor_arena_available(arena));
    tflite_tensor_arena_reset(arena);
    printf("[OK] tflite_tensor_arena_reset() -- used=%zu\n", tflite_tensor_arena_used(arena));

    /* Tensor creation */
    TFLiteTensor *input_tensor = tflite_tensor_create();
    int32_t dims[4] = { 1, 224, 224, 3 };
    tflite_tensor_alloc_data(input_tensor, TFLITE_TYPE_FLOAT32, dims, 4);
    size_t elem_count = tflite_tensor_element_count(input_tensor);
    printf("[OK] tflite_tensor_create/alloc_data() -- %s, %zu elements\n",
           tflite_tensor_type_to_string(input_tensor->type), elem_count);

    float test_data[10];
    for (int i = 0; i < 10; i++) test_data[i] = (float)i * 0.1f;
    tflite_tensor_copy_float_data(input_tensor, test_data, 10);
    float readback[5];
    tflite_tensor_get_float(input_tensor, readback, 5);
    printf("[OK] tflite_tensor_copy/get_float() -- readback[0]=%.3f\n", readback[0]);
    tflite_tensor_free(input_tensor);

    /* Op resolver */
    MicroMutableOpResolver *resolver = tflite_micro_op_resolver_create();
    tflite_micro_op_resolver_add_builtin(resolver, "CONV_2D", 3, 1, 3);
    tflite_micro_op_resolver_add_builtin(resolver, "DEPTHWISE_CONV_2D", 4, 1, 3);
    tflite_micro_op_resolver_add_builtin(resolver, "FULLY_CONNECTED", 9, 1, 5);
    tflite_micro_op_resolver_add_builtin(resolver, "SOFTMAX", 25, 1, 3);

    TFLiteOpEntry entry;
    TFLiteOpResolveStatus rs = tflite_micro_op_resolver_resolve(resolver, 3, 2, &entry);
    printf("[OK] tflite_micro_op_resolver_add_builtin x4 -- resolve(CONV_2D v3) = %s\n",
           rs == TFLITE_OP_RESOLVE_OK ? "OK" : "NOT_FOUND");
    tflite_micro_op_resolver_free(resolver);

    tflite_tensor_arena_free(arena);
    (void)p1; (void)p2;

    /* ---------------------------------------------------------------
     *  SECTION 5 -- wake_word.h  (Wake Word / Keyword Spotting)
     * --------------------------------------------------------------- */
    printf("\n--- Section 5: Wake Word Detection ---\n\n");

    /* MFCC Extractor */
    MFCCExtractor *mfcc = mfcc_extractor_create(16000, 25, 10, 13, 26);
    printf("[OK] mfcc_extractor_create() -- 16kHz, 25ms frame, 13 coeffs, 26 mel bins\n");

    /* Generate dummy audio (simulated sine wave) */
    int16_t audio_buffer[400];
    for (int i = 0; i < 400; i++) {
        audio_buffer[i] = (int16_t)(sinf(2.0f * 3.14159f * 440.0f * (float)i / 16000.0f) * 4096.0f);
    }

    MFCCFrame frame;
    WakeWordStatus ws = mfcc_extractor_process_frame(mfcc, audio_buffer, 400, &frame);
    printf("[OK] mfcc_extractor_process_frame() -- status=%d, energy=%.3f, frame_idx=%d\n",
           ws, frame.energy, frame.frame_index);

    /* Print first MFCC coefficients */
    printf("     MFCC[0..4] = [%.3f, %.3f, %.3f, %.3f, %.3f]\n",
           frame.coefficients[0], frame.coefficients[1], frame.coefficients[2],
           frame.coefficients[3], frame.coefficients[4]);

    /* Mel Filterbank */
    MelFilterbank *fb = mel_filterbank_create(26, 512, 16000, 0.0f, 8000.0f);
    printf("[OK] mel_filterbank_create() -- %d filters, %d-point FFT\n",
           fb->num_filters, fb->fft_size);

    /* Convert frequency values */
    float mel_1000 = mel_hz_to_mel(1000.0f);
    float hz_back = mel_mel_to_hz(mel_1000);
    printf("[OK] mel conversion: 1000 Hz -> %.1f mel -> %.1f Hz\n", mel_1000, hz_back);

    float spectrum[513];
    float mel_energies[26];
    for (int i = 0; i < 513; i++) spectrum[i] = 1.0f;
    mel_filterbank_apply(fb, spectrum, mel_energies);
    printf("[OK] mel_filterbank_apply() -- mel[0]=%.4f mel[12]=%.4f mel[25]=%.4f\n",
           mel_energies[0], mel_energies[12], mel_energies[25]);
    mel_filterbank_free(fb);

    /* Sliding Window */
    SlidingWindow *window = sliding_window_create(1000, 10);
    for (int i = 0; i < 5; i++) {
        sliding_window_push(window, &frame);
    }
    printf("[OK] sliding_window_create/push x5 -- count=%d, full=%s\n",
           sliding_window_count(window),
           sliding_window_is_full(window) ? "yes" : "no");

    float window_features[256];
    sliding_window_get_features(window, window_features, 256);
    sliding_window_reset(window);
    printf("[OK] sliding_window_get_features/reset() -- count after reset=%d\n",
           sliding_window_count(window));
    sliding_window_free(window);

    /* Full Wake Word Detector */
    WakeWordDetector *wak = wake_word_detector_create(16000, "hey_nano");
    wake_word_detector_init(wak);
    wake_word_detector_set_threshold(wak, 0.85f);
    printf("[OK] wake_word_detector_create/init() -- keyword='%s', thresh=%.2f\n",
           wak->target_keyword, wak->probability_threshold);

    /* Process several frames of silence */
    int16_t silence[400];
    memset(silence, 0, sizeof(silence));
    for (int f = 0; f < 10; f++) {
        ws = wake_word_detector_process_samples(wak, silence, 400);
    }
    printf("[OK] wake_word_detector_process_samples x10 (silence) -- status=%d\n", ws);

    bool awake = wake_word_detector_is_awake(wak);
    printf("[OK] wake_word_detector_is_awake() -- %s\n", awake ? "YES" : "no");

    WakeWordState wstate = wake_word_detector_get_state(wak);
    printf("[OK] wake_word_detector_get_state() -- %d\n", wstate);

    /* PostProcessor */
    PostProcessor *pp = postprocessor_create(200, 500);
    float probs[3] = { 0.2f, 0.1f, 0.95f };
    postprocessor_process(pp, probs, 3, 1000);
    char detected_kw[64];
    bool detected = postprocessor_is_detected(pp, detected_kw, sizeof(detected_kw));
    printf("[OK] postprocessor_create/process() -- detected=%s\n",
           detected ? detected_kw : "(none)");
    postprocessor_free(pp);

    wake_word_detector_free(wak);
    mfcc_extractor_free(mfcc);

    /* ---------------------------------------------------------------
     *  COMPLETION BANNER
     * --------------------------------------------------------------- */
    printf("\n*************************************************************\n");
    printf("*         MINI-TINYML  --  Demo Complete                    *\n");
    printf("*  5 Modules | Anomaly | Compress | Learning | TFLM | Wake  *\n");
    printf("*************************************************************\n\n");

    return 0;
}
