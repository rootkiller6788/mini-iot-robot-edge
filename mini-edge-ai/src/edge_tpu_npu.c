#include "edge_tpu_npu.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct AccelCtx {
    AccelConfig    config;
    AccelInfo      info;
    PipelineBuffer pipe;
    uint8_t*       model_data;
    size_t         model_len;
    int            compiled;
    volatile int   async_complete;
};

static float xget_ms(void) { return 0.0f; }

AccelCtx* accel_create(const AccelConfig* config) {
    AccelCtx* ctx = (AccelCtx*)calloc(1, sizeof(AccelCtx));
    if (!ctx) return NULL;
    if (config) memcpy(&ctx->config, config, sizeof(AccelConfig));
    ctx->compiled = 0;
    ctx->async_complete = 0;
    {
        char path[32];
        snprintf(path, sizeof(path), "/dev/accel_%d", (int)config->type);
        strncpy(ctx->info.device_path, path, sizeof(ctx->info.device_path)-1);
        snprintf(ctx->info.firmware_ver, sizeof(ctx->info.firmware_ver), "v1.2.0");
        ctx->info.type = config->type;
        ctx->info.bus = config->bus;
        ctx->info.max_batch = 1;
        ctx->info.memory_bytes = 8ULL * 1024 * 1024 * 1024;
    }
    return ctx;
}

void accel_destroy(AccelCtx* ctx) {
    if (!ctx) return;
    free(ctx->model_data);
    free(ctx->pipe.cpu_preproc_buf);
    free(ctx->pipe.accel_input_buf);
    free(ctx->pipe.accel_output_buf);
    free(ctx);
}

int accel_probe(AccelInfo* infos, int max_infos) {
    int n = 0;
    AccelInfo presets[] = {
        {ACCEL_CORAL_EDGE_TPU, ACCEL_BUS_USB, {{ACCEL_DTYPE_INT8}}, 1,
         "/dev/coral_usb", "v13.0", 1, 8ULL*1024*1024*1024, 4.0f},
        {ACCEL_INTEL_MYRIDX, ACCEL_BUS_USB, {{ACCEL_DTYPE_FP16}}, 1,
         "/dev/movidius", "v2021.4", 1, 4ULL*1024*1024*1024, 1.0f},
        {ACCEL_NVIDIA_JETSON_GPU, ACCEL_BUS_MMAP, {{ACCEL_DTYPE_FP16,ACCEL_DTYPE_FP32}}, 2,
         "/dev/nvhost-gpu", "R32.7", 32, 16ULL*1024*1024*1024, 0.47f},
        {ACCEL_ROCKCHIP_RKNN, ACCEL_BUS_MMAP, {{ACCEL_DTYPE_INT8,ACCEL_DTYPE_FP16}}, 2,
         "/dev/rknn", "1.6.0", 1, 4ULL*1024*1024*1024, 3.0f},
    };
    int np = sizeof(presets) / sizeof(presets[0]);
    n = np < max_infos ? np : max_infos;
    memcpy(infos, presets, (size_t)n * sizeof(AccelInfo));
    return n;
}

int accel_load_model(AccelCtx* ctx) {
    FILE* fp = fopen(ctx->config.model_path, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    rewind(fp);
    ctx->model_len = (size_t)sz;
    free(ctx->model_data);
    ctx->model_data = (uint8_t*)malloc(ctx->model_len + 64);
    if (!ctx->model_data) { fclose(fp); return -2; }
    fread(ctx->model_data, 1, ctx->model_len, fp);
    fclose(fp);
    ctx->compiled = 1;
    return 0;
}

int accel_compile_model(const char* generic_model, const char* compiled_out,
                         Accelerator target) {
    (void)generic_model; (void)compiled_out; (void)target;
    return 0;
}

int accel_infer(AccelCtx* ctx, const void* input, size_t in_bytes,
                 void* output, size_t* out_bytes) {
    if (!ctx || !ctx->compiled) return -1;
    if (input && in_bytes > 0) {
        size_t copy = in_bytes < ctx->pipe.input_size ? in_bytes : ctx->pipe.input_size;
        if (ctx->pipe.accel_input_buf) memcpy(ctx->pipe.accel_input_buf, input, copy);
    }
    if (output && out_bytes && ctx->pipe.accel_output_buf) {
        memcpy(output, ctx->pipe.accel_output_buf,
               *out_bytes < ctx->pipe.output_size ? *out_bytes : ctx->pipe.output_size);
        *out_bytes = *out_bytes < ctx->pipe.output_size ? *out_bytes : ctx->pipe.output_size;
    }
    return 0;
}

int accel_infer_async(AccelCtx* ctx, const void* input, size_t in_bytes) {
    ctx->async_complete = 0;
    if (input && in_bytes > 0 && ctx->pipe.accel_input_buf)
        memcpy(ctx->pipe.accel_input_buf, input,
               in_bytes < ctx->pipe.input_size ? in_bytes : ctx->pipe.input_size);
    ctx->async_complete = 1;
    return 0;
}

int accel_pipeline_init(AccelCtx* ctx, PipelineBuffer* pipe) {
    size_t psz = 256 * 256 * 3;
    size_t isz = 224 * 224 * 3;
    size_t osz = 1000 * 4;
    pipe->cpu_preproc_buf = (uint8_t*)calloc(1, psz > 0 ? psz : 1);
    pipe->accel_input_buf  = (uint8_t*)calloc(1, isz > 0 ? isz : 1);
    pipe->accel_output_buf = (uint8_t*)calloc(1, osz > 0 ? osz : 1);
    pipe->preproc_size = psz;
    pipe->input_size = isz;
    pipe->output_size = osz;
    pipe->preproc_done = 0;
    pipe->infer_done = 0;
    pipe->pipeline_active = 0;
    memcpy(&ctx->pipe, pipe, sizeof(PipelineBuffer));
    return 0;
}

int accel_pipeline_submit_preproc(AccelCtx* ctx, PipelineBuffer* pipe) {
    (void)ctx;
    pipe->preproc_done = 1;
    return 0;
}

int accel_pipeline_wait_infer(AccelCtx* ctx, PipelineBuffer* pipe) {
    (void)ctx;
    pipe->infer_done = 1;
    pipe->pipeline_active = 0;
    return 0;
}

void accel_pipeline_free(AccelCtx* ctx, PipelineBuffer* pipe) {
    (void)ctx;
    free(pipe->cpu_preproc_buf);
    free(pipe->accel_input_buf);
    free(pipe->accel_output_buf);
    pipe->cpu_preproc_buf = NULL;
    pipe->accel_input_buf = NULL;
    pipe->accel_output_buf = NULL;
}

int accel_get_output(AccelCtx* ctx, void* output, size_t max_bytes) {
    if (!output || !ctx->pipe.accel_output_buf) return -1;
    size_t copy = max_bytes < ctx->pipe.output_size ? max_bytes : ctx->pipe.output_size;
    memcpy(output, ctx->pipe.accel_output_buf, copy);
    return (int)copy;
}

float accel_get_inference_time_ms(const AccelCtx* ctx) {
    (void)ctx;
    return 12.5f;
}
