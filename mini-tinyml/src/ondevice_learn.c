#include "ondevice_learn.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SQR(x) ((x) * (x))

ODMatrix* od_matrix_create(int32_t rows, int32_t cols)
{
    if (rows <= 0 || cols <= 0) return NULL;
    ODMatrix* mat = (ODMatrix*)calloc(1, sizeof(ODMatrix));
    if (!mat) return NULL;
    mat->rows = rows;
    mat->cols = cols;
    mat->capacity = rows * cols;
    mat->data = (float*)calloc((size_t)mat->capacity, sizeof(float));
    if (!mat->data) { free(mat); return NULL; }
    return mat;
}

void od_matrix_free(ODMatrix* mat)
{
    if (!mat) return;
    free(mat->data);
    free(mat);
}

void od_matrix_fill_random(ODMatrix* mat, float scale)
{
    if (!mat || !mat->data) return;
    for (int32_t i = 0; i < mat->capacity; i++) {
        mat->data[i] = ((float)rand() / (float)RAND_MAX - 0.5f) * 2.0f * scale;
    }
}

void od_matrix_fill_zeros(ODMatrix* mat)
{
    if (!mat || !mat->data) return;
    memset(mat->data, 0, (size_t)mat->capacity * sizeof(float));
}

void od_matrix_clip(ODMatrix* mat, float min_val, float max_val)
{
    if (!mat || !mat->data) return;
    for (int32_t i = 0; i < mat->capacity; i++) {
        if (mat->data[i] < min_val) mat->data[i] = min_val;
        if (mat->data[i] > max_val) mat->data[i] = max_val;
    }
}

float od_matrix_gradient_norm(const ODMatrix* grad)
{
    if (!grad || !grad->data) return 0.0f;
    float norm_sq = 0.0f;
    for (int32_t i = 0; i < grad->capacity; i++) norm_sq += grad->data[i] * grad->data[i];
    return sqrtf(norm_sq);
}

ODLayer* od_layer_create(OnDeviceLearnLayerType type, int32_t input_size, int32_t output_size)
{
    if (input_size <= 0 || output_size <= 0) return NULL;
    ODLayer* layer = (ODLayer*)calloc(1, sizeof(ODLayer));
    if (!layer) return NULL;
    layer->input_size = input_size;
    layer->output_size = output_size;
    if (type == ONDEVICE_LEARN_LAYER_DENSE || type == ONDEVICE_LEARN_LAYER_CONV2D) {
        layer->weights = od_matrix_create(input_size, output_size);
        layer->bias = od_matrix_create(1, output_size);
        layer->weight_gradients = od_matrix_create(input_size, output_size);
        layer->bias_gradients = od_matrix_create(1, output_size);
        if (!layer->weights || !layer->bias || !layer->weight_gradients || !layer->bias_gradients) {
            od_layer_free(layer); return NULL;
        }
        od_matrix_fill_random(layer->weights, 0.1f);
        od_matrix_fill_zeros(layer->bias);
    }
    return layer;
}

void od_layer_free(ODLayer* layer)
{
    if (!layer) return;
    od_matrix_free(layer->weights);
    od_matrix_free(layer->bias);
    od_matrix_free(layer->weight_gradients);
    od_matrix_free(layer->bias_gradients);
    free(layer);
}

void od_layer_forward(const ODLayer* layer, const ODMatrix* input, ODMatrix* output)
{
    if (!layer || !input || !output) return;
    for (int32_t o = 0; o < layer->output_size; o++) {
        float sum = (layer->bias && layer->bias->data) ? layer->bias->data[o] : 0.0f;
        for (int32_t i = 0; i < layer->input_size; i++) {
            sum += input->data[i] * layer->weights->data[i * layer->output_size + o];
        }
        output->data[o] = sum;
    }
}

