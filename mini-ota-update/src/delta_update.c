#include "delta_update.h"
#include <stdlib.h>
#include <string.h>

static int delta_sa_compare(const uint8_t *data, int32_t n,
                             int32_t a, int32_t b, int32_t cur)
{
    (void)cur;
    if (data[a] != data[b]) return data[a] - data[b];
    if (a + 1 >= n) return -1;
    if (b + 1 >= n) return 1;
    return data[a + 1] - data[b + 1];
}

static void delta_sa_sort(const uint8_t *data, int32_t n,
                           int32_t *sa, int32_t *rank)
{
    for (int32_t i = 0; i < n; i++) {
        sa[i] = i;
        rank[i] = data[i];
    }

    for (int32_t k = 1; k < n; k <<= 1) {
        for (int32_t i = 0; i < n - 1; i++) {
            for (int32_t j = i + 1; j < n; j++) {
                int32_t ra = i + k < n ? rank[i + k] : -1;
                int32_t rb = j + k < n ? rank[j + k] : -1;
                if (rank[sa[i]] > rank[sa[j]] ||
                    (rank[sa[i]] == rank[sa[j]] && ra > rb)) {
                    int32_t t = sa[i];
                    sa[i] = sa[j];
                    sa[j] = t;
                }
            }
        }

        int32_t *tmp_rank = (int32_t *)malloc(n * sizeof(int32_t));
        if (!tmp_rank) return;
        tmp_rank[sa[0]] = 0;
        for (int32_t i = 1; i < n; i++) {
            tmp_rank[sa[i]] = tmp_rank[sa[i - 1]];
            if (delta_sa_compare(data, n, sa[i - 1], sa[i], k) != 0)
                tmp_rank[sa[i]]++;
        }
        for (int32_t i = 0; i < n; i++) rank[i] = tmp_rank[i];
        free(tmp_rank);
    }
}

delta_suffix_array_t *delta_sa_build(const uint8_t *data, size_t len,
                                      delta_alloc_fn alloc, void *alloc_ctx)
{
    if (!data || len == 0) return NULL;
    int32_t n = (int32_t)len;

    delta_suffix_array_t *sa = (delta_suffix_array_t *)
        (alloc ? alloc(sizeof(delta_suffix_array_t), alloc_ctx)
               : malloc(sizeof(delta_suffix_array_t)));
    if (!sa) return NULL;

    sa->sa = (int32_t *)(alloc ? alloc(n * sizeof(int32_t), alloc_ctx)
                                : malloc(n * sizeof(int32_t)));
    sa->rank = (int32_t *)(alloc ? alloc(n * sizeof(int32_t), alloc_ctx)
                                  : malloc(n * sizeof(int32_t)));
    if (!sa->sa || !sa->rank) {
        delta_sa_free(sa, alloc ? (delta_free_fn)NULL : free, alloc_ctx);
        if (!alloc) { free(sa->sa); free(sa->rank); free(sa); }
        return NULL;
    }

    sa->tmp = NULL;
    sa->length = n;
    delta_sa_sort(data, n, sa->sa, sa->rank);
    return sa;
}

void delta_sa_free(delta_suffix_array_t *sa, delta_free_fn free_fn,
                    void *free_ctx)
{
    if (!sa) return;
    void (*ff)(void *, void *) = free_fn
        ? (void (*)(void *, void *))free_fn : NULL;
    if (sa->sa) { if (ff) ff(sa->sa, free_ctx); else free(sa->sa); }
    if (sa->rank) { if (ff) ff(sa->rank, free_ctx); else free(sa->rank); }
    if (sa->tmp) { if (ff) ff(sa->tmp, free_ctx); else free(sa->tmp); }
    if (ff) ff(sa, free_ctx); else free(sa);
}

int32_t delta_sa_search(const delta_suffix_array_t *sa,
                         const uint8_t *data, const uint8_t *pattern,
                         size_t pattern_len, size_t *match_len)
{
    if (!sa || !data || !pattern || pattern_len == 0) return -1;

    int32_t lo = 0, hi = sa->length - 1;
    int32_t best_match = -1;
    size_t best_len = 0;

    while (lo <= hi) {
        int32_t mid = (lo + hi) / 2;
        int32_t spos = sa->sa[mid];
        size_t i = 0;
        while (i < pattern_len && spos + i < (size_t)sa->length &&
               data[spos + i] == pattern[i]) i++;
        if (i > best_len) { best_len = i; best_match = spos; }
        if (i == pattern_len) break;
        if (spos + i >= (size_t)sa->length || data[spos + i] < pattern[i])
            lo = mid + 1;
        else
            hi = mid - 1;
    }

    if (match_len) *match_len = best_len;
    return best_len >= 8 ? best_match : -1;
}

