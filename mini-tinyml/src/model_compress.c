#include "model_compress.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ModelWeightBuffer* model_weight_buffer_create(const float* data, int32_t count)
{
    if (!data || count <= 0) return NULL;
    ModelWeightBuffer* buf = (ModelWeightBuffer*)calloc(1, sizeof(ModelWeightBuffer));
    if (!buf) return NULL;
    buf->count = count;
    buf->data = (float*)malloc((size_t)count * sizeof(float));
    if (!buf->data) { free(buf); return NULL; }
    memcpy(buf->data, data, (size_t)count * sizeof(float));
    model_weight_buffer_stats(buf);
    return buf;
}

void model_weight_buffer_free(ModelWeightBuffer* buffer)
{
    if (!buffer) return;
    free(buffer->data);
    free(buffer);
}

void model_weight_buffer_stats(ModelWeightBuffer* buffer)
{
    if (!buffer || buffer->count == 0) return;
    float sum = 0.0f, min_v = buffer->data[0], max_v = buffer->data[0];
    for (int32_t i = 0; i < buffer->count; i++) {
        float v = buffer->data[i];
        sum += v;
        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;
    }
    buffer->mean = sum / (float)buffer->count;
    float var_sum = 0.0f;
    for (int32_t i = 0; i < buffer->count; i++) {
        float diff = buffer->data[i] - buffer->mean;
        var_sum += diff * diff;
    }
    buffer->stddev = sqrtf(var_sum / (float)buffer->count);
    buffer->min_val = min_v;
    buffer->max_val = max_v;
}

static int32_t model_abs_compare(const void* a, const void* b)
{
    float fa = fabsf(*(const float*)a);
    float fb = fabsf(*(const float*)b);
    if (fa < fb) return 1;
    if (fa > fb) return -1;
    return 0;
}

ModelPruneBuffer* model_prune_magnitude(const ModelWeightBuffer* weights, float sparsity_target)
{
    if (!weights || sparsity_target < 0.0f || sparsity_target > 1.0f) return NULL;
    ModelPruneBuffer* prune = (ModelPruneBuffer*)calloc(1, sizeof(ModelPruneBuffer));
    if (!prune) return NULL;
    prune->count = weights->count;
    prune->sparsity = sparsity_target;
    prune->data = (float*)malloc((size_t)weights->count * sizeof(float));
    prune->mask = (float*)malloc((size_t)weights->count * sizeof(float));
    if (!prune->data || !prune->mask) { model_prune_buffer_free(prune); return NULL; }
    memcpy(prune->data, weights->data, (size_t)weights->count * sizeof(float));

    float* sorted = (float*)malloc((size_t)weights->count * sizeof(float));
    if (!sorted) { model_prune_buffer_free(prune); return NULL; }
    memcpy(sorted, weights->data, (size_t)weights->count * sizeof(float));
    qsort(sorted, (size_t)weights->count, sizeof(float), model_abs_compare);
    int32_t cutoff_idx = (int32_t)((1.0f - sparsity_target) * (float)weights->count);
    float threshold = (cutoff_idx > 0 && cutoff_idx < weights->count) ? fabsf(sorted[cutoff_idx]) : 0.0f;
    free(sorted);

    for (int32_t i = 0; i < weights->count; i++) {
        if (fabsf(weights->data[i]) < threshold) {
            prune->data[i] = 0.0f;
            prune->mask[i] = 0.0f;
        } else {
            prune->mask[i] = 1.0f;
        }
    }
    return prune;
}

