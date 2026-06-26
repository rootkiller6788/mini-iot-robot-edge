# TensorFlow Lite Micro 推理引擎指南

## 概述

`tflite_micro.h` 模块实现了 TensorFlow Lite for Microcontrollers (TFLite Micro) 的核心推理功能，
专为资源受限的嵌入式设备设计。它支持运行 `.tflite` 格式的 FlatBuffers 模型文件。

## 架构设计

```
┌─────────────────────────────────────────────────────────┐
│                    TFLiteInterpreter                     │
│  ┌───────────────────────────────────────────────────┐  │
│  │               MicroMutableOpResolver               │  │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────────────┐   │  │
│  │  │ Conv2D  │  │ Dense   │  │ DepthwiseConv2D │   │  │
│  │  │ v1      │  │ v1      │  │ v1              │   │  │
│  │  └─────────┘  └─────────┘  └─────────────────┘   │  │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────────────┐   │  │
│  │  │ Softmax │  │ Relu    │  │ Add             │   │  │
│  │  │ v1-v2   │  │ v1      │  │ v1              │   │  │
│  │  └─────────┘  └─────────┘  └─────────────────┘   │  │
│  └───────────────────────────────────────────────────┘  │
│                                                         │
│  ┌───────────────────────────────────────────────────┐  │
│  │                  TFLiteTensorArena                  │  │
│  │  ┌───────┬───────┬───────┬───────────────────┐    │  │
│  │  │Input  │Hidden │Hidden │Output             │    │  │
│  │  │Tensor │Tensor │Tensor │Tensor             │    │  │
│  │  └───────┴───────┴───────┴───────────────────┘    │  │
│  └───────────────────────────────────────────────────┘  │
│                                                         │
│  ┌───────────────────────────────────────────────────┐  │
│  │               TFLiteTensor[]                        │  │
│  │  [0] Input Tensor      (float32[1,28,28,1])       │  │
│  │  [1] Output Tensor     (float32[1,10])             │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

## 核心概念

### 1. 张量竞技场 (Tensor Arena)

张量竞技场是一块预分配的连续内存区域，所有张量的数据都在此区域内分配。
这种设计避免了动态内存分配，确保在嵌入式设备上的确定性行为。

```c
// 创建 64 KB 的竞技场
TFLiteTensorArena* arena = tflite_tensor_arena_create(64 * 1024);

// 从竞技场分配内存 (16字节对齐)
void* ptr = tflite_tensor_arena_alloc(arena, 1024);

// 查看使用情况
size_t used = tflite_tensor_arena_used(arena);
size_t free = tflite_tensor_arena_available(arena);
printf("Arena: %zu used, %zu free\n", used, free);

// 重置 (重用已分配的内存)
tflite_tensor_arena_reset(arena);

tflite_tensor_arena_free(arena);
```

### 2. 张量 (Tensor)

张量是模型中的数据容器，支持多种数据类型：

| 类型 | 枚举值 | 字节 |
|------|--------|------|
| float32 | TFLITE_TYPE_FLOAT32 | 4 |
| int8 | TFLITE_TYPE_INT8 | 1 |
| uint8 | TFLITE_TYPE_UINT8 | 1 |
| int32 | TFLITE_TYPE_INT32 | 4 |
| bool | TFLITE_TYPE_BOOL | 1 |
| int16 | TFLITE_TYPE_INT16 | 2 |

```c
// 创建张量
TFLiteTensor* tensor = tflite_tensor_create();

// 分配数据 (2D: 1x784)
int32_t dims[] = {1, 784};
tflite_tensor_alloc_data(tensor, TFLITE_TYPE_FLOAT32, dims, 2);

// 写入数据
float input_data[784];
tflite_tensor_copy_float_data(tensor, input_data, 784);

// 读取数据
float output_data[10];
tflite_tensor_get_float(tensor, output_data, 10);

size_t elms = tflite_tensor_element_count(tensor);
printf("Elements: %zu\n", elms);

