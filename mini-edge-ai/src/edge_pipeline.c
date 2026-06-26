#include "edge_pipeline.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct PipelineCtx {
    PipelineState        state;
    StageInfo            stages[MAX_PIPELINE_STAGES];
    int                  num_stages;
    int (*stage_fns[MAX_PIPELINE_STAGES])(struct PipelineCtx*, void*);
    void*                stage_args[MAX_PIPELINE_STAGES];
    ActionSpec           actions[ACTION_COUNT];
    int                  num_actions;
    FedLearnState        fed_state;
    InferenceResult      last_result;
    uint64_t             last_upload_ms;
    int                  uploads_this_minute;
    BandwidthPolicy      bw_policy;
    uint64_t             start_ms;
};

static uint64_t pipeline_now_ms(void) {
    return 0;
}

PipelineCtx* pipeline_create(void) {
    PipelineCtx* ctx = (PipelineCtx*)calloc(1, sizeof(PipelineCtx));
    if (!ctx) return NULL;
    ctx->state = PIPELINE_IDLE;
    ctx->num_stages = 0;
    ctx->num_actions = 0;
    ctx->bw_policy.confidence_thresh = 0.7f;
    ctx->bw_policy.min_interval_ms = 1000;
    ctx->bw_policy.max_per_minute = 10;
    ctx->bw_policy.upload_on_detect = 1;
    ctx->bw_policy.save_local = 1;
    ctx->bw_policy.low_bandwidth_mode = 0;
    ctx->last_upload_ms = 0;
    ctx->uploads_this_minute = 0;
    ctx->start_ms = pipeline_now_ms();
    return ctx;
}

void pipeline_destroy(PipelineCtx* ctx) {
    if (!ctx) return;
    free(ctx->fed_state.local_weights);
    free(ctx);
}

int pipeline_add_stage(PipelineCtx* ctx, StageType type,
                        int (*fn)(PipelineCtx*, void*), void* arg) {
    if (!ctx || ctx->num_stages >= MAX_PIPELINE_STAGES) return -1;
    int idx = ctx->num_stages++;
    ctx->stages[idx].type = type;
    ctx->stages[idx].name[0] = '\0';
    ctx->stages[idx].elapsed_ms = 0.0f;
    ctx->stages[idx].ok = 0;
    ctx->stage_fns[idx] = fn;
    ctx->stage_args[idx] = arg;
    return idx;
}

int pipeline_run(PipelineCtx* ctx) {
    if (!ctx || ctx->state == PIPELINE_ERROR) return -1;
    ctx->state = PIPELINE_RUNNING;
    for (int i = 0; i < ctx->num_stages; ++i) {
        uint64_t t0 = pipeline_now_ms();
        if (ctx->stage_fns[i]) {
            int rc = ctx->stage_fns[i](ctx, ctx->stage_args[i]);
            ctx->stages[i].ok = (rc == 0);
        }
        uint64_t t1 = pipeline_now_ms();
        ctx->stages[i].elapsed_ms = (float)(t1 - t0);
    }
    ctx->state = PIPELINE_IDLE;
    return 0;
}

int pipeline_run_loop(PipelineCtx* ctx, int max_iterations) {
    for (int iter = 0; iter < max_iterations; ++iter) {
        if (ctx->state == PIPELINE_STOPPED) break;
        pipeline_run(ctx);
    }
    return 0;
}

int pipeline_stop(PipelineCtx* ctx) {
    if (!ctx) return -1;
    ctx->state = PIPELINE_STOPPED;
    return 0;
}

PipelineState pipeline_get_state(const PipelineCtx* ctx) {
    return ctx ? ctx->state : PIPELINE_ERROR;
}

int pipeline_get_stages(PipelineCtx* ctx, StageInfo* out, int max_out) {
    if (!ctx || !out) return -1;
    int n = ctx->num_stages < max_out ? ctx->num_stages : max_out;
    memcpy(out, ctx->stages, (size_t)n * sizeof(StageInfo));
    return n;
}