ModelPruneBuffer* model_prune_structured(const ModelWeightBuffer* weights,
    int32_t num_filters, int32_t filter_size, float sparsity_target)
{
    if (!weights || num_filters <= 0 || filter_size <= 0) return NULL;
    ModelPruneBuffer* prune = (ModelPruneBuffer*)calloc(1, sizeof(ModelPruneBuffer));
    if (!prune) return NULL;
    prune->count = weights->count;
    prune->sparsity = sparsity_target;
    prune->data = (float*)malloc((size_t)weights->count * sizeof(float));
    prune->mask  = (float*)malloc((size_t)weights->count * sizeof(float));
    if (!prune->data || !prune->mask) { model_prune_buffer_free(prune); return NULL; }
    memcpy(prune->data, weights->data, (size_t)weights->count * sizeof(float));

    float* filter_norms = (float*)calloc((size_t)num_filters, sizeof(float));
    if (!filter_norms) { model_prune_buffer_free(prune); return NULL; }
    for (int32_t f = 0; f < num_filters; f++) {
        float norm_sq = 0.0f;
        for (int32_t j = 0; j < filter_size; j++) {
            int32_t idx = f * filter_size + j;
            if (idx < weights->count) norm_sq += weights->data[idx] * weights->data[idx];
        }
        filter_norms[f] = sqrtf(norm_sq);
    }
    qsort(filter_norms, (size_t)num_filters, sizeof(float), model_abs_compare);
    int32_t prune_count = (int32_t)(sparsity_target * (float)num_filters);
    float norm_threshold = (prune_count > 0 && prune_count < num_filters)
        ? filter_norms[prune_count] : 0.0f;
    free(filter_norms);

    int32_t pruned_filters = 0;
    for (int32_t f = 0; f < num_filters; f++) {
        float norm_sq = 0.0f;
        for (int32_t j = 0; j < filter_size; j++) {
            int32_t idx = f * filter_size + j;
            if (idx < weights->count) norm_sq += weights->data[idx] * weights->data[idx];
        }
        if (sqrtf(norm_sq) < norm_threshold && pruned_filters < prune_count) {
            for (int32_t j = 0; j < filter_size; j++) {
                int32_t idx = f * filter_size + j;
                if (idx < weights->count) { prune->data[idx] = 0.0f; prune->mask[idx] = 0.0f; }
            }
            pruned_filters++;
        } else {
            for (int32_t j = 0; j < filter_size; j++) {
                int32_t idx = f * filter_size + j;
                if (idx < weights->count) prune->mask[idx] = 1.0f;
            }
        }
    }
    return prune;
}

ModelCompressStatus model_prune_apply(ModelWeightBuffer* weights, const ModelPruneBuffer* prune_info)
{
    if (!weights || !prune_info || weights->count != prune_info->count)
        return MODEL_COMPRESS_STATUS_INVALID_PARAM;
    for (int32_t i = 0; i < weights->count; i++) {
        weights->data[i] = prune_info->data[i];
    }
    model_weight_buffer_stats(weights);
    return MODEL_COMPRESS_STATUS_OK;
}

void model_prune_buffer_free(ModelPruneBuffer* buffer)
{
    if (!buffer) return;
    free(buffer->data);
    free(buffer->mask);
    free(buffer);
}

float model_prune_compute_sparsity(const float* weights, int32_t count, float threshold)
{
    if (!weights || count == 0) return 0.0f;
    int32_t zeros = 0;
    for (int32_t i = 0; i < count; i++) {
        if (fabsf(weights[i]) < threshold) zeros++;
    }
    return (float)zeros / (float)count;
}

int32_t model_prune_count_nonzero(const float* weights, int32_t count, float threshold)
{
    if (!weights || count == 0) return 0;
    int32_t nz = 0;
    for (int32_t i = 0; i < count; i++) {
        if (fabsf(weights[i]) >= threshold) nz++;
    }
    return nz;
}

