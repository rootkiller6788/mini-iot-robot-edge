#ifndef MODEL_COMPRESS_H
#define MODEL_COMPRESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODEL_COMPRESS_MAX_WEIGHTS         (1024 * 1024)
#define MODEL_COMPRESS_MAX_CLUSTERS        256
#define MODEL_COMPRESS_MAX_HUFFMAN_CODES   4096
#define MODEL_COMPRESS_MAX_KMEANS_ITER     100
#define MODEL_COMPRESS_MAX_LAYERS          128
#define MODEL_COMPRESS_MAX_FILTERS         1024
#define MODEL_COMPRESS_DEFAULT_SPARSITY    0.50f

typedef enum {
    MODEL_COMPRESS_STATUS_OK = 0,
    MODEL_COMPRESS_STATUS_ERROR,
    MODEL_COMPRESS_STATUS_INVALID_PARAM,
    MODEL_COMPRESS_STATUS_MEMORY,
    MODEL_COMPRESS_STATUS_CONVERGENCE,
    MODEL_COMPRESS_STATUS_EMPTY_BUFFER
} ModelCompressStatus;

typedef enum {
    MODEL_COMPRESS_QUANT_INT8 = 0,
    MODEL_COMPRESS_QUANT_UINT8,
    MODEL_COMPRESS_QUANT_INT16
} ModelCompressQuantType;

typedef enum {
    MODEL_COMPRESS_PRUNE_MAGNITUDE = 0,
    MODEL_COMPRESS_PRUNE_STRUCTURED,
    MODEL_COMPRESS_PRUNE_RANDOM
} ModelCompressPruneType;

typedef enum {
    MODEL_COMPRESS_METHOD_QUANTIZE = 0,
    MODEL_COMPRESS_METHOD_PRUNE,
    MODEL_COMPRESS_METHOD_CLUSTER,
    MODEL_COMPRESS_METHOD_HUFFMAN,
    MODEL_COMPRESS_METHOD_COMBINED
} ModelCompressMethod;

typedef struct {
    float*   data;
    int32_t  count;
    float    min_val;
    float    max_val;
    float    mean;
    float    stddev;
} ModelWeightBuffer;

typedef struct {
    float    scale;
    int32_t  zero_point;
    float    quant_min;
    float    quant_max;
    int32_t  num_bits;
} ModelQuantParams;

typedef struct {
    int8_t*  data;
    int32_t  count;
    ModelQuantParams params;
} ModelQuantBuffer;

typedef struct {
    float*   data;
    int32_t  count;
    float    sparsity;
    float*   mask;
} ModelPruneBuffer;

typedef struct {
    float    centroids[MODEL_COMPRESS_MAX_CLUSTERS];
    int32_t  assignments[MODEL_COMPRESS_MAX_WEIGHTS];
    int32_t  num_clusters;
    int32_t  num_items;
} ModelClusterResult;

typedef struct {
    int32_t   symbol;
    uint32_t  code;
    int32_t   code_length;
    float     frequency;
} HuffmanCode;

typedef struct {
    HuffmanCode codes[MODEL_COMPRESS_MAX_HUFFMAN_CODES];
    int32_t     num_codes;
    float       original_bits;
    float       compressed_bits;
} HuffmanTable;

typedef struct {
    ModelCompressMethod method;
    float               original_size_bytes;
    float               compressed_size_bytes;
    float               compression_ratio;
    float               size_reduction_percent;
    float               accuracy_loss_percent;
    char                description[256];
} ModelCompressionStats;

ModelWeightBuffer*     model_weight_buffer_create(const float* data, int32_t count);
void                   model_weight_buffer_free(ModelWeightBuffer* buffer);
void                   model_weight_buffer_stats(ModelWeightBuffer* buffer);

ModelPruneBuffer*      model_prune_magnitude(const ModelWeightBuffer* weights, float sparsity_target);
ModelPruneBuffer*      model_prune_structured(const ModelWeightBuffer* weights, int32_t num_filters, int32_t filter_size, float sparsity_target);
ModelCompressStatus    model_prune_apply(ModelWeightBuffer* weights, const ModelPruneBuffer* prune_info);
void                   model_prune_buffer_free(ModelPruneBuffer* buffer);
float                  model_prune_compute_sparsity(const float* weights, int32_t count, float threshold);
int32_t                model_prune_count_nonzero(const float* weights, int32_t count, float threshold);

ModelQuantParams       model_quant_compute_params(const ModelWeightBuffer* weights, ModelCompressQuantType qtype);
ModelQuantBuffer*      model_quant_float_to_int8(const ModelWeightBuffer* weights);
ModelQuantBuffer*      model_quant_float_to_uint8(const ModelWeightBuffer* weights);
float*                 model_quant_dequant_int8(const ModelQuantBuffer* quant);
void                   model_quant_buffer_free(ModelQuantBuffer* buffer);
float                  model_quant_absolute_error(const ModelWeightBuffer* original, const ModelQuantBuffer* quant);
int32_t                model_quant_calc_zero_point(float min_val, float max_val, int32_t num_bits, bool is_signed);

ModelClusterResult*    model_cluster_kmeans(const ModelWeightBuffer* weights, int32_t k, int32_t max_iterations);
void                   model_cluster_result_free(ModelClusterResult* result);
float*                 model_cluster_decompress(const ModelClusterResult* result);
float                  model_cluster_reconstruction_error(const ModelWeightBuffer* original, const ModelClusterResult* result);
int32_t                model_cluster_elbow_point(const ModelWeightBuffer* weights, int32_t max_k);

HuffmanTable*          model_huffman_build(const float* weights, int32_t count, int32_t num_bins);
void                   model_huffman_free(HuffmanTable* table);
uint32_t*              model_huffman_encode(const HuffmanTable* table, const float* weights, int32_t count, int32_t* out_size);
float*                 model_huffman_decode(const HuffmanTable* table, const uint32_t* encoded, int32_t encoded_count);
float                  model_huffman_compression_ratio(const HuffmanTable* table);

ModelCompressionStats* model_compress_pipeline(const ModelWeightBuffer* weights, ModelCompressMethod method);
ModelCompressionStats* model_compress_combined(const ModelWeightBuffer* weights, float prune_sparsity, ModelCompressQuantType qtype);
ModelCompressionStats* model_compress_benchmark(const ModelWeightBuffer* weights);
void                   model_compression_stats_free(ModelCompressionStats* stats);
void                   model_compression_stats_print(const ModelCompressionStats* stats);
float                  model_compression_ratio_calc(float original_bytes, float compressed_bytes);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_COMPRESS_H */
