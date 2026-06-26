#include "tflite_micro.h"
#include "ondevice_learn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define FEATURE_DIM 10
#define NUM_CLASSES 3
#define NUM_SAMPLES 100

static void generate_training_data(float* features, float* labels, int32_t num_samples,
    int32_t feature_dim, int32_t num_classes)
{
    for (int32_t s = 0; s < num_samples; s++) {
        for (int32_t f = 0; f < feature_dim; f++) {
            float u1 = (float)rand() / (float)RAND_MAX;
            float u2 = (float)rand() / (float)RAND_MAX;
            features[s * feature_dim + f] = sqrtf(-2.0f * logf(u1 + 1e-7f)) * cosf(2.0f * 3.14159265358979323846f * u2);
        }
        int32_t label_class = s % num_classes;
        for (int32_t c = 0; c < num_classes; c++) {
            labels[s * num_classes + c] = (c == label_class) ? 1.0f : 0.0f;
        }
    }
}

int main(void)
{
    printf("=== mini-tinyml On-Device Learning Example ===\n\n");

    float* features = (float*)malloc((size_t)(NUM_SAMPLES * FEATURE_DIM) * sizeof(float));
    float* labels   = (float*)malloc((size_t)(NUM_SAMPLES * NUM_CLASSES) * sizeof(float));
    generate_training_data(features, labels, NUM_SAMPLES, FEATURE_DIM, NUM_CLASSES);

    printf("--- Test: Matrix Operations ---\n");
    ODMatrix* mat = od_matrix_create(3, 4);
    od_matrix_fill_random(mat, 0.5f);
    printf("[OK] Matrix: %d x %d\n", mat->rows, mat->cols);
    od_matrix_clip(mat, -1.0f, 1.0f);
    float norm = od_matrix_gradient_norm(mat);
    printf("     Gradient norm: %.6f\n", (double)norm);
    od_matrix_free(mat);

    ODMatrix* zeros = od_matrix_create(5, 2);
    od_matrix_fill_zeros(zeros);
    printf("[OK] Zero matrix: %d x %d\n\n", zeros->rows, zeros->cols);
    od_matrix_free(zeros);

    printf("--- Test: Layer Operations ---\n");
    ODLayer* layer = od_layer_create(ONDEVICE_LEARN_LAYER_DENSE, FEATURE_DIM, 16);
    printf("[OK] Dense layer: input=%d, output=%d\n", layer->input_size, layer->output_size);
    printf("     is_frozen: %s\n", od_layer_is_frozen(layer) ? "true" : "false");
    od_layer_freeze(layer);
    printf("     after freeze: %s\n", od_layer_is_frozen(layer) ? "true" : "false");

    ODMatrix* input_mat = od_matrix_create(1, FEATURE_DIM);
    memcpy(input_mat->data, features, (size_t)FEATURE_DIM * sizeof(float));
    ODMatrix* output_mat = od_matrix_create(1, 16);
    od_layer_forward(layer, input_mat, output_mat);
    printf("     Forward pass: first output=%.6f\n", (double)output_mat->data[0]);

    ODMatrix* grad_input = od_matrix_create(1, FEATURE_DIM);
    od_layer_backward(layer, input_mat, output_mat, grad_input);
    printf("     Backward pass: grad_input norm=%.6f\n\n", (double)od_matrix_gradient_norm(grad_input));
    od_matrix_free(input_mat);
    od_matrix_free(output_mat);
    od_matrix_free(grad_input);
    od_layer_free(layer);

    printf("--- Test: Transfer Learning ---\n");
    ODTransferModel* model = od_transfer_model_create(FEATURE_DIM, NUM_CLASSES, 0.01f, ONDEVICE_LEARN_OPTIMIZER_SGD);
    od_transfer_model_add_layer(model, ONDEVICE_LEARN_LAYER_DENSE, 32);
    od_transfer_model_add_layer(model, ONDEVICE_LEARN_LAYER_RELU, 32);
    od_transfer_model_add_layer(model, ONDEVICE_LEARN_LAYER_DENSE, NUM_CLASSES);
    printf("[OK] Model: %d layers\n", model->num_layers);
    od_transfer_model_freeze_base(model, 2);
    printf("     Frozen layers: %d\n", od_transfer_model_frozen_layer_count(model));

    od_transfer_model_train_classifier_head(model, features, labels, NUM_SAMPLES, FEATURE_DIM, NUM_CLASSES, 20);
    printf("     Training: %d samples, %d epochs\n", NUM_SAMPLES, 20);

    float test_input[FEATURE_DIM];
    float test_output[NUM_CLASSES];
    memcpy(test_input, features, (size_t)FEATURE_DIM * sizeof(float));
    od_transfer_model_predict(model, test_input, FEATURE_DIM, test_output, NUM_CLASSES);
    printf("     Prediction: [");
    for (int32_t i = 0; i < NUM_CLASSES; i++) printf(" %.4f", (double)test_output[i]);
    printf(" ]\n\n");

    printf("--- Test: Incremental Learning ---\n");
    ODTransferModel* inc_model = od_transfer_model_create(FEATURE_DIM, NUM_CLASSES, 0.01f, ONDEVICE_LEARN_OPTIMIZER_ADAM);
    od_transfer_model_add_layer(inc_model, ONDEVICE_LEARN_LAYER_DENSE, 16);
    od_transfer_model_add_layer(inc_model, ONDEVICE_LEARN_LAYER_DENSE, NUM_CLASSES);
    ODIncrementalLearner* incremental = od_incremental_learner_create(inc_model, 0.01f, 64);
    inc_model = NULL;
    printf("[OK] Incremental learner: buffer_size=%d\n", incremental->buffer_size);

    for (int32_t s = 0; s < 30; s++) {
        od_incremental_learner_add_sample(incremental,
            features + s * FEATURE_DIM, FEATURE_DIM,
            labels + s * NUM_CLASSES, NUM_CLASSES);
    }
    od_incremental_learner_progressive_update(incremental, 5);
    printf("     Loss after updates: %.6f\n\n", (double)od_incremental_learner_current_loss(incremental));
    od_incremental_learner_free(incremental);

    printf("--- Test: Federated Learning ---\n");
    ODFederatedClient* client1 = od_federated_client_create(1, model, (size_t)(FEATURE_DIM * NUM_CLASSES) * sizeof(float));
    ODFederatedClient* client2 = od_federated_client_create(2, model, (size_t)(FEATURE_DIM * NUM_CLASSES) * sizeof(float));
    printf("[OK] Federated clients: %d + %d\n", client1->client_id, client2->client_id);

    od_federated_client_train_local(client1, features, labels, 50, FEATURE_DIM, NUM_CLASSES);
    od_federated_client_train_local(client2, features + 50 * FEATURE_DIM, labels + 50 * NUM_CLASSES, 50, FEATURE_DIM, NUM_CLASSES);
    od_federated_client_compute_gradients(client1);
    od_federated_client_compute_gradients(client2);
    printf("     Client 1 loss: %.6f, grad size: %zu bytes\n",
        (double)od_federated_client_get_local_loss(client1),
        od_federated_client_gradient_size(client1));
    printf("     Client 2 loss: %.6f\n", (double)od_federated_client_get_local_loss(client2));

    ODFederatedAggregator* agg = od_federated_aggregator_create(2, "demo_model_v1");
    od_federated_aggregator_add_client(agg, client1);
    od_federated_aggregator_federated_averaging(agg, client1->model_weights, client1->weights_size);
    printf("     Round: %d (FedAvg done)\n\n", od_federated_aggregator_round(agg));

    od_federated_aggregator_free(agg);
    od_federated_client_free(client1);
    od_federated_client_free(client2);

    printf("--- Test: Personalization ---\n");
    ODUserProfile* profile = od_user_profile_create("user_001", FEATURE_DIM);
    od_user_profile_add_data(profile, features, 20, FEATURE_DIM);
    printf("[OK] User '%s': %d samples\n", profile->user_id, profile->num_samples);

    ODTransferModel* pers_model = od_transfer_model_create(FEATURE_DIM, NUM_CLASSES, 0.001f, ONDEVICE_LEARN_OPTIMIZER_MOMENTUM);
    od_transfer_model_add_layer(pers_model, ONDEVICE_LEARN_LAYER_DENSE, 16);
    od_transfer_model_add_layer(pers_model, ONDEVICE_LEARN_LAYER_DENSE, NUM_CLASSES);
    od_user_profile_adapt(profile, pers_model, 5);
    od_user_profile_fine_tune(profile, pers_model, features, 20, FEATURE_DIM, 10);
    printf("     Personalization score: %.2f\n\n", (double)od_user_profile_personalization_score(profile));
    od_transfer_model_free(pers_model);
    od_user_profile_free(profile);

    printf("--- Test: Full On-Device Learner ---\n");
    OnDeviceLearner* learner = ondevice_learner_create(ONDEVICE_LEARN_METHOD_TRANSFER);
    ondevice_learner_init(learner, FEATURE_DIM, NUM_CLASSES);
    ondevice_learner_train(learner, features, labels, NUM_SAMPLES, FEATURE_DIM, NUM_CLASSES);

    float learner_output[NUM_CLASSES];
    ondevice_learner_predict(learner, test_input, FEATURE_DIM, learner_output, NUM_CLASSES);
    printf("[OK] Learner prediction: [");
    for (int32_t i = 0; i < NUM_CLASSES; i++) printf(" %.4f", (double)learner_output[i]);
    printf(" ]\n");

    OnDeviceLearnStatus gs = ondevice_learner_check_gradient_storage(learner);
    printf("     Gradient check: %s\n",
        gs == ONDEVICE_LEARN_STATUS_GRADIENT_OVERFLOW ? "OVERFLOW" : "OK");
    ondevice_learner_clip_gradients(learner, 1.0f);
    ondevice_learner_free(learner);

    od_transfer_model_free(model);
    free(features);
    free(labels);
    printf("\n=== Done ===\n");
    return 0;
}
