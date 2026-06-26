#include "edge_inference.h"
#include "edge_pipeline.h"
#include "inference_opt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define CAM_W  320
#define CAM_H  240
#define CAM_C  3
#define MODEL_W 300
#define MODEL_H 300
#define MAX_PERSONS 5

static uint64_t demo_tick(void) {
    return (uint64_t)clock();
}

typedef struct {
    uint8_t  frame[CAM_W * CAM_H * CAM_C];
    int      frame_id;
    uint64_t ts;
} CameraFrame;

typedef struct {
    InferCtx*    infer_ctx;
    PipelineCtx* pipeline;
    int          person_count;
    int          total_frames;
    int          total_uploads;
    uint64_t     start_ts;
    BandwidthPolicy bw;
} PersonDetectApp;

static int stage_sensor_read(PipelineCtx* pipe, void* arg) {
    PersonDetectApp* app = (PersonDetectApp*)arg;
    CameraFrame* f = malloc(sizeof(CameraFrame));
    if (!f) return -1;
    f->frame_id = app->total_frames++;
    f->ts = demo_tick();
    for (int i = 0; i < CAM_W * CAM_H * CAM_C; ++i)
        f->frame[i] = (uint8_t)((i * 41 + (f->frame_id * 97)) % 256);
    return 0;
}

static int stage_preprocess(PipelineCtx* pipe, void* arg) {
    PersonDetectApp* app = (PersonDetectApp*)arg;
    CameraFrame f;
    f.frame_id = app->total_frames - 1;
    f.ts = demo_tick();
    for (int i = 0; i < CAM_W * CAM_H * CAM_C; ++i)
        f.frame[i] = (uint8_t)((i * 41 + (f.frame_id * 97)) % 256);
    uint8_t* resized = (uint8_t*)malloc((size_t)MODEL_W * MODEL_H * CAM_C);
    if (!resized) return -1;
    preproc_resize_bilinear(f.frame, CAM_W, CAM_H, CAM_C,
                             resized, MODEL_W, MODEL_H);
    float* norm = (float*)malloc((size_t)MODEL_W * MODEL_H * CAM_C * sizeof(float));
    float mean[3] = {127.5f, 127.5f, 127.5f}, std[3] = {127.5f, 127.5f, 127.5f};
    if (norm) {
        preproc_normalize_f32(resized, MODEL_W, MODEL_H, CAM_C, norm, mean, std);
        infer_set_input_tensor(app->infer_ctx, norm,
                               (size_t)MODEL_W * MODEL_H * CAM_C * sizeof(float));
    }
    free(resized);
    free(norm);
    return 0;
}

static int stage_inference(PipelineCtx* pipe, void* arg) {
    PersonDetectApp* app = (PersonDetectApp*)arg;
    infer_run(app->infer_ctx, NULL, 0);
    return 0;
}

static int stage_postprocess(PipelineCtx* pipe, void* arg) {
    PersonDetectApp* app = (PersonDetectApp*)arg;
    Detection dets[MAX_DETECTIONS];
    int nd = infer_get_detections(app->infer_ctx, dets, MAX_DETECTIONS);
    int kept[MAX_DETECTIONS];
    int nk = postproc_nms(dets, nd, 0.5f, kept);
    app->person_count = 0;
    for (int i = 0; i < nk; ++i) {
        Detection* d = &dets[kept[i]];
        if (d->class_id == 0 && d->score > app->bw.confidence_thresh)
            app->person_count++;
    }
    return 0;
}

static int stage_decision(PipelineCtx* pipe, void* arg) {
    PersonDetectApp* app = (PersonDetectApp*)arg;
    InferenceResult res;
    memset(&res, 0, sizeof(res));
    res.task = TASK_PERSON_DETECT;
    res.person_detected = (app->person_count > 0);
    res.confidence = app->person_count > 0 ? 0.85f : 0.05f;
    res.timestamp_ms = demo_tick();
    if (pipeline_should_upload(&app->bw, &res,
                                app->total_uploads > 0 ? demo_tick() : 0,
                                app->total_uploads)) {
        pipeline_execute_action(pipe, ACTION_SNAPSHOT, &res);
        pipeline_execute_action(pipe, ACTION_CLOUD_UPLOAD, &res);
        app->total_uploads++;
    }
    return 0;
}

static int stage_action(PipelineCtx* pipe, void* arg) {
    PersonDetectApp* app = (PersonDetectApp*)arg;
    if (app->person_count > 0 && app->total_uploads > 0) {
        pipeline_execute_action(pipe, ACTION_ALERT,
                                &(InferenceResult){.confidence = 0.85f,
                                                   .person_detected = 1,
                                                   .timestamp_ms = demo_tick()});
    }
    return 0;
}

