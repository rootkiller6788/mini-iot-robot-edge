#include "anomaly_detect.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SQR(x) ((x) * (x))

Autoencoder* autoencoder_create(int32_t input_dim, int32_t latent_dim,
    const int32_t* hidden_layers, int32_t num_hidden_layers)
{
    if (input_dim <= 0 || latent_dim <= 0 || input_dim > ANOMALY_DETECT_MAX_FEATURES)
        return NULL;
    Autoencoder* ae = (Autoencoder*)calloc(1, sizeof(Autoencoder));
    if (!ae) return NULL;
    ae->input_dim = input_dim;
    ae->latent_dim = latent_dim;
    ae->num_layers = num_hidden_layers;
    for (int32_t i = 0; i < num_hidden_layers && i < ANOMALY_DETECT_AUTOENCODER_MAX_LAYERS; i++) {
        ae->layer_sizes[i] = hidden_layers[i];
    }
    ae->weights = (float**)calloc((size_t)(num_hidden_layers + 1), sizeof(float*));
    ae->biases  = (float**)calloc((size_t)(num_hidden_layers + 1), sizeof(float*));
    if (!ae->weights || !ae->biases) { autoencoder_free(ae); return NULL; }
    int32_t prev = input_dim;
    for (int32_t i = 0; i < num_hidden_layers + 1; i++) {
        int32_t curr = (i < num_hidden_layers) ? ae->layer_sizes[i] : input_dim;
        ae->weights[i] = (float*)calloc((size_t)(prev * curr), sizeof(float));
        ae->biases[i]  = (float*)calloc((size_t)curr, sizeof(float));
        if (!ae->weights[i] || !ae->biases[i]) { autoencoder_free(ae); return NULL; }
        for (int32_t j = 0; j < prev * curr; j++) {
            ae->weights[i][j] = ((float)rand() / (float)RAND_MAX - 0.5f) * 0.1f;
        }
        prev = curr;
    }
    return ae;
}

void autoencoder_free(Autoencoder* ae)
{
    if (!ae) return;
    if (ae->weights) {
        for (int32_t i = 0; i < ae->num_layers + 1; i++) free(ae->weights[i]);
        free(ae->weights);
    }
    if (ae->biases) {
        for (int32_t i = 0; i < ae->num_layers + 1; i++) free(ae->biases[i]);
        free(ae->biases);
    }
    free(ae);
}

static float autoencoder_relu(float x) { return x > 0.0f ? x : 0.0f; }

AnomalyDetectStatus autoencoder_train(Autoencoder* ae, const float* data,
    int32_t num_samples, int32_t input_dim, int32_t epochs, float learning_rate)
{
    if (!ae || !data || input_dim != ae->input_dim) return ANOMALY_DETECT_STATUS_INVALID_PARAM;
    for (int32_t epoch = 0; epoch < epochs; epoch++) {
        float total_loss = 0.0f;
        for (int32_t s = 0; s < num_samples; s++) {
            const float* sample = data + s * input_dim;
            float hidden[ANOMALY_DETECT_MAX_FEATURES];
            float output[ANOMALY_DETECT_MAX_FEATURES];
            autoencoder_reconstruct(ae, sample, output);
            for (int32_t i = 0; i < input_dim; i++) {
                float err = output[i] - sample[i];
                total_loss += err * err;
                for (int32_t w = 0; w < ae->input_dim; w++) {
                    ae->weights[ae->num_layers][i * ae->input_dim + w] -= learning_rate * 2.0f * err * sample[w];
                }
                ae->biases[ae->num_layers][i] -= learning_rate * 2.0f * err;
            }
        }
        total_loss /= (float)num_samples;
        if (total_loss < 1e-6f) break;
    }
    ae->is_trained = true;
    ae->reconstruction_error = 0.0f;
    return ANOMALY_DETECT_STATUS_OK;
}

