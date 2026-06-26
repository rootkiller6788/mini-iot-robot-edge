#ifndef EDGE_TPU_NPU_H
#define EDGE_TPU_NPU_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ACCEL_NONE,
    ACCEL_CORAL_EDGE_TPU,
    ACCEL_INTEL_MYRIDX,
    ACCEL_NVIDIA_JETSON_GPU,
    ACCEL_ROCKCHIP_RKNN,
    ACCEL_MEDIATEK_APU,
    ACCEL_COUNT
} Accelerator;

typedef enum {
    ACCEL_BUS_USB,
    ACCEL_BUS_PCIE,
    ACCEL_BUS_MMAP,
    ACCEL_BUS_COUNT
} AccelBus;

typedef enum {
    ACCEL_DTYPE_FP32,
    ACCEL_DTYPE_FP16,
    ACCEL_DTYPE_INT8,
    ACCEL_DTYPE_UINT8,
    ACCEL_DTYPE_COUNT
} AccelDataType;

typedef struct {
    Accelerator type;
    AccelBus    bus;
    AccelDataType supported_dtypes[4];
    int         num_supported_dtypes;
    char        device_path[256];
    char        firmware_ver[32];
    int         max_batch;
    uint64_t    memory_bytes;
    float       peak_tops;
} AccelInfo;

typedef struct {
    Accelerator type;
    AccelBus    bus;
    char        model_path[256];
    char        compiled_path[256];
    int         num_threads;
    AccelDataType preferred_dtype;
    uint8_t     enable_pipelining;
} AccelConfig;

typedef struct {
    uint8_t* cpu_preproc_buf;
    uint8_t* accel_input_buf;
    uint8_t* accel_output_buf;
    size_t   preproc_size;
    size_t   input_size;
    size_t   output_size;
    volatile int preproc_done;
    volatile int infer_done;
    int      pipeline_active;
} PipelineBuffer;

typedef struct AccelCtx AccelCtx;

AccelCtx*     accel_create(const AccelConfig* config);
void          accel_destroy(AccelCtx* ctx);
int           accel_probe(AccelInfo* infos, int max_infos);
int           accel_load_model(AccelCtx* ctx);
int           accel_compile_model(const char* generic_model, const char* compiled_out,
                                  Accelerator target);
int           accel_infer(AccelCtx* ctx, const void* input, size_t in_bytes,
                          void* output, size_t* out_bytes);
int           accel_infer_async(AccelCtx* ctx, const void* input, size_t in_bytes);

int           accel_pipeline_init(AccelCtx* ctx, PipelineBuffer* pipe);
int           accel_pipeline_submit_preproc(AccelCtx* ctx, PipelineBuffer* pipe);
int           accel_pipeline_wait_infer(AccelCtx* ctx, PipelineBuffer* pipe);
void          accel_pipeline_free(AccelCtx* ctx, PipelineBuffer* pipe);

int           accel_get_output(AccelCtx* ctx, void* output, size_t max_bytes);
float         accel_get_inference_time_ms(const AccelCtx* ctx);

/* ── L4: Roofline Model (Williams, Waterman, Patterson 2009) ──
   P = min(Peak FLOPs, AI * Peak BW)
   where AI = FLOPs / bytes accessed (Arithmetic Intensity).
   Determines if a kernel is compute-bound or memory-bound. ── */
typedef struct {
    float   peak_gflops;      /* peak compute in GFLOPS */
    float   peak_bandwidth;   /* peak memory BW in GB/s */
    float   ridge_point;      /* AI at intersection = GFLOPS / BW */
} RooflineModel;

void          roofline_init(RooflineModel* rm, float peak_gflops, float peak_bw);
float         roofline_predict_tflops(const RooflineModel* rm, float flops,
                                       float bytes);
const char*   roofline_bound_type(const RooflineModel* rm, float flops,
                                   float bytes);

/* ── L3: DMA Double Buffering for Pipelined Data Transfer ── */
typedef struct {
    uint8_t*  buf_a;
    uint8_t*  buf_b;
    size_t    buf_size;
    int       active_buf;     /* 0 = A front, 1 = B front */
    volatile int dma_active;
    volatile int transfer_done;
} DMADoubleBuffer;

int           dma_dbuf_init(DMADoubleBuffer* db, size_t buf_size);
void          dma_dbuf_free(DMADoubleBuffer* db);
int           dma_dbuf_swap(DMADoubleBuffer* db);
uint8_t*      dma_dbuf_get_front(DMADoubleBuffer* db);
uint8_t*      dma_dbuf_get_back(DMADoubleBuffer* db);

/* ── L3: Accelerator Memory Pool ── */
#define ACCEL_MEMPOOL_MAX_BLOCKS 32
typedef struct {
    uint8_t* base;
    size_t   total_size;
    size_t   block_sizes[ACCEL_MEMPOOL_MAX_BLOCKS];
    uint8_t* block_ptrs[ACCEL_MEMPOOL_MAX_BLOCKS];
    int      num_blocks;
    size_t   used;
} AccelMemPool;

AccelMemPool* accel_mempool_create(size_t total_size);
void          accel_mempool_destroy(AccelMemPool* pool);
void*         accel_mempool_alloc(AccelMemPool* pool, size_t size);
void          accel_mempool_reset(AccelMemPool* pool);
size_t        accel_mempool_available(const AccelMemPool* pool);

/* ── L4: Power & Energy Estimation ── */
typedef struct {
    float   mac_energy_pj;     /* picojoules per MAC operation */
    float   sram_read_pj;      /* picojoules per SRAM read */
    float   dram_read_pj;      /* picojoules per DRAM read */
    float   static_power_mw;   /* static leakage power */
} PowerModel;

void          power_model_init(PowerModel* pm, float mac_pj, float sram_pj,
                                float dram_pj, float static_mw);
float         power_estimate_energy_uj(const PowerModel* pm, uint64_t macs,
                                        uint64_t sram_bytes, uint64_t dram_bytes,
                                        float time_us);

/* ── L3: Tensor Layout Conversion NHWC ↔ NCHW ── */
int           tensor_nhwc_to_nchw(const float* nhwc, int n, int h, int w, int c,
                                   float* nchw);
int           tensor_nchw_to_nhwc(const float* nchw, int n, int c, int h, int w,
                                   float* nhwc);
int           tensor_nhwc_to_nchw_int8(const int8_t* nhwc, int n, int h, int w,
                                        int c, int8_t* nchw);

#ifdef __cplusplus
}
#endif

#endif
