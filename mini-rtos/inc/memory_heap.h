#ifndef MEMORY_HEAP_H
#define MEMORY_HEAP_H

#include <stdint.h>
#include <stddef.h>
#include "task_scheduler.h"

#define HEAP_ALIGNMENT       8
#define HEAP_MIN_BLOCK_SIZE  32
#define HEAP_MAX_REGIONS     8
#define BLOCK_HEADER_SIZE    32
#define BLOCK_MAGIC_ALLOC    0xA110C8EDU
#define BLOCK_MAGIC_FREE     0xF2EEF2EEU

#define HEAP_TYPE_1  1
#define HEAP_TYPE_2  2
#define HEAP_TYPE_3  3
#define HEAP_TYPE_4  4
#define HEAP_TYPE_5  5

typedef struct block_header {
    uint32_t             size;
    uint32_t             magic;
    uint8_t              free;
    struct block_header *next;
    struct block_header *prev;
} block_header_t;

typedef struct {
    uint8_t         *start;
    uint8_t         *end;
    block_header_t  *free_list;
} heap_region_t;

typedef struct {
    uint32_t total_size;
    uint32_t free_size;
    uint32_t allocated_size;
    uint32_t min_free_ever;
    uint32_t alloc_count;
    uint32_t free_count;
} heap_stats_t;

void     heap_init(void *start, uint32_t size, uint8_t type);
void     heap_add_region(void *start, uint32_t size);
void    *malloc_rtos(size_t size);
void     free_rtos(void *ptr);
void    *calloc_rtos(size_t num, size_t size);
void    *realloc_rtos(void *ptr, size_t new_size);
heap_stats_t heap_get_stats(void);
void     heap_dump(void);
uint32_t heap_get_free_size(void);
uint32_t heap_get_min_free(void);
uint32_t heap_calc_largest_free(void);
uint32_t heap_fragmentation_permil(void);
uint32_t heap_total_alloc_count(void);
uint32_t heap_total_free_count(void);

void heap_malloc_failed_hook(void);
void stack_overflow_init(tcb_t *task);
uint32_t stack_overflow_check(tcb_t *task);
void stack_overflow_hook(tcb_t *task);

#endif
