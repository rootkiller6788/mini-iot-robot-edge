#ifndef DELTA_UPDATE_H
#define DELTA_UPDATE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DELTA_UPDATE_HASH_SIZE        32
#define DELTA_UPDATE_SIG_SIZE         72
#define DELTA_UPDATE_HEADER_SIZE      128
#define DELTA_UPDATE_MAX_PATCH_SIZE   (16 * 1024 * 1024)
#define DELTA_UPDATE_CHUNK_SIZE       4096
#define DELTA_UPDATE_SA_MARKER       (-1)

typedef enum {
    DELTA_RESULT_OK = 0,
    DELTA_RESULT_ERR_PARAM,
    DELTA_RESULT_ERR_NOMEM,
    DELTA_RESULT_ERR_IO,
    DELTA_RESULT_ERR_HASH,
    DELTA_RESULT_ERR_SIGNATURE,
    DELTA_RESULT_ERR_PATCH_CORRUPT,
    DELTA_RESULT_ERR_PATCH_APPLY,
    DELTA_RESULT_ERR_OLD_MISMATCH,
    DELTA_RESULT_ERR_NEW_MISMATCH,
    DELTA_RESULT_ERR_COMPRESS,
    DELTA_RESULT_ERR_DECOMPRESS,
    DELTA_RESULT_ERR_PARTIAL
} delta_result_t;

typedef enum {
    DELTA_OP_ADD = 0,
    DELTA_OP_COPY,
    DELTA_OP_SEEK
} delta_op_t;

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t old_file_size;
    uint32_t new_file_size;
    uint32_t patch_data_size;
    uint32_t compressed_size;
    uint32_t chunk_count;
    uint8_t old_file_hash[DELTA_UPDATE_HASH_SIZE];
    uint8_t new_file_hash[DELTA_UPDATE_HASH_SIZE];
    uint8_t patch_hash[DELTA_UPDATE_HASH_SIZE];
    uint8_t signature[DELTA_UPDATE_SIG_SIZE];
    uint32_t flags;
    uint8_t reserved[32];
} delta_patch_header_t;
#pragma pack(pop)

typedef struct delta_suffix_array {
    int32_t *sa;
    int32_t *rank;
    int32_t *tmp;
    int32_t length;
} delta_suffix_array_t;

typedef void *(*delta_alloc_fn)(size_t size, void *ctx);
typedef void (*delta_free_fn)(void *ptr, void *ctx);
typedef int (*delta_read_fn)(uint8_t *buf, size_t size, void *ctx);
typedef int (*delta_write_fn)(const uint8_t *buf, size_t size, void *ctx);
typedef int (*delta_seek_fn)(int64_t offset, int whence, void *ctx);

typedef struct {
    delta_read_fn read_old;
    delta_read_fn read_patch;
    delta_write_fn write_new;
    delta_seek_fn seek_old;
    delta_seek_fn seek_patch;
    delta_alloc_fn alloc;
    delta_free_fn free;
    void *ctx;
} delta_io_t;

typedef struct {
    uint8_t *public_key;
    size_t public_key_len;
    void *hash_ctx;
    int (*hash_init)(void *ctx);
    int (*hash_update)(void *ctx, const uint8_t *data, size_t len);
    int (*hash_final)(void *ctx, uint8_t *out);
    int (*ecdsa_verify)(const uint8_t *hash, size_t hash_len,
                        const uint8_t *sig, size_t sig_len,
                        const uint8_t *pub_key, size_t pub_key_len);
} delta_crypto_t;

delta_suffix_array_t *delta_sa_build(const uint8_t *data, size_t len, delta_alloc_fn alloc, void *alloc_ctx);
void delta_sa_free(delta_suffix_array_t *sa, delta_free_fn free_fn, void *free_ctx);
int32_t delta_sa_search(const delta_suffix_array_t *sa, const uint8_t *data,
                        const uint8_t *pattern, size_t pattern_len,
                        size_t *match_len);

delta_result_t delta_create_patch(const uint8_t *old_data, size_t old_size,
                                  const uint8_t *new_data, size_t new_size,
                                  uint8_t **patch_out, size_t *patch_size,
                                  const delta_crypto_t *crypto,
                                  delta_alloc_fn alloc, void *alloc_ctx);

delta_result_t delta_apply_patch(const delta_io_t *io,
                                 const delta_crypto_t *crypto);

delta_result_t delta_verify_patch(const delta_io_t *io,
                                  const delta_crypto_t *crypto);

delta_result_t delta_patch_partial_apply(const delta_io_t *io,
                                         const delta_crypto_t *crypto,
                                         uint32_t start_chunk,
                                         uint32_t end_chunk);

int delta_compress(const uint8_t *input, size_t input_len,
                   uint8_t *output, size_t *output_len);
int delta_decompress(const uint8_t *input, size_t input_len,
                     uint8_t *output, size_t *output_len);

const char *delta_result_str(delta_result_t result);

#ifdef __cplusplus
}
#endif

#endif