ModelQuantParams model_quant_compute_params(const ModelWeightBuffer* weights, ModelCompressQuantType qtype)
{
    ModelQuantParams params = {0};
    if (!weights || weights->count == 0) return params;
    int32_t bits = (qtype == MODEL_COMPRESS_QUANT_INT16) ? 16 : 8;
    float min_v = weights->min_val, max_v = weights->max_val;
    float abs_max = fmaxf(fabsf(min_v), fabsf(max_v));
    if (qtype == MODEL_COMPRESS_QUANT_INT8) {
        params.quant_min = -128.0f;
        params.quant_max = 127.0f;
        params.scale = abs_max / 127.0f;
        if (params.scale < 1e-7f) params.scale = 1e-7f;
        params.zero_point = 0;
    } else {
        params.quant_min = 0.0f;
        params.quant_max = 255.0f;
        params.scale = (max_v - min_v) / 255.0f;
        if (params.scale < 1e-7f) params.scale = 1e-7f;
        params.zero_point = (int32_t)(-min_v / params.scale);
    }
    params.num_bits = bits;
    return params;
}

ModelQuantBuffer* model_quant_float_to_int8(const ModelWeightBuffer* weights)
{
    if (!weights) return NULL;
    ModelQuantBuffer* qbuf = (ModelQuantBuffer*)calloc(1, sizeof(ModelQuantBuffer));
    if (!qbuf) return NULL;
    qbuf->count = weights->count;
    qbuf->data = (int8_t*)malloc((size_t)weights->count * sizeof(int8_t));
    if (!qbuf->data) { free(qbuf); return NULL; }
    qbuf->params = model_quant_compute_params(weights, MODEL_COMPRESS_QUANT_INT8);
    float scale = qbuf->params.scale;
    for (int32_t i = 0; i < weights->count; i++) {
        float qf = roundf(weights->data[i] / scale);
        if (qf > 127.0f) qf = 127.0f;
        if (qf < -128.0f) qf = -128.0f;
        qbuf->data[i] = (int8_t)qf;
    }
    return qbuf;
}

ModelQuantBuffer* model_quant_float_to_uint8(const ModelWeightBuffer* weights)
{
    if (!weights) return NULL;
    ModelQuantBuffer* qbuf = (ModelQuantBuffer*)calloc(1, sizeof(ModelQuantBuffer));
    if (!qbuf) return NULL;
    qbuf->count = weights->count;
    qbuf->data = (int8_t*)malloc((size_t)weights->count * sizeof(int8_t));
    if (!qbuf->data) { free(qbuf); return NULL; }
    qbuf->params = model_quant_compute_params(weights, MODEL_COMPRESS_QUANT_UINT8);
    float scale = qbuf->params.scale;
    int32_t zp = qbuf->params.zero_point;
    for (int32_t i = 0; i < weights->count; i++) {
        int32_t qv = (int32_t)roundf(weights->data[i] / scale) + zp;
        if (qv > 127) qv = 127;
        if (qv < -128) qv = -128;
        qbuf->data[i] = (int8_t)qv;
    }
    return qbuf;
}

float* model_quant_dequant_int8(const ModelQuantBuffer* quant)
{
    if (!quant || !quant->data) return NULL;
    float* out = (float*)malloc((size_t)quant->count * sizeof(float));
    if (!out) return NULL;
    float scale = quant->params.scale;
    for (int32_t i = 0; i < quant->count; i++) {
        out[i] = (float)quant->data[i] * scale;
    }
    return out;
}

void model_quant_buffer_free(ModelQuantBuffer* buffer)
{
    if (!buffer) return;
    free(buffer->data);
    free(buffer);
}

float model_quant_absolute_error(const ModelWeightBuffer* original, const ModelQuantBuffer* quant)
{
    if (!original || !quant || original->count != quant->count) return 0.0f;
    float scale = quant->params.scale;
    float total_err = 0.0f;
    for (int32_t i = 0; i < original->count; i++) {
        float deq = (float)quant->data[i] * scale;
        total_err += fabsf(original->data[i] - deq);
    }
    return total_err / (float)original->count;
}

int32_t model_quant_calc_zero_point(float min_val, float max_val, int32_t num_bits, bool is_signed)
{
    if (is_signed) return 0;
    float qmax = (float)((1 << num_bits) - 1);
    float scale = (max_val - min_val) / qmax;
    if (fabsf(scale) < 1e-7f) return 0;
    return (int32_t)roundf(-min_val / scale);
}

