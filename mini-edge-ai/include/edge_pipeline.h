#ifndef EDGE_PIPELINE_H
#define EDGE_PIPELINE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PIPELINE_STAGES 8
#define MAX_ACTION_NAME    64
#define MAX_UPLOAD_URL    512
#define MAX_FED_WEIGHTS    1048576

typedef enum {
    PIPELINE_IDLE,
    PIPELINE_RUNNING,
    PIPELINE_STOPPED,
    PIPELINE_ERROR
} PipelineState;

typedef enum {
    STAGE_SENSOR_READ,
    STAGE_PREPROCESS,
    STAGE_INFERENCE,
    STAGE_POSTPROCESS,
    STAGE_DECISION,
    STAGE_ACTION,
    STAGE_COUNT
} StageType;

typedef enum {
    ACTION_NONE,
    ACTION_SNAPSHOT,
    ACTION_CLOUD_UPLOAD,
    ACTION_ALERT,
    ACTION_MOTOR_CTRL,
    ACTION_COUNT
} ActionType;

typedef enum {
    TASK_PERSON_DETECT,
    TASK_KEYWORD_SPOT,
    TASK_ANOMALY_DETECT,
    TASK_GESTURE_RECOG,
    TASK_COUNT
} TaskType;

typedef struct {
    StageType  type;
    char       name[64];
    float      elapsed_ms;
    int        ok;
} StageInfo;

typedef struct {
    TaskType   task;
    int        person_detected;
    int        keyword_id;
    float      anomaly_score;
    float      confidence;
    uint64_t   timestamp_ms;
    char       snapshot_path[256];
} InferenceResult;

typedef struct {
    ActionType type;
    char       name[MAX_ACTION_NAME];
    int        immediate;
    float      threshold;
    char       cloud_url[MAX_UPLOAD_URL];
} ActionSpec;

typedef struct {
    float      confidence_thresh;
    int        min_interval_ms;
    int        max_per_minute;
    uint8_t    upload_on_detect;
    uint8_t    save_local;
    uint8_t    low_bandwidth_mode;
} BandwidthPolicy;

typedef struct {
    float*     local_weights;
    size_t     num_weights;
    int        local_epochs;
    int        round;
    float      learning_rate;
    uint64_t   samples_seen;
} FedLearnState;

typedef struct PipelineCtx PipelineCtx;

PipelineCtx*   pipeline_create(void);
void           pipeline_destroy(PipelineCtx* ctx);
int            pipeline_add_stage(PipelineCtx* ctx, StageType type,
                                  int (*fn)(PipelineCtx*, void*), void* arg);
int            pipeline_run(PipelineCtx* ctx);
int            pipeline_run_loop(PipelineCtx* ctx, int max_iterations);
int            pipeline_stop(PipelineCtx* ctx);
PipelineState  pipeline_get_state(const PipelineCtx* ctx);
int            pipeline_get_stages(PipelineCtx* ctx, StageInfo* out, int max_out);

int            pipeline_set_action(PipelineCtx* ctx, const ActionSpec* action);
int            pipeline_execute_action(PipelineCtx* ctx, ActionType type,
                                       const InferenceResult* result);
int            pipeline_should_upload(const BandwidthPolicy* policy,
                                      const InferenceResult* result,
                                      uint64_t last_upload_ms, int uploads_this_minute);

int            fed_learn_init(PipelineCtx* ctx, int num_weights);
int            fed_learn_train_local(PipelineCtx* ctx, const float* samples,
                                     int num_samples, int epochs);
int            fed_learn_get_weights(const PipelineCtx* ctx, float* weights, int max_w);
int            fed_learn_upload_weights(const PipelineCtx* ctx, const char* server_url);
int            fed_learn_download_global(PipelineCtx* ctx, const char* server_url);
int            fed_learn_aggregate(const float* local_weights, int num_clients,
                                   const float** client_weights, int n_weights,
                                   float* global_weights);

#ifdef __cplusplus
}
#endif

#endif
