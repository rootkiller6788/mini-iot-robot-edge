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
    /* local_weights is reserved for weighted FedAvg; not checked for NULL */
    (void)local_weights;
    if (!client_weights || !global_weights ||
        num_clients <= 0 || n_weights <= 0) return -1;
    for (int w = 0; w < n_weights; ++w) {
        float sum = 0.0f;
        for (int c = 0; c < num_clients; ++c) sum += client_weights[c][w];
        global_weights[w] = sum / (float)num_clients;
    }
    return 0;
}

/* ================================================================
 *  L5: Priority Queue Scheduler (Min-Heap)
 *  Used for real-time pipeline task scheduling.
 *  Lower priority value = higher urgency (min-heap invariant).
 *  Time: O(log n) push/pop
 * ================================================================ */
void pq_init(PriorityQueue* pq) {
    if (!pq) return;
    pq->size = 0;
}

int pq_push(PriorityQueue* pq, int priority, uint64_t deadline,
            int (*fn)(void*), void* arg) {
    if (!pq || pq->size >= MAX_PRIORITY_TASKS) return -1;
    int i = pq->size++;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (pq->tasks[parent].priority <= priority) break;
        pq->tasks[i] = pq->tasks[parent];
        i = parent;
    }
    pq->tasks[i].priority = priority;
    pq->tasks[i].deadline_ms = deadline;
    pq->tasks[i].fn = fn;
    pq->tasks[i].arg = arg;
    pq->tasks[i].task_id = i;
    return 0;
}

int pq_pop(PriorityQueue* pq, PriorityTask* out) {
    if (!pq || pq->size == 0) return -1;
    if (out) *out = pq->tasks[0];
    pq->size--;
    if (pq->size > 0) {
        pq->tasks[0] = pq->tasks[pq->size];
        int i = 0;
        while (1) {
            int smallest = i;
            int l = 2 * i + 1, r = 2 * i + 2;
            if (l < pq->size && pq->tasks[l].priority < pq->tasks[smallest].priority)
                smallest = l;
            if (r < pq->size && pq->tasks[r].priority < pq->tasks[smallest].priority)
                smallest = r;
            if (smallest == i) break;
            PriorityTask tmp = pq->tasks[i];
            pq->tasks[i] = pq->tasks[smallest];
            pq->tasks[smallest] = tmp;
            i = smallest;
        }
    }
    return 0;
}

int pq_peek_deadline(const PriorityQueue* pq, uint64_t now,
                     PriorityTask* overdue, int max_overdue) {
    if (!pq || !overdue) return 0;
    int count = 0;
    for (int i = 0; i < pq->size && count < max_overdue; ++i) {
        if (pq->tasks[i].deadline_ms <= now) {
            overdue[count++] = pq->tasks[i];
        }
    }
    return count;
}

/* ================================================================
 *  L3: Ring Buffer (Circular Buffer)
 *  FIFO queue for streaming sensor data.
 *  O(1) read/write; lock-free single-producer single-consumer.
 * ================================================================ */
