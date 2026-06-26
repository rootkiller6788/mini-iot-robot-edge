#include "tflite_micro.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TFLiteTensorArena* tflite_tensor_arena_create(size_t arena_size)
{
    TFLiteTensorArena* arena = (TFLiteTensorArena*)calloc(1, sizeof(TFLiteTensorArena));
    if (!arena) return NULL;
    arena->arena = (uint8_t*)calloc(1, arena_size);
    if (!arena->arena) {
        free(arena);
        return NULL;
    }
    arena->arena_size = arena_size;
    arena->used_bytes = 0;
    return arena;
}

void tflite_tensor_arena_free(TFLiteTensorArena* arena)
{
    if (!arena) return;
    free(arena->arena);
    free(arena);
}

void* tflite_tensor_arena_alloc(TFLiteTensorArena* arena, size_t bytes)
{
    if (!arena || bytes == 0) return NULL;
    size_t aligned = (bytes + 15) & ~15UL;
    if (arena->used_bytes + aligned > arena->arena_size) return NULL;
    void* ptr = arena->arena + arena->used_bytes;
    arena->used_bytes += aligned;
    return ptr;
}

void tflite_tensor_arena_reset(TFLiteTensorArena* arena)
{
    if (!arena) return;
    arena->used_bytes = 0;
    memset(arena->arena, 0, arena->arena_size);
}

size_t tflite_tensor_arena_available(const TFLiteTensorArena* arena)
{
    if (!arena) return 0;
    return arena->arena_size - arena->used_bytes;
}

size_t tflite_tensor_arena_used(const TFLiteTensorArena* arena)
{
    if (!arena) return 0;
    return arena->used_bytes;
}

TFLiteTensor* tflite_tensor_create(void)
{
    TFLiteTensor* tensor = (TFLiteTensor*)calloc(1, sizeof(TFLiteTensor));
    return tensor;
}

void tflite_tensor_free(TFLiteTensor* tensor)
{
    if (!tensor) return;
    free(tensor->data);
    free(tensor);
}

static size_t tflite_tensor_type_size(TFLiteTensorType type)
{
    switch (type) {
    case TFLITE_TYPE_FLOAT32: return 4;
    case TFLITE_TYPE_INT8:
    case TFLITE_TYPE_UINT8:
    case TFLITE_TYPE_BOOL:    return 1;
    case TFLITE_TYPE_INT32:   return 4;
    case TFLITE_TYPE_INT16:   return 2;
    default:                  return 0;
    }
}

TFLiteStatus tflite_tensor_alloc_data(TFLiteTensor* tensor, TFLiteTensorType type, const int32_t* dims, int32_t num_dims)
{
    if (!tensor || !dims || num_dims <= 0 || num_dims > TFLITE_MICRO_MAX_DIMENSIONS)
        return TFLITE_STATUS_INVALID_HANDLE;
    tensor->type = type;
    tensor->num_dims = num_dims;
    size_t count = 1;
    for (int32_t i = 0; i < num_dims; i++) {
        tensor->dims[i] = dims[i];
        count *= (size_t)dims[i];
    }
    tensor->bytes = count * tflite_tensor_type_size(type);
    tensor->data = calloc(1, tensor->bytes);
    if (!tensor->data) return TFLITE_STATUS_ALLOCATION_FAILED;
    return TFLITE_STATUS_OK;
}

TFLiteStatus tflite_tensor_copy_float_data(TFLiteTensor* dst, const float* data, size_t count)
{
    if (!dst || !data) return TFLITE_STATUS_INVALID_HANDLE;
    if (dst->type != TFLITE_TYPE_FLOAT32) return TFLITE_STATUS_TENSOR_MISMATCH;
    size_t elms = tflite_tensor_element_count(dst);
    size_t copy_n = count < elms ? count : elms;
    memcpy(dst->data, data, copy_n * sizeof(float));
    return TFLITE_STATUS_OK;
}

TFLiteStatus tflite_tensor_get_float(const TFLiteTensor* tensor, float* out, size_t count)
{
    if (!tensor || !out) return TFLITE_STATUS_INVALID_HANDLE;
    if (tensor->type != TFLITE_TYPE_FLOAT32) return TFLITE_STATUS_TENSOR_MISMATCH;
    size_t elms = tflite_tensor_element_count(tensor);
    size_t copy_n = count < elms ? count : elms;
    memcpy(out, tensor->data, copy_n * sizeof(float));
    return TFLITE_STATUS_OK;
}

size_t tflite_tensor_element_count(const TFLiteTensor* tensor)
{
    if (!tensor || tensor->num_dims <= 0) return 0;
    size_t count = 1;
    for (int32_t i = 0; i < tensor->num_dims; i++) {
        count *= (size_t)tensor->dims[i];
    }
    return count;
}

MicroMutableOpResolver* tflite_micro_op_resolver_create(void)
{
    MicroMutableOpResolver* resolver = (MicroMutableOpResolver*)calloc(1, sizeof(MicroMutableOpResolver));
    return resolver;
}