AnomalyDetectStatus autoencoder_reconstruct(const Autoencoder* ae, const float* input, float* reconstruction)
{
    if (!ae || !input || !reconstruction) return ANOMALY_DETECT_STATUS_INVALID_PARAM;
    float current[ANOMALY_DETECT_MAX_FEATURES];
    memcpy(current, input, (size_t)ae->input_dim * sizeof(float));
    int32_t prev_dim = ae->input_dim;
    for (int32_t l = 0; l < ae->num_layers; l++) {
        int32_t curr_dim = ae->layer_sizes[l];
        float* next = (float*)calloc((size_t)curr_dim, sizeof(float));
        if (!next) return ANOMALY_DETECT_STATUS_MEMORY;
        for (int32_t j = 0; j < curr_dim; j++) {
            float sum = ae->biases[l][j];
            for (int32_t k = 0; k < prev_dim; k++) {
                sum += ae->weights[l][j * prev_dim + k] * current[k];
            }
            next[j] = autoencoder_relu(sum);
        }
        memcpy(current, next, (size_t)curr_dim * sizeof(float));
        free(next);
        prev_dim = curr_dim;
    }
    {
        int32_t curr_dim = ae->input_dim;
        for (int32_t j = 0; j < curr_dim; j++) {
            float sum = ae->biases[ae->num_layers][j];
            for (int32_t k = 0; k < prev_dim; k++) {
                sum += ae->weights[ae->num_layers][j * prev_dim + k] * current[k];
            }
            current[j] = sum;
        }
    }
    memcpy(reconstruction, current, (size_t)ae->input_dim * sizeof(float));
    return ANOMALY_DETECT_STATUS_OK;
}

float autoencoder_reconstruction_error(const Autoencoder* ae, const float* input)
{
    if (!ae || !input) return 0.0f;
    float recon[ANOMALY_DETECT_MAX_FEATURES];
    autoencoder_reconstruct(ae, input, recon);
    float err = 0.0f;
    for (int32_t i = 0; i < ae->input_dim; i++) {
        err += SQR(input[i] - recon[i]);
    }
    return sqrtf(err / (float)ae->input_dim);
}

bool autoencoder_is_anomaly(const Autoencoder* ae, const float* input)
{
    if (!ae) return false;
    return autoencoder_reconstruction_error(ae, input) > ae->threshold;
}

void autoencoder_set_threshold(Autoencoder* ae, float threshold)
{
    if (ae) ae->threshold = threshold;
}

OneClassSVM* one_class_svm_create(int32_t feature_dim, float nu, float gamma)
{
    if (feature_dim <= 0 || feature_dim > ANOMALY_DETECT_MAX_FEATURES) return NULL;
    OneClassSVM* svm = (OneClassSVM*)calloc(1, sizeof(OneClassSVM));
    if (!svm) return NULL;
    svm->feature_dim = feature_dim;
    svm->nu = nu;
    svm->gamma = gamma;
    return svm;
}

void one_class_svm_free(OneClassSVM* svm) { free(svm); }

AnomalyDetectStatus one_class_svm_train(OneClassSVM* svm, const float* data,
    int32_t num_samples, int32_t feature_dim)
{
    if (!svm || !data || feature_dim != svm->feature_dim) return ANOMALY_DETECT_STATUS_INVALID_PARAM;
    int32_t n = num_samples < ANOMALY_DETECT_SVM_MAX_SV ? num_samples : ANOMALY_DETECT_SVM_MAX_SV;
    for (int32_t i = 0; i < n; i++) {
        memcpy(svm->support_vectors[i], data + i * feature_dim, (size_t)feature_dim * sizeof(float));
        svm->alphas[i] = 1.0f / (float)n;
    }
    svm->num_support_vectors = n;
    svm->rho = 0.0f;
    svm->is_trained = true;
    svm->threshold = 0.5f;
    return ANOMALY_DETECT_STATUS_OK;
}

static float svm_rbf_kernel(const float* a, const float* b, int32_t dim, float gamma)
{
    float dist = 0.0f;
    for (int32_t i = 0; i < dim; i++) dist += SQR(a[i] - b[i]);
    return expf(-gamma * dist);
}

float one_class_svm_decision(const OneClassSVM* svm, const float* features)
{
    if (!svm || !features) return 0.0f;
    float decision = -svm->rho;
    for (int32_t i = 0; i < svm->num_support_vectors; i++) {
        decision += svm->alphas[i] * svm_rbf_kernel(svm->support_vectors[i], features, svm->feature_dim, svm->gamma);
    }
    return decision;
}

bool one_class_svm_is_anomaly(const OneClassSVM* svm, const float* features)
{
    if (!svm || !svm->is_trained) return false;
    return one_class_svm_decision(svm, features) < svm->threshold;
}

StatisticalAnomaly* statistical_anomaly_create(float z_threshold, float ma_window)
{
    StatisticalAnomaly* sa = (StatisticalAnomaly*)calloc(1, sizeof(StatisticalAnomaly));
    if (!sa) return NULL;
    sa->z_score_threshold = z_threshold;
    sa->moving_average_window = ma_window;
    return sa;
}

void statistical_anomaly_free(StatisticalAnomaly* stat) { free(stat); }

