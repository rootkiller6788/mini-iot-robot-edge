# mini-tinyml -- TinyML微机器学习 (C 语言实现)

mini-tinyml 是一个轻量级 TinyML 库，使用纯 C99 编写，专为嵌入式设备和物联网边缘节点设计。它提供了模型推理、模型压缩、唤醒词检测、异常检测和端侧学习等核心功能模块，可在资源受限的微控制器（如 ARM Cortex-M、ESP32 等）上运行。

## 目录结构

```
mini-tinyml/
├── include/                     # 头文件 (5 个模块)
│   ├── tflite_micro.h           #   TensorFlow Lite Micro 推理引擎
│   ├── model_compress.h         #   模型压缩 — 剪枝/量化/聚类/Huffman
│   ├── wake_word.h              #   唤醒词检测 — MFCC + DNN/CNN 模型
│   ├── anomaly_detect.h         #   异常检测 — 自编码器/SVM/统计/LSTM
│   └── ondevice_learn.h         #   端侧学习 — 迁移学习/增量/联邦/个性化
├── src/                         # 实现文件 (5 个 .c, 共 800+ 行)
│   ├── tflite_micro.c
│   ├── model_compress.c
│   ├── wake_word.c
│   ├── anomaly_detect.c
│   └── ondevice_learn.c
├── examples/                    # 使用示例
│   ├── example_wake_word.c
│   ├── example_anomaly.c
│   └── example_ondevice_learn.c
├── demos/                       # 演示项目
│   ├── demo_wake_word/
│   └── demo_anomaly_detect/
├── docs/                        # 文档
│   ├── tflite_micro_guide.md
│   └── model_optimization.md
├── Makefile
└── README.md
```

## 快速开始

### 依赖

- GCC 或兼容的 C99 编译器
- GNU Make (或兼容的 Make 工具)
- `<stdbool.h>`, `<math.h>` (标准 C 库)

### 编译

```bash
make          # 编译所有库目标文件到 bin/
make examples # 编译库 + 所有示例可执行文件
make clean    # 清理构建产物
```

### 基础用法

```c
#include "tflite_micro.h"
#include "wake_word.h"
#include "anomaly_detect.h"
#include "ondevice_learn.h"
#include "model_compress.h"

int main(void) {
    // 1. TFLite Micro 推理
    TFLiteTensorArena* arena = tflite_tensor_arena_create(64 * 1024);
    MicroMutableOpResolver* resolver = tflite_micro_op_resolver_create();
    TFLiteInterpreter* interpreter = tflite_interpreter_create();
    tflite_interpreter_init(interpreter, NULL, 0, resolver, arena);
    tflite_interpreter_allocate_tensors(interpreter);
    tflite_interpreter_invoke(interpreter);

    // 2. 唤醒词检测
    WakeWordDetector* ww = wake_word_detector_create(16000, "hey_device");
    wake_word_detector_init(ww);
    int16_t audio[800];
    wake_word_detector_process_samples(ww, audio, 800);

    // 3. 模型压缩
    float weights[] = {0.1f, -0.5f, 0.3f, 0.0f};
    ModelWeightBuffer* buf = model_weight_buffer_create(weights, 4);
    ModelCompressionStats* stats = model_compress_benchmark(buf);
    model_compression_stats_print(stats);

    return 0;
}
```

## 模块概览

### 1. tflite_micro.h -- TensorFlow Lite Micro 推理引擎

| 功能 | API |
|------|-----|
| 张量竞技场 (预分配内存) | `tflite_tensor_arena_create()`, `tflite_tensor_arena_alloc()` |
| 张量管理 | `tflite_tensor_create()`, `tflite_tensor_alloc_data()` |
| 运算符注册裁决器 | `MicroMutableOpResolver`, `tflite_micro_op_resolver_resolve()` |
| Flatbuffers 模型 | `TFLiteModel`, `tflite_model_validate()` |
| 解释器 | `tflite_interpreter_allocate_tensors()`, `tflite_interpreter_invoke()` |

### 2. model_compress.h -- 模型压缩

- **剪枝**: 幅度剪枝 + 结构化剪枝 (整滤波器移除)
- **量化**: FP32 → INT8/UINT8/INT16
- **聚类**: K-Means 权值聚类
- **Huffman**: 权值 Huffman 编码
- **压缩率**: `model_compression_ratio_calc()` 计算压缩比

### 3. wake_word.h -- 唤醒词检测

- **MFCC**: Mel 滤波器组，13 系数 MFCC 提取
- **模型**: DNN/CNN/DS-CNN/TC-ResNet 支持
- **滑动窗口**: 1 秒滑动窗口分析
- **后处理**: 短激活忽略，检测冷却期

### 4. anomaly_detect.h -- 异常检测

- **自编码器**: 重构误差检测异常
- **单类 SVM**: 核化决策函数
- **统计方法**: Z-score + 滑动平均
- **序列模型**: LSTM 预测误差
- **阈值自调**: 基于历史数据百分位

### 5. ondevice_learn.h -- 端侧学习

- **迁移学习**: 冻结基础层，训练分类头
- **增量学习**: 渐进式在线更新
- **联邦学习**: 客户端训练 + FedAvg 聚合
- **个性化**: 用户数据微调

## 命名规范

| 类别 | 规范 | 示例 |
|------|------|------|
| 宏常量 | UPPER_SNAKE_CASE | `TFLITE_MICRO_MAX_TENSORS` |
| 类型名 | PascalCase | `TFLiteInterpreter`, `ModelWeightBuffer` |
| 枚举类型 | PascalCase | `TFLiteStatus`, `WakeWordState` |
| 枚举值 | UPPER_SNAKE_CASE (prefixed) | `TFLITE_STATUS_OK` |
| 函数名 | snake_case | `tflite_tensor_arena_create()` |
| 变量名 | snake_case | `arena_size`, `num_clusters` |
| 文件名 | snake_case | `tflite_micro.h`, `wake_word.c` |

## 许可

MIT License
