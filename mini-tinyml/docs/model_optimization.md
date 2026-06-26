# 模型优化与压缩指南

## 概述

`model_compress.h` 模块提供四种模型压缩技术，用于减小神经网络模型的大小并加速推理，
同时尽可能保持模型精度。这些技术适用于资源受限的 TinyML 部署场景。

## 压缩技术矩阵

| 技术 | 压缩率 | 精度损失 | 计算开销 | 适用场景 |
|------|--------|---------|---------|---------|
| **剪枝 (Pruning)** | 2-10x | 低 (<1%) | 推理加速 | 连接密集的网络 |
| **量化 (Quantization)** | 2-4x | 低-中 (0.5-3%) | 推理加速 | 通用卷积/全连接网络 |
| **聚类 (Clustering)** | 3-8x | 中 (1-5%) | 无变化 | 大模型存储受限 |
| **Huffman** | 1.5-3x | 无 | 额外解压开销 | 无损压缩补充 |

## 1. 剪枝 (Pruning)

### 幅度剪枝 (Magnitude-Based Pruning)

将绝对值小于阈值的权重设为零，生成稀疏网络。

```c
// 原始权重
float weights[] = {0.1f, -0.5f, 0.02f, -0.8f, 0.003f, 0.2f, -0.001f, 0.15f};
ModelWeightBuffer* buf = model_weight_buffer_create(weights, 8);

// 50% 稀疏度剪枝
ModelPruneBuffer* pruned = model_prune_magnitude(buf, 0.50f);

// 计算实际稀疏度
float sparsity = model_prune_compute_sparsity(pruned->data, pruned->count, 1e-6f);
printf("Sparsity: %.1f%%\n", sparsity * 100.0f);

// 应用剪枝结果
model_prune_apply(buf, pruned);
```

**剪枝算法**:

```
输入: 权重矩阵 W, 目标稀疏度 s
输出: 稀疏权重 W'

1. 对所有权重 |w_i| 排序
2. 找到分位数 cutoff = sort_indices[(1-s) * N]
3. 阈值 threshold = |W[cutoff]|
4. 对于每个权重 w_i:
      如果 |w_i| < threshold:
          w_i = 0  (剪枝)
      否则:
          w_i = w_i (保留)
5. 记录 mask[i] = (|w_i| >= threshold)
```

### 结构化剪枝 (Structured Pruning)

移除整个滤波器（通道），而非单个权重。

```c
// 10 个滤波器，每个 16 个权重 (10x16 = 160)
ModelPruneBuffer* structured = model_prune_structured(buf, 10, 16, 0.3f);
// 移除最低范数的 30% 滤波器
```

**滤波器范数计算**:

```
对于每个滤波器 f_k:
    norm_k = sqrt(Σ(w_i^2))  对于 i 在滤波器 k 中

移除 norm_k 最小的 N * sparsity 个滤波器
```

### 非零值统计

```c
int32_t nz = model_prune_count_nonzero(pruned->data, pruned->count, 1e-6f);
printf("Non-zero: %d / %d = %.1f%%\n", nz, pruned->count,
       (float)nz / pruned->count * 100.0f);
```

## 2. 量化 (Quantization)

### FP32 → INT8

将 32 位浮点权重映射到 8 位整数。

```c
// 32-bit float → 8-bit int
ModelQuantBuffer* quant = model_quant_float_to_int8(buf);
printf("Scale: %.6f, Zero Point: %d\n",
       (double)quant->params.scale, quant->params.zero_point);

// 大小对比
float original_size = buf->count * sizeof(float);      // e.g. 4000 bytes
float quantized_size = quant->count * sizeof(int8_t);  // e.g. 1000 bytes (4x reduction)

// 精度评估
float avg_error = model_quant_absolute_error(buf, quant);
printf("Average quantization error: %.6f\n", (double)avg_error);
```

### 量化参数计算

```
对称量化 (INT8):
  scale = max(|W_min|, |W_max|) / 127
  zero_point = 0
  quantized_value = round(float_value / scale)

非对称量化 (UINT8):
  scale = (W_max - W_min) / 255
  zero_point = round(-W_min / scale)
  quantized_value = round(float_value / scale) + zero_point
```

