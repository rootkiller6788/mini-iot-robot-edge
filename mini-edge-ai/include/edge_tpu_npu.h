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

#ifdef __cplusplus
}
#endif

#endif
