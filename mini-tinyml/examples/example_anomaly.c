#include "tflite_micro.h"
#include "anomaly_detect.h"
#include "model_compress.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void generate_normal_data(float* data, int32_t num_samples, int32_t feature_dim)
{
    for (int32_t s = 0; s < num_samples; s++) {
        for (int32_t f = 0; f < feature_dim; f++) {
            float u1 = (float)rand() / (float)RAND_MAX;
            float u2 = (float)rand() / (float)RAND_MAX;
            data[s * feature_dim + f] = sqrtf(-2.0f * logf(u1 + 1e-7f)) * cosf(2.0f * 3.14159265358979323846f * u2) * 0.5f + 1.0f;
        }
    }
}

static void inject_anomaly(float* data, int32_t num_samples, int32_t feature_dim, int32_t anomaly_idx)
{
    if (anomaly_idx >= num_samples) return;
    for (int32_t f = 0; f < feature_dim; f++) {
        data[anomaly_idx * feature_dim + f] += 5.0f;
    }
}

int main(void)
{
    printf("=== mini-tinyml Anomaly Detection Example ===\n\n");

    int32_t feature_dim = 5;
    int32_t num_normal = 200;
    float* normal_data = (float*)malloc((size_t)(num_normal * feature_dim) * sizeof(float));
    generate_normal_data(normal_data, num_normal, feature_dim);

    printf("--- Test: Autoencoder ---\n");
    Autoencoder* ae = autoencoder_create(feature_dim, 2, NULL, 0);
    printf("[OK] Autoencoder created: input=%d, latent=%d\n", ae->input_dim, ae->latent_dim);
    autoencoder_train(ae, normal_data, num_normal, feature_dim, 50, 0.01f);
    printf("     Trained on %d samples\n", num_normal);

    float normal_sample[] = {1.0f, 1.1f, 0.9f, 1.2f, 1.0f};
    float recon[5];
    autoencoder_reconstruct(ae, normal_sample, recon);
    float normal_err = autoencoder_reconstruction_error(ae, normal_sample);
    printf("     Normal sample reconstruction error: %.6f\n", (double)normal_err);

    float anomaly_sample[] = {8.0f, 7.5f, 9.0f, 8.2f, 7.8f};
    float anomaly_err = autoencoder_reconstruction_error(ae, anomaly_sample);
    printf("     Anomaly reconstruction error:       %.6f\n", (double)anomaly_err);

    autoencoder_set_threshold(ae, 1.5f);
    printf("     Normal is anomaly: %s\n", autoencoder_is_anomaly(ae, normal_sample) ? "yes" : "no");
    printf("     Anomaly is anomaly: %s\n\n", autoencoder_is_anomaly(ae, anomaly_sample) ? "yes" : "no");
    autoencoder_free(ae);

    printf("--- Test: One-Class SVM ---\n");
    OneClassSVM* svm = one_class_svm_create(feature_dim, 0.1f, 0.01f);
    printf("[OK] SVM created: feature_dim=%d, nu=%.2f, gamma=%.4f\n", svm->feature_dim, (double)svm->nu, (double)svm->gamma);
    one_class_svm_train(svm, normal_data, num_normal, feature_dim);
    printf("     Trained with %d support vectors\n", svm->num_support_vectors);
    float svm_dec_n = one_class_svm_decision(svm, normal_sample);
    float svm_dec_a = one_class_svm_decision(svm, anomaly_sample);
    printf("     Normal decision:  %.6f (%s)\n", (double)svm_dec_n,
        one_class_svm_is_anomaly(svm, normal_sample) ? "anomaly" : "normal");
    printf("     Anomaly decision: %.6f (%s)\n\n", (double)svm_dec_a,
        one_class_svm_is_anomaly(svm, anomaly_sample) ? "anomaly" : "normal");
    one_class_svm_free(svm);

    printf("--- Test: Statistical Anomaly ---\n");
    StatisticalAnomaly* stat = statistical_anomaly_create(3.0f, 50.0f);
    statistical_anomaly_fit(stat, normal_data, num_normal, feature_dim);
    float z_scores[5];
    statistical_anomaly_z_score(stat, anomaly_sample, feature_dim, z_scores);
    printf("     Z-scores (anomaly): [");
    for (int32_t i = 0; i < feature_dim; i++) printf(" %.2f", (double)z_scores[i]);
    printf(" ]\n");
    printf("     Normal is anomaly:     %s\n", statistical_anomaly_is_anomaly(stat, normal_sample, feature_dim) ? "yes" : "no");
    printf("     Anomaly is anomaly:    %s\n\n", statistical_anomaly_is_anomaly(stat, anomaly_sample, feature_dim) ? "yes" : "no");
    statistical_anomaly_free(stat);

    printf("--- Test: LSTM Predictor ---\n");
    LSTMPredictor* lstm = lstm_predictor_create(feature_dim, 8, 10);
    float sequence[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    float prediction[5];
    lstm_predictor_predict(lstm, sequence, 5, prediction);
    printf("     Prediction: [");
    for (int32_t i = 0; i < feature_dim; i++) printf(" %.3f", (double)prediction[i]);
    printf(" ]\n");
    float lstm_err = lstm_predictor_prediction_error(lstm, sequence, prediction, 5);
    printf("     Prediction error: %.6f\n", (double)lstm_err);
    printf("     Is anomaly: %s\n\n", lstm_predictor_is_anomaly(lstm, anomaly_sample, prediction, 5) ? "yes" : "no");
    lstm_predictor_free(lstm);

    printf("--- Test: Full Anomaly Detector ---\n");
    AnomalyDetector* det = anomaly_detector_create(ANOMALY_DETECT_METHOD_STATISTICAL);
    anomaly_detector_init(det);
    anomaly_detector_learn_normal(det, normal_data, num_normal, feature_dim);
    AnomalyDetection result_n = anomaly_detector_detect(det, normal_sample, feature_dim, 1000);
    AnomalyDetection result_a = anomaly_detector_detect(det, anomaly_sample, feature_dim, 2000);
    printf("     Normal:  is_anomaly=%s, severity=%s, score=%.4f\n",
        result_n.is_anomaly ? "yes" : "no",
        anomaly_detector_severity_string(result_n.severity),
        (double)result_n.score);
    printf("     Anomaly: is_anomaly=%s, severity=%s, score=%.4f\n",
        result_a.is_anomaly ? "yes" : "no",
        anomaly_detector_severity_string(result_a.severity),
        (double)result_a.score);
    anomaly_detector_free(det);

    printf("\n--- Test: Model Compression on Anomaly Data ---\n");
    ModelWeightBuffer* buf = model_weight_buffer_create(normal_data, num_normal * feature_dim);
    printf("[OK] Buffer: count=%d, mean=%.4f, stddev=%.4f\n",
        buf->count, (double)buf->mean, (double)buf->stddev);
    ModelCompressionStats* cs = model_compress_pipeline(buf, MODEL_COMPRESS_METHOD_QUANTIZE);
    if (cs) {
        model_compression_stats_print(cs);
        model_compression_stats_free(cs);
    }
    model_weight_buffer_free(buf);

    free(normal_data);
    printf("\n=== Done ===\n");
    return 0;
}