### 反量化

```c
// INT8 → FP32
float* dequant = model_quant_dequant_int8(quant);
// dequant[i] = scale * quant->data[i]
```

### 量化精度损失

```
典型值:
  INT8:     0.1% ~ 1% 精度损失
  UINT8:    0.1% ~ 1% 精度损失
  INT16:    < 0.01% 精度损失

受影响的层:
  - 第一层 (输入层): 精度损失最大
  - 最后一层 (输出层): 敏感度较高
  - 中间层: 鲁棒性较好
```

## 3. 权重聚类 (K-Means)

将相似的权重值归为一组，用聚类中心替代。

```c
ModelClusterResult* cluster = model_cluster_kmeans(buf, 16, 100);
printf("Clusters: %d / %d items\n", cluster->num_clusters, cluster->num_items);

// 重建误差
float error = model_cluster_reconstruction_error(buf, cluster);
printf("Reconstruction RMSE: %.6f\n", (double)error);

// 存储: 16 个 centroid (float) + N 个 assignment (int8)
float compressed = 16 * sizeof(float) + buf->count * sizeof(int8_t);
float ratio = model_compression_ratio_calc(buf->count * sizeof(float), compressed);
printf("Compression ratio: %.2fx\n", (double)ratio);

// 解压
float* decompressed = model_cluster_decompress(cluster);
```

### 最佳 K 值 (肘部方法)

```c
int32_t best_k = model_cluster_elbow_point(buf, 64);
printf("Elbow point: k = %d\n", best_k);
```

**肘部方法**:

```
对于 k = 1 到 64:
    运行 K-Means(k)
    计算重构误差 e_k

绘制 e_k 曲线, 选取拐点:
    k=1:  e=10.5  ┃
    k=2:  e=5.2   ┃╲
    k=4:  e=2.8   ┃ ╲
    k=8:  e=1.5   ┃  ╲___  ← 肘部点
    k=16: e=1.3   ┃      ╲
    k=32: e=1.2   ┃       ─
    k=64: e=1.15  ┃        ─
```

## 4. Huffman 编码

对量化后的权重进行无损熵编码。

```c
HuffmanTable* huff = model_huffman_build(buf->data, buf->count, 256);
float huff_ratio = model_huffman_compression_ratio(huff);
printf("Huffman ratio: %.2fx\n", (double)huff_ratio);

int32_t encoded_size;
uint32_t* encoded = model_huffman_encode(huff, buf->data, buf->count, &encoded_size);
float* decoded = model_huffman_decode(huff, encoded, encoded_size);

model_huffman_free(huff);
free(encoded);
free(decoded);
```

**熵编码原理**:

```
频率高的符号 → 短编码
频率低的符号 → 长编码

例如权重分布:
  -0.1: 出现 300 次 → 编码: "0"  (1 bit)
   0.0: 出现 500 次 → 编码: "10" (2 bit)
   0.1: 出现 150 次 → 编码: "110" (3 bit)
   0.5: 出现 50 次  → 编码: "111" (3 bit)

平均每个权重: (300*1 + 500*2 + 150*3 + 50*3) / 1000 = 1.9 bits
原始: 32 bits per weight → 压缩比: 32/1.9 ≈ 16.8x

注意: 实际压缩比受权值分布影响
```

## 5. 压缩流水线 (Pipeline)

### 单方法压缩

```c
ModelCompressionStats* stats;

// 量化
stats = model_compress_pipeline(buf, MODEL_COMPRESS_METHOD_QUANTIZE);
model_compression_stats_print(stats);
model_compression_stats_free(stats);

// 剪枝
stats = model_compress_pipeline(buf, MODEL_COMPRESS_METHOD_PRUNE);
model_compression_stats_print(stats);
model_compression_stats_free(stats);

// 聚类
stats = model_compress_pipeline(buf, MODEL_COMPRESS_METHOD_CLUSTER);
model_compression_stats_print(stats);
model_compression_stats_free(stats);
```

### 组合压缩 (剪枝 + 量化)

