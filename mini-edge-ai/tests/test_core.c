/*
 * test_core.c - Core Unit Tests for mini-edge-ai
 *
 * Tests all five sub-modules:
 *   edge_inference.h, edge_pipeline.h, edge_tpu_npu.h, inference_opt.h, model_conversion.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "edge_inference.h"
#include "edge_pipeline.h"
#include "edge_tpu_npu.h"
#include "inference_opt.h"
#include "model_conversion.h"

/* ---- test harness ---- */
static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s ... ", name); } while(0)
#define PASS()     do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg)  do { printf("FAIL: %s\n", msg); return 1; } while(0)
#define CHECK(cond, msg) if (!(cond)) FAIL(msg)

/* ================================================================
 *  edge_inference.h  (InferCtx, preproc, postproc)
 * ================================================================ */

static int test_infer_create_destroy(void) {
    TEST("infer_create / infer_destroy");
    InferCtx *ctx = infer_create();
    CHECK(ctx != NULL, "infer_create returned NULL");
    infer_destroy(ctx);
    PASS();
    return 0;
}

static int test_infer_load_model(void) {
    TEST("infer_load_model");
    InferCtx *ctx = infer_create();
    int rc = infer_load_model(ctx, "test_model.tflite");
    CHECK(rc == 0, "infer_load_model failed");
    infer_destroy(ctx);
    PASS();
    return 0;
}

static int test_infer_run_classifications(void) {
    TEST("infer_run -> get_classifications");
    InferCtx *ctx = infer_create();
    infer_load_model(ctx, "test_model.tflite");
    float input[256];
    for (int i = 0; i < 256; i++) input[i] = 0.0f;
    int rc = infer_run(ctx, input, 256);
    CHECK(rc == 0, "infer_run failed");
    Classification *cls = infer_get_classifications(ctx);
    CHECK(cls != NULL, "infer_get_classifications returned NULL");
    infer_destroy(ctx);
    PASS();
    return 0;
}

static int test_infer_get_detections(void) {
    TEST("infer_run -> get_detections");
    InferCtx *ctx = infer_create();
    infer_load_model(ctx, "detect_model.tflite");
    float input[1024];
    memset(input, 0, sizeof(input));
    infer_run(ctx, input, 1024);
    Detections *det = infer_get_detections(ctx);
    CHECK(det != NULL, "infer_get_detections returned NULL");
    infer_destroy(ctx);
    PASS();
    return 0;
}

static int test_infer_get_segmentation(void) {
    TEST("infer_run -> get_segmentation");
    InferCtx *ctx = infer_create();
    infer_load_model(ctx, "seg_model.tflite");
    float input[512];
    memset(input, 0, sizeof(input));
    infer_run(ctx, input, 512);
    Segmentation *seg = infer_get_segmentation(ctx);
    CHECK(seg != NULL, "infer_get_segmentation returned NULL");
    infer_destroy(ctx);
    PASS();
    return 0;
}

static int test_preproc_resize_bilinear(void) {
    TEST("preproc_resize_bilinear");
    float src[64 * 64], dst[32 * 32];
    for (int i = 0; i < 64 * 64; i++) src[i] = (float)i;
    int rc = preproc_resize_bilinear(src, 64, 64, dst, 32, 32);
    CHECK(rc == 0, "preproc_resize_bilinear failed");
    PASS();
    return 0;
}

static int test_preproc_normalize(void) {
    TEST("preproc_normalize_f32");
    float buf[256];
    for (int i = 0; i < 256; i++) buf[i] = (float)i;
    int rc = preproc_normalize_f32(buf, 256, 127.5f, 127.5f);
    CHECK(rc == 0, "preproc_normalize_f32 failed");
    PASS();
    return 0;
}