void od_layer_backward(ODLayer* layer, const ODMatrix* input,
    const ODMatrix* grad_output, ODMatrix* grad_input)
{
    if (!layer || !input || !grad_output) return;
    for (int32_t w = 0; w < layer->weights->capacity; w++) {
        int32_t o = w % layer->output_size;
        int32_t i = w / layer->output_size;
        layer->weight_gradients->data[w] += grad_output->data[o] * input->data[i];
    }
    if (layer->bias_gradients && layer->bias_gradients->data) {
        for (int32_t o = 0; o < layer->output_size; o++) {
            layer->bias_gradients->data[o] += grad_output->data[o];
        }
    }
    if (grad_input && grad_input->data) {
        for (int32_t i = 0; i < layer->input_size; i++) {
            float g = 0.0f;
            for (int32_t o = 0; o < layer->output_size; o++) {
                g += grad_output->data[o] * layer->weights->data[i * layer->output_size + o];
            }
            grad_input->data[i] = g;
        }
    }
}

void od_layer_freeze(ODLayer* layer)  { if (layer) layer->is_frozen = true; }
void od_layer_unfreeze(ODLayer* layer) { if (layer) layer->is_frozen = false; }
bool od_layer_is_frozen(const ODLayer* layer) { return layer ? layer->is_frozen : false; }

ODTransferModel* od_transfer_model_create(int32_t input_size, int32_t output_size,
    float learning_rate, OnDeviceLearnOptimizer optimizer)
{
    if (input_size <= 0 || output_size <= 0) return NULL;
    ODTransferModel* model = (ODTransferModel*)calloc(1, sizeof(ODTransferModel));
    if (!model) return NULL;
    model->input_size = input_size;
    model->output_size = output_size;
    model->learning_rate = learning_rate;
    model->optimizer = optimizer;
    model->momentum = 0.9f;
    model->beta1 = 0.9f;
    model->beta2 = 0.999f;
    model->epsilon = 1e-7f;
    return model;
}

void od_transfer_model_free(ODTransferModel* model)
{
    if (!model) return;
    for (int32_t i = 0; i < model->num_layers; i++) od_layer_free(model->layers[i]);
    free(model);
}

OnDeviceLearnStatus od_transfer_model_add_layer(ODTransferModel* model,
    OnDeviceLearnLayerType type, int32_t units)
{
    if (!model || model->num_layers >= ONDEVICE_LEARN_MAX_LAYERS)
        return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    int32_t input_dim = (model->num_layers == 0) ? model->input_size : model->layers[model->num_layers - 1]->output_size;
    ODLayer* layer = od_layer_create(type, input_dim, units);
    if (!layer) return ONDEVICE_LEARN_STATUS_MEMORY;
    model->layers[model->num_layers++] = layer;
    return ONDEVICE_LEARN_STATUS_OK;
}

OnDeviceLearnStatus od_transfer_model_freeze_base(ODTransferModel* model,
    int32_t num_layers_to_freeze)
{
    if (!model) return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    int32_t freeze_up_to = num_layers_to_freeze < model->num_layers
        ? num_layers_to_freeze : model->num_layers - 1;
    if (freeze_up_to < 0) freeze_up_to = 0;
    for (int32_t i = 0; i < freeze_up_to; i++) {
        od_layer_freeze(model->layers[i]);
    }
    model->num_frozen_layers = freeze_up_to;
    model->is_compiled = true;
    return ONDEVICE_LEARN_STATUS_OK;
}

OnDeviceLearnStatus od_transfer_model_train_classifier_head(ODTransferModel* model,
    const float* features, const float* labels, int32_t num_samples,
    int32_t input_dim, int32_t num_classes, int32_t epochs)
{
    if (!model || !features || !labels) return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    if (!model->is_compiled) return ONDEVICE_LEARN_STATUS_NOT_INITIALIZED;
    for (int32_t epoch = 0; epoch < epochs; epoch++) {
        float epoch_loss = 0.0f;
        for (int32_t s = 0; s < num_samples; s++) {
            float output[ONDEVICE_LEARN_MAX_CLASSES];
            od_transfer_model_predict(model, features + s * input_dim, input_dim, output, num_classes);
            for (int32_t c = 0; c < num_classes; c++) {
                float err = output[c] - labels[s * num_classes + c];
                epoch_loss += err * err;
                if (model->num_layers > 0) {
                    ODLayer* last = model->layers[model->num_layers - 1];
                    if (!od_layer_is_frozen(last) && last->bias && last->bias->data) {
                        last->bias->data[c] -= model->learning_rate * 2.0f * err;
                        if (last->weights && last->weights->data) {
                            for (int32_t i = 0; i < last->input_size; i++) {
                                float fi = (i < input_dim) ? features[s * input_dim + i] : 0.0f;
                                last->weights->data[i * last->output_size + c] -= model->learning_rate * 2.0f * err * fi;
                            }
                        }
                    }
                }
            }
        }
        epoch_loss /= (float)num_samples;
        if (epoch_loss < 1e-6f) break;
    }
    return ONDEVICE_LEARN_STATUS_OK;
}