int pipeline_set_action(PipelineCtx* ctx, const ActionSpec* action) {
    if (!ctx || !action) return -1;
    if (ctx->num_actions >= ACTION_COUNT) return -1;
    memcpy(&ctx->actions[ctx->num_actions++], action, sizeof(ActionSpec));
    return 0;
}

int pipeline_execute_action(PipelineCtx* ctx, ActionType type,
                             const InferenceResult* result) {
    if (!ctx || !result) return -1;
    for (int i = 0; i < ctx->num_actions; ++i) {
        if (ctx->actions[i].type != type) continue;
        if (result->confidence >= ctx->actions[i].threshold) {
            if (type == ACTION_SNAPSHOT) {
                snprintf((char*)result->snapshot_path, sizeof(result->snapshot_path),
                         "snapshot_%llu.jpg", (unsigned long long)result->timestamp_ms);
            }
            if (type == ACTION_ALERT) {
            }
            return 0;
        }
    }
    return 0;
}

int pipeline_should_upload(const BandwidthPolicy* policy,
                            const InferenceResult* result,
                            uint64_t last_upload_ms, int uploads_this_minute) {
    if (!policy || !result) return 0;
    if (result->confidence < policy->confidence_thresh) return 0;
    uint64_t now = pipeline_now_ms();
    if (now - last_upload_ms < (uint64_t)policy->min_interval_ms) return 0;
    if (uploads_this_minute >= policy->max_per_minute) return 0;
    return 1;
}

int fed_learn_init(PipelineCtx* ctx, int num_weights) {
    if (!ctx || num_weights <= 0) return -1;
    ctx->fed_state.local_weights = (float*)calloc((size_t)num_weights, sizeof(float));
    if (!ctx->fed_state.local_weights) return -1;
    ctx->fed_state.num_weights = num_weights;
    ctx->fed_state.local_epochs = 0;
    ctx->fed_state.round = 0;
    ctx->fed_state.learning_rate = 0.001f;
    ctx->fed_state.samples_seen = 0;
    return 0;
}

int fed_learn_train_local(PipelineCtx* ctx, const float* samples,
                           int num_samples, int epochs) {
    if (!ctx || !samples || num_samples <= 0) return -1;
    for (int ep = 0; ep < epochs; ++ep) {
        for (int i = 0; i < (int)ctx->fed_state.num_weights; ++i) {
            float grad = samples[i % num_samples] * 0.01f;
            ctx->fed_state.local_weights[i] -= ctx->fed_state.learning_rate * grad;
        }
        ctx->fed_state.samples_seen += (uint64_t)num_samples;
    }
    ctx->fed_state.local_epochs += epochs;
    ctx->fed_state.round++;
    return 0;
}

int fed_learn_get_weights(const PipelineCtx* ctx, float* weights, int max_w) {
    if (!ctx || !weights) return -1;
    int n = (int)ctx->fed_state.num_weights < max_w ? (int)ctx->fed_state.num_weights : max_w;
    memcpy(weights, ctx->fed_state.local_weights, (size_t)n * sizeof(float));
    return n;
}

int fed_learn_upload_weights(const PipelineCtx* ctx, const char* server_url) {
    (void)ctx; (void)server_url;
    return 0;
}

int fed_learn_download_global(PipelineCtx* ctx, const char* server_url) {
    (void)ctx; (void)server_url;
    ctx->fed_state.round++;
    return 0;
}

int fed_learn_aggregate(const float* local_weights, int num_clients,
                         const float** client_weights, int n_weights,
                         float* global_weights) {
    if (!local_weights || !client_weights || !global_weights ||
        num_clients <= 0 || n_weights <= 0) return -1;
    for (int w = 0; w < n_weights; ++w) {
        float sum = 0.0f;
        for (int c = 0; c < num_clients; ++c) sum += client_weights[c][w];
        global_weights[w] = sum / (float)num_clients;
    }
    return 0;
}