int main(int argc, char* argv[]) {
    fprintf(stdout, "=== mini-edge-ai: Person Detection Demo ===\n");
    fprintf(stdout, "Camera: %dx%d @ 3ch, Model: %dx%d\n", CAM_W, CAM_H, MODEL_W, MODEL_H);
    PersonDetectApp app;
    memset(&app, 0, sizeof(app));
    InferConfig icfg;
    memset(&icfg, 0, sizeof(icfg));
    strncpy(icfg.model_path, "ssd_mobilenet_v2_person.tflite", MAX_MODEL_PATH - 1);
    icfg.model_type = MODEL_TYPE_DETECTION;
    icfg.backend = INFER_BACKEND_TFLITE;
    icfg.input_shape.width = MODEL_W;
    icfg.input_shape.height = MODEL_H;
    icfg.input_shape.channels = CAM_C;
    icfg.input_shape.format = INPUT_FMT_RGB888;
    icfg.preproc = PREPROC_NORMALIZE;
    icfg.postproc = POSTPROC_NMS;
    icfg.num_classes = 90;
    icfg.num_threads = 4;
    icfg.mean[0] = 127.5f; icfg.mean[1] = 127.5f; icfg.mean[2] = 127.5f;
    icfg.std[0] = 127.5f; icfg.std[1] = 127.5f; icfg.std[2] = 127.5f;
    app.infer_ctx = infer_create(&icfg);
    if (!app.infer_ctx) { fprintf(stderr, "Failed to create inference ctx\n"); return 1; }
    app.pipeline = pipeline_create();
    if (!app.pipeline) { infer_destroy(app.infer_ctx); return 1; }
    app.bw.confidence_thresh = 0.6f;
    app.bw.min_interval_ms = 500;
    app.bw.max_per_minute = 12;
    app.bw.upload_on_detect = 1;
    app.bw.save_local = 1;
    app.bw.low_bandwidth_mode = 0;

    pipeline_add_stage(app.pipeline, STAGE_SENSOR_READ, stage_sensor_read, &app);
    pipeline_add_stage(app.pipeline, STAGE_PREPROCESS, stage_preprocess, &app);
    pipeline_add_stage(app.pipeline, STAGE_INFERENCE, stage_inference, &app);
    pipeline_add_stage(app.pipeline, STAGE_POSTPROCESS, stage_postprocess, &app);
    pipeline_add_stage(app.pipeline, STAGE_DECISION, stage_decision, &app);
    pipeline_add_stage(app.pipeline, STAGE_ACTION, stage_action, &app);

    ActionSpec snap;
    memset(&snap, 0, sizeof(snap));
    snap.type = ACTION_SNAPSHOT;
    snap.threshold = 0.6f;
    snap.immediate = 1;
    pipeline_set_action(app.pipeline, &snap);
    snap.type = ACTION_CLOUD_UPLOAD;
    snap.threshold = 0.7f;
    strncpy(snap.cloud_url, "https://cloud.example.com/upload", MAX_UPLOAD_URL - 1);
    pipeline_set_action(app.pipeline, &snap);

    app.start_ts = demo_tick();
    int num_frames = 50;
    fprintf(stdout, "Running %d frames...\n", num_frames);
    for (int frame = 0; frame < num_frames; ++frame) {
        int sim_persons = (frame % 10 == 0) ? 1 : 0;
        if (sim_persons && frame >= 10) {
            app.person_count = 1;
        } else {
            app.person_count = 0;
        }
        pipeline_run(app.pipeline);
        if (app.person_count > 0)
            fprintf(stdout, "  Frame %3d: PERSON DETECTED (persons=%d)\n",
                    frame, app.person_count);
        else if (frame % 5 == 0)
            fprintf(stdout, "  Frame %3d: no person\n", frame);
    }
    uint64_t elapsed = demo_tick() - app.start_ts;
    float avg_ms = elapsed > 0 ? (float)elapsed / (float)num_frames : 0.0f;
    fprintf(stdout, "\nSummary:\n");
    fprintf(stdout, "  Total frames:  %d\n", num_frames);
    fprintf(stdout, "  Total uploads: %d\n", app.total_uploads);
    fprintf(stdout, "  Total time:    %.1f ms\n", (float)elapsed);
    fprintf(stdout, "  Avg per frame: %.1f ms\n", avg_ms);
    pipeline_destroy(app.pipeline);
    infer_destroy(app.infer_ctx);
    return 0;
}