OnDeviceLearnStatus od_transfer_model_predict(const ODTransferModel* model,
    const float* input, int32_t input_dim, float* output, int32_t output_dim)
{
    if (!model || !input || !output) return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    float current[ONDEVICE_LEARN_MAX_FEATURES];
    int32_t current_dim = input_dim < model->input_size ? input_dim : model->input_size;
    memcpy(current, input, (size_t)current_dim * sizeof(float));
    for (int32_t l = 0; l < model->num_layers; l++) {
        ODLayer* layer = model->layers[l];
        float* next = (float*)calloc((size_t)layer->output_size, sizeof(float));
        if (!next) return ONDEVICE_LEARN_STATUS_MEMORY;
        for (int32_t o = 0; o < layer->output_size; o++) {
            float sum = (layer->bias && layer->bias->data) ? layer->bias->data[o] : 0.0f;
            for (int32_t i = 0; i < layer->input_size && i < current_dim; i++) {
                sum += current[i] * layer->weights->data[i * layer->output_size + o];
            }
            next[o] = sum;
        }
        memcpy(current, next, (size_t)layer->output_size * sizeof(float));
        free(next);
        current_dim = layer->output_size;
    }
    memcpy(output, current, (size_t)output_dim * sizeof(float));
    return ONDEVICE_LEARN_STATUS_OK;
}

int32_t od_transfer_model_frozen_layer_count(const ODTransferModel* model)
{
    if (!model) return 0;
    return model->num_frozen_layers;
}

ODIncrementalLearner* od_incremental_learner_create(ODTransferModel* base_model,
    float learning_rate, int32_t buffer_size)
{
    ODIncrementalLearner* learner = (ODIncrementalLearner*)calloc(1, sizeof(ODIncrementalLearner));
    if (!learner) return NULL;
    learner->model = base_model;
    learner->learning_rate = learning_rate;
    learner->buffer_size = buffer_size;
    learner->decay_factor = 0.95f;
    learner->update_interval = 100;
    learner->sample_buffer = (float*)calloc((size_t)(buffer_size * base_model->input_size + buffer_size * base_model->output_size), sizeof(float));
    if (!learner->sample_buffer) { free(learner); return NULL; }
    return learner;
}

void od_incremental_learner_free(ODIncrementalLearner* learner)
{
    if (!learner) return;
    free(learner->sample_buffer);
    free(learner);
}

OnDeviceLearnStatus od_incremental_learner_add_sample(ODIncrementalLearner* learner,
    const float* features, int32_t feature_dim, const float* label, int32_t label_dim)
{
    if (!learner || !features || !label) return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    if (learner->buffer_count >= learner->buffer_size)
        learner->buffer_count = 0;
    int32_t feat_offset = learner->buffer_count * feature_dim;
    int32_t label_offset = learner->buffer_size * feature_dim + learner->buffer_count * label_dim;
    memcpy(learner->sample_buffer + feat_offset, features, (size_t)feature_dim * sizeof(float));
    memcpy(learner->sample_buffer + label_offset, label, (size_t)label_dim * sizeof(float));
    learner->buffer_count++;
    learner->samples_since_update++;
    if (learner->samples_since_update >= learner->update_interval) {
        od_incremental_learner_update(learner);
        learner->samples_since_update = 0;
    }
    return ONDEVICE_LEARN_STATUS_OK;
}

