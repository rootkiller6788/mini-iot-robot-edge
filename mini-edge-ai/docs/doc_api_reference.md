# mini-edge-ai API Reference

## edge_inference.h — 边缘推理核心

### 类型

| 枚举 | 值 | 说明 |
|------|------|------|
| `InferBackend` | `TFLITE / ONNX / OPENVINO / NATIVE` | 推理后端 |
| `InputFormat` | `BGR888 / RGB888 / GRAY8 / FLOAT32 / INT8` | 输入数据格式 |
| `ModelType` | `CLASSIFICATION / DETECTION / SEGMENTATION / REGRESSION` | 模型类型 |
| `PreprocMode` | `NONE / RESIZE / NORMALIZE / MEAN_STD` | 预处理模式 |
| `PostprocMode` | `SOFTMAX / SIGMOID / NMS / ARGMAX` | 后处理模式 |

### 结构体

- `InputShape`: width, height, channels, format
- `Classification`: class_id, label[64], score
- `Detection`: class_id, label[64], score, bbox (xmin,ymin,xmax,ymax)
- `SegmentationMask`: width, height, num_classes, class_map[4096]
- `InferConfig`: 完整推理配置 (model_path, backend, shapes, preproc/postproc, threads)

### 函数

- `infer_create(config)` → `InferCtx*`: 创建推理上下文
- `infer_destroy(ctx)`: 销毁上下文
- `infer_load_model(ctx)` → 0=ok: 加载模型文件
- `infer_run(ctx, input, bytes)` → 0=ok: 执行推理
- `infer_get_classifications(ctx, out, max)` → count: 获取分类结果 (按 score 降序)
- `infer_get_detections(ctx, out, max)` → count: 获取检测结果
- `infer_get_segmentation(ctx, out)` → 0: 获取分割掩码
- `infer_get_output_tensor(ctx, out, max)` → bytes: 读取原始输出张量
- `infer_set_input_tensor(ctx, data, bytes)` → bytes: 设置输入张量
- `preproc_resize_bilinear(src, sw,sh,sc, dst, dw,dh)` → 0: 双线性 resize
- `preproc_normalize_f32(src, w,h,c, dst, mean, std)` → 0: 归一化到 float32
- `preproc_bgr_to_rgb(data, w,h,c)` → 0: BGR 转 RGB
- `postproc_softmax_f32(logits, probs, n)` → 0: Softmax
- `postproc_argmax_f32(probs, n)` → index: Argmax
- `postproc_nms(dets, count, iou_thresh, kept)` → n_kept: NMS
- `postproc_sigmoid_f32(x, y, n)` → 0: Sigmoid

---

## model_conversion.h — 模型转换

### 枚举

- `SrcFramework`: `PYTORCH / TENSORFLOW / KERAS / CAFFE`
- `DstFramework`: `ONNX / TFLITE / OPENVINO_IR / RKNN`
- `QuantMode`: `NONE / INT8_POST_TRAINING / INT8_QAT / FP16 / DYNAMIC_RANGE`
- `GraphOpt`: `FOLD_BN / FUSE_CONV_RELU / FUSE_CONV_BN_RELU / ELIM_DROPOUT / ELIM_IDENTITY`

### 函数

- `convert_create(config)` → `ConvertCtx*`
- `convert_destroy(ctx)`
- `convert_run(ctx)` → 0=ok: 执行转换
- `convert_check_ops(ctx, results, max)` → count: 算子兼容性检查
- `convert_calibrate_int8(ctx, rep_dataset, n)` → 0: INT8 校准
- `convert_get_log(ctx, buf, max)` → bytes: 获取转换日志
- `graph_opt_fold_batchnorm(weights, bias, bn_*, eps, ch)` → 0: BN 折叠
- `graph_fuse_conv_relu_check(conv_kernel, relu_slope)` → 1/0: 是否可融合

---

## edge_tpu_npu.h — 边缘加速器

### 类型

- `Accelerator`: `NONE / CORAL_EDGE_TPU / INTEL_MYRIDX / NVIDIA_JETSON_GPU / ROCKCHIP_RKNN / MEDIATEK_APU`
- `AccelBus`: `USB / PCIE / MMAP`
- `AccelDataType`: `FP32 / FP16 / INT8 / UINT8`

### 函数

