/*
 * bench_core.c - Core Benchmarks for mini-edge-ai
 *
 * Measures performance of the major API functions across all five sub-modules:
 *   edge_inference.h, edge_pipeline.h, edge_tpu_npu.h, inference_opt.h, model_conversion.h
 *
 * Usage: bench_core [N]
 *   N = iteration scale factor (default 5000)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "edge_inference.h"
#include "edge_pipeline.h"
#include "edge_tpu_npu.h"
#include "inference_opt.h"
#include "model_conversion.h"

/* ---- helper: high-resolution timer ---- */
static double now_ms(void) {
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

/* ---- benchmark runner ---- */
static void bench_run(const char *name, void (*fn)(int), int n) {
    double t0 = now_ms();
    fn(n);
    double t1 = now_ms();
    double elapsed = t1 - t0;
    printf("  %-35s %d ops in %9.1f ms  (%8.1f µs/op)\n",
           name, n, elapsed, (elapsed * 1000.0) / (double)n);
}

/* ================================================================
 *  BENCHMARKS – edge_inference.h
 * ================================================================ */

static void bm_infer_create_destroy(int n) {
    for (int i = 0; i < n; i++) {
        InferCtx *ctx = infer_create();
        infer_destroy(ctx);
    }
}

static void bm_infer_load_run(int n) {
    InferCtx *ctx = infer_create();
    infer_load_model(ctx, "bench_model.tflite");
    float input[256];
    for (int i = 0; i < 256; i++) input[i] = (float)i / 256.0f;
    for (int i = 0; i < n; i++) {
        infer_run(ctx, input, 256);
    }
    infer_destroy(ctx);
}

/* ================================================================
 *  BENCHMARKS – preproc / postproc (edge_inference.h)
 * ================================================================ */

static void bm_preproc_resize(int n) {
    float src[64 * 64], dst[32 * 32];
    for (int i = 0; i < 64 * 64; i++) src[i] = (float)i;
    int scaled = n / 5;
    for (int i = 0; i < scaled; i++) {
        preproc_resize_bilinear(src, 64, 64, dst, 32, 32);
    }
}

static void bm_preproc_normalize(int n) {
    float buf[1024];
    for (int i = 0; i < 1024; i++) buf[i] = (float)i;
    for (int i = 0; i < n; i++) {
        preproc_normalize_f32(buf, 1024, 127.5f, 127.5f);
    }
}

static void bm_postproc_nms(int n) {
    DetBox boxes[256];
    for (int i = 0; i < 256; i++) {
        boxes[i].x = (float)(i % 16) * 10.0f;
        boxes[i].y = (float)(i / 16) * 10.0f;
        boxes[i].w = 50.0f; boxes[i].h = 50.0f;
        boxes[i].score = 0.5f + ((float)(i % 10)) * 0.05f;
    }
    int scaled = n / 20;
    for (int i = 0; i < scaled; i++) {
        postproc_nms(boxes, 256, 0.5f);
    }
}

/* ================================================================
 *  BENCHMARKS – edge_pipeline.h
 * ================================================================ */

static void bm_pipeline_create_destroy(int n) {
    for (int i = 0; i < n; i++) {
        PipelineCtx *p = pipeline_create();
        pipeline_destroy(p);
    }
}

static void bm_pipeline_run(int n) {
    PipelineCtx *p = pipeline_create();
    void *frame = NULL;
    int scaled = n / 10;
    for (int i = 0; i < scaled; i++) {
        pipeline_run(p, frame);
    }
    pipeline_destroy(p);
}

/* ================================================================
 *  BENCHMARKS – edge_tpu_npu.h
 * ================================================================ */

static void bm_accel_probe_load(int n) {
    AccelCtx *acc = accel_create();
    int scaled = n / 20;
    for (int i = 0; i < scaled; i++) {
        accel_probe(acc);
    }
    accel_destroy(acc);
}

/* ================================================================
 *  BENCHMARKS – inference_opt.h
 * ================================================================ */

static void bm_arena_alloc_reset(int n) {
    ArenaCtx *arena = arena_create(1024 * 1024);
    for (int i = 0; i < n; i++) {
        void *p = arena_alloc(arena, 256);
        if ((i % 100) == 0) arena_reset(arena);
        (void)p;
    }
    arena_reset(arena);
}

/* ================================================================
 *  main
 * ================================================================ */

int main(int argc, char **argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 5000;
    if (N < 1) N = 5000;

    printf("mini-edge-ai  --  Core Benchmarks  (N=%d)\n\n", N);

    bench_run("infer_create/destroy (roundtrip)",      bm_infer_create_destroy, N);
    bench_run("infer_load_model + infer_run",           bm_infer_load_run, N);
    bench_run("preproc_resize_bilinear",                bm_preproc_resize, N);
    bench_run("preproc_normalize_f32",                  bm_preproc_normalize, N);
    bench_run("postproc_nms",                           bm_postproc_nms, N);
    bench_run("pipeline_create/destroy (roundtrip)",    bm_pipeline_create_destroy, N);
    bench_run("pipeline_run",                           bm_pipeline_run, N);
    bench_run("accel_probe",                            bm_accel_probe_load, N);
    bench_run("arena_alloc + arena_reset (pooled)",     bm_arena_alloc_reset, N);

    printf("\nDone.\n");
    return 0;
}
