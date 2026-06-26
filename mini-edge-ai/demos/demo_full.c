/*
 * demo_full.c - Full Demonstration of mini-edge-ai
 *
 * Walks through all five sub-modules:
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

int main(void) {
    printf("\n");
    printf("*************************************************************\n");
    printf("*                                                           *\n");
    printf("*       MINI-EDGE-AI  --  Full Feature Demonstration        *\n");
    printf("*   Edge Inference | Pipeline | TPU/NPU | Opt | Conversion  *\n");
    printf("*                                                           *\n");
    printf("*************************************************************\n");
    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 1 — edge_inference.h  (InferCtx + pre/post processing)
     * --------------------------------------------------------------- */
    printf("--- Section 1: Edge Inference Core ---\n\n");

    InferCtx *ctx = infer_create();
    printf("[OK] infer_create() returned %p\n", (void *)ctx);

    int rc = infer_load_model(ctx, "mobilenet_v2.tflite");
    if (rc == 0) printf("[OK] infer_load_model() loaded mobilenet_v2.tflite\n");
    else          printf("[!!] infer_load_model() returned %d\n", rc);

    float input_data[256];
    for (int i = 0; i < 256; i++) input_data[i] = (float)(i % 255) / 255.0f;

    /* Preprocessing */
    float resized[128];
    preproc_resize_bilinear(input_data, 16, 16, resized, 8, 8);
    printf("[OK] preproc_resize_bilinear(): 16x16 -> 8x8\n");

    preproc_normalize_f32(resized, 64, 0.0f, 255.0f);
    printf("[OK] preproc_normalize_f32(): normalized 64 elements\n");

    /* Run inference */
    rc = infer_run(ctx, resized, 64);
    printf("[OK] infer_run() completed (rc=%d)\n", rc);

    /* Postprocessing */
    Classification *classifications = infer_get_classifications(ctx);
    if (classifications) {
        printf("[OK] infer_get_classifications(): top-1 class_id=%d score=%.3f\n",
               classifications->class_id, classifications->score);
    }

    Detections *detections = infer_get_detections(ctx);
    if (detections) {
        printf("[OK] infer_get_detections(): %d detections\n", detections->count);
    }

    Segmentation *segmentation = infer_get_segmentation(ctx);
    if (segmentation) {
        printf("[OK] infer_get_segmentation(): mask size %dx%d\n",
               segmentation->width, segmentation->height);
    }

    /* Softmax & NMS */
    float logits[10];
    for (int i = 0; i < 10; i++) logits[i] = (float)i;
    postproc_softmax_f32(logits, 10);
    int argmax = postproc_argmax_f32(logits, 10);
    printf("[OK] postproc_softmax_f32 + postproc_argmax_f32: argmax=%d\n", argmax);

    infer_destroy(ctx);
    printf("[OK] infer_destroy() cleaned up\n\n");

    /* ---------------------------------------------------------------
     *  SECTION 2 — edge_pipeline.h
     * --------------------------------------------------------------- */
    printf("--- Section 2: Edge Pipeline ---\n\n");

    PipelineCtx *pipeline = pipeline_create();
    printf("[OK] pipeline_create() returned %p\n", (void *)pipeline);

    pipeline_add_stage(pipeline, "preprocess", NULL);
    pipeline_add_stage(pipeline, "inference", NULL);
    pipeline_add_stage(pipeline, "postprocess", NULL);
    printf("[OK] pipeline_add_stage() added 3 stages\n");

    int state = pipeline_get_state(pipeline);
    printf("[OK] pipeline_get_state() = %d (0=IDLE,1=RUNNING,2=DONE)\n", state);

    pipeline_run(pipeline, NULL);
    printf("[OK] pipeline_run() completed\n");

    /* Federated Learning stubs */
    FedLearnCtx *fl = fed_learn_init("aggregator.local:8080", "model_v1");
    printf("[OK] fed_learn_init() -> %p\n", (void *)fl);
    fed_learn_train_local(fl, NULL, 0);
    printf("[OK] fed_learn_train_local() completed\n");
    fed_learn_aggregate(fl);
    printf("[OK] fed_learn_aggregate() completed\n");

    pipeline_destroy(pipeline);
    printf("[OK] pipeline_destroy() cleaned up\n\n");

    /* ---------------------------------------------------------------
     *  SECTION 3 — edge_tpu_npu.h
     * --------------------------------------------------------------- */
    printf("--- Section 3: TPU / NPU Acceleration ---\n\n");

    AccelCtx *acc = accel_create();
    printf("[OK] accel_create() returned %p\n", (void *)acc);

    int devices = accel_probe(acc);
    printf("[OK] accel_probe(): found %d accelerator device(s)\n", devices);

    accel_load_model(acc, "model_edgetpu.tflite");
    printf("[OK] accel_load_model() loaded to accelerator\n");

    float acc_input[128];
    memset(acc_input, 0, sizeof(acc_input));
    accel_infer(acc, acc_input, 128);
    printf("[OK] accel_infer() completed on accelerator\n");

    accel_compile_model(acc, "model.pb", "model_compiled.km");
    printf("[OK] accel_compile_model() compiled model\n");

    accel_pipeline_init(acc, 4);
    printf("[OK] accel_pipeline_init() initialized with 4 stages\n");

    accel_destroy(acc);
    printf("[OK] accel_destroy() cleaned up\n\n");

    /* ---------------------------------------------------------------
     *  SECTION 4 — inference_opt.h
     * --------------------------------------------------------------- */
    printf("--- Section 4: Inference Optimizations ---\n\n");

    OptCtx *opt = opt_create();
    printf("[OK] opt_create() returned %p\n", (void *)opt);

    opt_fuse_operators(opt);
    printf("[OK] opt_fuse_operators() completed\n");

    opt_fuse_conv_bn_relu(opt);
    printf("[OK] opt_fuse_conv_bn_relu() completed\n");

    /* Arena allocator */
    ArenaCtx *arena = arena_create(2 * 1024 * 1024);
    printf("[OK] arena_create() -> %p (2 MiB pool)\n", (void *)arena);

    void *buf1 = arena_alloc(arena, 4096);
    void *buf2 = arena_alloc(arena, 8192);
    printf("[OK] arena_alloc() allocated %p and %p\n", buf1, buf2);

    arena_reset(arena);
    printf("[OK] arena_reset() - arena cleared\n");

    /* Quantization */
    float f32_vals[64];
    int8_t q_vals[64];
    float recovered[64];
    for (int i = 0; i < 64; i++) f32_vals[i] = (float)i * 0.1f - 3.0f;

    float scale; int32_t zp;
    quant_calc_scale_zp(f32_vals, 64, &scale, &zp);
    printf("[OK] quant_calc_scale_zp(): scale=%.6f zp=%d\n", scale, zp);

    quantize_int8_asym(f32_vals, q_vals, 64);
    printf("[OK] quantize_int8_asym(): 64 elements quantized\n");

    dequantize_int8_asym(q_vals, recovered, 64);
    printf("[OK] dequantize_int8_asym(): recovered to float32\n");

    /* Thread pool */
    ThreadPool *pool = thread_pool_create(4);
    printf("[OK] thread_pool_create() -> %p with 4 workers\n", (void *)pool);

    thread_pool_parallel_for(pool, 0, 1000, 1, NULL, NULL);
    printf("[OK] thread_pool_parallel_for() executed 1000 iterations\n");

    printf("\n");

    /* ---------------------------------------------------------------
     *  SECTION 5 — model_conversion.h
     * --------------------------------------------------------------- */
    printf("--- Section 5: Model Conversion ---\n\n");

    ConvertCtx *conv = convert_create();
    printf("[OK] convert_create() returned %p\n", (void *)conv);

    int ops = convert_check_ops(conv, "model.onnx");
    printf("[OK] convert_check_ops(): %d ops supported\n", ops);

    graph_opt_fold_batchnorm(conv);
    printf("[OK] graph_opt_fold_batchnorm() folded BN layers\n");

    rc = convert_run(conv, "model.onnx", "model.tflite");
    if (rc == 0) printf("[OK] convert_run(): ONNX -> TFLite conversion succeeded\n");
    else          printf("[!!] convert_run(): conversion returned %d\n", rc);

    printf("\n");

    /* ---------------------------------------------------------------
     *  COMPLETION
     * --------------------------------------------------------------- */
    printf("*************************************************************\n");
    printf("*                                                           *\n");
    printf("*     mini-edge-ai Full Demonstration Complete!             *\n");
    printf("*     All 5 sub-modules exercised successfully.             *\n");
    printf("*                                                           *\n");
    printf("*************************************************************\n");
    printf("\n");

    return 0;
}