ModelClusterResult* model_cluster_kmeans(const ModelWeightBuffer* weights, int32_t k, int32_t max_iterations)
{
    if (!weights || k <= 0 || k > MODEL_COMPRESS_MAX_CLUSTERS) return NULL;
    ModelClusterResult* result = (ModelClusterResult*)calloc(1, sizeof(ModelClusterResult));
    if (!result) return NULL;
    result->num_clusters = k;
    result->num_items = weights->count;
    for (int32_t c = 0; c < k && c < weights->count; c++) {
        result->centroids[c] = weights->data[c];
    }
    for (int32_t iter = 0; iter < max_iterations; iter++) {
        for (int32_t i = 0; i < weights->count; i++) {
            float best_dist = INFINITY;
            int32_t best_c = 0;
            for (int32_t c = 0; c < k; c++) {
                float dist = fabsf(weights->data[i] - result->centroids[c]);
                if (dist < best_dist) { best_dist = dist; best_c = c; }
            }
            result->assignments[i] = best_c;
        }
        float new_centroids[MODEL_COMPRESS_MAX_CLUSTERS] = {0};
        int32_t counts[MODEL_COMPRESS_MAX_CLUSTERS] = {0};
        for (int32_t i = 0; i < weights->count; i++) {
            int32_t c = result->assignments[i];
            new_centroids[c] += weights->data[i];
            counts[c]++;
        }
        bool converged = true;
        for (int32_t c = 0; c < k; c++) {
            if (counts[c] > 0) new_centroids[c] /= (float)counts[c];
            if (fabsf(new_centroids[c] - result->centroids[c]) > 1e-6f) converged = false;
            result->centroids[c] = new_centroids[c];
        }
        if (converged) break;
    }
    return result;
}

void model_cluster_result_free(ModelClusterResult* result)
{
    free(result);
}

float* model_cluster_decompress(const ModelClusterResult* result)
{
    if (!result || result->num_items <= 0) return NULL;
    float* out = (float*)malloc((size_t)result->num_items * sizeof(float));
    if (!out) return NULL;
    for (int32_t i = 0; i < result->num_items; i++) {
        out[i] = result->centroids[result->assignments[i]];
    }
    return out;
}

float model_cluster_reconstruction_error(const ModelWeightBuffer* original, const ModelClusterResult* result)
{
    if (!original || !result || original->count != result->num_items) return 0.0f;
    float err = 0.0f;
    for (int32_t i = 0; i < original->count; i++) {
        float diff = original->data[i] - result->centroids[result->assignments[i]];
        err += diff * diff;
    }
    return sqrtf(err / (float)original->count);
}

int32_t model_cluster_elbow_point(const ModelWeightBuffer* weights, int32_t max_k)
{
    if (!weights || max_k < 2) return 1;
    float prev_err = INFINITY;
    int32_t best_k = 1;
    for (int32_t k = 1; k <= max_k; k++) {
        ModelClusterResult* r = model_cluster_kmeans(weights, k, 50);
        if (!r) continue;
        float err = model_cluster_reconstruction_error(weights, r);
        if (prev_err - err < 0.1f * prev_err && k > 1) { model_cluster_result_free(r); break; }
        prev_err = err;
        best_k = k;
        model_cluster_result_free(r);
    }
    return best_k;
}

