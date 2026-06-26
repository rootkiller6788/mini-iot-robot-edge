#ifndef ANOMALY_DETECT_H
#define ANOMALY_DETECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ANOMALY_DETECT_MAX_HISTORY            1024
#define ANOMALY_DETECT_MAX_FEATURES            128
#define ANOMALY_DETECT_MAX_SEQUENCE_LEN        256
#define ANOMALY_DETECT_DEFAULT_Z_THRESHOLD     3.0f
#define ANOMALY_DETECT_DEFAULT_MA_WINDOW        50
#define ANOMALY_DETECT_AUTOENCODER_MAX_LAYERS    8
#define ANOMALY_DETECT_SVM_MAX_SV              256
#define ANOMALY_DETECT_LSTM_MAX_UNITS           64
#define ANOMALY_DETECT_THRESHOLD_PERCENTILE    95.0f
#define ANOMALY_DETECT_MAX_CONSECUTIVE          8

typedef enum {
    ANOMALY_DETECT_STATUS_OK = 0,
    ANOMALY_DETECT_STATUS_ERROR,
    ANOMALY_DETECT_STATUS_INVALID_PARAM,
    ANOMALY_DETECT_STATUS_NOT_INITIALIZED,
    ANOMALY_DETECT_STATUS_MEMORY,
    ANOMALY_DETECT_STATUS_UNTRAINED
} AnomalyDetectStatus;

typedef enum {
    ANOMALY_DETECT_METHOD_AUTOENCODER = 0,
    ANOMALY_DETECT_METHOD_ONE_CLASS_SVM,
    ANOMALY_DETECT_METHOD_STATISTICAL,
    ANOMALY_DETECT_METHOD_SEQUENCE_LSTM,
    ANOMALY_DETECT_METHOD_ENSEMBLE
} AnomalyDetectMethod;

typedef enum {
    ANOMALY_DETECT_SEVERITY_LOW = 0,
    ANOMALY_DETECT_SEVERITY_MEDIUM,
    ANOMALY_DETECT_SEVERITY_HIGH,
    ANOMALY_DETECT_SEVERITY_CRITICAL
} AnomalyDetectSeverity;

typedef struct {
    float    features[ANOMALY_DETECT_MAX_FEATURES];
    int32_t  num_features;
    int32_t  timestamp_ms;
    char     label[64];
} AnomalySample;

typedef struct {
    AnomalySample samples[ANOMALY_DETECT_MAX_HISTORY];
    int32_t       count;
    int32_t       capacity;
    float         mean[ANOMALY_DETECT_MAX_FEATURES];
    float         stddev[ANOMALY_DETECT_MAX_FEATURES];
    bool          stats_computed;
} AnomalyHistory;

typedef struct {
    float                  score;
    float                  threshold;
    bool                   is_anomaly;
    AnomalyDetectSeverity  severity;
    int32_t                timestamp_ms;
} AnomalyDetection;

typedef struct {
    int32_t   layer_sizes[ANOMALY_DETECT_AUTOENCODER_MAX_LAYERS];
    int32_t   num_layers;
    float**   weights;
    float**   biases;
    int32_t   input_dim;
    int32_t   latent_dim;
    bool      is_trained;
    float     reconstruction_error;
    float     threshold;
} Autoencoder;

typedef struct {
    float    support_vectors[ANOMALY_DETECT_SVM_MAX_SV][ANOMALY_DETECT_MAX_FEATURES];
    float    alphas[ANOMALY_DETECT_SVM_MAX_SV];
    int32_t  num_support_vectors;
    int32_t  feature_dim;
    float    nu;
    float    gamma;
    float    rho;
    float    threshold;
    bool     is_trained;
} OneClassSVM;

typedef struct {
    float    z_score_threshold;
    float    moving_average_window;
    float    z_scores[ANOMALY_DETECT_MAX_FEATURES];
    float    moving_average[ANOMALY_DETECT_MAX_FEATURES];
    float    moving_stddev[ANOMALY_DETECT_MAX_FEATURES];
    bool     is_initialized;
} StatisticalAnomaly;

typedef struct {
    float    prediction_error[ANOMALY_DETECT_MAX_FEATURES];
    float    hidden_state[ANOMALY_DETECT_LSTM_MAX_UNITS];
    float    cell_state[ANOMALY_DETECT_LSTM_MAX_UNITS];
    int32_t  hidden_size;
    int32_t  input_size;
    int32_t  sequence_length;
    float    threshold;
    bool     is_trained;
} LSTMPredictor;