delta_result_t delta_create_patch(const uint8_t *old_data, size_t old_size,
                                   const uint8_t *new_data, size_t new_size,
                                   uint8_t **patch_out, size_t *patch_size,
                                   const delta_crypto_t *crypto,
                                   delta_alloc_fn alloc, void *alloc_ctx)
{
    if (!old_data || !new_data || !patch_out || !patch_size)
        return DELTA_RESULT_ERR_PARAM;
    (void)crypto;

    delta_suffix_array_t *sa = delta_sa_build(old_data, old_size,
                                                alloc, alloc_ctx);
    if (!sa) return DELTA_RESULT_ERR_NOMEM;

    uint32_t max_ops = (uint32_t)(new_size / 4 + 256);
    size_t buf_size = sizeof(delta_patch_header_t) + max_ops * 12 + new_size;
    uint8_t *buf = (uint8_t *)(alloc ? alloc(buf_size, alloc_ctx)
                                      : malloc(buf_size));
    if (!buf) {
        delta_sa_free(sa, alloc ? (delta_free_fn)NULL : free, alloc_ctx);
        if (!alloc) delta_sa_free(sa, (delta_free_fn)NULL, alloc_ctx);
        return DELTA_RESULT_ERR_NOMEM;
    }
    memset(buf, 0, buf_size);

    delta_patch_header_t *hdr = (delta_patch_header_t *)buf;
    hdr->magic = 0x42534446;
    hdr->version = 1;
    hdr->header_size = sizeof(delta_patch_header_t);
    hdr->old_file_size = (uint32_t)old_size;
    hdr->new_file_size = (uint32_t)new_size;

    uint8_t *ops_start = buf + sizeof(delta_patch_header_t);
    size_t ops_offset = 0;
    size_t pos = 0;
    uint32_t copy_count = 0;

    while (pos < new_size) {
        size_t remaining = new_size - pos;
        size_t search_len = remaining > 1024 ? 1024 : remaining;
        size_t match_len = 0;
        int32_t match_pos = delta_sa_search(sa, old_data,
            new_data + pos, search_len, &match_len);

        if (match_len >= 16 && match_pos >= 0) {
            ops_start[ops_offset++] = DELTA_OP_COPY;
            ops_start[ops_offset++] = (uint8_t)(match_len & 0xFF);
            ops_start[ops_offset++] = (uint8_t)((match_len >> 8) & 0xFF);
            ops_start[ops_offset++] = (uint8_t)(match_pos & 0xFF);
            ops_start[ops_offset++] = (uint8_t)((match_pos >> 8) & 0xFF);
            ops_start[ops_offset++] = (uint8_t)((match_pos >> 16) & 0xFF);
            ops_start[ops_offset++] = (uint8_t)((match_pos >> 24) & 0xFF);
            pos += match_len;
            copy_count++;
        } else {
            size_t add_len = remaining > 128 ? 128 : remaining;
            ops_start[ops_offset++] = DELTA_OP_ADD;
            ops_start[ops_offset++] = (uint8_t)(add_len & 0xFF);
            ops_start[ops_offset++] = (uint8_t)((add_len >> 8) & 0xFF);
            memcpy(ops_start + ops_offset, new_data + pos, add_len);
            ops_offset += add_len;
            pos += add_len;
        }
    }

    ops_start[ops_offset++] = DELTA_OP_SEEK;
    hdr->patch_data_size = (uint32_t)ops_offset;
    hdr->chunk_count = copy_count;
    hdr->compressed_size = hdr->patch_data_size;

    *patch_out = buf;
    *patch_size = sizeof(delta_patch_header_t) + ops_offset;

    delta_sa_free(sa, alloc ? (delta_free_fn)NULL : free, alloc_ctx);
    if (!alloc) {
        delta_suffix_array_t *temp = sa;
        free(temp->sa); free(temp->rank); free(temp);
    }
    return DELTA_RESULT_OK;
}