AnomalyDetectStatus statistical_anomaly_fit(StatisticalAnomaly* stat,
    const float* history_data, int32_t num_samples, int32_t feature_dim)
{
    if (!stat || !history_data || feature_dim > ANOMALY_DETECT_MAX_FEATURES)
        return ANOMALY_DETECT_STATUS_INVALID_PARAM;
    for (int32_t f = 0; f < feature_dim; f++) {
        float sum = 0.0f;
        for (int32_t s = 0; s < num_samples; s++) sum += history_data[s * feature_dim + f];
        stat->moving_average[f] = sum / (float)num_samples;
        float var = 0.0f;
        for (int32_t s = 0; s < num_samples; s++)
            var += SQR(history_data[s * feature_dim + f] - stat->moving_average[f]);
        stat->moving_stddev[f] = sqrtf(var / (float)num_samples);
        if (stat->moving_stddev[f] < 1e-7f) stat->moving_stddev[f] = 1e-7f;
    }
    stat->is_initialized = true;
    return ANOMALY_DETECT_STATUS_OK;
}

AnomalyDetectStatus statistical_anomaly_z_score(const StatisticalAnomaly* stat,
    const float* features, int32_t feature_dim, float* z_scores)
{
    if (!stat || !features || !z_scores) return ANOMALY_DETECT_STATUS_INVALID_PARAM;
    if (!stat->is_initialized) return ANOMALY_DETECT_STATUS_NOT_INITIALIZED;
    for (int32_t f = 0; f < feature_dim; f++) {
        z_scores[f] = fabsf((features[f] - stat->moving_average[f]) / stat->moving_stddev[f]);
    }
    return ANOMALY_DETECT_STATUS_OK;
}

bool statistical_anomaly_is_anomaly(const StatisticalAnomaly* stat,
    const float* features, int32_t feature_dim)
{
    if (!stat || !features) return false;
    float z_scores[ANOMALY_DETECT_MAX_FEATURES];
    statistical_anomaly_z_score(stat, features, feature_dim, z_scores);
    for (int32_t f = 0; f < feature_dim; f++) {
        if (z_scores[f] > stat->z_score_threshold) return true;
    }
    return false;
}

AnomalyDetectStatus statistical_anomaly_update_ma(StatisticalAnomaly* stat,
    const float* features, int32_t feature_dim)
{
    if (!stat || !features) return ANOMALY_DETECT_STATUS_INVALID_PARAM;
    float alpha = 2.0f / (stat->moving_average_window + 1.0f);
    for (int32_t f = 0; f < feature_dim; f++) {
        stat->moving_average[f] = alpha * features[f] + (1.0f - alpha) * stat->moving_average[f];
        float diff = features[f] - stat->moving_average[f];
        stat->moving_stddev[f] = alpha * SQR(diff) + (1.0f - alpha) * SQR(stat->moving_stddev[f]);
        stat->moving_stddev[f] = sqrtf(stat->moving_stddev[f]);
        if (stat->moving_stddev[f] < 1e-7f) stat->moving_stddev[f] = 1e-7f;
    }
    return ANOMALY_DETECT_STATUS_OK;
}

LSTMPredictor* lstm_predictor_create(int32_t input_size, int32_t hidden_size, int32_t sequence_length)
{
    if (input_size <= 0 || hidden_size <= 0 || hidden_size > ANOMALY_DETECT_LSTM_MAX_UNITS)
        return NULL;
    LSTMPredictor* lstm = (LSTMPredictor*)calloc(1, sizeof(LSTMPredictor));
    if (!lstm) return NULL;
    lstm->input_size = input_size;
    lstm->hidden_size = hidden_size;
    lstm->sequence_length = sequence_length;
    lstm->threshold = 0.1f;
    return lstm;
}

void lstm_predictor_free(LSTMPredictor* lstm) { free(lstm); }

static float lstm_sigmoid_scalar(float x) { return 1.0f / (1.0f + expf(-x)); }
static float lstm_tanh_scalar(float x) { return tanhf(x); }

AnomalyDetectStatus lstm_predictor_predict(LSTMPredictor* lstm,
    const float* sequence, int32_t seq_length, float* prediction)
{
    if (!lstm || !sequence || !prediction) return ANOMALY_DETECT_STATUS_INVALID_PARAM;
    for (int32_t t = 0; t < seq_length; t++) {
        for (int32_t h = 0; h < lstm->hidden_size; h++) {
            float input_sum = lstm->cell_state[h] * 0.01f;
            for (int32_t i = 0; i < lstm->input_size && i < seq_length; i++) {
                input_sum += sequence[i] * 0.01f;
            }
            lstm->hidden_state[h] = lstm_tanh_scalar(input_sum);
            lstm->cell_state[h] = lstm_sigmoid_scalar(input_sum) * lstm->cell_state[h]
                + lstm_sigmoid_scalar(sequence[t % lstm->input_size]) * lstm->hidden_state[h];
        }
    }
    for (int32_t i = 0; i < lstm->input_size; i++) {
        prediction[i] = sequence[seq_length > 1 ? (seq_length - 1 + i) % lstm->input_size : 0];
    }
    lstm->is_trained = true;
    return ANOMALY_DETECT_STATUS_OK;
}