void tflite_micro_op_resolver_free(MicroMutableOpResolver* resolver)
{
    free(resolver);
}

TFLiteOpResolveStatus tflite_micro_op_resolver_add_builtin(
    MicroMutableOpResolver* resolver, const char* op_name,
    int32_t op_code, int32_t min_version, int32_t max_version)
{
    if (!resolver || !op_name) return TFLITE_OP_RESOLVE_NOT_FOUND;
    if (resolver->num_entries >= TFLITE_MICRO_OP_RESOLVER_MAX_OPS)
        return TFLITE_OP_RESOLVE_NOT_FOUND;
    TFLiteOpEntry* entry = &resolver->entries[resolver->num_entries];
    strncpy(entry->op_name, op_name, TFLITE_MICRO_MAX_OP_NAME_LEN - 1);
    entry->op_name[TFLITE_MICRO_MAX_OP_NAME_LEN - 1] = '\0';
    entry->op_code = op_code;
    entry->min_version = min_version;
    entry->max_version = max_version;
    entry->is_builtin = true;
    resolver->num_entries++;
    return TFLITE_OP_RESOLVE_OK;
}

TFLiteOpResolveStatus tflite_micro_op_resolver_resolve(
    const MicroMutableOpResolver* resolver, int32_t op_code,
    int32_t version, TFLiteOpEntry* out_entry)
{
    if (!resolver || !out_entry) return TFLITE_OP_RESOLVE_NOT_FOUND;
    for (int32_t i = 0; i < resolver->num_entries; i++) {
        if (resolver->entries[i].op_code == op_code) {
            if (version < resolver->entries[i].min_version)
                return TFLITE_OP_RESOLVE_VERSION_TOO_OLD;
            if (version > resolver->entries[i].max_version)
                return TFLITE_OP_RESOLVE_VERSION_TOO_NEW;
            *out_entry = resolver->entries[i];
            return TFLITE_OP_RESOLVE_OK;
        }
    }
    return TFLITE_OP_RESOLVE_NOT_FOUND;
}

TFLiteInterpreter* tflite_interpreter_create(void)
{
    TFLiteInterpreter* interpreter = (TFLiteInterpreter*)calloc(1, sizeof(TFLiteInterpreter));
    return interpreter;
}

void tflite_interpreter_free(TFLiteInterpreter* interpreter)
{
    if (!interpreter) return;
    for (int32_t i = 0; i < interpreter->num_tensors; i++) {
        tflite_tensor_free(interpreter->tensors[i]);
    }
    free(interpreter);
}

TFLiteStatus tflite_interpreter_init(TFLiteInterpreter* interpreter,
    const uint8_t* model_data, size_t model_size,
    MicroMutableOpResolver* resolver, TFLiteTensorArena* arena)
{
    if (!interpreter || !model_data || !resolver || !arena)
        return TFLITE_STATUS_INVALID_HANDLE;
    interpreter->model_data = model_data;
    interpreter->model_size = model_size;
    interpreter->resolver = resolver;
    interpreter->arena = arena;
    interpreter->is_allocated = false;
    interpreter->is_invoked = false;
    interpreter->model_version = 1;
    interpreter->input_tensor_index = 0;
    interpreter->output_tensor_index = 1;
    interpreter->subgraph_index = 0;
    return TFLITE_STATUS_OK;
}

TFLiteStatus tflite_interpreter_allocate_tensors(TFLiteInterpreter* interpreter)
{
    if (!interpreter) return TFLITE_STATUS_INVALID_HANDLE;
    tflite_tensor_arena_reset(interpreter->arena);

    TFLiteTensor* input_tensor = tflite_tensor_create();
    if (!input_tensor) return TFLITE_STATUS_ALLOCATION_FAILED;
    int32_t input_dims[] = {1, 28, 28, 1};
    TFLiteStatus s = tflite_tensor_alloc_data(input_tensor, TFLITE_TYPE_FLOAT32, input_dims, 4);
    if (s != TFLITE_STATUS_OK) { tflite_tensor_free(input_tensor); return s; }

    TFLiteTensor* output_tensor = tflite_tensor_create();
    if (!output_tensor) { tflite_tensor_free(input_tensor); return TFLITE_STATUS_ALLOCATION_FAILED; }
    int32_t output_dims[] = {1, 10};
    s = tflite_tensor_alloc_data(output_tensor, TFLITE_TYPE_FLOAT32, output_dims, 2);
    if (s != TFLITE_STATUS_OK) {
        tflite_tensor_free(input_tensor);
        tflite_tensor_free(output_tensor);
        return s;
    }

    interpreter->tensors[0] = input_tensor;
    interpreter->tensors[1] = output_tensor;
    interpreter->num_tensors = 2;
    interpreter->is_allocated = true;
    interpreter->input_tensor_index = 0;
    interpreter->output_tensor_index = 1;
    return TFLITE_STATUS_OK;
}

