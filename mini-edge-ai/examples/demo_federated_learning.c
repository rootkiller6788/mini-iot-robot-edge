#include "edge_pipeline.h"
#include "edge_inference.h"
#include "inference_opt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define FED_WEIGHTS      1024
#define FED_CLIENTS      5
#define FED_ROUNDS       20
#define FED_LOCAL_EPOCHS 3
#define FED_BATCH_SIZE   32
#define FED_LEARNING_RATE 0.01f

typedef struct {
    float    local_weights[FED_WEIGHTS];
    float    global_weights[FED_WEIGHTS];
    int      client_id;
    int      samples_seen;
    float    local_loss;
} FedClient;

typedef struct {
    FedClient    clients[FED_CLIENTS];
    PipelineCtx* pipeline;
    float        global_weights[FED_WEIGHTS];
    int          round;
    float        best_loss;
    int          total_samples;
} FedDemo;

static uint64_t fed_tick(void) { return (uint64_t)clock(); }

static void xavier_init(float* w, int n, int seed) {
    float scale = sqrtf(2.0f / (float)n);
    for (int i = 0; i < n; ++i)
        w[i] = (float)((i * 2654435761ULL + seed * 314159265ULL) % 10000) / 10000.0f * scale;
}

static float synthetic_sample(int idx, int dim) {
    return sinf((float)(idx * dim) * 0.1f) + cosf((float)(idx + dim) * 0.05f);
}

static float compute_loss(const float* pred, const float* target, int n) {
    float mse = 0.0f;
    for (int i = 0; i < n; ++i) {
        float d = pred[i] - target[i];
        mse += d * d;
    }
    return mse / (float)n;
}

static void local_train(FedClient* cl, int num_samples, int epochs) {
    float grad[FED_WEIGHTS];
    for (int ep = 0; ep < epochs; ++ep) {
        memset(grad, 0, sizeof(grad));
        for (int s = 0; s < num_samples; ++s) {
            float sample_value = synthetic_sample(s, FED_WEIGHTS);
            for (int w = 0; w < FED_WEIGHTS; ++w) {
                float target = sinf((float)(s + w) * 0.05f);
                float pred = cl->local_weights[w] * sample_value;
                float err = pred - target;
                grad[w] += err * sample_value;
            }
        }
        for (int w = 0; w < FED_WEIGHTS; ++w) {
            cl->local_weights[w] -= FED_LEARNING_RATE * grad[w] / (float)num_samples;
        }
        cl->samples_seen += num_samples;
    }
    float pred[FED_WEIGHTS];
    float target[FED_WEIGHTS];
    for (int w = 0; w < FED_WEIGHTS; ++w) {
        pred[w] = cl->local_weights[w];
        target[w] = sinf((float)w * 0.1f) + cosf((float)w * 0.05f);
    }
    cl->local_loss = compute_loss(pred, target, FED_WEIGHTS);
}

static void fed_aggregate(FedDemo* demo) {
    for (int w = 0; w < FED_WEIGHTS; ++w) {
        float sum = 0.0f;
        for (int c = 0; c < FED_CLIENTS; ++c) sum += demo->clients[c].local_weights[w];
        demo->global_weights[w] = sum / (float)FED_CLIENTS;
    }
}

static void broadcast_global(FedDemo* demo) {
    for (int c = 0; c < FED_CLIENTS; ++c)
        memcpy(demo->clients[c].local_weights, demo->global_weights, sizeof(demo->global_weights));
}

static int stage_fed_init(PipelineCtx* pipe, void* arg) {
    FedDemo* demo = (FedDemo*)arg;
    demo->round = 0;
    demo->best_loss = 1e9f;
    demo->total_samples = 0;
    xavier_init(demo->global_weights, FED_WEIGHTS, 42);
    for (int c = 0; c < FED_CLIENTS; ++c) {
        demo->clients[c].client_id = c;
        demo->clients[c].samples_seen = 0;
        demo->clients[c].local_loss = 0.0f;
        memcpy(demo->clients[c].local_weights, demo->global_weights,
               sizeof(demo->global_weights));
    }
    return 0;
}