float lstm_predictor_prediction_error(const LSTMPredictor* lstm,
    const float* actual, const float* predicted, int32_t length)
{
    if (!actual || !predicted || length <= 0) return 0.0f;
    float err = 0.0f;
    for (int32_t i = 0; i < length; i++) err += SQR(actual[i] - predicted[i]);
    return sqrtf(err / (float)length);
}

bool lstm_predictor_is_anomaly(const LSTMPredictor* lstm,
    const float* actual, const float* predicted, int32_t length)
{
    if (!lstm) return false;
    return lstm_predictor_prediction_error(lstm, actual, predicted, length) > lstm->threshold;
}

AnomalyDetector* anomaly_detector_create(AnomalyDetectMethod method)
{
    AnomalyDetector* det = (AnomalyDetector*)calloc(1, sizeof(AnomalyDetector));
    if (!det) return NULL;
    det->method = method;
    det->threshold = 0.5f;
    det->max_consecutive = ANOMALY_DETECT_MAX_CONSECUTIVE;
    return det;
}

void anomaly_detector_free(AnomalyDetector* detector)
{
    if (!detector) return;
    autoencoder_free(detector->autoencoder);
    one_class_svm_free(detector->svm);
    statistical_anomaly_free(detector->statistical);
    lstm_predictor_free(detector->lstm);
    free(detector->history);
    free(detector);
}

AnomalyDetectStatus anomaly_detector_init(AnomalyDetector* detector)
{
    if (!detector) return ANOMALY_DETECT_STATUS_INVALID_PARAM;
    detector->history = (AnomalyHistory*)calloc(1, sizeof(AnomalyHistory));
    if (!detector->history) return ANOMALY_DETECT_STATUS_MEMORY;
    detector->history->capacity = ANOMALY_DETECT_MAX_HISTORY;
    switch (detector->method) {
    case ANOMALY_DETECT_METHOD_AUTOENCODER:
        detector->autoencoder = autoencoder_create(10, 4, NULL, 0);
        if (detector->autoencoder) detector->autoencoder->threshold = detector->threshold;
        break;
    case ANOMALY_DETECT_METHOD_ONE_CLASS_SVM:
        detector->svm = one_class_svm_create(10, 0.1f, 0.01f);
        if (detector->svm) detector->svm->threshold = detector->threshold;
        break;
    case ANOMALY_DETECT_METHOD_STATISTICAL:
        detector->statistical = statistical_anomaly_create(3.0f, 50.0f);
        break;
    case ANOMALY_DETECT_METHOD_SEQUENCE_LSTM:
        detector->lstm = lstm_predictor_create(10, 16, 10);
        if (detector->lstm) detector->lstm->threshold = detector->threshold;
        break;
    default: break;
    }
    detector->is_initialized = true;
    return ANOMALY_DETECT_STATUS_OK;
}

AnomalyDetectStatus anomaly_detector_learn_normal(AnomalyDetector* detector,
    const float* data, int32_t num_samples, int32_t feature_dim)
{
    if (!detector || !data) return ANOMALY_DETECT_STATUS_INVALID_PARAM;
    switch (detector->method) {
    case ANOMALY_DETECT_METHOD_AUTOENCODER:
        return autoencoder_train(detector->autoencoder, data, num_samples, feature_dim, 10, 0.001f);
    case ANOMALY_DETECT_METHOD_ONE_CLASS_SVM:
        return one_class_svm_train(detector->svm, data, num_samples, feature_dim);
    case ANOMALY_DETECT_METHOD_STATISTICAL:
        return statistical_anomaly_fit(detector->statistical, data, num_samples, feature_dim);
    default: return ANOMALY_DETECT_STATUS_ERROR;
    }
}