static float tflite_hard_swish(float x)
{
    return x * fmaxf(0.0f, fminf(1.0f, x / 6.0f + 0.5f));
}

static float tflite_sigmoid(float x)
{
    return 1.0f / (1.0f + expf(-x));
}

TFLiteStatus tflite_interpreter_invoke(TFLiteInterpreter* interpreter)
{
    if (!interpreter) return TFLITE_STATUS_INVALID_HANDLE;
    if (!interpreter->is_allocated) return TFLITE_STATUS_ERROR;
    TFLiteTensor* input  = interpreter->tensors[interpreter->input_tensor_index];
    TFLiteTensor* output = interpreter->tensors[interpreter->output_tensor_index];
    if (!input || !output) return TFLITE_STATUS_ERROR;

    size_t in_count  = tflite_tensor_element_count(input);
    size_t out_count = tflite_tensor_element_count(output);
    float* in_data  = (float*)input->data;
    float* out_data = (float*)output->data;

    for (size_t i = 0; i < out_count; i++) {
        float sum = 0.0f;
        for (size_t j = 0; j < in_count; j++) {
            float w = 0.001f * (float)((i * in_count + j) % 37);
            sum += in_data[j] * w;
        }
        out_data[i] = tflite_sigmoid(sum);
    }
    interpreter->is_invoked = true;
    return TFLITE_STATUS_OK;
}

TFLiteTensor* tflite_interpreter_input_tensor(TFLiteInterpreter* interpreter, int32_t index)
{
    if (!interpreter || index < 0 || index >= interpreter->num_tensors)
        return NULL;
    return interpreter->tensors[interpreter->input_tensor_index + index];
}

TFLiteTensor* tflite_interpreter_output_tensor(TFLiteInterpreter* interpreter, int32_t index)
{
    if (!interpreter || index < 0 || index >= interpreter->num_tensors)
        return NULL;
    return interpreter->tensors[interpreter->output_tensor_index + index];
}

int32_t tflite_interpreter_input_count(const TFLiteInterpreter* interpreter)
{
    if (!interpreter) return 0;
    return 1;
}

int32_t tflite_interpreter_output_count(const TFLiteInterpreter* interpreter)
{
    if (!interpreter) return 0;
    return 1;
}

TFLiteStatus tflite_interpreter_reset(TFLiteInterpreter* interpreter)
{
    if (!interpreter) return TFLITE_STATUS_INVALID_HANDLE;
    for (int32_t i = 0; i < interpreter->num_tensors; i++) {
        if (interpreter->tensors[i] && interpreter->tensors[i]->data) {
            memset(interpreter->tensors[i]->data, 0, interpreter->tensors[i]->bytes);
        }
    }
    interpreter->is_invoked = false;
    return TFLITE_STATUS_OK;
}

TFLiteModel* tflite_model_create(const uint8_t* data, size_t size)
{
    if (!data || size == 0) return NULL;
    TFLiteModel* model = (TFLiteModel*)calloc(1, sizeof(TFLiteModel));
    if (!model) return NULL;
    model->data = data;
    model->size = size;
    model->model_version = 3;
    snprintf(model->description, sizeof(model->description),
             "TFLite flatbuffers model (%zu bytes)", size);
    model->is_valid = tflite_model_validate(model);
    return model;
}

void tflite_model_free(TFLiteModel* model)
{
    free(model);
}

bool tflite_model_validate(const TFLiteModel* model)
{
    if (!model || !model->data || model->size < 8) return false;
    if (model->size > 256UL * 1024 * 1024) return false;
    return true;
}

const char* tflite_model_description(const TFLiteModel* model)
{
    if (!model) return "null";
    return model->description;
}

const char* tflite_status_to_string(TFLiteStatus status)
{
    switch (status) {
    case TFLITE_STATUS_OK:                return "OK";
    case TFLITE_STATUS_ERROR:             return "Error";
    case TFLITE_STATUS_ALLOCATION_FAILED: return "Allocation failed";
    case TFLITE_STATUS_INVALID_HANDLE:    return "Invalid handle";
    case TFLITE_STATUS_UNSUPPORTED_OP:    return "Unsupported op";
    case TFLITE_STATUS_TENSOR_MISMATCH:   return "Tensor mismatch";
    case TFLITE_STATUS_INVOKE_FAILED:     return "Invoke failed";
    case TFLITE_STATUS_VERSION_MISMATCH:  return "Version mismatch";
    case TFLITE_STATUS_MODEL_INVALID:     return "Model invalid";
    default:                              return "Unknown";
    }
}

const char* tflite_tensor_type_to_string(TFLiteTensorType type)
{
    switch (type) {
    case TFLITE_TYPE_FLOAT32: return "float32";
    case TFLITE_TYPE_INT8:    return "int8";
    case TFLITE_TYPE_UINT8:   return "uint8";
    case TFLITE_TYPE_INT32:   return "int32";
    case TFLITE_TYPE_BOOL:    return "bool";
    case TFLITE_TYPE_INT16:   return "int16";
    default:                  return "unknown";
    }
}
