#ifndef TFLITE_MICRO_H
#define TFLITE_MICRO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TFLITE_MICRO_MAX_TENSORS            32
#define TFLITE_MICRO_MAX_OPERATORS          64
#define TFLITE_MICRO_MAX_DIMENSIONS          8
#define TFLITE_MICRO_DEFAULT_ARENA_SIZE     (64 * 1024)
#define TFLITE_MICRO_OP_RESOLVER_MAX_OPS    16
#define TFLITE_MICRO_MAX_OP_NAME_LEN        32
#define TFLITE_MICRO_MAX_TENSOR_NAME_LEN    32

typedef enum {
    TFLITE_STATUS_OK = 0,
    TFLITE_STATUS_ERROR,
    TFLITE_STATUS_ALLOCATION_FAILED,
    TFLITE_STATUS_INVALID_HANDLE,
    TFLITE_STATUS_UNSUPPORTED_OP,
    TFLITE_STATUS_TENSOR_MISMATCH,
    TFLITE_STATUS_INVOKE_FAILED,
    TFLITE_STATUS_VERSION_MISMATCH,
    TFLITE_STATUS_MODEL_INVALID
} TFLiteStatus;

typedef enum {
    TFLITE_TYPE_FLOAT32 = 0,
    TFLITE_TYPE_INT8,
    TFLITE_TYPE_UINT8,
    TFLITE_TYPE_INT32,
    TFLITE_TYPE_BOOL,
    TFLITE_TYPE_INT16,
    TFLITE_TYPE_UNKNOWN
} TFLiteTensorType;

typedef enum {
    TFLITE_OP_RESOLVE_OK = 0,
    TFLITE_OP_RESOLVE_NOT_FOUND,
    TFLITE_OP_RESOLVE_VERSION_TOO_NEW,
    TFLITE_OP_RESOLVE_VERSION_TOO_OLD
} TFLiteOpResolveStatus;

typedef struct {
    uint8_t* arena;
    size_t   arena_size;
    size_t   used_bytes;
} TFLiteTensorArena;

typedef struct {
    TFLiteTensorType type;
    int32_t          dims[TFLITE_MICRO_MAX_DIMENSIONS];
    int32_t          num_dims;
    size_t           bytes;
    void*            data;
    bool             is_quantized;
    float            scale;
    int32_t          zero_point;
    char             name[TFLITE_MICRO_MAX_TENSOR_NAME_LEN];
} TFLiteTensor;

typedef struct TFLiteOpRegistration {
    int32_t  op_code;
    int32_t  version;
    bool     is_builtin;
} TFLiteOpRegistration;

typedef struct {
    char             op_name[TFLITE_MICRO_MAX_OP_NAME_LEN];
    int32_t          op_code;
    int32_t          min_version;
    int32_t          max_version;
    bool             is_builtin;
} TFLiteOpEntry;

typedef struct {
    TFLiteOpEntry  entries[TFLITE_MICRO_OP_RESOLVER_MAX_OPS];
    int32_t        num_entries;
} MicroMutableOpResolver;

typedef struct {
    MicroMutableOpResolver* resolver;
    TFLiteTensorArena*      arena;
    TFLiteTensor*           tensors[TFLITE_MICRO_MAX_TENSORS];
    int32_t                 num_tensors;
    TFLiteOpRegistration*   op_registrations[TFLITE_MICRO_MAX_OPERATORS];
    void*                   op_instances[TFLITE_MICRO_MAX_OPERATORS];
    int32_t                 num_ops;
    const uint8_t*          model_data;
    size_t                  model_size;
    bool                    is_allocated;
    bool                    is_invoked;
    int32_t                 input_tensor_index;
    int32_t                 output_tensor_index;
    int32_t                 subgraph_index;
    int32_t                 model_version;
} TFLiteInterpreter;

typedef struct {
    const uint8_t* data;
    size_t         size;
    bool           is_valid;
    int32_t        model_version;
    char           description[128];
} TFLiteModel;

TFLiteTensorArena*     tflite_tensor_arena_create(size_t arena_size);
void                   tflite_tensor_arena_free(TFLiteTensorArena* arena);
void*                  tflite_tensor_arena_alloc(TFLiteTensorArena* arena, size_t bytes);
void                   tflite_tensor_arena_reset(TFLiteTensorArena* arena);
size_t                 tflite_tensor_arena_available(const TFLiteTensorArena* arena);
size_t                 tflite_tensor_arena_used(const TFLiteTensorArena* arena);

TFLiteTensor*          tflite_tensor_create(void);
void                   tflite_tensor_free(TFLiteTensor* tensor);
TFLiteStatus           tflite_tensor_alloc_data(TFLiteTensor* tensor, TFLiteTensorType type, const int32_t* dims, int32_t num_dims);
TFLiteStatus           tflite_tensor_copy_float_data(TFLiteTensor* dst, const float* data, size_t count);
TFLiteStatus           tflite_tensor_get_float(const TFLiteTensor* tensor, float* out, size_t count);
size_t                 tflite_tensor_element_count(const TFLiteTensor* tensor);

MicroMutableOpResolver* tflite_micro_op_resolver_create(void);
void                    tflite_micro_op_resolver_free(MicroMutableOpResolver* resolver);
TFLiteOpResolveStatus   tflite_micro_op_resolver_add_builtin(MicroMutableOpResolver* resolver, const char* op_name, int32_t op_code, int32_t min_version, int32_t max_version);
TFLiteOpResolveStatus   tflite_micro_op_resolver_resolve(const MicroMutableOpResolver* resolver, int32_t op_code, int32_t version, TFLiteOpEntry* out_entry);

TFLiteInterpreter*     tflite_interpreter_create(void);
void                   tflite_interpreter_free(TFLiteInterpreter* interpreter);
TFLiteStatus           tflite_interpreter_init(TFLiteInterpreter* interpreter, const uint8_t* model_data, size_t model_size, MicroMutableOpResolver* resolver, TFLiteTensorArena* arena);
TFLiteStatus           tflite_interpreter_allocate_tensors(TFLiteInterpreter* interpreter);
TFLiteStatus           tflite_interpreter_invoke(TFLiteInterpreter* interpreter);
TFLiteTensor*          tflite_interpreter_input_tensor(TFLiteInterpreter* interpreter, int32_t index);
TFLiteTensor*          tflite_interpreter_output_tensor(TFLiteInterpreter* interpreter, int32_t index);
int32_t                tflite_interpreter_input_count(const TFLiteInterpreter* interpreter);
int32_t                tflite_interpreter_output_count(const TFLiteInterpreter* interpreter);
TFLiteStatus           tflite_interpreter_reset(TFLiteInterpreter* interpreter);

TFLiteModel*           tflite_model_create(const uint8_t* data, size_t size);
void                   tflite_model_free(TFLiteModel* model);
bool                   tflite_model_validate(const TFLiteModel* model);
const char*            tflite_model_description(const TFLiteModel* model);

const char*            tflite_status_to_string(TFLiteStatus status);
const char*            tflite_tensor_type_to_string(TFLiteTensorType type);

#ifdef __cplusplus
}
#endif

#endif /* TFLITE_MICRO_H */
