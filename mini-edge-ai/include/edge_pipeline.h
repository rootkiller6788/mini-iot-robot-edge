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

/* ── L5: Priority Queue Scheduler for Pipeline Tasks ── */
#define MAX_PRIORITY_TASKS 64
typedef struct {
    int      task_id;
    int      priority;    /* lower = higher priority (min-heap) */
    uint64_t deadline_ms;
    int      (*fn)(void*);
    void*    arg;
} PriorityTask;

typedef struct {
    PriorityTask tasks[MAX_PRIORITY_TASKS];
    int          size;
} PriorityQueue;

void          pq_init(PriorityQueue* pq);
int           pq_push(PriorityQueue* pq, int priority, uint64_t deadline,
                      int (*fn)(void*), void* arg);
int           pq_pop(PriorityQueue* pq, PriorityTask* out);
int           pq_peek_deadline(const PriorityQueue* pq, uint64_t now,
                               PriorityTask* overdue, int max_overdue);

/* ── L3: Ring Buffer (Circular Buffer) for Streaming Data ── */
#define RINGBUF_CAPACITY 4096
typedef struct {
    uint8_t  buf[RINGBUF_CAPACITY];
    int      head;       /* write position */
    int      tail;       /* read position */
    int      count;
} RingBuffer;

void          ringbuf_init(RingBuffer* rb);
int           ringbuf_write(RingBuffer* rb, const void* data, int bytes);
int           ringbuf_read(RingBuffer* rb, void* out, int bytes);
int           ringbuf_available(const RingBuffer* rb);
int           ringbuf_free_space(const RingBuffer* rb);

/* ── L3: Watchdog Timer — fault detection for critical pipelines ── */
typedef struct {
    uint64_t timeout_ms;
    uint64_t last_kick_ms;
    int      expired;
    int      enabled;
} Watchdog;

void          watchdog_init(Watchdog* wd, uint64_t timeout_ms);
void          watchdog_kick(Watchdog* wd, uint64_t now_ms);
int           watchdog_is_expired(const Watchdog* wd, uint64_t now_ms);

/* ── L5: Complementary Filter (Sensor Fusion) ──
   Combines high-freq gyro and low-freq accelerometer data.
   θ = α*(θ + ω*dt) + (1-α)*θ_accel
   α ∈ [0,1]; α close to 1 trusts gyro more. ── */
typedef struct {
    float angle;       /* fused angle estimate */
    float alpha;       /* filter coefficient */
    float dt;          /* time step */
} CompFilter;

void          comp_filter_init(CompFilter* cf, float alpha, float dt);
float         comp_filter_update(CompFilter* cf, float gyro_rate,
                                 float accel_angle);

/* ── L4: SHA-256 Hash (FIPS 180-4) for OTA Update Integrity ── */
#define SHA256_DIGEST_SIZE  32
#define SHA256_BLOCK_SIZE   64

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  block[SHA256_BLOCK_SIZE];
    int      block_idx;
    uint8_t  digest[SHA256_DIGEST_SIZE];
    int      finalized;
} SHA256Ctx;

void          sha256_init(SHA256Ctx* ctx);
void          sha256_update(SHA256Ctx* ctx, const uint8_t* data, size_t len);
void          sha256_final(SHA256Ctx* ctx);
int           sha256_verify(const uint8_t* data, size_t len,
                            const uint8_t* expected_hash);

/* ── L8: Energy-Aware Task Scheduling ── */
typedef struct {
    float    energy_per_op;     /* μJ per operation */
    float    idle_power_mw;     /* idle power in milliwatts */
    float    peak_power_mw;     /* peak power in milliwatts */
    uint64_t last_active_ms;
    uint64_t total_idle_ms;
    float    total_energy_mj;   /* total energy in millijoules */
} EnergyTracker;

void          energy_tracker_init(EnergyTracker* et, float e_per_op,
                                  float idle_mw, float peak_mw);
void          energy_tracker_record_active(EnergyTracker* et, uint64_t now_ms,
                                            int num_ops);
void          energy_tracker_record_idle(EnergyTracker* et, uint64_t now_ms);
float         energy_tracker_get_total_mj(const EnergyTracker* et);
int           energy_aware_should_defer(const EnergyTracker* et,
                                         uint64_t now_ms,
                                         float energy_budget_mj);

#ifdef __cplusplus
}
#endif

#endif