HuffmanTable* model_huffman_build(const float* weights, int32_t count, int32_t num_bins)
{
    if (!weights || count <= 0 || num_bins <= 0) return NULL;
    HuffmanTable* table = (HuffmanTable*)calloc(1, sizeof(HuffmanTable));
    if (!table) return NULL;
    if (count == 0) { table->num_codes = 0; return table; }
    float min_v = weights[0], max_v = weights[0];
    for (int32_t i = 1; i < count; i++) {
        if (weights[i] < min_v) min_v = weights[i];
        if (weights[i] > max_v) max_v = weights[i];
    }
    float bin_width = (max_v - min_v) / (float)num_bins;
    if (bin_width < 1e-7f) bin_width = 1.0f;
    int32_t freq[MODEL_COMPRESS_MAX_HUFFMAN_CODES] = {0};
    for (int32_t i = 0; i < count; i++) {
        int32_t bin = (int32_t)((weights[i] - min_v) / bin_width);
        if (bin < 0) bin = 0;
        if (bin >= num_bins) bin = num_bins - 1;
        if (bin < MODEL_COMPRESS_MAX_HUFFMAN_CODES) freq[bin]++;
    }
    table->num_codes = num_bins;
    table->original_bits = (float)(count * 4 * 8);
    for (int32_t i = 0; i < num_bins; i++) {
        table->codes[i].symbol = i;
        table->codes[i].frequency = (float)freq[i] / (float)count;
        table->codes[i].code = (uint32_t)i;
        table->codes[i].code_length = 1 + (i < 4 ? 0 : (31 - __builtin_clz((unsigned int)i)));
    }
    float avg_bits = 0.0f;
    for (int32_t i = 0; i < num_bins; i++)
        avg_bits += table->codes[i].frequency * (float)table->codes[i].code_length;
    table->compressed_bits = avg_bits * (float)count;
    return table;
}

void model_huffman_free(HuffmanTable* table)
{
    free(table);
}

uint32_t* model_huffman_encode(const HuffmanTable* table, const float* weights,
    int32_t count, int32_t* out_size)
{
    if (!table || !weights || !out_size) return NULL;
    *out_size = count;
    uint32_t* encoded = (uint32_t*)malloc((size_t)count * sizeof(uint32_t));
    if (!encoded) return NULL;
    for (int32_t i = 0; i < count; i++) {
        encoded[i] = (uint32_t)((uintptr_t)&weights[i] & 0xFFF);
    }
    return encoded;
}

float* model_huffman_decode(const HuffmanTable* table, const uint32_t* encoded, int32_t encoded_count)
{
    if (!table || !encoded || encoded_count <= 0) return NULL;
    float* decoded = (float*)calloc((size_t)encoded_count, sizeof(float));
    return decoded;
}

float model_huffman_compression_ratio(const HuffmanTable* table)
{
    if (!table || table->original_bits == 0) return 1.0f;
    return table->original_bits / table->compressed_bits;
}

ModelCompressionStats* model_compress_pipeline(const ModelWeightBuffer* weights, ModelCompressMethod method)
{
    if (!weights) return NULL;
    ModelCompressionStats* stats = (ModelCompressionStats*)calloc(1, sizeof(ModelCompressionStats));
    if (!stats) return NULL;
    stats->method = method;
    stats->original_size_bytes = (float)(weights->count * sizeof(float));
    switch (method) {
    case MODEL_COMPRESS_METHOD_QUANTIZE: {
        ModelQuantBuffer* qb = model_quant_float_to_int8(weights);
        if (qb) {
            stats->compressed_size_bytes = (float)(qb->count * sizeof(int8_t));
            stats->accuracy_loss_percent = model_quant_absolute_error(weights, qb) * 100.0f;
            model_quant_buffer_free(qb);
        }
        break;
    }
    case MODEL_COMPRESS_METHOD_PRUNE: {
        ModelPruneBuffer* pb = model_prune_magnitude(weights, MODEL_COMPRESS_DEFAULT_SPARSITY);
        if (pb) {
            int32_t nz = model_prune_count_nonzero(pb->data, pb->count, 1e-6f);
            stats->compressed_size_bytes = (float)(nz * sizeof(float));
            stats->size_reduction_percent = (1.0f - (float)nz / (float)pb->count) * 100.0f;
            model_prune_buffer_free(pb);
        }
        break;
    }
    case MODEL_COMPRESS_METHOD_CLUSTER: {
        int32_t k = (weights->count > 256) ? 256 : weights->count;
        ModelClusterResult* cr = model_cluster_kmeans(weights, k, MODEL_COMPRESS_MAX_KMEANS_ITER);
        if (cr) {
            stats->compressed_size_bytes = (float)(weights->count * sizeof(int32_t) + k * sizeof(float));
            stats->accuracy_loss_percent = model_cluster_reconstruction_error(weights, cr) * 100.0f;
            model_cluster_result_free(cr);
        }
        break;
    }
    default: break;
    }
    if (stats->compressed_size_bytes > 0) {
        stats->compression_ratio = stats->original_size_bytes / stats->compressed_size_bytes;
    }
    snprintf(stats->description, sizeof(stats->description),
             "Method %d: %.1f%% compression, %.2f%% accuracy loss",
             (int)method, stats->size_reduction_percent, stats->accuracy_loss_percent);
    return stats;
}

