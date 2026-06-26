#include "memory_map.h"
#include "nvic_dma.h"
#include "cortex_m_core.h"
#include "gpio_uart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void demo_memory_map(void) {
    printf("\n=== Memory Map Demo ===\n");

    memory_map_ctx_t mm_ctx;
    memory_map_init(&mm_ctx);

    printf("  Memory map context initialized\n");
    printf("  Regions: %lu\n", (unsigned long)mm_ctx.region_count);

    uint32_t test_addr = 0x20001000UL;
    bool valid = memory_map_is_address_valid(&mm_ctx, test_addr);
    printf("  Address 0x%08lX valid: %s\n", (unsigned long)test_addr, valid ? "yes" : "no");

    memory_type_t type = memory_map_get_region_type(&mm_ctx, test_addr);
    const char *type_names[] = {"FLASH", "SRAM", "CCMRAM", "PERIPH", "EXTERNAL"};
    printf("  Region type: %s\n", type_names[type]);

    memory_access_t access = memory_map_get_access(&mm_ctx, test_addr);
    const char *acc_names[] = {"NONE", "R", "W", "RW", "X", "RX"};
    printf("  Access: %s\n", acc_names[access & 0x7]);

    bool in_flash = memory_map_is_in_flash(&mm_ctx, 0x08001000UL);
    bool in_sram  = memory_map_is_in_sram(&mm_ctx, 0x20001000UL);
    bool in_periph = memory_map_is_in_peripheral(&mm_ctx, 0x40020000UL);
    printf("  0x08001000 in flash: %s\n", in_flash ? "yes" : "no");
    printf("  0x20001000 in SRAM:  %s\n", in_sram ? "yes" : "no");
    printf("  0x40020000 periph:   %s\n", in_periph ? "yes" : "no");

    uint32_t free_sram = memory_map_get_free_sram(&mm_ctx);
    uint32_t free_flash = memory_map_get_free_flash(&mm_ctx);
    printf("  Free SRAM:  %lu bytes\n", (unsigned long)free_sram);
    printf("  Free FLASH: %lu bytes\n", (unsigned long)free_flash);

    uint32_t partition = memory_map_get_partition(&mm_ctx);
    printf("  Partition: 0x%08lX\n", (unsigned long)partition);

    uint32_t text_addr = memory_map_get_section_addr(&mm_ctx, MEM_SECTION_TEXT);
    uint32_t text_size = memory_map_get_section_size(&mm_ctx, MEM_SECTION_TEXT);
    uint32_t data_addr = memory_map_get_section_addr(&mm_ctx, MEM_SECTION_DATA);
    uint32_t bss_addr  = memory_map_get_section_addr(&mm_ctx, MEM_SECTION_BSS);
    uint32_t heap_addr = memory_map_get_section_addr(&mm_ctx, MEM_SECTION_HEAP);
    uint32_t stack_addr = memory_map_get_section_addr(&mm_ctx, MEM_SECTION_STACK);
    printf("  .text:  0x%08lX (%lu B)\n", (unsigned long)text_addr,  (unsigned long)text_size);
    printf("  .data:  0x%08lX\n", (unsigned long)data_addr);
    printf("  .bss:   0x%08lX\n", (unsigned long)bss_addr);
    printf("  .heap:  0x%08lX\n", (unsigned long)heap_addr);
    printf("  .stack: 0x%08lX\n", (unsigned long)stack_addr);

    printf("\n");
    memory_map_print_layout(&mm_ctx);
}

static void demo_bitband(void) {
    printf("\n=== Bit-Band Demo ===\n");

    uint32_t sram_addr = 0x20000000UL;
    volatile uint32_t *word = (volatile uint32_t *)sram_addr;
    *word = 0x00;

    bitband_write(sram_addr, 0, true);
    bitband_write(sram_addr, 7, true);
    printf("  Wrote bits 0 and 7 via bit-band alias\n");

    bool b0 = bitband_read(sram_addr, 0);
    bool b7 = bitband_read(sram_addr, 7);
    bool b3 = bitband_read(sram_addr, 3);
    printf("  Read: bit0=%d bit7=%d bit3=%d\n", b0, b7, b3);
    printf("  Word value: 0x%08lX\n", (unsigned long)*word);

    uint32_t periph_addr = 0x40020000UL;
    volatile uint32_t *periph = (volatile uint32_t *)periph_addr;
    *periph = 0x0000;
    bitband_write(periph_addr, 15, true);
    printf("  Peripheral bit-band: set bit15 @ 0x40020000 = 0x%08lX\n", (unsigned long)*periph);
}

