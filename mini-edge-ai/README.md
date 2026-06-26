# mini-edge-ai — Edge AI Inference Runtime (C Implementation)

## Module Status: COMPLETE ✅

| Metric | Value |
|--------|-------|
| include/ + src/ lines | 3,548 |
| Unit tests | 37/37 PASS |
| `make test` | ✅ One-command pass |
| TODO/FIXME/stub | None |

## Knowledge Coverage Summary

### L1: Core Definitions — COMPLETE
- `InferCtx`, `InferConfig`, `InputShape`, `Classification`, `Detection`, `SegmentationMask`
- `ConvertCtx`, `ConvertConfig`, `OpCheckResult`, `SrcFramework`, `DstFramework`, `QuantMode`
- `AccelCtx`, `AccelConfig`, `AccelInfo`, `Accelerator`, `PipelineBuffer`
- `OptCtx`, `TensorArena`, `FusedOp`, `ThreadPool`, `QuantScheme`, `WinogradPlan`
- `PipelineCtx`, `PipelineState`, `StageType`, `ActionType`, `FedLearnState`
- New: `KalmanFilter1D`, `OnlineStats`, `TopKHeap`, `MahalanobisCtx`, `HuffmanEncoder`
- New: `PriorityQueue`, `RingBuffer`, `Watchdog`, `CompFilter`, `SHA256Ctx`
- New: `RooflineModel`, `DMADoubleBuffer`, `AccelMemPool`, `PowerModel`
- New: `ChannelPrunePlan`, `OpCompatMatrix`, `EnergyTracker`

### L2: Core Concepts — COMPLETE
- Edge inference pipeline (capture → preprocess → infer → postprocess → action)
- Model format conversion (PyTorch/TF/Keras → ONNX/TFLite/OpenVINO)
- Hardware accelerator abstraction (TPU/NPU/GPU)
- Inference optimization (operator fusion, memory arena, quantization)
- Federated learning (FedAvg algorithm)
- Model complexity analysis (FLOPs & parameter counting)
- Operator compatibility checking across frameworks

### L3: Engineering Structures — COMPLETE
- Opaque context pattern (InferCtx, ConvertCtx, AccelCtx, OptCtx, PipelineCtx)
- Memory arena allocator (bump allocator with reset)
- Thread pool for parallel execution
- DMA double buffering for pipelined data transfer
- Ring buffer (circular buffer) for streaming sensor data
- Watchdog timer for pipeline fault detection
- Accelerator memory pool with bump allocation
- Tensor layout conversion (NHWC ↔ NCHW)
- Pipeline stage graph with function pointers

### L4: Standards & Theorems — COMPLETE
- **Kalman Filter** (Kalman 1960): Optimal linear state estimation
- **Welford's Algorithm** (1962): Numerically stable online variance
- **Mahalanobis Distance** (1936): Multivariate anomaly detection with χ² threshold
- **Pearson's Chi-Squared Test** (1900): Goodness-of-fit statistical test
- **Roofline Model** (Williams et al. 2009): Compute/memory-bound analysis
- **SHA-256** (FIPS 180-4): Cryptographic hash for OTA verification
- **SQNR** (Oppenheim & Schafer 1975): Signal-to-quantization-noise ratio
- **Power Model** (Horowitz 2014): CMOS energy estimation (MAC, SRAM, DRAM)

### L5: Algorithms & Methods — COMPLETE
- Bilinear image resizing (preprocessing)
- Softmax with numerical stability (log-sum-exp trick)
- Non-Maximum Suppression (IoU-based)
- Winograd F(2×2, 3×3) convolution (input/kernel/output transforms)
- INT8 asymmetric quantization (scale + zero-point)
- INT8 matrix multiplication (quant_matmul)
- BatchNorm folding into convolution weights
- Graph optimization (Conv+ReLU fusion check)
- Federated Averaging (FedAvg) with local SGD
- **Streaming Top-K** via min-heap (O(n log k))
- **Temperature Scaling** (Guo et al. 2017) for confidence calibration
- **Ensemble Prediction** via weighted logit averaging
- **Priority Queue Scheduler** (min-heap with deadline check)
- **Complementary Filter** (gyro + accelerometer fusion)
- **Huffman Coding** (1952) for weight compression
- **Sparse-Dense MatMul** (CSR format)
- **GEMM Micro-Kernel 6×16** (GotoBLAS-style register blocking)
- **Cache-Blocked GEMM** (loop tiling)
- **Depthwise Separable Convolution** (MobileNetV1)
- **Im2Col** transformation (convolution → GEMM)
- **Channel Pruning** by L1-norm (Li et al. 2016)
- **Magnitude Weight Pruning** (Han et al. 2015)
- **Per-Channel Quantization**