static int stage_fed_round(PipelineCtx* pipe, void* arg) {
    FedDemo* demo = (FedDemo*)arg;
    int samples_per_client = FED_BATCH_SIZE * (1 + demo->round % 3);
    for (int c = 0; c < FED_CLIENTS; ++c) {
        local_train(&demo->clients[c], samples_per_client, FED_LOCAL_EPOCHS);
        demo->total_samples += samples_per_client;
    }
    fed_aggregate(demo);
    broadcast_global(demo);
    float avg_loss = 0.0f;
    for (int c = 0; c < FED_CLIENTS; ++c) avg_loss += demo->clients[c].local_loss;
    avg_loss /= (float)FED_CLIENTS;
    if (avg_loss < demo->best_loss) demo->best_loss = avg_loss;
    demo->round++;
    return 0;
}

static int stage_fed_report(PipelineCtx* pipe, void* arg) {
    FedDemo* demo = (FedDemo*)arg;
    float* w1 = demo->clients[0].local_weights;
    float* gw = demo->global_weights;
    float weight_diff = 0.0f;
    for (int w = 0; w < FED_WEIGHTS; ++w) weight_diff += fabsf(w1[w] - gw[w]);
    weight_diff /= (float)FED_WEIGHTS;
    fprintf(stdout, "Round %2d: loss=%.6f, best=%.6f, wdiff=%.6f, samples=%d\n",
            demo->round, demo->clients[0].local_loss,
            demo->best_loss, weight_diff, demo->total_samples);
    return 0;
}

int main(int argc, char* argv[]) {
    fprintf(stdout, "=== mini-edge-ai: Federated Learning Demo ===\n");
    fprintf(stdout, "Clients: %d, Rounds: %d, Weights: %d\n",
            FED_CLIENTS, FED_ROUNDS, FED_WEIGHTS);
    fprintf(stdout, "Local epochs: %d, Batch size: %d, LR: %.4f\n",
            FED_LOCAL_EPOCHS, FED_BATCH_SIZE, FED_LEARNING_RATE);

    FedDemo demo;
    memset(&demo, 0, sizeof(demo));
    demo.pipeline = pipeline_create();
    if (!demo.pipeline) { fprintf(stderr, "Failed\n"); return 1; }
    fed_learn_init(demo.pipeline, FED_WEIGHTS);

    pipeline_add_stage(demo.pipeline, STAGE_SENSOR_READ, stage_fed_init, &demo);
    pipeline_add_stage(demo.pipeline, STAGE_INFERENCE, stage_fed_round, &demo);
    pipeline_add_stage(demo.pipeline, STAGE_POSTPROCESS, stage_fed_report, &demo);

    stage_fed_init(demo.pipeline, &demo);
    uint64_t t0 = fed_tick();
    for (int r = 0; r < FED_ROUNDS; ++r) {
        stage_fed_round(demo.pipeline, &demo);
        stage_fed_report(demo.pipeline, &demo);
    }
    uint64_t t1 = fed_tick();
    fprintf(stdout, "\nSummary:\n");
    fprintf(stdout, "  Total rounds:  %d\n", FED_ROUNDS);
    fprintf(stdout, "  Total samples: %d\n", demo.total_samples);
    fprintf(stdout, "  Best loss:     %.6f\n", demo.best_loss);
    fprintf(stdout, "  Time:          %.1f ms\n", (float)(t1 - t0));
    float avg_per_round = (t1 > t0) ? (float)(t1 - t0) / (float)FED_ROUNDS : 0.0f;
    fprintf(stdout, "  Avg/round:     %.1f ms\n", avg_per_round);

    float client_weights_ptr[FED_CLIENTS][FED_WEIGHTS];
    const float* client_ptrs[FED_CLIENTS];
    for (int c = 0; c < FED_CLIENTS; ++c) {
        memcpy(client_weights_ptr[c], demo.clients[c].local_weights,
               sizeof(client_weights_ptr[c]));
        client_ptrs[c] = client_weights_ptr[c];
    }
    float final_global[FED_WEIGHTS];
    fed_learn_aggregate(demo.global_weights, FED_CLIENTS, client_ptrs,
                         FED_WEIGHTS, final_global);
    float agg_diff = 0.0f;
    for (int w = 0; w < FED_WEIGHTS; ++w)
        agg_diff += fabsf(final_global[w] - demo.global_weights[w]);
    fprintf(stdout, "  Aggregate diff: %.6f\n", agg_diff / (float)FED_WEIGHTS);
    fed_learn_get_weights(demo.pipeline, final_global, FED_WEIGHTS);
    pipeline_destroy(demo.pipeline);
    return 0;
}