static void demo_nvic_priority(void) {
    printf("\n=== NVIC Priority Demo ===\n");

    nvic_init();
    printf("  NVIC initialized (4 group, 2 sub-priorities)\n");

    nvic_set_priority(NVIC_IRQ_USART1, NVIC_PRIORITY_HIGH);
    nvic_set_priority(NVIC_IRQ_TIM2,   NVIC_PRIORITY_MEDIUM);
    nvic_set_priority(NVIC_IRQ_SPI1,   NVIC_PRIORITY_LOW);
    printf("  USART1 IRQ%02d priority: HIGH\n",  NVIC_IRQ_USART1);
    printf("  TIM2   IRQ%02d priority: MEDIUM\n", NVIC_IRQ_TIM2);
    printf("  SPI1   IRQ%02d priority: LOW\n",    NVIC_IRQ_SPI1);

    uint8_t p1 = nvic_get_priority(NVIC_IRQ_USART1);
    uint8_t p2 = nvic_get_priority(NVIC_IRQ_TIM2);
    uint8_t p3 = nvic_get_priority(NVIC_IRQ_SPI1);
    printf("  Readback priorities: 0x%02X 0x%02X 0x%02X\n", p1, p2, p3);

    nvic_enable_irq(NVIC_IRQ_TIM2);
    printf("  TIM2 IRQ enabled\n");

    nvic_set_pending(NVIC_IRQ_TIM2);
    bool pending = nvic_get_pending(NVIC_IRQ_TIM2);
    printf("  TIM2 IRQ pending: %s\n", pending ? "yes" : "no");

    nvic_clear_pending(NVIC_IRQ_TIM2);
    bool active = nvic_get_active(NVIC_IRQ_TIM2);
    printf("  TIM2 IRQ active: %s\n", active ? "yes" : "no");

    nvic_disable_irq(NVIC_IRQ_TIM2);
    printf("  TIM2 IRQ disabled\n");

    cortex_m4_scb_set_priority_grouping(PRIGROUP_NVIC_2_3);
    priority_grouping_t pg = cortex_m4_scb_get_priority_grouping();
    printf("  Priority grouping set to NVIC_2_3 (value=%d)\n", (int)pg);

    uint32_t act = nvic_get_active_vector();
    uint32_t pend = nvic_get_pending_vector();
    printf("  Active vector: %lu, Pending vector: %lu\n", (unsigned long)act, (unsigned long)pend);
}