- `accel_create(config)` → `AccelCtx*`: 创建加速器上下文
- `accel_destroy(ctx)`
- `accel_probe(infos, max)` → count: 探测可用加速器
- `accel_load_model(ctx)` → 0: 加载模型
- `accel_compile_model(generic, compiled, target)` → 0: 为特定加速器编译模型
- `accel_infer(ctx, in, in_bytes, out, &out_bytes)` → 0: 同步推理
- `accel_infer_async(ctx, in, in_bytes)` → 0: 异步推理
- `accel_pipeline_init(ctx, pipe)` → 0: 初始化流水线缓冲
- `accel_pipeline_submit_preproc(ctx, pipe)` → 0: 提交预处理
- `accel_pipeline_wait_infer(ctx, pipe)` → 0: 等待推理完成
- `accel_pipeline_free(ctx, pipe)`: 释放流水线缓冲
- `accel_get_output(ctx, out, max)` → bytes: 获取输出
- `accel_get_inference_time_ms(ctx)` → float: 推理耗时 ms

---

## inference_opt.h — 推理优化

### 类型

- `FuseType`: `CONV_BN_RELU / CONV_BIAS_RELU / CONV_BN / FC_RELU`
- `ConvAlgo`: `WINOGRAD_F6x6_3x3 / F4x4_3x3 / F2x2_3x3 / IM2COL / GEMM`
- `QuantScheme`: `ASYMMETRIC / SYMMETRIC / PER_CHANNEL`

### 函数

- `opt_create()` → `OptCtx*`
- `opt_destroy(ctx)`
- `opt_fuse_operators(ctx, ops, &num)` → 0: 批量算子融合
- `opt_fuse_conv_bn_relu(conv_w, conv_b, bn_*, eps, oc, ic, ks, fused_w, fused_b)` → 0
- `arena_create(size)` → `TensorArena*`: 创建张量内存池
- `arena_destroy(arena)`
- `arena_alloc(arena, size)` → `void*`: 64 字节对齐分配
- `arena_reset(arena)`: 重置 (不释放内存)
- `arena_available(arena)` → bytes: 剩余可用空间
- `winograd_f23_transform_input(in, in_c, in_h, in_w, out)` → 0: Winograd F(2x2,3x3) 输入变换
- `winograd_f23_transform_kernel(kernel, oc, ic, kh, kw, out)` → 0: 核变换
- `winograd_f23_transform_output(in, oc, oh, ow, out)` → 0: 输出逆变换
- `winograd_f23_conv(A, B, tiles, ic, oc, G, C)` → 0: Winograd 卷积
- `quantize_int8_asym(src, n, scale, zp, dst)` → 0: Float→INT8 不对称量化
- `dequantize_int8_asym(src, n, scale, zp, dst)` → 0: INT8→Float
- `quant_calc_scale_zp(data, n, &scale, &zp)`: 计算 scale 和 zero_point
- `quant_matmul_int8(A, B, C, M, N, K)` → 0: INT8 矩阵乘 (结果 int32)
- `quant_requantize(src, n, scale, zp, dst)` → 0: INT32→INT8 重新量化
- `thread_pool_create(n)` → `ThreadPool*`: 创建线程池
- `thread_pool_destroy(pool)`
- `thread_pool_parallel_for(pool, fn, arg, start, end)` → 0: 并行 for

---

## edge_pipeline.h — 边缘 ML 流水线

### 类型

- `StageType`: `SENSOR_READ / PREPROCESS / INFERENCE / POSTPROCESS / DECISION / ACTION`
- `ActionType`: `NONE / SNAPSHOT / CLOUD_UPLOAD / ALERT / MOTOR_CTRL`
- `TaskType`: `PERSON_DETECT / KEYWORD_SPOT / ANOMALY_DETECT / GESTURE_RECOG`

### 函数

- `pipeline_create()` → `PipelineCtx*`
- `pipeline_destroy(ctx)`
- `pipeline_add_stage(ctx, type, fn, arg)` → idx: 添加阶段
- `pipeline_run(ctx)` → 0: 运行一次全部阶段
- `pipeline_run_loop(ctx, max_iters)` → 0: 循环运行
- `pipeline_stop(ctx)` → 0: 停止
- `pipeline_get_state(ctx)` → PipelineState
- `pipeline_get_stages(ctx, out, max)` → count
- `pipeline_set_action(ctx, action)` → 0: 注册动作
- `pipeline_execute_action(ctx, type, result)` → 0: 执行动作
- `pipeline_should_upload(policy, result, last_ms, count)` → 1/0: 带宽策略判断
- `fed_learn_init(ctx, n_weights)` → 0: 初始化联邦学习
- `fed_learn_train_local(ctx, samples, n, epochs)` → 0: 本地训练
- `fed_learn_get_weights(ctx, weights, max)` → count: 获取本地权重
- `fed_learn_upload_weights(ctx, url)` → 0: 上传权重
- `fed_learn_download_global(ctx, url)` → 0: 下载全局权重
- `fed_learn_aggregate(local_w, n_clients, client_ws, n_w, global_w)` → 0: FedAvg 聚合
