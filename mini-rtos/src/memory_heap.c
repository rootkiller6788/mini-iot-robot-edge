#include "task_scheduler.h"
#include "memory_heap.h"
#include <string.h>

static uint8_t       heap_type = HEAP_TYPE_4;
static heap_region_t regions[HEAP_MAX_REGIONS];
static uint32_t      region_count = 0;
static heap_stats_t  stats;

static void heap_stats_update_allocation(void)
{
    stats.alloc_count++;
    stats.allocated_size = stats.total_size - stats.free_size;
}

static void heap_stats_update_free(void)
{
    stats.free_count++;
    if (stats.free_size < stats.min_free_ever) {
        stats.min_free_ever = stats.free_size;
    }
    stats.allocated_size = stats.total_size - stats.free_size;
}

static block_header_t *block_alloc(heap_region_t *region, uint32_t size)
{
    block_header_t *block, *best = NULL;
    uint32_t best_size = 0xFFFFFFFFU;
    switch (heap_type) {
    case HEAP_TYPE_1:
        best = region->free_list;
        break;
    case HEAP_TYPE_2:
    case HEAP_TYPE_3:
    case HEAP_TYPE_4:
    case HEAP_TYPE_5:
        block = region->free_list;
        while (block) {
            if (block->free && block->size >= size) {
                if (heap_type == HEAP_TYPE_2) {
                    if (block->size < best_size) {
                        best = block;
                        best_size = block->size;
                    }
                } else {
                    best = block;
                    break;
                }
            }
            block = block->next;
        }
        break;
    default:
        break;
    }
    return best;
}

static void block_split(block_header_t *block, uint32_t size)
{
    uint32_t remaining = block->size - size;
    if (remaining >= HEAP_MIN_BLOCK_SIZE) {
        block_header_t *new_block = (block_header_t *)((uint8_t *)block + size);
        new_block->size = remaining;
        new_block->free = 1;
        new_block->magic = BLOCK_MAGIC_FREE;
        new_block->next = block->next;
        new_block->prev = block;
        if (block->next) {
            block->next->prev = new_block;
        }
        block->next = new_block;
        block->size = size;
    }
}