OnDeviceLearnStatus od_incremental_learner_update(ODIncrementalLearner* learner)
{
    if (!learner || !learner->model || learner->buffer_count == 0)
        return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    int32_t feat_dim = learner->model->input_size;
    int32_t lab_dim = learner->model->output_size;
    float total_loss = 0.0f;
    for (int32_t s = 0; s < learner->buffer_count; s++) {
        const float* features = learner->sample_buffer + s * feat_dim;
        const float* labels = learner->sample_buffer + learner->buffer_size * feat_dim + s * lab_dim;
        float output[ONDEVICE_LEARN_MAX_CLASSES];
        od_transfer_model_predict(learner->model, features, feat_dim, output, lab_dim);
        for (int32_t c = 0; c < lab_dim; c++) {
            float err = output[c] - labels[c];
            total_loss += err * err;
        }
    }
    total_loss /= (float)learner->buffer_count;
    learner->loss_moving_average = learner->decay_factor * learner->loss_moving_average
        + (1.0f - learner->decay_factor) * total_loss;
    return ONDEVICE_LEARN_STATUS_OK;
}

OnDeviceLearnStatus od_incremental_learner_progressive_update(ODIncrementalLearner* learner,
    int32_t num_iterations)
{
    if (!learner || num_iterations <= 0) return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    for (int32_t i = 0; i < num_iterations; i++) {
        od_incremental_learner_update(learner);
    }
    return ONDEVICE_LEARN_STATUS_OK;
}

float od_incremental_learner_current_loss(const ODIncrementalLearner* learner)
{
    if (!learner) return 0.0f;
    return learner->loss_moving_average;
}

void od_incremental_learner_reset_buffer(ODIncrementalLearner* learner)
{
    if (!learner) return;
    learner->buffer_count = 0;
    learner->samples_since_update = 0;
    if (learner->sample_buffer) memset(learner->sample_buffer, 0,
        (size_t)(learner->buffer_size * (learner->model->input_size + learner->model->output_size)) * sizeof(float));
}

ODFederatedClient* od_federated_client_create(int32_t client_id,
    void* model_weights, size_t weights_size)
{
    ODFederatedClient* client = (ODFederatedClient*)calloc(1, sizeof(ODFederatedClient));
    if (!client) return NULL;
    client->client_id = client_id;
    client->weights_size = weights_size;
    client->model_weights = malloc(weights_size);
    client->model_gradients = calloc(1, weights_size);
    if (!client->model_weights || !client->model_gradients) {
        od_federated_client_free(client); return NULL;
    }
    if (model_weights) memcpy(client->model_weights, model_weights, weights_size);
    client->learning_rate = ONDEVICE_LEARN_DEFAULT_LR;
    client->local_epochs = ONDEVICE_LEARN_DEFAULT_EPOCHS;
    client->batch_size = ONDEVICE_LEARN_DEFAULT_BATCH_SIZE;
    client->gradients_size = weights_size;
    return client;
}

void od_federated_client_free(ODFederatedClient* client)
{
    if (!client) return;
    free(client->model_weights);
    free(client->model_gradients);
    free(client);
}

OnDeviceLearnStatus od_federated_client_train_local(ODFederatedClient* client,
    const float* data, const float* labels, int32_t num_samples,
    int32_t input_dim, int32_t output_dim)
{
    if (!client || !data || !labels) return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    client->num_training_samples = num_samples;
    client->is_training = true;
    client->local_loss = 0.0f;
    for (int32_t s = 0; s < num_samples; s++) {
        for (int32_t j = 0; j < output_dim; j++) {
            float pred = 0.0f;
            for (int32_t i = 0; i < input_dim; i++) {
                pred += data[s * input_dim + i] * 0.001f;
            }
            float err = pred - labels[s * output_dim + j];
            client->local_loss += err * err;
        }
    }
    client->local_loss /= (float)(num_samples * output_dim);
    return ONDEVICE_LEARN_STATUS_OK;
}

OnDeviceLearnStatus od_federated_client_compute_gradients(ODFederatedClient* client)
{
    if (!client) return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    float* grads = (float*)client->model_gradients;
    float* weights = (float*)client->model_weights;
    int32_t count = (int32_t)(client->gradients_size / sizeof(float));
    for (int32_t i = 0; i < count; i++) {
        grads[i] = client->local_loss * weights[i] * 0.01f;
    }
    return ONDEVICE_LEARN_STATUS_OK;
}

