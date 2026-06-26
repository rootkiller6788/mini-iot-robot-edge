#ifndef ONDEVICE_LEARN_H
#define ONDEVICE_LEARN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ONDEVICE_LEARN_MAX_LAYERS          32
#define ONDEVICE_LEARN_MAX_GRADIENTS       (256 * 1024)
#define ONDEVICE_LEARN_MAX_CLASSES         64
#define ONDEVICE_LEARN_MAX_FEATURES        1024
#define ONDEVICE_LEARN_MAX_CLIENTS         32
#define ONDEVICE_LEARN_MAX_USER_PROFILES   16
#define ONDEVICE_LEARN_DEFAULT_LR          0.001f
#define ONDEVICE_LEARN_DEFAULT_EPOCHS      10
#define ONDEVICE_LEARN_DEFAULT_BATCH_SIZE  32
#define ONDEVICE_LEARN_GRADIENT_CLIP       1.0f

typedef enum {
    ONDEVICE_LEARN_STATUS_OK = 0,
    ONDEVICE_LEARN_STATUS_ERROR,
    ONDEVICE_LEARN_STATUS_INVALID_PARAM,
    ONDEVICE_LEARN_STATUS_MEMORY,
    ONDEVICE_LEARN_STATUS_NOT_INITIALIZED,
    ONDEVICE_LEARN_STATUS_CONVERGED,
    ONDEVICE_LEARN_STATUS_GRADIENT_OVERFLOW
} OnDeviceLearnStatus;

typedef enum {
    ONDEVICE_LEARN_METHOD_TRANSFER = 0,
    ONDEVICE_LEARN_METHOD_INCREMENTAL,
    ONDEVICE_LEARN_METHOD_FEDERATED,
    ONDEVICE_LEARN_METHOD_PERSONALIZE
} OnDeviceLearnMethod;

typedef enum {
    ONDEVICE_LEARN_OPTIMIZER_SGD = 0,
    ONDEVICE_LEARN_OPTIMIZER_MOMENTUM,
    ONDEVICE_LEARN_OPTIMIZER_ADAM,
    ONDEVICE_LEARN_OPTIMIZER_RMSPROP
} OnDeviceLearnOptimizer;

typedef enum {
    ONDEVICE_LEARN_LAYER_DENSE = 0,
    ONDEVICE_LEARN_LAYER_RELU,
    ONDEVICE_LEARN_LAYER_SOFTMAX,
    ONDEVICE_LEARN_LAYER_DROPOUT,
    ONDEVICE_LEARN_LAYER_CONV2D
} OnDeviceLearnLayerType;

typedef struct {
    float*  data;
    int32_t rows;
    int32_t cols;
    int32_t capacity;
} ODMatrix;

typedef struct {
    ODMatrix* weights;
    ODMatrix* bias;
    ODMatrix* weight_gradients;
    ODMatrix* bias_gradients;
    float     momentum_w;
    float     momentum_b;
    float     velocity_w;
    float     velocity_b;
    int32_t   input_size;
    int32_t   output_size;
    bool      is_frozen;
    bool      is_trained;
} ODLayer;

typedef struct {
    ODLayer*              layers[ONDEVICE_LEARN_MAX_LAYERS];
    int32_t               num_layers;
    int32_t               input_size;
    int32_t               output_size;
    int32_t               num_frozen_layers;
    float                 learning_rate;
    OnDeviceLearnOptimizer optimizer;
    float                 momentum;
    float                 beta1;
    float                 beta2;
    float                 epsilon;
    bool                  is_compiled;
} ODTransferModel;

typedef struct {
    ODTransferModel* model;
    float*           sample_buffer;
    int32_t          buffer_size;
    int32_t          buffer_count;
    float            learning_rate;
    float            decay_factor;
    int32_t          update_interval;
    int32_t          samples_since_update;
    float            loss_moving_average;
} ODIncrementalLearner;

typedef struct {
    void*    model_weights;
    void*    model_gradients;
    size_t   weights_size;
    size_t   gradients_size;
    int32_t  client_id;
    int32_t  num_training_samples;
    float    local_loss;
    float    learning_rate;
    int32_t  local_epochs;
    int32_t  batch_size;
    bool     is_training;
} ODFederatedClient;

typedef struct {
    ODFederatedClient* clients[ONDEVICE_LEARN_MAX_CLIENTS];
    int32_t            num_clients;
    float              aggregation_weight;
    int32_t            round_number;
    char               model_id[64];
} ODFederatedAggregator;

typedef struct {
    float*   user_data;
    int32_t  num_samples;
    int32_t  feature_dim;
    float*   adaptation_weights;
    int32_t  num_adaptations;
    float    personalization_factor;
    char     user_id[64];
} ODUserProfile;

typedef struct {
    OnDeviceLearnMethod  method;
    ODTransferModel*     transfer_model;
    ODIncrementalLearner* incremental;
    ODFederatedClient*   federated;
    ODUserProfile*       user_profile;
    float                total_gradient_size;
    float                max_gradient_storage;
    bool                 is_initialized;
} OnDeviceLearner;

ODMatrix*             od_matrix_create(int32_t rows, int32_t cols);
void                  od_matrix_free(ODMatrix* mat);
void                  od_matrix_fill_random(ODMatrix* mat, float scale);
void                  od_matrix_fill_zeros(ODMatrix* mat);
void                  od_matrix_clip(ODMatrix* mat, float min_val, float max_val);
float                 od_matrix_gradient_norm(const ODMatrix* grad);