// 释放
tflite_tensor_free(tensor);
```

### 3. 运算符注册裁决器 (Op Resolver)

MicroMutableOpResolver 维护一个已注册运算符的列表。当解释器解析模型时，
它通过 Op Resolver 查找每个运算符的实现。

```c
MicroMutableOpResolver* resolver = tflite_micro_op_resolver_create();

// 注册内置运算符
tflite_micro_op_resolver_add_builtin(resolver, "FULLY_CONNECTED", 9, 1, 1);
tflite_micro_op_resolver_add_builtin(resolver, "SOFTMAX", 25, 1, 2);
tflite_micro_op_resolver_add_builtin(resolver, "CONV_2D", 3, 1, 3);
tflite_micro_op_resolver_add_builtin(resolver, "DEPTHWISE_CONV_2D", 4, 1, 3);
tflite_micro_op_resolver_add_builtin(resolver, "AVERAGE_POOL_2D", 1, 1, 2);

// 兼容性检查: 运算符版本在 [min, max] 之间才可被解析
TFLiteOpEntry entry;
TFLiteOpResolveStatus s = tflite_micro_op_resolver_resolve(resolver, 9, 1, &entry);
if (s == TFLITE_OP_RESOLVE_OK) {
    printf("Op '%s' (v%d) resolved\n", entry.op_name, 1);
}

tflite_micro_op_resolver_free(resolver);
```

### 4. FlatBuffers 模型

TFLite 使用 FlatBuffers 作为模型序列化格式。

```c
const uint8_t* model_bytes = load_file("model.tflite");
size_t model_size = get_file_size("model.tflite");

TFLiteModel* model = tflite_model_create(model_bytes, model_size);
if (tflite_model_validate(model)) {
    printf("Model: %s\n", tflite_model_description(model));
} else {
    printf("Invalid model!\n");
}

tflite_model_free(model);
```

**FlatBuffers 模型结构**:

```
Root Table: Model
├── version: int32
├── operator_codes: [OperatorCode]
│   ├── builtin_code: int8
│   └── version: int32
├── subgraphs: [SubGraph]
│   ├── tensors: [Tensor]
│   │   ├── shape: [int32]
│   │   ├── type: TensorType
│   │   ├── buffer: uint32 (index)
│   │   └── quantization: QuantizationParameters
│   ├── inputs: [int32]  (indices into tensors)
│   ├── outputs: [int32] (indices into tensors)
│   ├── operators: [Operator]
│   │   ├── opcode_index: uint32
│   │   ├── inputs: [int32]
│   │   └── outputs: [int32]
│   └── name: string (optional)
├── buffers: [Buffer]
│   └── data: [uint8]
└── metadata: [Metadata] (optional)
```

### 5. 解释器 (Interpreter)

解释器是运行 TFLite 模型的核心组件。

```c
TFLiteInterpreter* interpreter = tflite_interpreter_create();

// 初始化 (传入模型数据、Op Resolver、Tensor Arena)
tflite_interpreter_init(interpreter, model_data, model_size, resolver, arena);

// 分配张量 (在竞技场中为所有张量分配内存)
tflite_interpreter_allocate_tensors(interpreter);

// 获取输入张量并写入数据
TFLiteTensor* input = tflite_interpreter_input_tensor(interpreter, 0);
float img[28*28];
tflite_tensor_copy_float_data(input, img, 28*28);

// 执行推理
tflite_interpreter_invoke(interpreter);

// 读取输出
TFLiteTensor* output = tflite_interpreter_output_tensor(interpreter, 0);
float predictions[10];
tflite_tensor_get_float(output, predictions, 10);

// 释放
tflite_interpreter_free(interpreter);
```

## 完整推理流程

```
1. 创建 Arena
       │
2. 加载 .tflite FlatBuffers 模型
       │
3. 创建 Op Resolver 并注册运算符
       │
4. 创建 Interpreter
       │
