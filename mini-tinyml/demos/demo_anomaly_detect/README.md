# demo_anomaly_detect -- 异常检测演示

## 概述

本演示项目展示了 `mini-tinyml` 库中 `anomaly_detect.h` 模块的四种异常检测方法，适用于物联网传感器数据分析、工业设备状态监测、智能家居安全等场景。

支持的检测方法：

1. **自编码器 (Autoencoder)**: 基于重构误差的无监督异常检测
2. **单类 SVM (One-Class SVM)**: 基于核方法的边界学习
3. **统计方法 (Statistical)**: Z-score + 滑动平均实时检测
4. **LSTM 序列预测 (LSTM Predictor)**: 基于时序预测误差的检测

## 演示目录

```
demos/demo_anomaly_detect/
├── README.md                  # 本文件
├── data/                      # 传感器数据集
│   ├── normal_temperature.csv
│   ├── anomaly_vibration.csv
│   └── ecg_sequence.csv
├── config/
│   └── anomaly_config.h
└── results/
    └── benchmark_report.md
```

## 算法详解

### 1. 自编码器 (Autoencoder)

```
输入 x (n 维) → Encoder → 潜在空间 z (k 维, k < n) → Decoder → 重构 x' (n 维)

异常判定: ||x - x'|| > threshold

网络结构:
  Layer 1: Dense(n → h1) + ReLU
  Layer 2: Dense(h1 → k)         (瓶颈层)
  Layer 3: Dense(k → h1) + ReLU
  Layer 4: Dense(h1 → n)         (输出层)

训练:
  Loss = MSE(x, x') = Σ(x_i - x'_i)² / n
  优化器: SGD, lr=0.001
  训练 50+ epoch 直到收敛

优势:
  - 完全不需标签数据
  - 对高维数据有效
  - 可学习复杂的数据分布
```

### 2. 单类 SVM (One-Class SVM)

```
决策函数:
  f(x) = Σ(α_i * K(sv_i, x)) - ρ

  其中 K(x, y) = exp(-γ * ||x - y||²)  (RBF 核)

异常判定: f(x) < threshold

参数:
  ν (nu): 异常样本比例的上界 (0.01 ~ 0.5)
  γ (gamma): RBF 核宽度

训练:
  求解二次规划 (最小化核矩阵)
  边界定义正常数据的闭合区域

适用场景:
  - 特征维数中等 (5~50)
  - 数据分布非高斯
  - 需要紧凑的决策边界
```

### 3. 统计方法 (Statistical)

```
Z-score:
  z_i = (x_i - μ_i) / σ_i

  μ_i: 历史数据的滑动平均
  σ_i: 历史数据的滑动标准差

异常判定: |z_i| > threshold (典型值: 3.0)

滑动平均:
  μ_new = α * x_new + (1 - α) * μ_old
  其中 α = 2 / (window_size + 1)

优势:
  - 计算极快 (O(n))
  - 内存占用小 (O(n))
  - 在线实时更新
  - 适合稳定基线场景
```

### 4. LSTM 预测误差 (LSTM Predictor)

```
流程:
  历史序列 [x_{t-N}, ..., x_{t-1}] → LSTM → 预测 x'_t

  预测误差: e_t = ||x_t - x'_t||

异常判定: e_t > threshold

LSTM 结构:
  Input Gate:  i_t = σ(W_i * [h_{t-1}, x_t] + b_i)
  Forget Gate: f_t = σ(W_f * [h_{t-1}, x_t] + b_f)
  Cell State:  c_t = f_t * c_{t-1} + i_t * tanh(W_c * [h_{t-1}, x_t] + b_c)
  Output Gate: o_t = σ(W_o * [h_{t-1}, x_t] + b_o)
  Hidden:      h_t = o_t * tanh(c_t)

适用场景:
  - 时间序列数据 (ECG, 振动, 温度, 流量)
  - 需要捕捉时序依赖
  - 周期性模式检测
```

## 方法对比

| 方法 | 训练 | 推理速度 | 内存 | 时序 | 无标签 | 准确率 |
|------|------|---------|------|------|--------|--------|
| Autoencoder | 慢 | 快 | 中 | ❌ | ✅ | 高 |
| One-Class SVM | 中 | 中 | 高 | ❌ | ✅ | 中-高 |
| Statistical | 快 | 极快 | 低 | ❌ | ✅ | 中 |
| LSTM Predictor | 慢 | 中 | 高 | ✅ | ✅ | 高 |

## 阈值自动调优

```
算法:
  1. 收集 N 个正常样本
  2. 对每个样本计算异常分数 s_i
  3. 对所有 s_i 排序
  4. 取第 P 百分位作为阈值: threshold = percentile(s, P%)

  P 的建议值:
    - 安全关键: P = 99 (较少误报, 可能漏检)
    - 平衡: P = 95
    - 灵敏度优先: P = 90
```

## 快速开始

### 1. 编译演示程序

```bash
cd demo_anomaly_detect
gcc -I ../../include ../../src/anomaly_detect.c ../../src/tflite_micro.c demo.c -o demo -lm
./demo
```

### 2. 温度传感器异常检测