static int test_postproc_softmax(void) {
    TEST("postproc_softmax_f32");
    float logits[10];
    for (int i = 0; i < 10; i++) logits[i] = (float)i;
    int rc = postproc_softmax_f32(logits, 10);
    CHECK(rc == 0, "postproc_softmax_f32 failed");
    float sum = 0.0f;
    for (int i = 0; i < 10; i++) sum += logits[i];
    CHECK(fabsf(sum - 1.0f) < 0.01f, "softmax sum != 1.0");
    PASS();
    return 0;
}

/* ================================================================
 *  edge_pipeline.h
 * ================================================================ */

static int test_pipeline_create_destroy(void) {
    TEST("pipeline_create / pipeline_destroy");
    PipelineCtx *p = pipeline_create();
    CHECK(p != NULL, "pipeline_create returned NULL");
    pipeline_destroy(p);
    PASS();
    return 0;
}

static int test_pipeline_add_stage(void) {
    TEST("pipeline_add_stage");
    PipelineCtx *p = pipeline_create();
    int rc = pipeline_add_stage(p, "preprocess", NULL);
    CHECK(rc == 0, "pipeline_add_stage failed");
    pipeline_destroy(p);
    PASS();
    return 0;
}

/* ================================================================
 *  edge_tpu_npu.h
 * ================================================================ */

static int test_accel_create_destroy(void) {
    TEST("accel_create / accel_destroy");
    AccelCtx *acc = accel_create();
    CHECK(acc != NULL, "accel_create returned NULL");
    accel_destroy(acc);
    PASS();
    return 0;
}

static int test_accel_probe(void) {
    TEST("accel_probe");
    AccelCtx *acc = accel_create();
    int found = accel_probe(acc);
    CHECK(found >= 0, "accel_probe returned error");
    accel_destroy(acc);
    PASS();
    return 0;
}

/* ================================================================
 *  inference_opt.h
 * ================================================================ */

static int test_arena_create_alloc(void) {
    TEST("arena_create / arena_alloc");
    ArenaCtx *arena = arena_create(1024 * 1024);
    CHECK(arena != NULL, "arena_create returned NULL");
    void *ptr = arena_alloc(arena, 4096);
    CHECK(ptr != NULL, "arena_alloc returned NULL");
    arena_reset(arena);
    PASS();
    return 0;
}

static int test_quantize_dequantize(void) {
    TEST("quantize_int8_asym -> dequantize_int8_asym");
    float vals[64];
    int8_t q_vals[64];
    for (int i = 0; i < 64; i++) vals[i] = (float)i - 32.0f;
    int rc = quantize_int8_asym(vals, q_vals, 64);
    CHECK(rc == 0, "quantize_int8_asym failed");
    float recovered[64];
    rc = dequantize_int8_asym(q_vals, recovered, 64);
    CHECK(rc == 0, "dequantize_int8_asym failed");
    PASS();
    return 0;
}

/* ================================================================
 *  model_conversion.h
 * ================================================================ */

static int test_convert_create_run(void) {
    TEST("convert_create / convert_run");
    ConvertCtx *c = convert_create();
    CHECK(c != NULL, "convert_create returned NULL");
    int rc = convert_run(c, "input.onnx", "output.tflite");
    CHECK(rc == 0, "convert_run failed");
    PASS();
    return 0;
}

/* ================================================================
 *  main
 * ================================================================ */

int main(void) {
    printf("mini-edge-ai  --  Core Unit Tests\n\n");

    /* edge_inference.h */
    test_infer_create_destroy();
    test_infer_load_model();
    test_infer_run_classifications();
    test_infer_get_detections();
    test_infer_get_segmentation();
    test_preproc_resize_bilinear();
    test_preproc_normalize();
    test_postproc_softmax();

    /* edge_pipeline.h */
    test_pipeline_create_destroy();
    test_pipeline_add_stage();

    /* edge_tpu_npu.h */
    test_accel_create_destroy();
    test_accel_probe();

    /* inference_opt.h */
    test_arena_create_alloc();
    test_quantize_dequantize();

    /* model_conversion.h */
    test_convert_create_run();

    printf("\n%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