OnDeviceLearnStatus od_federated_client_apply_updates(ODFederatedClient* client,
    const void* global_weights, size_t weights_size)
{
    if (!client || !global_weights || weights_size != client->weights_size)
        return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    memcpy(client->model_weights, global_weights, weights_size);
    return ONDEVICE_LEARN_STATUS_OK;
}

float od_federated_client_get_local_loss(const ODFederatedClient* client)
{
    return client ? client->local_loss : 0.0f;
}

size_t od_federated_client_gradient_size(const ODFederatedClient* client)
{
    return client ? client->gradients_size : 0;
}

ODFederatedAggregator* od_federated_aggregator_create(int32_t num_clients, const char* model_id)
{
    ODFederatedAggregator* agg = (ODFederatedAggregator*)calloc(1, sizeof(ODFederatedAggregator));
    if (!agg) return NULL;
    if (model_id) {
        strncpy(agg->model_id, model_id, 63);
        agg->model_id[63] = '\0';
    }
    agg->num_clients = num_clients;
    agg->aggregation_weight = 1.0f;
    return agg;
}

void od_federated_aggregator_free(ODFederatedAggregator* aggregator)
{
    free(aggregator);
}

OnDeviceLearnStatus od_federated_aggregator_add_client(ODFederatedAggregator* aggregator,
    ODFederatedClient* client)
{
    if (!aggregator || !client) return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    if (aggregator->num_clients >= ONDEVICE_LEARN_MAX_CLIENTS)
        return ONDEVICE_LEARN_STATUS_ERROR;
    aggregator->clients[aggregator->num_clients] = client;
    return ONDEVICE_LEARN_STATUS_OK;
}

OnDeviceLearnStatus od_federated_aggregator_federated_averaging(
    ODFederatedAggregator* aggregator, void* global_weights, size_t weights_size)
{
    if (!aggregator || !global_weights || aggregator->num_clients == 0)
        return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    float* gw = (float*)global_weights;
    int32_t count = (int32_t)(weights_size / sizeof(float));
    memset(gw, 0, weights_size);
    int32_t total_samples = 0;
    for (int32_t c = 0; c < aggregator->num_clients; c++) {
        total_samples += aggregator->clients[c]->num_training_samples;
    }
    for (int32_t c = 0; c < aggregator->num_clients; c++) {
        float weight = (total_samples > 0)
            ? (float)aggregator->clients[c]->num_training_samples / (float)total_samples
            : 1.0f / (float)aggregator->num_clients;
        float* cw = (float*)aggregator->clients[c]->model_weights;
        for (int32_t i = 0; i < count; i++) {
            gw[i] += weight * cw[i];
        }
    }
    aggregator->round_number++;
    return ONDEVICE_LEARN_STATUS_OK;
}

int32_t od_federated_aggregator_round(const ODFederatedAggregator* aggregator)
{
    return aggregator ? aggregator->round_number : 0;
}

ODUserProfile* od_user_profile_create(const char* user_id, int32_t feature_dim)
{
    ODUserProfile* profile = (ODUserProfile*)calloc(1, sizeof(ODUserProfile));
    if (!profile) return NULL;
    if (user_id) {
        strncpy(profile->user_id, user_id, 63);
        profile->user_id[63] = '\0';
    }
    profile->feature_dim = feature_dim;
    profile->personalization_factor = 0.5f;
    return profile;
}

void od_user_profile_free(ODUserProfile* profile)
{
    if (!profile) return;
    free(profile->user_data);
    free(profile->adaptation_weights);
    free(profile);
}

OnDeviceLearnStatus od_user_profile_add_data(ODUserProfile* profile,
    const float* data, int32_t num_samples, int32_t feature_dim)
{
    if (!profile || !data || feature_dim != profile->feature_dim)
        return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    profile->user_data = (float*)realloc(profile->user_data,
        (size_t)(profile->num_samples + num_samples) * (size_t)feature_dim * sizeof(float));
    if (!profile->user_data) return ONDEVICE_LEARN_STATUS_MEMORY;
    memcpy(profile->user_data + profile->num_samples * feature_dim,
        data, (size_t)(num_samples * feature_dim) * sizeof(float));
    profile->num_samples += num_samples;
    return ONDEVICE_LEARN_STATUS_OK;
}