5. interpreter_init(model, resolver, arena)
       │
6. interpreter_allocate_tensors()
       │
7. 获取 Input Tensor, 写入输入数据
       │
8. interpreter_invoke()
       │
9. 获取 Output Tensor, 读取输出数据
       │
10. 释放所有资源
```

## 内置运算符列表

| Op Code | 名称 | 版本 | 说明 |
|---------|------|------|------|
| 0 | ADD | 1-2 | 张量加法 |
| 1 | AVERAGE_POOL_2D | 1-2 | 平均池化 |
| 3 | CONV_2D | 1-3 | 2D 卷积 |
| 4 | DEPTHWISE_CONV_2D | 1-3 | 深度可分离卷积 |
| 6 | DEQUANTIZE | 1-2 | 反量化 |
| 9 | FULLY_CONNECTED | 1-5 | 全连接 |
| 14 | LOGISTIC | 1 | Sigmoid |
| 18 | MUL | 1-2 | 张量乘法 |
| 21 | PAD | 1 | 填充 |
| 22 | MAX_POOL_2D | 1-2 | 最大池化 |
| 23 | QUANTIZE | 1-2 | 量化 |
| 25 | RESHAPE | 1 | 形状变换 |
| 29 | SOFTMAX | 1-2 | Softmax |
| 30 | SPACE_TO_DEPTH | 1 | 空间到深度 |
| 34 | STRIDED_SLICE | 1 | 切片 |
| 41 | CONCATENATION | 1-2 | 张量拼接 |

## 量化支持

### 整数量化流程

```
float32 模型
    │
    ▼
[quantize weights]  weights_scale * (weights_int8 - zp)
    │
    ▼
int8 模型 (TFLITE_TYPE_INT8)
    │
    ▼
推理 (int8 矩阵乘法)
    │
    ▼
[dequantize output]  output_scale * (output_int8 - zp)
    │
    ▼
float32 输出
```

### 量化张量配置

```c
// 量化感知推理
tensor->is_quantized = true;
tensor->scale = 0.02f;      // 量化比例因子
tensor->zero_point = -128;   // 零点偏移

// 量化公式
// real_value = scale * (quantized_value - zero_point)
```

## 内存优化

### 竞技场大小估算

```
arena_size = 输入张量大小
           + 输出张量大小
           + 中间张量大小 (由模型拓扑决定)
           + 运算符工作缓冲区
           + 对齐填充 (16 字节)
```

经验法则：取模型权重总大小的 2~4 倍作为 Arena 大小。

### 运算符共享

多个运算符可以复用同一个工作缓冲区：

```c
// 仅当两个算符在不同时间执行时
tflite_tensor_arena_reset(arena);  // 重置使用计数器
```

## 调试技巧

### 状态检查

```c
if (status != TFLITE_STATUS_OK) {
    printf("Error: %s\n", tflite_status_to_string(status));
}
```

### 张量信息

```c
for (int i = 0; i < tensor->num_dims; i++) {
    printf("dim[%d] = %d\n", i, tensor->dims[i]);
}
printf("type = %s, bytes = %zu\n",
    tflite_tensor_type_to_string(tensor->type), tensor->bytes);
```

## 限制与约束

1. **不支持控制流**: if/while 操作符未实现
2. **最大张量数**: 32 (`TFLITE_MICRO_MAX_TENSORS`)
3. **最大维度**: 8 (`TFLITE_MICRO_MAX_DIMENSIONS`)
4. **仅支持单子图**: `subgraph_index` 必须为 0
5. **不支持委托**: GPU/NPU/Hexagon 委托不可用
6. **模型限制**: 最大 256 MB

## 参考

- [TensorFlow Lite Micro 官方文档](https://www.tensorflow.org/lite/microcontrollers)
- [FlatBuffers 格式](https://google.github.io/flatbuffers/)
- [TFLite 模型转换指南](https://www.tensorflow.org/lite/models/convert)
