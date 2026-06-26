#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include <stdbool.h>
#include <stdint.h>

/* ── Core memory regions ─────────────────────────────── */
#define FLASH_BASE            0x08000000UL
#define FLASH_SIZE_DEFAULT    (1UL * 1024UL * 1024UL)
#define FLASH_SECTOR_SIZE     0x4000UL

#define SRAM_BASE             0x20000000UL
#define SRAM_SIZE_DEFAULT     (192UL * 1024UL)

#define CCMRAM_BASE           0x10000000UL
#define CCMRAM_SIZE_DEFAULT   (64UL * 1024UL)

#define BOOTROM_BASE          0x1FFF0000UL
#define BOOTROM_SIZE          0x7800UL

/* ── Peripheral bus regions ──────────────────────────── */
#define PERIPH_BASE           0x40000000UL
#define AHB1_PERIPH_BASE      PERIPH_BASE
#define AHB2_PERIPH_BASE      0x50000000UL
#define APB1_PERIPH_BASE      0x40000000UL
#define APB2_PERIPH_BASE      0x40010000UL

#define AHB1_PERIPH_SIZE      0x00020000UL
#define APB1_PERIPH_SIZE      0x00010000UL
#define APB2_PERIPH_SIZE      0x00010000UL

/* ── External memory regions ─────────────────────────── */
#define FSMC_BANK1_BASE       0x60000000UL
#define FSMC_BANK2_BASE       0x70000000UL
#define FSMC_BANK3_BASE       0x80000000UL
#define FSMC_BANK4_BASE       0x90000000UL

/* ── Cortex-M4 internal regions ──────────────────────── */
#define CORTEX_INTERNAL_BASE  0xE0000000UL
#define ITM_BASE              0xE0000000UL
#define DWT_BASE              0xE0001000UL
#define FPB_BASE              0xE0002000UL
#define NVIC_BASE             0xE000E100UL
#define SCB_BASE              0xE000ED00UL
#define SYSTICK_BASE          0xE000E010UL
#define MPU_BASE              0xE000ED90UL

/* ── Bit-band region definitions ─────────────────────── */
#define BITBAND_SRAM_BASE     0x22000000UL
#define BITBAND_PERIPH_BASE   0x42000000UL
#define BITBAND_SRAM_REF      0x20000000UL
#define BITBAND_PERIPH_REF    0x40000000UL

#define BITBAND_SRAM_ADDR(byte_addr, bit_num) \
    (BITBAND_SRAM_BASE + (((uint32_t)(byte_addr) - BITBAND_SRAM_REF) * 32) + ((uint32_t)(bit_num) * 4))

#define BITBAND_PERIPH_ADDR(byte_addr, bit_num) \
    (BITBAND_PERIPH_BASE + (((uint32_t)(byte_addr) - BITBAND_PERIPH_REF) * 32) + ((uint32_t)(bit_num) * 4))

/* ── Memory section types ────────────────────────────── */
typedef enum {
    MEM_SECTION_TEXT,
    MEM_SECTION_RODATA,
    MEM_SECTION_DATA,
    MEM_SECTION_BSS,
    MEM_SECTION_HEAP,
    MEM_SECTION_STACK,
    MEM_SECTION_COUNT
} memory_section_t;

typedef enum {
    MEM_ACCESS_R   = 0x1,
    MEM_ACCESS_W   = 0x2,
    MEM_ACCESS_RW  = 0x3,
    MEM_ACCESS_X   = 0x4,
    MEM_ACCESS_RX  = 0x5,
    MEM_ACCESS_NONE = 0x0
} memory_access_t;

typedef enum {
    MEM_TYPE_FLASH,
    MEM_TYPE_SRAM,
    MEM_TYPE_CCMRAM,
    MEM_TYPE_PERIPH,
    MEM_TYPE_EXTERNAL
} memory_type_t;

/* ── Memory region descriptor ────────────────────────── */
typedef struct {
    const char *name;
    uint32_t start_addr;
    uint32_t size;
    memory_type_t mem_type;
    memory_access_t access;
    bool is_cached;
    bool is_bufferable;
} memory_region_t;

/* ── Linker script section descriptor ────────────────── */
typedef struct {
    memory_section_t section;
    const char *section_name;
    uint32_t load_addr;
    uint32_t run_addr;
    uint32_t size;
    uint32_t alignment;
    bool is_initialized;
    bool is_zeroed;
} linker_section_t;

/* ── Boot vector descriptor ──────────────────────────── */
typedef struct {
    uint32_t stack_pointer;
    void (*reset_handler)(void);
} boot_vector_t;

/* ── Memory map context ──────────────────────────────── */
typedef struct {
    linker_section_t sections[MEM_SECTION_COUNT];
    memory_region_t regions[8];
    uint32_t region_count;
    boot_vector_t boot_vector;
    bool is_valid;
} memory_map_ctx_t;

/* ── Memory map API ──────────────────────────────────── */
void memory_map_init(memory_map_ctx_t *ctx);
void memory_map_add_region(memory_map_ctx_t *ctx, const char *name,
    uint32_t start, uint32_t size, memory_type_t type,
    memory_access_t access, bool cached, bool bufferable);
bool memory_map_is_address_valid(const memory_map_ctx_t *ctx, uint32_t addr);
memory_type_t memory_map_get_region_type(const memory_map_ctx_t *ctx,
    uint32_t addr);
memory_access_t memory_map_get_access(const memory_map_ctx_t *ctx,
    uint32_t addr);
void memory_map_configure_section(memory_map_ctx_t *ctx,
    memory_section_t section, uint32_t load_addr, uint32_t run_addr,
    uint32_t size, uint32_t alignment, bool is_initialized, bool is_zeroed);
void memory_map_set_boot_vector(memory_map_ctx_t *ctx,
    uint32_t stack_pointer, void (*reset_handler)(void));
void memory_map_print_layout(const memory_map_ctx_t *ctx);

/* ── Bit-band API ────────────────────────────────────── */
void bitband_write(uint32_t addr, uint8_t bit, bool value);
bool bitband_read(uint32_t addr, uint8_t bit);

/* ── Section placement simulation API ────────────────── */
uint32_t memory_map_get_section_addr(const memory_map_ctx_t *ctx,
    memory_section_t section);
uint32_t memory_map_get_section_size(const memory_map_ctx_t *ctx,
    memory_section_t section);
bool memory_map_is_in_flash(const memory_map_ctx_t *ctx, uint32_t addr);
bool memory_map_is_in_sram(const memory_map_ctx_t *ctx, uint32_t addr);
bool memory_map_is_in_peripheral(const memory_map_ctx_t *ctx, uint32_t addr);
uint32_t memory_map_get_free_sram(const memory_map_ctx_t *ctx);
uint32_t memory_map_get_free_flash(const memory_map_ctx_t *ctx);
uint32_t memory_map_get_partition(const memory_map_ctx_t *ctx);

#endif /* MEMORY_MAP_H */