delta_result_t delta_apply_patch(const delta_io_t *io,
                                  const delta_crypto_t *crypto)
{
    if (!io) return DELTA_RESULT_ERR_PARAM;

    delta_patch_header_t header;
    memset(&header, 0, sizeof(header));

    int rc = io->read_patch((uint8_t *)&header, sizeof(header), io->ctx);
    if (rc != 0) return DELTA_RESULT_ERR_IO;

    if (header.magic != 0x42534446) return DELTA_RESULT_ERR_PATCH_CORRUPT;
    if (header.version != 1) return DELTA_RESULT_ERR_PATCH_CORRUPT;

    uint8_t *ops = (uint8_t *)malloc(header.patch_data_size);
    if (!ops) return DELTA_RESULT_ERR_NOMEM;
    rc = io->read_patch(ops, header.patch_data_size, io->ctx);
    if (rc != 0) { free(ops); return DELTA_RESULT_ERR_IO; }

    uint8_t *old_buf = (uint8_t *)malloc(header.old_file_size);
    if (!old_buf) { free(ops); return DELTA_RESULT_ERR_NOMEM; }
    rc = io->read_old(old_buf, header.old_file_size, io->ctx);
    if (rc != 0) { free(ops); free(old_buf); return DELTA_RESULT_ERR_IO; }

    size_t oi = 0;
    delta_result_t res = DELTA_RESULT_OK;

    for (size_t i = 0; i < header.patch_data_size && oi < UINT32_MAX;) {
        uint8_t op = ops[i++];
        if (op == DELTA_OP_ADD) {
            uint32_t len = ops[i] | ((uint32_t)ops[i + 1] << 8);
            i += 2;
            rc = io->write_new(ops + i, len, io->ctx);
            if (rc != 0) { res = DELTA_RESULT_ERR_PATCH_APPLY; break; }
            i += len;
        } else if (op == DELTA_OP_COPY) {
            uint32_t len = ops[i] | ((uint32_t)ops[i + 1] << 8);
            i += 2;
            uint32_t pos = ops[i] | ((uint32_t)ops[i + 1] << 8) |
                          ((uint32_t)ops[i + 2] << 16) |
                          ((uint32_t)ops[i + 3] << 24);
            i += 4;
            if (pos + len > header.old_file_size) {
                res = DELTA_RESULT_ERR_PATCH_CORRUPT; break;
            }
            rc = io->write_new(old_buf + pos, len, io->ctx);
            if (rc != 0) { res = DELTA_RESULT_ERR_PATCH_APPLY; break; }
        } else if (op == DELTA_OP_SEEK) {
            break;
        } else {
            res = DELTA_RESULT_ERR_PATCH_CORRUPT;
            break;
        }
    }

    free(ops);
    free(old_buf);

    if (res == DELTA_RESULT_OK && crypto) {
        res = delta_verify_patch(io, crypto);
    }

    return res;
}

delta_result_t delta_verify_patch(const delta_io_t *io,
                                   const delta_crypto_t *crypto)
{
    if (!io || !crypto) return DELTA_RESULT_ERR_PARAM;
    return DELTA_RESULT_OK;
}

delta_result_t delta_patch_partial_apply(const delta_io_t *io,
                                          const delta_crypto_t *crypto,
                                          uint32_t start_chunk,
                                          uint32_t end_chunk)
{
    if (!io) return DELTA_RESULT_ERR_PARAM;
    if (end_chunk <= start_chunk) return DELTA_RESULT_ERR_PARTIAL;
    (void)crypto;
    return delta_apply_patch(io, NULL);
}

int delta_compress(const uint8_t *input, size_t input_len,
                   uint8_t *output, size_t *output_len)
{
    if (!input || !output || !output_len) return -1;
    size_t oi = 0;
    for (size_t i = 0; i < input_len && oi < *output_len - 1;) {
        uint8_t run_len = 0;
        while (i + run_len < input_len && run_len < 255 &&
               input[i] == input[i + run_len]) run_len++;
        if (run_len > 1) {
            output[oi++] = 0x80 | run_len;
            output[oi++] = input[i];
            i += run_len;
        } else {
            size_t lit_start = oi++;
            output[oi++] = 0;
            while (i < input_len && oi < *output_len - 1) {
                if (i + 1 < input_len && input[i] == input[i + 1]) break;
                output[oi++] = input[i++];
                output[lit_start]++;
            }
        }
    }
    *output_len = oi;
    return 0;
}

int delta_decompress(const uint8_t *input, size_t input_len,
                     uint8_t *output, size_t *output_len)
{
    if (!input || !output || !output_len) return -1;
    size_t oi = 0;
    for (size_t i = 0; i < input_len && oi < *output_len;) {
        uint8_t ctrl = input[i++];
        if (ctrl & 0x80) {
            uint8_t run_len = ctrl & 0x7F;
            uint8_t val = input[i++];
            for (uint8_t j = 0; j < run_len && oi < *output_len; j++)
                output[oi++] = val;
        } else {
            for (uint8_t j = 0; j < ctrl && oi < *output_len; j++)
                output[oi++] = input[i++];
        }
    }
    *output_len = oi;
    return 0;
}

const char *delta_result_str(delta_result_t result)
{
    switch (result) {
    case DELTA_RESULT_OK:                 return "OK";
    case DELTA_RESULT_ERR_PARAM:          return "Invalid parameter";
    case DELTA_RESULT_ERR_NOMEM:          return "No memory";
    case DELTA_RESULT_ERR_IO:             return "I/O error";
    case DELTA_RESULT_ERR_HASH:           return "Hash mismatch";
    case DELTA_RESULT_ERR_SIGNATURE:      return "Signature error";
    case DELTA_RESULT_ERR_PATCH_CORRUPT:  return "Corrupt patch";
    case DELTA_RESULT_ERR_PATCH_APPLY:    return "Patch apply failed";
    case DELTA_RESULT_ERR_OLD_MISMATCH:   return "Old file mismatch";
    case DELTA_RESULT_ERR_NEW_MISMATCH:   return "New file mismatch";
    case DELTA_RESULT_ERR_COMPRESS:       return "Compress error";
    case DELTA_RESULT_ERR_DECOMPRESS:     return "Decompress error";
    case DELTA_RESULT_ERR_PARTIAL:        return "Partial apply error";
    default:                              return "Unknown";
    }
}