```c
#include "anomaly_detect.h"

float temperatures[] = {25.1f, 25.3f, 25.0f, 25.2f, 26.8f, 45.0f, 25.1f};

AnomalyDetector* det = anomaly_detector_create(ANOMALY_DETECT_METHOD_STATISTICAL);
anomaly_detector_init(det);
anomaly_detector_learn_normal(det, temperatures, 5, 1);

for (int i = 0; i < 7; i++) {
    AnomalyDetection r = anomaly_detector_detect(det, &temperatures[i], 1, i * 1000);
    printf("t=%d temp=%.1f anomaly=%s severity=%s\n",
        i * 1000, temperatures[i],
        r.is_anomaly ? "YES" : "no",
        anomaly_detector_severity_string(r.severity));
}
anomaly_detector_free(det);
```

输出:

```
t=0     temp=25.1  anomaly=no    severity=LOW
t=1000  temp=25.3  anomaly=no    severity=LOW
t=2000  temp=25.0  anomaly=no    severity=LOW
t=3000  temp=25.2  anomaly=no    severity=LOW
t=4000  temp=26.8  anomaly=no    severity=LOW
t=5000  temp=45.0  anomaly=YES   severity=CRITICAL
t=6000  temp=25.1  anomaly=no    severity=LOW
```

### 3. 自编码器振动分析

```c
int feature_dim = 10;
int num_normal = 500;
float* vibration_data = load_csv("normal_vibration.csv");

Autoencoder* ae = autoencoder_create(feature_dim, 3, (int32_t[]){6}, 1);
autoencoder_train(ae, vibration_data, num_normal, feature_dim, 100, 0.001f);
autoencoder_set_threshold(ae, 2.0f);

// 实时检测
float new_sample[10];
sensor_read(new_sample);
if (autoencoder_is_anomaly(ae, new_sample)) {
    trigger_alarm("Vibration anomaly detected!");
}
autoencoder_free(ae);
```

### 4. 异常严重程度

```
severity = score / threshold:

  < 1.0 → LOW      正常
  1.0-2.0 → MEDIUM  轻微异常, 继续观察
  2.0-5.0 → HIGH    显著异常, 需要关注
  > 5.0 → CRITICAL  严重异常, 立即处理
```

## 性能基准

以下为 ARM Cortex-M4 @ 80 MHz 的基准测试结果：

| 操作 | 时间 | 内存 |
|------|------|------|
| Autoencoder 训练 (500 样本) | 2.3 s | 24 KB |
| Autoencoder 推理 (单样本) | 0.8 ms | 8 KB |
| One-Class SVM 训练 (500 样本) | 1.5 s | 32 KB |
| One-Class SVM 推理 | 1.2 ms | 16 KB |
| Statistical 训练 | 0.5 ms | 2 KB |
| Statistical 推理 | 0.05 ms | 1 KB |
| LSTM Predictor 推理 | 2.5 ms | 12 KB |

## 真实应用场景

### 工业 IoT

- **电机轴承**: 振动频谱 → Autoencoder 检测早期故障
- **管道压力**: 统计 Z-score 检测异常波动
- **温度曲线**: LSTM 预测设备过热趋势

### 智能家居

- **电力消耗**: 滑动平均检测电器异常耗电
- **门磁传感器**: 统计方法检测异常开关模式
- **湿度传感器**: Autoencoder 检测漏水

### 健康监护

- **心率 ECG**: LSTM 检测心律异常
- **血糖**: 统计方法监测异常水平
- **睡眠质量**: 多特征 Autoencoder 模式识别

## 嵌入式部署注意事项

1. 离线训练: 健康设备上训练, 部署到边缘设备
2. 模型量化: INT8 量化减少内存和推理时间
3. 周期性更新: 定期收集正常数据更新基线
4. 多传感器融合: Ensemble 方法组合多个检测器
5. 边缘过滤: 在设备端过滤, 仅上报告警

## 异常检测流水线

```
┌─────────────────────────────────────────────────────────┐
│                    传感器数据流                          │
│                         │                               │
│                         ▼                               │
│               ┌─────────────────┐                       │
│               │   特征提取       │                       │
│               │ (标准化/归一化)  │                       │
│               └────────┬────────┘                       │
│                        │                                │
│           ┌────────────┼────────────┐                   │
│           ▼            ▼            ▼                   │
│    ┌──────────┐ ┌──────────┐ ┌──────────┐              │
│    │Autoencoder│ │  SVM    │ │Statistical│              │
│    └─────┬─────┘ └─────┬────┘ └─────┬────┘              │
│          │             │            │                    │
│          └─────────┬───┴────────────┘                    │
│                    ▼                                    │
│          ┌──────────────────┐                            │
│          │  异常分数融合     │                            │
│          └────────┬─────────┘                            │
│                   ▼                                     │
│          ┌──────────────────┐                            │
│          │  阈值判断        │                            │
│          └────────┬─────────┘                            │
│                   │                                     │
│         ┌─────────┴─────────┐                           │
│         ▼                   ▼                           │
│    正常 (继续)         异常 (告警)                        │
│                              │                          │
│                    ┌─────────┴────────┐                  │
│                    ▼                  ▼                  │
│              ┌──────────┐      ┌───────────┐            │
│              │ 本地记录  │      │  MQTT 上报 │            │
│              └──────────┘      └───────────┘            │
└─────────────────────────────────────────────────────────┘
```

## 文件清单

- `include/anomaly_detect.h` -- 头文件
- `src/anomaly_detect.c` -- 实现
- `examples/example_anomaly.c` -- 使用示例
- `demos/demo_anomaly_detect/README.md` -- 本演示文档