typedef struct {
    AnomalyDetectMethod method;
    Autoencoder*        autoencoder;
    OneClassSVM*        svm;
    StatisticalAnomaly* statistical;
    LSTMPredictor*      lstm;
    AnomalyHistory*     history;
    float               threshold;
    int32_t             consecutive_anomalies;
    int32_t             max_consecutive;
    bool                is_initialized;
    bool                auto_tune_enabled;
} AnomalyDetector;

Autoencoder*          autoencoder_create(int32_t input_dim, int32_t latent_dim, const int32_t* hidden_layers, int32_t num_hidden_layers);
void                  autoencoder_free(Autoencoder* ae);
AnomalyDetectStatus   autoencoder_train(Autoencoder* ae, const float* data, int32_t num_samples, int32_t input_dim, int32_t epochs, float learning_rate);
AnomalyDetectStatus   autoencoder_reconstruct(const Autoencoder* ae, const float* input, float* reconstruction);
float                 autoencoder_reconstruction_error(const Autoencoder* ae, const float* input);
bool                  autoencoder_is_anomaly(const Autoencoder* ae, const float* input);
void                  autoencoder_set_threshold(Autoencoder* ae, float threshold);

OneClassSVM*          one_class_svm_create(int32_t feature_dim, float nu, float gamma);
void                  one_class_svm_free(OneClassSVM* svm);
AnomalyDetectStatus   one_class_svm_train(OneClassSVM* svm, const float* data, int32_t num_samples, int32_t feature_dim);
float                 one_class_svm_decision(const OneClassSVM* svm, const float* features);
bool                  one_class_svm_is_anomaly(const OneClassSVM* svm, const float* features);

StatisticalAnomaly*   statistical_anomaly_create(float z_threshold, float ma_window);
void                  statistical_anomaly_free(StatisticalAnomaly* stat);
AnomalyDetectStatus   statistical_anomaly_fit(StatisticalAnomaly* stat, const float* history_data, int32_t num_samples, int32_t feature_dim);
AnomalyDetectStatus   statistical_anomaly_z_score(const StatisticalAnomaly* stat, const float* features, int32_t feature_dim, float* z_scores);
bool                  statistical_anomaly_is_anomaly(const StatisticalAnomaly* stat, const float* features, int32_t feature_dim);
AnomalyDetectStatus   statistical_anomaly_update_ma(StatisticalAnomaly* stat, const float* features, int32_t feature_dim);

LSTMPredictor*        lstm_predictor_create(int32_t input_size, int32_t hidden_size, int32_t sequence_length);
void                  lstm_predictor_free(LSTMPredictor* lstm);
AnomalyDetectStatus   lstm_predictor_predict(LSTMPredictor* lstm, const float* sequence, int32_t seq_length, float* prediction);
float                 lstm_predictor_prediction_error(const LSTMPredictor* lstm, const float* actual, const float* predicted, int32_t length);
bool                  lstm_predictor_is_anomaly(const LSTMPredictor* lstm, const float* actual, const float* predicted, int32_t length);

AnomalyDetector*      anomaly_detector_create(AnomalyDetectMethod method);
void                  anomaly_detector_free(AnomalyDetector* detector);
AnomalyDetectStatus   anomaly_detector_init(AnomalyDetector* detector);
AnomalyDetectStatus   anomaly_detector_learn_normal(AnomalyDetector* detector, const float* data, int32_t num_samples, int32_t feature_dim);
AnomalyDetection      anomaly_detector_detect(AnomalyDetector* detector, const float* features, int32_t feature_dim, int32_t timestamp_ms);
AnomalyDetectStatus   anomaly_detector_set_threshold(AnomalyDetector* detector, float threshold);
AnomalyDetectStatus   anomaly_detector_auto_tune_threshold(AnomalyDetector* detector, const AnomalyHistory* history);
AnomalyDetectStatus   anomaly_detector_add_to_history(AnomalyDetector* detector, const float* features, int32_t feature_dim, int32_t timestamp_ms);
AnomalyDetectSeverity anomaly_detector_calc_severity(float score, float threshold);
const char*           anomaly_detector_severity_string(AnomalyDetectSeverity severity);
void                  anomaly_detector_reset(AnomalyDetector* detector);

#ifdef __cplusplus
}
#endif

#endif /* ANOMALY_DETECT_H */