```c
// 先剪枝 50%，再生产量化
ModelCompressionStats* combined = model_compress_combined(
    buf, 0.50f, MODEL_COMPRESS_QUANT_INT8);
model_compression_stats_print(combined);
model_compression_stats_free(combined);
```

### 批量基准测试

```c
ModelCompressionStats* bench = model_compress_benchmark(buf);
model_compression_stats_print(bench);
model_compression_stats_free(bench);
```

## 压缩比计算

```c
float original = 1000.0f;    // 1000 bytes (FP32)
float compressed = 250.0f;   // 250 bytes (quantized)

float ratio = model_compression_ratio_calc(original, compressed);
// ratio = 1000 / 250 = 4.00x
```

```c
// 统计信息
typedef struct {
    ModelCompressMethod method;
    float original_size_bytes;
    float compressed_size_bytes;
    float compression_ratio;        // 压缩比
    float size_reduction_percent;   // 缩小百分比
    float accuracy_loss_percent;    // 精度损失百分比
    char description[256];
} ModelCompressionStats;
```

## 实践建议

### 典型压缩流水线

```
原始 FP32 模型 (100%)
    │
    ▼
[结构化剪枝 30%]  → 70% 大小, ~0.5% 精度损失
    │
    ▼
[INT8 量化]       → 17.5% 大小 (原始), ~1% 额外精度损失
    │
    ▼
[Huffman 编码]    → 12% 大小 (原始), 无额外损失
    │
    ▼
最终模型: ~12% 原始大小, ~1.5% 精度损失, ~3x 推理加速
```

### 不同场景策略

| 场景 | 推荐策略 |
|------|---------|
| **MCU, < 256KB Flash** | INT8 量化 + 剪枝 (50-80% 稀疏度) |
| **MCU, < 512KB Flash** | INT8 量化 + 聚类 (k=16-64) |
| **NPU/DSP 推理** | 结构化剪枝 + INT8 量化 |
| **精度关键** | 仅 INT16 量化, 不剪枝 |
| **极致压缩** | 剪枝 → 量化 → 聚类 → Huffman |

### 权重分析

```c
ModelWeightBuffer* buf = model_weight_buffer_create(weights, count);

// 分析权重分布
model_weight_buffer_stats(buf);
printf("Range: [%.4f, %.4f]\n", (double)buf->min_val, (double)buf->max_val);
printf("Mean: %.4f, StdDev: %.4f\n", (double)buf->mean, (double)buf->stddev);

// 根据分布选择策略:
// - 标准差大 (>1.0): 适合聚类
// - 标准差小 (<0.1): 适合大幅度剪枝
// - 接近正态分布: 量化效果好
// - 长尾分布: Huffman 压缩效果好
```

## 嵌入式部署清单

- [ ] 确定 Flash/RAM 预算
- [ ] 选择压缩方法组合
- [ ] 运行基准测试评估精度损失
- [ ] 量化为 INT8 并验证推理正确性
- [ ] (可选) 应用 Huffman 进一步压缩
- [ ] 生成压缩后模型
- [ ] 编写解压/反量化代码 (如需要)
- [ ] 部署到设备并验证输出

## 常见问题

**Q: 量化后模型精度大幅下降怎么办？**
A: 尝试量化感知训练 (QAT) 或使用 INT16 量化。

**Q: 剪枝后推理速度没有提升？**
A: 需要稀疏矩阵乘法 (SpMM) 支持。结构化剪枝更容易加速。

**Q: 聚类中心数 k 如何选择？**
A: 使用肘部方法或尝试 k ∈ {8, 16, 32, 64, 128}，选取误差拐点。

**Q: 可以同时使用所有压缩方法吗？**
A: 可以，推荐顺序: 剪枝 → 量化 → 聚类 → Huffman。

## 参考

- S. Han et al., "Learning both Weights and Connections for Efficient Neural Networks", NeurIPS 2015
- B. Jacob et al., "Quantization and Training of Neural Networks for Efficient Integer-Arithmetic-Only Inference", CVPR 2018
- Y. Gong et al., "Compressing Deep Convolutional Networks using Vector Quantization", arXiv 2014
- A. Gholami et al., "A Survey of Quantization Methods for Efficient Neural Network Inference", arXiv 2021