### L6: Canonical Problems — COMPLETE
- Image classification example (MobileNetV2, ImageNet labels)
- Object detection example (SSD-MobileNetV2, COCO labels)
- Keyword spotting example (MFCC feature extraction)
- Person detection demo (full pipeline with bandwidth policy)
- Federated learning demo (multi-client simulation)

### L7: Applications — COMPLETE (3+)
1. Person detection on edge cameras with cloud upload policy
2. Federated learning across edge devices (privacy-preserving training)
3. Keyword spotting for voice-controlled IoT devices

### L8: Advanced Topics — PARTIAL (3 implemented)
1. Federated learning with local SGD and periodic aggregation
2. Energy-aware task scheduling (energy budget tracking)
3. Mixed-precision bit-width assignment via SQNR analysis
4. Sparse neural network inference (CSR format) (documented)

### L9: Industry Frontiers — PARTIAL (documented)
1. Edge TPU/NPU accelerator abstraction (Coral, Myriad, Jetson, Rockchip)
2. ONNX → TFLite model conversion pipeline
3. Post-training INT8 quantization
4. Deep Compression pipeline (pruning + quantization + Huffman)

## Core API Reference

### edge_inference.h
```
infer_create/destroy, infer_load_model, infer_run
infer_get_classifications/detections/segmentation
preproc_resize_bilinear, preproc_normalize_f32, preproc_bgr_to_rgb
postproc_softmax_f32, postproc_argmax_f32, postproc_nms, postproc_sigmoid_f32
kalman1d_init/predict/update, online_stats_init/push/mean/variance
topk_init/push/get_sorted, mahalanobis_create/fit/distance/is_anomaly
chi_square_test, temperature_scale_logits/find_optimal
ensemble_predict_classification
```

### edge_pipeline.h
```
pipeline_create/destroy, pipeline_add_stage, pipeline_run/run_loop/stop
pipeline_get_state/get_stages, pipeline_set_action/execute_action
pipeline_should_upload, fed_learn_init/train_local/get_weights
fed_learn_upload_weights/download_global/aggregate
pq_init/push/pop/peek_deadline, ringbuf_init/write/read
watchdog_init/kick/is_expired, comp_filter_init/update
sha256_init/update/final/verify, energy_tracker_*
```

### edge_tpu_npu.h
```
accel_create/destroy, accel_probe, accel_load_model
accel_compile_model, accel_infer/infer_async
accel_pipeline_init/submit_preproc/wait_infer/free
roofline_init/predict_tflops/bound_type
dma_dbuf_init/free/swap/get_front/get_back
accel_mempool_create/destroy/alloc/reset/available
power_model_init, power_estimate_energy_uj
tensor_nhwc_to_nchw/nchw_to_nhwc/nhwc_to_nchw_int8
```

### inference_opt.h
```
opt_create/destroy, opt_fuse_operators, opt_fuse_conv_bn_relu
arena_create/destroy/alloc/reset/available
winograd_f23_transform_input/kernel/output/conv
quantize_int8_asym, dequantize_int8_asym, quant_calc_scale_zp
quant_matmul_int8, quant_requantize
thread_pool_create/destroy/parallel_for
gemm_micro_6x16, gemm_blocked
depthwise_conv2d_3x3, pointwise_conv2d_1x1
huffman_build_tree/encode/decode
sparse_dense_matmul_csr, per_channel_quantize/dequantize
im2col, col2im_gradient
```

### model_conversion.h
```
convert_create/destroy, convert_run, convert_check_ops
convert_calibrate_int8, convert_get_log
graph_opt_fold_batchnorm, graph_fuse_conv_relu_check
op_compat_init/check_conversion
channel_prune_init/destroy/compute_l1/select/apply
count_params_conv2d, count_flops_conv2d
count_params_fully_connected, count_total_params
magnitude_prune_weights/create_mask, compute_sparsity
block_sparsity_4x1, compute_sqnr_uniform, assign_bitwidth_sqnr
```

## Build & Test

```
make          # Build library + examples + demos + tests
make test     # Build and run all unit tests (37/37)
make bench    # Build and run benchmarks
make clean    # Clean build artifacts
```

## University Course Alignment

| School | Course | Topic Coverage |
|--------|--------|---------------|
| MIT | 6.004 Computation Structures | Pipeline architecture |
| Stanford | CS 229 Machine Learning | Model inference, ensemble methods |
| Berkeley | CS 267 HPC | GEMM optimization, roofline model |
| CMU | 15-418 Parallel Computing | Thread pool, parallel-for |
| UT Austin | CS 395T Systems ML | Quantization, pruning, model compression |
| ETH | 263-3501 Parallel Programming | Cache blocking, loop tiling |
| Cambridge | Part II: Concurrent Systems | Watchdog, ring buffer |
| 清华 | 操作系统 | Memory arena, scheduling |
| Georgia Tech | CS 7641 ML | Ensemble, Kalman filter |