static void demo_dma(void) {
    printf("\n=== DMA Demo ===\n");

    dma_init(DMA1_BASE);
    printf("  DMA1 initialized\n");

    dma_config_t dma_cfg;
    memset(&dma_cfg, 0, sizeof(dma_cfg));
    dma_cfg.dma_base         = DMA1_BASE;
    dma_cfg.stream           = DMA_STREAM_0;
    dma_cfg.channel          = DMA_CHANNEL_0;
    dma_cfg.direction        = DMA_DIR_MEM_TO_MEM;
    dma_cfg.periph_inc       = true;
    dma_cfg.mem_inc          = true;
    dma_cfg.periph_data_size = DMA_DATA_SIZE_WORD;
    dma_cfg.mem_data_size    = DMA_DATA_SIZE_WORD;
    dma_cfg.periph_burst_size= DMA_BURST_SINGLE;
    dma_cfg.mem_burst_size   = DMA_BURST_SINGLE;
    dma_cfg.circular_mode    = false;
    dma_cfg.double_buffer    = false;
    dma_cfg.fifo_threshold   = DMA_FIFO_THRESHOLD_FULL;
    dma_cfg.use_scatter_gather = false;

    dma_configure_stream(DMA1_BASE, &dma_cfg);
    printf("  Stream 0 configured: Mem-to-Mem, word size\n");

    uint32_t src_buf[64];
    uint32_t dst_buf[64];
    for (int i = 0; i < 64; i++) src_buf[i] = (uint32_t)(i * 0x01010101);
    memset(dst_buf, 0, sizeof(dst_buf));

    dma_set_source(DMA1_BASE, DMA_STREAM_0, (uint32_t)(uintptr_t)src_buf);
    dma_set_destination(DMA1_BASE, DMA_STREAM_0, (uint32_t)(uintptr_t)dst_buf);
    dma_set_transfer_count(DMA1_BASE, DMA_STREAM_0, 64);
    printf("  Source: %p, Dest: %p, Count: 64 words\n", (void *)src_buf, (void *)dst_buf);

    dma_enable_stream(DMA1_BASE, DMA_STREAM_0);
    bool enabled = dma_is_stream_enabled(DMA1_BASE, DMA_STREAM_0);
    printf("  Stream 0 enabled: %s\n", enabled ? "yes" : "no");

    dma_disable_stream(DMA1_BASE, DMA_STREAM_0);
    printf("  Stream 0 disabled\n");

    uint32_t remaining = dma_get_transfer_count(DMA1_BASE, DMA_STREAM_0);
    printf("  Remaining transfers: %lu\n", (unsigned long)remaining);

    bool fe = dma_is_fifo_error(DMA1_BASE, DMA_STREAM_0);
    printf("  FIFO error: %s\n", fe ? "yes" : "no");
}

static void demo_scatter_gather(void) {
    printf("\n=== DMA Scatter-Gather Demo ===\n");

    dma_sg_descriptor_t *sg_list = dma_sg_allocate_list(3);
    printf("  Allocated 3 SG descriptors\n");

    uint32_t buf1[16], buf2[16], buf3[16];
    dma_sg_set_descriptor(&sg_list[0], (uint32_t)(uintptr_t)buf1, (uint32_t)(uintptr_t)buf1, &sg_list[1], 16);
    dma_sg_set_descriptor(&sg_list[1], (uint32_t)(uintptr_t)buf2, (uint32_t)(uintptr_t)buf2, &sg_list[2], 16);
    dma_sg_set_descriptor(&sg_list[2], (uint32_t)(uintptr_t)buf3, (uint32_t)(uintptr_t)buf3, NULL, 16);
    printf("  SG chain: desc0 -> desc1 -> desc2 -> NULL\n");
    printf("  desc0: src=%p dest=%p next=%p size=%lu\n",
           (void *)(uintptr_t)sg_list[0].src_addr,
           (void *)(uintptr_t)sg_list[0].dest_addr,
           (void *)(uintptr_t)sg_list[0].next_descriptor,
           (unsigned long)sg_list[0].control);
    printf("  desc1: src=%p dest=%p next=%p size=%lu\n",
           (void *)(uintptr_t)sg_list[1].src_addr,
           (void *)(uintptr_t)sg_list[1].dest_addr,
           (void *)(uintptr_t)sg_list[1].next_descriptor,
           (unsigned long)sg_list[1].control);
    printf("  desc2: src=%p dest=%p next=%p size=%lu (last)\n",
           (void *)(uintptr_t)sg_list[2].src_addr,
           (void *)(uintptr_t)sg_list[2].dest_addr,
           (void *)(uintptr_t)sg_list[2].next_descriptor,
           (unsigned long)sg_list[2].control);

    dma_sg_free_list(sg_list);
    printf("  SG descriptors freed\n");
}

int main(void) {
    printf("╔═══════════════════════════════════════╗\n");
    printf("║  mini-mcu Memory/NVIC/DMA Demo        ║\n");
    printf("╚═══════════════════════════════════════╝\n");

    demo_memory_map();
    demo_bitband();
    demo_nvic_priority();
    demo_dma();
    demo_scatter_gather();

    printf("\n=== All memory/NVIC/DMA demos complete ===\n");
    return 0;
}