ODLayer*              od_layer_create(OnDeviceLearnLayerType type, int32_t input_size, int32_t output_size);
void                  od_layer_free(ODLayer* layer);
void                  od_layer_forward(const ODLayer* layer, const ODMatrix* input, ODMatrix* output);
void                  od_layer_backward(ODLayer* layer, const ODMatrix* input, const ODMatrix* grad_output, ODMatrix* grad_input);
void                  od_layer_freeze(ODLayer* layer);
void                  od_layer_unfreeze(ODLayer* layer);
bool                  od_layer_is_frozen(const ODLayer* layer);

ODTransferModel*      od_transfer_model_create(int32_t input_size, int32_t output_size, float learning_rate, OnDeviceLearnOptimizer optimizer);
void                  od_transfer_model_free(ODTransferModel* model);
OnDeviceLearnStatus   od_transfer_model_add_layer(ODTransferModel* model, OnDeviceLearnLayerType type, int32_t units);
OnDeviceLearnStatus   od_transfer_model_freeze_base(ODTransferModel* model, int32_t num_layers_to_freeze);
OnDeviceLearnStatus   od_transfer_model_train_classifier_head(ODTransferModel* model, const float* features, const float* labels, int32_t num_samples, int32_t input_dim, int32_t num_classes, int32_t epochs);
OnDeviceLearnStatus   od_transfer_model_predict(const ODTransferModel* model, const float* input, int32_t input_dim, float* output, int32_t output_dim);
int32_t               od_transfer_model_frozen_layer_count(const ODTransferModel* model);

ODIncrementalLearner* od_incremental_learner_create(ODTransferModel* base_model, float learning_rate, int32_t buffer_size);
void                  od_incremental_learner_free(ODIncrementalLearner* learner);
OnDeviceLearnStatus   od_incremental_learner_add_sample(ODIncrementalLearner* learner, const float* features, int32_t feature_dim, const float* label, int32_t label_dim);
OnDeviceLearnStatus   od_incremental_learner_update(ODIncrementalLearner* learner);
OnDeviceLearnStatus   od_incremental_learner_progressive_update(ODIncrementalLearner* learner, int32_t num_iterations);
float                 od_incremental_learner_current_loss(const ODIncrementalLearner* learner);
void                  od_incremental_learner_reset_buffer(ODIncrementalLearner* learner);

ODFederatedClient*    od_federated_client_create(int32_t client_id, void* model_weights, size_t weights_size);
void                  od_federated_client_free(ODFederatedClient* client);
OnDeviceLearnStatus   od_federated_client_train_local(ODFederatedClient* client, const float* data, const float* labels, int32_t num_samples, int32_t input_dim, int32_t output_dim);
OnDeviceLearnStatus   od_federated_client_compute_gradients(ODFederatedClient* client);
OnDeviceLearnStatus   od_federated_client_apply_updates(ODFederatedClient* client, const void* global_weights, size_t weights_size);
float                 od_federated_client_get_local_loss(const ODFederatedClient* client);
size_t                od_federated_client_gradient_size(const ODFederatedClient* client);

ODFederatedAggregator* od_federated_aggregator_create(int32_t num_clients, const char* model_id);
void                   od_federated_aggregator_free(ODFederatedAggregator* aggregator);
OnDeviceLearnStatus    od_federated_aggregator_add_client(ODFederatedAggregator* aggregator, ODFederatedClient* client);
OnDeviceLearnStatus    od_federated_aggregator_federated_averaging(ODFederatedAggregator* aggregator, void* global_weights, size_t weights_size);
int32_t                od_federated_aggregator_round(const ODFederatedAggregator* aggregator);

ODUserProfile*         od_user_profile_create(const char* user_id, int32_t feature_dim);
void                   od_user_profile_free(ODUserProfile* profile);
OnDeviceLearnStatus    od_user_profile_add_data(ODUserProfile* profile, const float* data, int32_t num_samples, int32_t feature_dim);
OnDeviceLearnStatus    od_user_profile_adapt(ODUserProfile* profile, ODTransferModel* model, int32_t num_epochs);
OnDeviceLearnStatus    od_user_profile_fine_tune(ODUserProfile* profile, ODTransferModel* model, const float* user_data, int32_t num_samples, int32_t feature_dim, int32_t num_epochs);
float                  od_user_profile_personalization_score(const ODUserProfile* profile);

OnDeviceLearner*       ondevice_learner_create(OnDeviceLearnMethod method);
void                   ondevice_learner_free(OnDeviceLearner* learner);
OnDeviceLearnStatus    ondevice_learner_init(OnDeviceLearner* learner, int32_t input_size, int32_t output_size);
OnDeviceLearnStatus    ondevice_learner_train(OnDeviceLearner* learner, const float* data, const float* labels, int32_t num_samples, int32_t feature_dim, int32_t label_dim);
OnDeviceLearnStatus    ondevice_learner_predict(const OnDeviceLearner* learner, const float* input, int32_t input_dim, float* output, int32_t output_dim);
OnDeviceLearnStatus    ondevice_learner_check_gradient_storage(const OnDeviceLearner* learner);
OnDeviceLearnStatus    ondevice_learner_clip_gradients(OnDeviceLearner* learner, float clip_value);
void                   ondevice_learner_reset(OnDeviceLearner* learner);

#ifdef __cplusplus
}
#endif

#endif /* ONDEVICE_LEARN_H */