AnomalyDetection anomaly_detector_detect(AnomalyDetector* detector,
    const float* features, int32_t feature_dim, int32_t timestamp_ms)
{
    AnomalyDetection result = {0};
    result.timestamp_ms = timestamp_ms;
    if (!detector || !features || !detector->is_initialized) return result;
    bool is_anom = false;
    switch (detector->method) {
    case ANOMALY_DETECT_METHOD_AUTOENCODER:
        result.score = autoencoder_reconstruction_error(detector->autoencoder, features);
        is_anom = autoencoder_is_anomaly(detector->autoencoder, features);
        break;
    case ANOMALY_DETECT_METHOD_ONE_CLASS_SVM:
        result.score = -one_class_svm_decision(detector->svm, features);
        is_anom = one_class_svm_is_anomaly(detector->svm, features);
        break;
    case ANOMALY_DETECT_METHOD_STATISTICAL:
        statistical_anomaly_z_score(detector->statistical, features, feature_dim, result.threshold ? NULL : NULL);
        is_anom = statistical_anomaly_is_anomaly(detector->statistical, features, feature_dim);
        break;
    case ANOMALY_DETECT_METHOD_SEQUENCE_LSTM: {
        float pred[ANOMALY_DETECT_MAX_FEATURES];
        lstm_predictor_predict(detector->lstm, features, feature_dim, pred);
        result.score = lstm_predictor_prediction_error(detector->lstm, features, pred, feature_dim);
        is_anom = lstm_predictor_is_anomaly(detector->lstm, features, pred, feature_dim);
        break;
    }
    default: break;
    }
    result.is_anomaly = is_anom;
    result.threshold = detector->threshold;
    result.severity = anomaly_detector_calc_severity(result.score, detector->threshold);
    if (is_anom) {
        detector->consecutive_anomalies++;
    } else {
        detector->consecutive_anomalies = 0;
    }
    if (detector->history && detector->history->count < detector->history->capacity) {
        anomaly_detector_add_to_history(detector, features, feature_dim, timestamp_ms);
    }
    return result;
}

AnomalyDetectStatus anomaly_detector_set_threshold(AnomalyDetector* detector, float threshold)
{
    if (!detector) return ANOMALY_DETECT_STATUS_INVALID_PARAM;
    detector->threshold = threshold;
    return ANOMALY_DETECT_STATUS_OK;
}

AnomalyDetectStatus anomaly_detector_auto_tune_threshold(AnomalyDetector* detector,
    const AnomalyHistory* history)
{
    if (!detector || !history || history->count == 0) return ANOMALY_DETECT_STATUS_INVALID_PARAM;
    float max_err = 0.0f;
    for (int32_t i = 0; i < history->count; i++) {
        float err = 0.0f;
        for (int32_t j = 0; j < history->samples[i].num_features; j++) {
            err += SQR(history->samples[i].features[j] - history->mean[j]);
        }
        if (err > max_err) max_err = err;
    }
    detector->threshold = max_err * (ANOMALY_DETECT_THRESHOLD_PERCENTILE / 100.0f);
    return ANOMALY_DETECT_STATUS_OK;
}

AnomalyDetectStatus anomaly_detector_add_to_history(AnomalyDetector* detector,
    const float* features, int32_t feature_dim, int32_t timestamp_ms)
{
    if (!detector || !features || !detector->history) return ANOMALY_DETECT_STATUS_INVALID_PARAM;
    if (detector->history->count >= detector->history->capacity) return ANOMALY_DETECT_STATUS_ERROR;
    AnomalySample* s = &detector->history->samples[detector->history->count];
    s->num_features = feature_dim;
    s->timestamp_ms = timestamp_ms;
    memcpy(s->features, features, (size_t)feature_dim * sizeof(float));
    detector->history->count++;
    return ANOMALY_DETECT_STATUS_OK;
}

AnomalyDetectSeverity anomaly_detector_calc_severity(float score, float threshold)
{
    if (threshold <= 0.0f) return ANOMALY_DETECT_SEVERITY_LOW;
    float ratio = score / threshold;
    if (ratio < 1.0f) return ANOMALY_DETECT_SEVERITY_LOW;
    if (ratio < 2.0f) return ANOMALY_DETECT_SEVERITY_MEDIUM;
    if (ratio < 5.0f) return ANOMALY_DETECT_SEVERITY_HIGH;
    return ANOMALY_DETECT_SEVERITY_CRITICAL;
}

const char* anomaly_detector_severity_string(AnomalyDetectSeverity severity)
{
    switch (severity) {
    case ANOMALY_DETECT_SEVERITY_LOW:     return "LOW";
    case ANOMALY_DETECT_SEVERITY_MEDIUM:  return "MEDIUM";
    case ANOMALY_DETECT_SEVERITY_HIGH:    return "HIGH";
    case ANOMALY_DETECT_SEVERITY_CRITICAL: return "CRITICAL";
    default: return "UNKNOWN";
    }
}

void anomaly_detector_reset(AnomalyDetector* detector)
{
    if (!detector) return;
    detector->consecutive_anomalies = 0;
    if (detector->history) detector->history->count = 0;
}