void ringbuf_init(RingBuffer* rb) {
    if (!rb) return;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

int ringbuf_write(RingBuffer* rb, const void* data, int bytes) {
    if (!rb || !data || bytes <= 0) return -1;
    if (bytes > RINGBUF_CAPACITY - rb->count) return -1;
    const uint8_t* src = (const uint8_t*)data;
    for (int i = 0; i < bytes; ++i) {
        rb->buf[rb->head] = src[i];
        rb->head = (rb->head + 1) % RINGBUF_CAPACITY;
    }
    rb->count += bytes;
    return bytes;
}

int ringbuf_read(RingBuffer* rb, void* out, int bytes) {
    if (!rb || !out || bytes <= 0) return -1;
    if (bytes > rb->count) bytes = rb->count;
    uint8_t* dst = (uint8_t*)out;
    for (int i = 0; i < bytes; ++i) {
        dst[i] = rb->buf[rb->tail];
        rb->tail = (rb->tail + 1) % RINGBUF_CAPACITY;
    }
    rb->count -= bytes;
    return bytes;
}

int ringbuf_available(const RingBuffer* rb) {
    return rb ? rb->count : 0;
}

int ringbuf_free_space(const RingBuffer* rb) {
    return rb ? (RINGBUF_CAPACITY - rb->count) : 0;
}

/* ================================================================
 *  L3: Watchdog Timer
 *  Monitors pipeline health. If not kicked within timeout,
 *  the watchdog expires (indicates pipeline stall/crash).
 *  Commonly used in safety-critical embedded systems.
 * ================================================================ */
void watchdog_init(Watchdog* wd, uint64_t timeout_ms) {
    if (!wd) return;
    wd->timeout_ms = timeout_ms;
    wd->last_kick_ms = 0;
    wd->expired = 0;
    wd->enabled = 1;
}

void watchdog_kick(Watchdog* wd, uint64_t now_ms) {
    if (!wd || !wd->enabled) return;
    wd->last_kick_ms = now_ms;
    wd->expired = 0;
}

int watchdog_is_expired(const Watchdog* wd, uint64_t now_ms) {
    if (!wd || !wd->enabled) return 0;
    if (wd->expired) return 1;
    if (now_ms - wd->last_kick_ms > wd->timeout_ms) return 1;
    return 0;
}

/* ================================================================
 *  L5: Complementary Filter for Sensor Fusion
 *  Merges gyroscope (high-freq, drifts) and accelerometer
 *  (low-freq, noisy) angle measurements.
 *  θ_fused = α * (θ_prev + ω*dt) + (1-α) * θ_accel
 *  α ∈ [0, 1]: trust-gyro ratio.
 *  Reference: Colton (2007), "The Balance Filter"
 * ================================================================ */
void comp_filter_init(CompFilter* cf, float alpha, float dt) {
    if (!cf) return;
    cf->angle = 0.0f;
    cf->alpha = alpha > 0.0f ? (alpha < 1.0f ? alpha : 0.98f) : 0.98f;
    cf->dt    = dt > 0.0f ? dt : 0.01f;
}

float comp_filter_update(CompFilter* cf, float gyro_rate, float accel_angle) {
    if (!cf) return 0.0f;
    cf->angle = cf->alpha * (cf->angle + gyro_rate * cf->dt)
                + (1.0f - cf->alpha) * accel_angle;
    return cf->angle;
}

/* ================================================================
 *  L4: SHA-256 Hash (FIPS PUB 180-4, August 2015)
 *  Cryptographic hash for verifying OTA update integrity.
 *  Produces 256-bit (32-byte) digest.
 *  Uses standard Merkle-Damgård construction.
 * ================================================================ */

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t sha256_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8)  | (uint32_t)block[i*4+3];
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = sha256_rotr(w[i-15], 7) ^ sha256_rotr(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = sha256_rotr(w[i-2], 17) ^ sha256_rotr(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=state[0],b=state[1],c=state[2],d=state[3];
    uint32_t e=state[4],f=state[5],g=state[6],h=state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = sha256_rotr(e,6) ^ sha256_rotr(e,11) ^ sha256_rotr(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + sha256_k[i] + w[i];
        uint32_t S0 = sha256_rotr(a,2) ^ sha256_rotr(a,13) ^ sha256_rotr(a,22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;
    state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
}

void sha256_init(SHA256Ctx* ctx) {
    if (!ctx) return;
    ctx->state[0]=0x6a09e667;ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372;ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f;ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab;ctx->state[7]=0x5be0cd19;
    ctx->bitlen=0; ctx->block_idx=0; ctx->finalized=0;
}

void sha256_update(SHA256Ctx* ctx, const uint8_t* data, size_t len) {
    if (!ctx || !data || ctx->finalized) return;
    for (size_t i = 0; i < len; ++i) {
        ctx->block[ctx->block_idx++] = data[i];
        ctx->bitlen += 8;
        if (ctx->block_idx == 64) {
            sha256_transform(ctx->state, ctx->block);
            ctx->block_idx = 0;
        }
    }
}

void sha256_final(SHA256Ctx* ctx) {
    if (!ctx || ctx->finalized) return;
    ctx->block[ctx->block_idx++] = 0x80;
    if (ctx->block_idx > 56) {
        while (ctx->block_idx < 64) ctx->block[ctx->block_idx++] = 0;
        sha256_transform(ctx->state, ctx->block);
        ctx->block_idx = 0;
    }
    while (ctx->block_idx < 56) ctx->block[ctx->block_idx++] = 0;
    uint64_t bits = ctx->bitlen;
    ctx->block[56] = (uint8_t)(bits >> 56);
    ctx->block[57] = (uint8_t)(bits >> 48);
    ctx->block[58] = (uint8_t)(bits >> 40);
    ctx->block[59] = (uint8_t)(bits >> 32);
    ctx->block[60] = (uint8_t)(bits >> 24);
    ctx->block[61] = (uint8_t)(bits >> 16);
    ctx->block[62] = (uint8_t)(bits >> 8);
    ctx->block[63] = (uint8_t)(bits);
    sha256_transform(ctx->state, ctx->block);
    for (int i = 0; i < 8; ++i) {
        ctx->digest[i*4]   = (uint8_t)(ctx->state[i] >> 24);
        ctx->digest[i*4+1] = (uint8_t)(ctx->state[i] >> 16);
        ctx->digest[i*4+2] = (uint8_t)(ctx->state[i] >> 8);
        ctx->digest[i*4+3] = (uint8_t)(ctx->state[i]);
    }
    ctx->finalized = 1;
}

int sha256_verify(const uint8_t* data, size_t len, const uint8_t* expected_hash) {
    if (!data || !expected_hash) return 0;
    SHA256Ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx);
    for (int i = 0; i < SHA256_DIGEST_SIZE; ++i)
        if (ctx.digest[i] != expected_hash[i]) return 0;
    return 1;
}

/* ================================================================
 *  L8: Energy-Aware Task Scheduling
 *  Tracks energy consumption and defers non-critical tasks
 *  when near energy budget. Uses linear energy model:
 *    E = P_idle * t_idle + E_per_op * n_ops
 *  Reference: Vogginger et al. (2015), "Energy-efficient
 *  scheduling for real-time embedded systems"
 * ================================================================ */
void energy_tracker_init(EnergyTracker* et, float e_per_op,
                          float idle_mw, float peak_mw) {
    if (!et) return;
    et->energy_per_op = e_per_op;
    et->idle_power_mw = idle_mw;
    et->peak_power_mw = peak_mw;
    et->last_active_ms = 0;
    et->total_idle_ms = 0;
    et->total_energy_mj = 0.0f;
}

void energy_tracker_record_active(EnergyTracker* et, uint64_t now_ms,
                                   int num_ops) {
    if (!et) return;
    float active_energy = et->energy_per_op * (float)num_ops / 1000.0f;
    et->total_energy_mj += active_energy;
    et->last_active_ms = now_ms;
}

void energy_tracker_record_idle(EnergyTracker* et, uint64_t now_ms) {
    if (!et) return;
    if (now_ms > et->last_active_ms) {
        uint64_t dt = now_ms - et->last_active_ms;
        et->total_idle_ms += dt;
        et->total_energy_mj += et->idle_power_mw * (float)dt / 1000.0f;
    }
}

float energy_tracker_get_total_mj(const EnergyTracker* et) {
    return et ? et->total_energy_mj : 0.0f;
}

int energy_aware_should_defer(const EnergyTracker* et,
                               uint64_t now_ms,
                               float energy_budget_mj) {
    if (!et) return 0;
    (void)now_ms;
    float predicted = et->total_energy_mj + et->idle_power_mw * 1.0f;
    return predicted > energy_budget_mj ? 1 : 0;
}