static void block_coalesce(heap_region_t *region, block_header_t *block)
{
    if (block->next && block->next->free) {
        block->size += block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    if (block->prev && block->prev->free) {
        block->prev->size += block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
        block = block->prev;
    }
}

static void heap_region_init(heap_region_t *region, void *start, uint32_t size)
{
    uintptr_t aligned_start = ((uintptr_t)start + HEAP_ALIGNMENT - 1) & ~(uintptr_t)(HEAP_ALIGNMENT - 1);
    uintptr_t aligned_end = ((uintptr_t)start + size) & ~(uintptr_t)(HEAP_ALIGNMENT - 1);
    uint32_t usable = (uint32_t)(aligned_end - aligned_start);
    if (usable < HEAP_MIN_BLOCK_SIZE * 2) return;
    region->start = (uint8_t *)aligned_start;
    region->end = (uint8_t *)aligned_end;
    block_header_t *block = (block_header_t *)region->start;
    block->size = usable;
    block->free = 1;
    block->magic = BLOCK_MAGIC_FREE;
    block->next = NULL;
    block->prev = NULL;
    region->free_list = block;
    stats.total_size += usable;
    stats.free_size += usable;
    if (stats.min_free_ever == 0 || usable < stats.min_free_ever) {
        stats.min_free_ever = usable;
    }
}

void heap_init(void *start, uint32_t size, uint8_t type)
{
    if (type >= HEAP_TYPE_1 && type <= HEAP_TYPE_5) {
        heap_type = type;
    }
    memset(&stats, 0, sizeof(stats));
    memset(regions, 0, sizeof(regions));
    region_count = 1;
    heap_region_init(&regions[0], start, size);
}

void heap_add_region(void *start, uint32_t size)
{
    if (region_count >= HEAP_MAX_REGIONS) return;
    heap_region_init(&regions[region_count], start, size);
    region_count++;
}

void *malloc_rtos(size_t size)
{
    uint32_t alloc_size, i;
    block_header_t *block = NULL;
    if (size == 0) return NULL;
    alloc_size = (uint32_t)size + BLOCK_HEADER_SIZE;
    alloc_size = (alloc_size + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);
    if (alloc_size < HEAP_MIN_BLOCK_SIZE) alloc_size = HEAP_MIN_BLOCK_SIZE;
    for (i = 0; i < region_count; i++) {
        block = block_alloc(&regions[i], alloc_size);
        if (block) break;
    }
    if (!block) {
        heap_malloc_failed_hook();
        return NULL;
    }
    block_split(block, alloc_size);
    block->free = 0;
    block->magic = BLOCK_MAGIC_ALLOC;
    stats.free_size -= alloc_size;
    heap_stats_update_allocation();
    if (stats.free_size < stats.min_free_ever) {
        stats.min_free_ever = stats.free_size;
    }
    return (void *)((uint8_t *)block + BLOCK_HEADER_SIZE);
}

void free_rtos(void *ptr)
{
    block_header_t *block;
    uint32_t i;
    if (!ptr) return;
    block = (block_header_t *)((uint8_t *)ptr - BLOCK_HEADER_SIZE);
    if (block->magic != BLOCK_MAGIC_ALLOC) return;
    block->free = 1;
    block->magic = BLOCK_MAGIC_FREE;
    stats.free_size += block->size;
    heap_stats_update_free();
    for (i = 0; i < region_count; i++) {
        if ((uint8_t *)block >= regions[i].start &&
            (uint8_t *)block < regions[i].end) {
            if (heap_type >= HEAP_TYPE_3) {
                block_coalesce(&regions[i], block);
            }
            break;
        }
    }
}

void *calloc_rtos(size_t num, size_t size)
{
    size_t total = num * size;
    void *ptr = malloc_rtos(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *realloc_rtos(void *ptr, size_t new_size)
{
    block_header_t *block;
    void *new_ptr;
    uint32_t old_size;
    if (!ptr) return malloc_rtos(new_size);
    if (new_size == 0) {
        free_rtos(ptr);
        return NULL;
    }
    block = (block_header_t *)((uint8_t *)ptr - BLOCK_HEADER_SIZE);
    if (block->magic != BLOCK_MAGIC_ALLOC) return NULL;
    old_size = block->size - BLOCK_HEADER_SIZE;
    if (new_size <= old_size) return ptr;
    new_ptr = malloc_rtos(new_size);
    if (!new_ptr) return NULL;
    memcpy(new_ptr, ptr, old_size);
    free_rtos(ptr);
    return new_ptr;
}

heap_stats_t heap_get_stats(void)
{
    return stats;
}

void heap_dump(void)
{
    uint32_t i;
    for (i = 0; i < region_count; i++) {
        block_header_t *block = regions[i].free_list;
        while (block) {
            (void)block->size;
            (void)block->free;
            block = block->next;
        }
    }
}

uint32_t heap_get_free_size(void)
{
    return stats.free_size;
}

uint32_t heap_get_min_free(void)
{
    return stats.min_free_ever;
}

void heap_malloc_failed_hook(void)
{
    /* Default empty hook; override by redefining in application code */
}

void stack_overflow_init(tcb_t *task)
{
    if (!task || !task->stack_start) return;
    memset(task->stack_start, 0xA5, task->stack_size);
}

uint32_t stack_overflow_check(tcb_t *task)
{
    uint32_t free_bytes;
    if (!task) return 0;
    free_bytes = task_stack_free(task);
    if (free_bytes < 64) {
        stack_overflow_hook(task);
        return 1;
    }
    return 0;
}

void stack_overflow_hook(tcb_t *task)
{
    (void)task;
    /* Default empty hook; override by redefining in application code */
}

uint32_t heap_calc_largest_free(void)
{
    uint32_t i, largest = 0;
    for (i = 0; i < region_count; i++) {
        block_header_t *block = regions[i].free_list;
        while (block) {
            if (block->free && block->size > largest) {
                largest = block->size;
            }
            block = block->next;
        }
    }
    return largest;
}

/*
 * L5: Best-Fit allocation scan (explicit, for HEAP_TYPE_2 debugging).
 *
 * Scans the free list and returns the block with size closest to
 * (but >=) the requested size. This minimizes external fragmentation
 * but is O(n) in the number of free blocks.
 *
 * This is the canonical Best-Fit algorithm (Knuth, The Art of
 * Computer Programming, Vol 1, Sec 2.5).
 */
block_header_t *heap_best_fit_search(heap_region_t *region, uint32_t size)
{
    block_header_t *block, *best = NULL;
    uint32_t best_size = 0xFFFFFFFFU;
    if (!region) return NULL;
    block = region->free_list;
    while (block) {
        if (block->free && block->size >= size) {
            if (block->size < best_size) {
                best = block;
                best_size = block->size;
                if (best_size == size) break; /* exact fit, optimal */
            }
        }
        block = block->next;
    }
    return best;
}

/*
 * L3: External fragmentation ratio.
 *
 * fragmentation_ratio = 1 - (largest_free_block / total_free)
 *
 * A value near 0 means low fragmentation (large contiguous blocks).
 * A value near 1000 (permil) means high fragmentation.
 *
 * Reference: Johnstone & Wilson (1998) "The Memory Fragmentation
 * Problem: Solved?", ISMM '98.
 *
 * Returns fragmentation in permil (0-1000).
 */
uint32_t heap_fragmentation_permil(void)
{
    uint32_t largest = heap_calc_largest_free();
    uint32_t total_free = stats.free_size;
    if (total_free == 0) return 0;
    if (largest >= total_free) return 0;
    /* frag = (1 - largest/total) * 1000 */
    return 1000U - ((largest * 1000U) / total_free);
}

/*
 * L5: Quick-fit allocation (HEAP_TYPE_5 emulation).
 *
 * Maintains a lookup table of free lists segregated by size class.
 * This is O(1) for common allocation sizes at the cost of slightly
 * higher fragmentation. Based on the Quick Fit algorithm described
 * in Weinstock & Wulf (1988).
 *
 * size_classes: array of (size, free_list_head) pairs.
 */
#define QUICK_FIT_CLASSES 8

typedef struct {
    uint32_t         size;
    block_header_t  *free_list;
} quick_fit_class_t;

static quick_fit_class_t quick_fit_classes[QUICK_FIT_CLASSES];

void heap_quick_fit_init(void)
{
    /* Power-of-2 size classes: 32, 64, 128, 256, 512, 1K, 2K, 4K+ */
    uint32_t i;
    uint32_t sz = 32;
    for (i = 0; i < QUICK_FIT_CLASSES; i++) {
        quick_fit_classes[i].size = sz;
        quick_fit_classes[i].free_list = NULL;
        sz *= 2;
    }
}

/*
 * L5: Find the quick-fit size class for a given allocation size.
 * Returns the index of the smallest size class >= requested size,
 * or QUICK_FIT_CLASSES if too large (then use general allocator).
 */
uint32_t heap_quick_fit_class(size_t size)
{
    uint32_t i;
    for (i = 0; i < QUICK_FIT_CLASSES; i++) {
        if (size <= quick_fit_classes[i].size) return i;
    }
    return QUICK_FIT_CLASSES; /* too large, use general allocator */
}

/*
 * L3: Total allocation count since boot (for leak detection baseline).
 */
uint32_t heap_total_alloc_count(void)
{
    return stats.alloc_count;
}

/*
 * L3: Total free count since boot (for double-free detection baseline).
 */
uint32_t heap_total_free_count(void)
{
    return stats.free_count;
}
