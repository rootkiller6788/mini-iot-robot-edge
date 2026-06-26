# mini-edge-ai 快速开始

## 概述
mini-edge-ai 是边缘 AI 推理的 C99 库, 支持 TFLite/ONNX/OpenVINO 模型加载,
INT8 量化推理, 算子融合优化, Google Coral/Intel MyriadX/NVIDIA Jetson 加速器,
以及边缘 ML 流水线 (传感器→预处理→推理→后处理→动作)。

## 编译
```sh
make          # 编译库和所有示例/演示
make lib      # 仅编译静态库 libmini_edge.a
make examples # 仅编译示例
make demos    # 仅编译演示
make clean    # 清理
```

## 硬件需求
- CPU: ARM Cortex-A / x86-64
- 内存: 64 MB+
- 选配: Google Coral Edge TPU, Intel Movidius MyriadX, NVIDIA Jetson
- 选配: Rockchip NPU, MediaTek APU

## 示例

### 1. 图像分类 (MobileNet v2)
```sh
./build/example_classification
```
加载 TFLite 或 ONNX 分类模型, 对输入图像进行 resize→normalize→softmax,
输出 Top-5 预测。

### 2. 目标检测 (SSD-MobileNet v2)
```sh
./build/example_detection
```
加载检测模型, 对输入图像进行预处理, 输出 bbox (NMS 后), 支持 COCO 90 类。

### 3. 关键词检测
```sh
./build/example_keyword_spotting
```
16kHz 音频→MFCC 特征→Keyword 分类, 支持 12 个关键词。

## 演示

### 人员检测演示
```sh
./build/demo_person_detection
```
模拟 320x240 摄像头输入, 运行完整流水线: 传感器读取→预处理→推理→后处理→决策→动作。
人员出现时拍照并上传云端。

### 联邦学习演示
```sh
./build/demo_federated_learning
```
模拟 5 个边缘客户端, 各训练本地模型后聚合全局权重 (FedAvg), 运行 20 轮。

## API 速查
| 头文件 | 功能 |
|--------|------|
| `edge_inference.h` | 模型加载、预处理/后处理、分类/检测/分割 |
| `model_conversion.h` | PyTorch→ONNX, TF→TFLite, INT8 校准, 图优化 |
| `edge_tpu_npu.h` | 加速器探测、模型编译、异步推理、流水线 |
| `inference_opt.h` | Conv+BN+ReLU 融合, Tensor Arena, Winograd, INT8 GEMM |
| `edge_pipeline.h` | 传感器→推理流水线, 联邦学习, 带宽策略 |

## 模型格式检测
库自动通过文件头魔数检测格式:
- TFLite: `TFL3` 魔数
- ONNX: `0x08 0x00 0x00 0x00` 开头

## 带宽节省策略
- confidence_threshold: 仅高置信度结果上传
- min_interval_ms: 最小上传间隔
- max_per_minute: 每分钟最大上传次数
- low_bandwidth_mode: 低带宽模式 (压缩特征)