OnDeviceLearnStatus od_user_profile_adapt(ODUserProfile* profile,
    ODTransferModel* model, int32_t num_epochs)
{
    if (!profile || !model || profile->num_samples == 0)
        return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    for (int32_t e = 0; e < num_epochs; e++) {
        for (int32_t s = 0; s < profile->num_samples; s++) {
            const float* sample = profile->user_data + s * profile->feature_dim;
            float output[ONDEVICE_LEARN_MAX_CLASSES];
            od_transfer_model_predict(model, sample, profile->feature_dim, output, model->output_size);
        }
    }
    profile->num_adaptations++;
    return ONDEVICE_LEARN_STATUS_OK;
}

OnDeviceLearnStatus od_user_profile_fine_tune(ODUserProfile* profile,
    ODTransferModel* model, const float* user_data, int32_t num_samples,
    int32_t feature_dim, int32_t num_epochs)
{
    if (!profile || !model || !user_data) return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    if (profile->num_samples == 0) return ONDEVICE_LEARN_STATUS_NOT_INITIALIZED;
    for (int32_t e = 0; e < num_epochs; e++) {
        for (int32_t s = 0; s < num_samples; s++) {
            const float* sample = user_data + s * feature_dim;
            float output[ONDEVICE_LEARN_MAX_CLASSES];
            od_transfer_model_predict(model, sample, feature_dim, output, model->output_size);
            for (int32_t c = 0; c < model->output_size; c++) {
                if (model->num_layers > 0) {
                    ODLayer* last = model->layers[model->num_layers - 1];
                    if (!od_layer_is_frozen(last)) {
                        float pred = output[c];
                        float err = pred - sample[c % feature_dim];
                        if (last->bias && last->bias->data)
                            last->bias->data[c] -= 0.0001f * err;
                    }
                }
            }
        }
    }
    profile->personalization_factor = fminf(1.0f, profile->personalization_factor + 0.1f);
    return ONDEVICE_LEARN_STATUS_OK;
}

float od_user_profile_personalization_score(const ODUserProfile* profile)
{
    return profile ? profile->personalization_factor : 0.0f;
}

OnDeviceLearner* ondevice_learner_create(OnDeviceLearnMethod method)
{
    OnDeviceLearner* learner = (OnDeviceLearner*)calloc(1, sizeof(OnDeviceLearner));
    if (!learner) return NULL;
    learner->method = method;
    learner->max_gradient_storage = (float)ONDEVICE_LEARN_MAX_GRADIENTS;
    return learner;
}

void ondevice_learner_free(OnDeviceLearner* learner)
{
    if (!learner) return;
    od_transfer_model_free(learner->transfer_model);
    od_incremental_learner_free(learner->incremental);
    od_federated_client_free(learner->federated);
    od_user_profile_free(learner->user_profile);
    free(learner);
}

OnDeviceLearnStatus ondevice_learner_init(OnDeviceLearner* learner,
    int32_t input_size, int32_t output_size)
{
    if (!learner) return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    switch (learner->method) {
    case ONDEVICE_LEARN_METHOD_TRANSFER:
    case ONDEVICE_LEARN_METHOD_INCREMENTAL:
        learner->transfer_model = od_transfer_model_create(input_size, output_size,
            ONDEVICE_LEARN_DEFAULT_LR, ONDEVICE_LEARN_OPTIMIZER_ADAM);
        if (!learner->transfer_model) return ONDEVICE_LEARN_STATUS_MEMORY;
        if (learner->method == ONDEVICE_LEARN_METHOD_INCREMENTAL) {
            learner->incremental = od_incremental_learner_create(
                learner->transfer_model, ONDEVICE_LEARN_DEFAULT_LR, 1024);
            if (!learner->incremental) return ONDEVICE_LEARN_STATUS_MEMORY;
            learner->transfer_model = NULL;
        }
        break;
    case ONDEVICE_LEARN_METHOD_FEDERATED: {
        size_t ws = (size_t)(input_size * output_size) * sizeof(float);
        learner->federated = od_federated_client_create(0, NULL, ws);
        if (!learner->federated) return ONDEVICE_LEARN_STATUS_MEMORY;
        break;
    }
    case ONDEVICE_LEARN_METHOD_PERSONALIZE:
        learner->user_profile = od_user_profile_create("default", input_size);
        if (!learner->user_profile) return ONDEVICE_LEARN_STATUS_MEMORY;
        break;
    default: break;
    }
    learner->is_initialized = true;
    return ONDEVICE_LEARN_STATUS_OK;
}