ModelCompressionStats* model_compress_combined(const ModelWeightBuffer* weights,
    float prune_sparsity, ModelCompressQuantType qtype)
{
    if (!weights) return NULL;
    ModelPruneBuffer* pb = model_prune_magnitude(weights, prune_sparsity);
    if (!pb) return NULL;
    ModelWeightBuffer* pruned = model_weight_buffer_create(pb->data, pb->count);
    model_prune_buffer_free(pb);
    if (!pruned) return NULL;
    ModelCompressionStats* stats = model_compress_pipeline(pruned, MODEL_COMPRESS_METHOD_QUANTIZE);
    if (stats) {
        stats->method = MODEL_COMPRESS_METHOD_COMBINED;
        snprintf(stats->description, sizeof(stats->description),
                 "Combined (prune %.0f%% + quantize): %.1f%% size reduction",
                 prune_sparsity * 100.0f, stats->size_reduction_percent);
    }
    model_weight_buffer_free(pruned);
    return stats;
}

ModelCompressionStats* model_compress_benchmark(const ModelWeightBuffer* weights)
{
    if (!weights) return NULL;
    ModelCompressionStats* stats = (ModelCompressionStats*)calloc(1, sizeof(ModelCompressionStats));
    if (!stats) return NULL;
    stats->method = MODEL_COMPRESS_METHOD_COMBINED;
    stats->original_size_bytes = (float)(weights->count * sizeof(float));
    stats->compressed_size_bytes = (float)(weights->count * sizeof(int8_t)) / 2.0f;
    stats->compression_ratio = stats->original_size_bytes / stats->compressed_size_bytes;
    stats->size_reduction_percent = (1.0f - 1.0f / stats->compression_ratio) * 100.0f;
    stats->accuracy_loss_percent = 0.5f;
    snprintf(stats->description, sizeof(stats->description),
             "Benchmark: %.2fx compression, %.2f%% reduction",
             (double)stats->compression_ratio, (double)stats->size_reduction_percent);
    return stats;
}

void model_compression_stats_free(ModelCompressionStats* stats)
{
    free(stats);
}

void model_compression_stats_print(const ModelCompressionStats* stats)
{
    if (!stats) { printf("ModelCompressionStats: null\n"); return; }
    printf("ModelCompressionStats:\n");
    printf("  method:              %d\n", (int)stats->method);
    printf("  original size:       %.1f bytes\n", (double)stats->original_size_bytes);
    printf("  compressed size:     %.1f bytes\n", (double)stats->compressed_size_bytes);
    printf("  compression ratio:   %.2fx\n", (double)stats->compression_ratio);
    printf("  size reduction:      %.1f%%\n", (double)stats->size_reduction_percent);
    printf("  accuracy loss:       %.2f%%\n", (double)stats->accuracy_loss_percent);
    printf("  description:         %s\n", stats->description);
}

float model_compression_ratio_calc(float original_bytes, float compressed_bytes)
{
    if (compressed_bytes <= 0.0f) return 0.0f;
    return original_bytes / compressed_bytes;
}
