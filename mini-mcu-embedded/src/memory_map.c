#include "memory_map.h"
#include "cortex_m_core.h"
#include <stdio.h>
#include <string.h>

#define MAX_REGIONS 8

static uint32_t s_flash_allocated;
static uint32_t s_sram_allocated;

void memory_map_init(memory_map_ctx_t *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    s_flash_allocated = 0;
    s_sram_allocated = 0;

    memory_map_configure_section(ctx, MEM_SECTION_TEXT,   0x08000000UL, 0x08000000UL, 0x00004000UL, 256, false, false);
    memory_map_configure_section(ctx, MEM_SECTION_RODATA, 0x08004000UL, 0x08004000UL, 0x00002000UL, 64,  false, false);
    memory_map_configure_section(ctx, MEM_SECTION_DATA,   0x08006000UL, 0x20000000UL, 0x00000800UL, 64,  true,  false);
    memory_map_configure_section(ctx, MEM_SECTION_BSS,    0x00000000UL, 0x20000800UL, 0x00000400UL, 64,  false, true);
    memory_map_configure_section(ctx, MEM_SECTION_HEAP,   0x00000000UL, 0x20000C00UL, 0x00010000UL, 8,   false, true);
    memory_map_configure_section(ctx, MEM_SECTION_STACK,  0x00000000UL, 0x20020C00UL, 0x00001000UL, 8,   false, true);

    memory_map_add_region(ctx, "FLASH",         FLASH_BASE,       FLASH_SIZE_DEFAULT, MEM_TYPE_FLASH,    MEM_ACCESS_RX, false, false);
    memory_map_add_region(ctx, "SRAM",          SRAM_BASE,        SRAM_SIZE_DEFAULT,  MEM_TYPE_SRAM,     MEM_ACCESS_RW, true,  true);
    memory_map_add_region(ctx, "CCMRAM",        CCMRAM_BASE,      CCMRAM_SIZE_DEFAULT,MEM_TYPE_CCMRAM,   MEM_ACCESS_RW, true,  false);
    memory_map_add_region(ctx, "AHB1_Periph",   AHB1_PERIPH_BASE, AHB1_PERIPH_SIZE,   MEM_TYPE_PERIPH,   MEM_ACCESS_RW, false, false);
    memory_map_add_region(ctx, "APB1_Periph",   APB1_PERIPH_BASE, APB1_PERIPH_SIZE,   MEM_TYPE_PERIPH,   MEM_ACCESS_RW, false, false);
    memory_map_add_region(ctx, "APB2_Periph",   APB2_PERIPH_BASE, APB2_PERIPH_SIZE,   MEM_TYPE_PERIPH,   MEM_ACCESS_RW, false, false);

    memory_map_set_boot_vector(ctx, SRAM_BASE + SRAM_SIZE_DEFAULT, NULL);
    ctx->is_valid = true;
}

void memory_map_add_region(memory_map_ctx_t *ctx, const char *name,
    uint32_t start, uint32_t size, memory_type_t type,
    memory_access_t access, bool cached, bool bufferable)
{
    if (!ctx || ctx->region_count >= MAX_REGIONS) return;
    memory_region_t *r = &ctx->regions[ctx->region_count];
    r->name = name;
    r->start_addr = start;
    r->size = size;
    r->mem_type = type;
    r->access = access;
    r->is_cached = cached;
    r->is_bufferable = bufferable;
    ctx->region_count++;
}

bool memory_map_is_address_valid(const memory_map_ctx_t *ctx, uint32_t addr) {
    if (!ctx) return false;
    for (uint32_t i = 0; i < ctx->region_count; i++) {
        const memory_region_t *r = &ctx->regions[i];
        if (addr >= r->start_addr && addr < (r->start_addr + r->size)) {
            return true;
        }
    }
    return false;
}

memory_type_t memory_map_get_region_type(const memory_map_ctx_t *ctx, uint32_t addr) {
    if (!ctx) return MEM_TYPE_FLASH;
    for (uint32_t i = 0; i < ctx->region_count; i++) {
        const memory_region_t *r = &ctx->regions[i];
        if (addr >= r->start_addr && addr < (r->start_addr + r->size)) {
            return r->mem_type;
        }
    }
    return MEM_TYPE_FLASH;
}

memory_access_t memory_map_get_access(const memory_map_ctx_t *ctx, uint32_t addr) {
    if (!ctx) return MEM_ACCESS_NONE;
    for (uint32_t i = 0; i < ctx->region_count; i++) {
        const memory_region_t *r = &ctx->regions[i];
        if (addr >= r->start_addr && addr < (r->start_addr + r->size)) {
            return r->access;
        }
    }
    return MEM_ACCESS_NONE;
}

void memory_map_configure_section(memory_map_ctx_t *ctx,
    memory_section_t section, uint32_t load_addr, uint32_t run_addr,
    uint32_t size, uint32_t alignment, bool is_initialized, bool is_zeroed)
{
    if (!ctx || section >= MEM_SECTION_COUNT) return;
    linker_section_t *s = &ctx->sections[section];
    s->section = section;
    s->section_name = NULL;
    s->load_addr = load_addr;
    s->run_addr = run_addr;
    s->size = size;
    s->alignment = alignment;
    s->is_initialized = is_initialized;
    s->is_zeroed = is_zeroed;
}

void memory_map_set_boot_vector(memory_map_ctx_t *ctx,
    uint32_t stack_pointer, void (*reset_handler)(void))
{
    if (!ctx) return;
    ctx->boot_vector.stack_pointer = stack_pointer;
    ctx->boot_vector.reset_handler = reset_handler;
}