OnDeviceLearnStatus ondevice_learner_train(OnDeviceLearner* learner,
    const float* data, const float* labels, int32_t num_samples,
    int32_t feature_dim, int32_t label_dim)
{
    if (!learner || !data || !labels) return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    if (!learner->is_initialized) return ONDEVICE_LEARN_STATUS_NOT_INITIALIZED;
    switch (learner->method) {
    case ONDEVICE_LEARN_METHOD_TRANSFER:
        od_transfer_model_freeze_base(learner->transfer_model, 1);
        return od_transfer_model_train_classifier_head(learner->transfer_model,
            data, labels, num_samples, feature_dim, label_dim, ONDEVICE_LEARN_DEFAULT_EPOCHS);
    case ONDEVICE_LEARN_METHOD_INCREMENTAL:
        for (int32_t s = 0; s < num_samples; s++) {
            od_incremental_learner_add_sample(learner->incremental,
                data + s * feature_dim, feature_dim, labels + s * label_dim, label_dim);
        }
        return od_incremental_learner_update(learner->incremental);
    case ONDEVICE_LEARN_METHOD_FEDERATED:
        return od_federated_client_train_local(learner->federated,
            data, labels, num_samples, feature_dim, label_dim);
    case ONDEVICE_LEARN_METHOD_PERSONALIZE:
        od_user_profile_add_data(learner->user_profile, data, num_samples, feature_dim);
        return ONDEVICE_LEARN_STATUS_OK;
    default: return ONDEVICE_LEARN_STATUS_ERROR;
    }
}

OnDeviceLearnStatus ondevice_learner_predict(const OnDeviceLearner* learner,
    const float* input, int32_t input_dim, float* output, int32_t output_dim)
{
    if (!learner || !input || !output) return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    if (learner->transfer_model) {
        return od_transfer_model_predict(learner->transfer_model, input, input_dim, output, output_dim);
    }
    if (learner->incremental && learner->incremental->model) {
        return od_transfer_model_predict(learner->incremental->model, input, input_dim, output, output_dim);
    }
    for (int32_t i = 0; i < output_dim; i++) {
        output[i] = 0.0f;
        for (int32_t j = 0; j < input_dim; j++) {
            output[i] += input[j] * 0.001f;
        }
    }
    return ONDEVICE_LEARN_STATUS_OK;
}

OnDeviceLearnStatus ondevice_learner_check_gradient_storage(const OnDeviceLearner* learner)
{
    if (!learner) return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    if (learner->total_gradient_size > learner->max_gradient_storage)
        return ONDEVICE_LEARN_STATUS_GRADIENT_OVERFLOW;
    return ONDEVICE_LEARN_STATUS_OK;
}

OnDeviceLearnStatus ondevice_learner_clip_gradients(OnDeviceLearner* learner, float clip_value)
{
    if (!learner) return ONDEVICE_LEARN_STATUS_INVALID_PARAM;
    if (learner->transfer_model) {
        for (int32_t l = 0; l < learner->transfer_model->num_layers; l++) {
            ODLayer* layer = learner->transfer_model->layers[l];
            if (layer->weight_gradients) od_matrix_clip(layer->weight_gradients, -clip_value, clip_value);
            if (layer->bias_gradients) od_matrix_clip(layer->bias_gradients, -clip_value, clip_value);
        }
    }
    return ONDEVICE_LEARN_STATUS_OK;
}

void ondevice_learner_reset(OnDeviceLearner* learner)
{
    if (!learner) return;
    if (learner->incremental) od_incremental_learner_reset_buffer(learner->incremental);
    learner->total_gradient_size = 0.0f;
}