void memory_map_print_layout(const memory_map_ctx_t *ctx) {
    if (!ctx) { printf("NULL context\n"); return; }
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║          MEMORY MAP LAYOUT (Cortex-M4)             ║\n");
    printf("╠════════════════════════════════════════════════════╣\n");
    printf("║ Boot vector: SP=0x%08lX  Reset=%p         ║\n",
           (unsigned long)ctx->boot_vector.stack_pointer,
           (const void *)ctx->boot_vector.reset_handler);
    printf("╠════════════════════════════════════════════════════╣\n");
    printf("║ Region  │ Start      │ End        │ Size      │ T ║\n");
    printf("║─────────┼────────────┼────────────┼───────────┼───╣\n");
    for (uint32_t i = 0; i < ctx->region_count; i++) {
        const memory_region_t *r = &ctx->regions[i];
        const char *type_c = "FSCPE"[r->mem_type];
        printf("║ %-7s │ 0x%08lX │ 0x%08lX │ %8lu B │ %c ║\n",
               r->name,
               (unsigned long)r->start_addr,
               (unsigned long)(r->start_addr + r->size - 1),
               (unsigned long)r->size,
               type_c);
    }
    printf("╠════════════════════════════════════════════════════╣\n");
    printf("║ Section   │ Load Addr  │ Run Addr   │ Size        ║\n");
    printf("║───────────┼────────────┼────────────┼─────────────╣\n");
    const char *sec_names[] = {"text", "rodata", "data", "bss", "heap", "stack"};
    for (int s = 0; s < MEM_SECTION_COUNT; s++) {
        const linker_section_t *ls = &ctx->sections[s];
        printf("║ %-9s │ 0x%08lX │ 0x%08lX │ %8lu B  ║\n",
               sec_names[s],
               (unsigned long)ls->load_addr,
               (unsigned long)ls->run_addr,
               (unsigned long)ls->size);
    }
    printf("╠════════════════════════════════════════════════════╣\n");
    printf("║ Free SRAM : %8lu B    Free FLASH: %8lu B   ║\n",
           (unsigned long)(SRAM_SIZE_DEFAULT - s_sram_allocated),
           (unsigned long)(FLASH_SIZE_DEFAULT - s_flash_allocated));
    printf("╚════════════════════════════════════════════════════╝\n");
}

void bitband_write(uint32_t addr, uint8_t bit, bool value) {
    if (bit > 31) return;
    if (addr >= BITBAND_SRAM_REF && addr < (BITBAND_SRAM_REF + SRAM_SIZE_DEFAULT)) {
        uint32_t bb_addr = BITBAND_SRAM_ADDR(addr, bit);
        volatile uint32_t *ptr = (volatile uint32_t *)bb_addr;
        *ptr = value ? 1UL : 0UL;
    } else if (addr >= BITBAND_PERIPH_REF && addr < (BITBAND_PERIPH_REF + 0x00100000UL)) {
        uint32_t bb_addr = BITBAND_PERIPH_ADDR(addr, bit);
        volatile uint32_t *ptr = (volatile uint32_t *)bb_addr;
        *ptr = value ? 1UL : 0UL;
    }
}

bool bitband_read(uint32_t addr, uint8_t bit) {
    if (bit > 31) return false;
    if (addr >= BITBAND_SRAM_REF && addr < (BITBAND_SRAM_REF + SRAM_SIZE_DEFAULT)) {
        uint32_t bb_addr = BITBAND_SRAM_ADDR(addr, bit);
        volatile uint32_t *ptr = (volatile uint32_t *)bb_addr;
        return *ptr != 0;
    }
    if (addr >= BITBAND_PERIPH_REF && addr < (BITBAND_PERIPH_REF + 0x00100000UL)) {
        uint32_t bb_addr = BITBAND_PERIPH_ADDR(addr, bit);
        volatile uint32_t *ptr = (volatile uint32_t *)bb_addr;
        return *ptr != 0;
    }
    return false;
}

uint32_t memory_map_get_section_addr(const memory_map_ctx_t *ctx, memory_section_t section) {
    if (!ctx || section >= MEM_SECTION_COUNT) return 0;
    return ctx->sections[section].run_addr;
}

uint32_t memory_map_get_section_size(const memory_map_ctx_t *ctx, memory_section_t section) {
    if (!ctx || section >= MEM_SECTION_COUNT) return 0;
    return ctx->sections[section].size;
}

bool memory_map_is_in_flash(const memory_map_ctx_t *ctx, uint32_t addr) {
    return memory_map_get_region_type(ctx, addr) == MEM_TYPE_FLASH;
}

bool memory_map_is_in_sram(const memory_map_ctx_t *ctx, uint32_t addr) {
    memory_type_t t = memory_map_get_region_type(ctx, addr);
    return t == MEM_TYPE_SRAM || t == MEM_TYPE_CCMRAM;
}

bool memory_map_is_in_peripheral(const memory_map_ctx_t *ctx, uint32_t addr) {
    return memory_map_get_region_type(ctx, addr) == MEM_TYPE_PERIPH;
}

uint32_t memory_map_get_free_sram(const memory_map_ctx_t *ctx) {
    (void)ctx;
    return SRAM_SIZE_DEFAULT - s_sram_allocated;
}

uint32_t memory_map_get_free_flash(const memory_map_ctx_t *ctx) {
    (void)ctx;
    return FLASH_SIZE_DEFAULT - s_flash_allocated;
}

uint32_t memory_map_get_partition(const memory_map_ctx_t *ctx) {
    (void)ctx;
    uint32_t partition = 0;
    partition |= (s_flash_allocated & 0xFFFFUL) << 16;
    partition |= (s_sram_allocated & 0xFFFFUL);
    return partition;
}
