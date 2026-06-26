#include "nvic_dma.h"
#include "cortex_m_core.h"
#include <stdlib.h>
#include <string.h>

#define DMA_SxCR_OFFSET(s)    (0x10UL + ((s) * 0x18UL))
#define DMA_SxNDTR_OFFSET(s)  (0x14UL + ((s) * 0x18UL))
#define DMA_SxPAR_OFFSET(s)   (0x18UL + ((s) * 0x18UL))
#define DMA_SxM0AR_OFFSET(s)  (0x1CUL + ((s) * 0x18UL))
#define DMA_SxFCR_OFFSET(s)   (0x20UL + ((s) * 0x18UL))

#define DMA_LISR_OFFSET       0x00UL
#define DMA_HISR_OFFSET       0x04UL
#define DMA_LIFCR_OFFSET      0x08UL
#define DMA_HIFCR_OFFSET      0x0CUL

#define DMA_SxCR_EN            (1UL << 0)
#define DMA_SxCR_DMEIE         (1UL << 1)
#define DMA_SxCR_TEIE          (1UL << 2)
#define DMA_SxCR_HTIE          (1UL << 3)
#define DMA_SxCR_TCIE          (1UL << 4)
#define DMA_SxCR_PFCTRL        (1UL << 5)
#define DMA_SxCR_DIR_POS       6
#define DMA_SxCR_CIRC          (1UL << 8)
#define DMA_SxCR_PINC          (1UL << 9)
#define DMA_SxCR_MINC          (1UL << 10)
#define DMA_SxCR_PSIZE_POS     11
#define DMA_SxCR_MSIZE_POS     13
#define DMA_SxCR_DBM           (1UL << 18)
#define DMA_SxCR_CT            (1UL << 19)
#define DMA_SxCR_PBURST_POS    21
#define DMA_SxCR_MBURST_POS    23
#define DMA_SxCR_CHSEL_POS     25

#define DMA_SxFCR_FTH_POS      0
#define DMA_SxFCR_DMDIS        (1UL << 2)

#define DMA_STREAM_MASK(s)     (0x3FUL << ((s) * 6))
#define DMA_STREAM_SHIFT(s)    ((s) * 6)

static dma_callback_t dma_callbacks[2][DMA_MAX_STREAMS];

static uint32_t dma_get_stream_mask(uint8_t stream) {
    uint32_t group = (stream < 4) ? 0 : 1;
    uint32_t pos   = (stream % 4);
    return (0x3FUL << (pos * 6));
}

void nvic_init(void) {
    uint32_t air = SCB_AIRCR;
    air &= ~(AIRCR_VECTKEY_MASK | AIRCR_PRIGROUP_MASK);
    air |= AIRCR_VECTKEY_VALUE;
    air |= ((uint32_t)PRIGROUP_NVIC_4_2 << AIRCR_PRIGROUP_POS);
    SCB_AIRCR = air;
    __asm__ volatile("dsb");
    __asm__ volatile("isb");
}

void nvic_enable_irq(uint8_t irq_num) {
    cortex_m4_nvic_enable_irq(irq_num);
}

void nvic_disable_irq(uint8_t irq_num) {
    cortex_m4_nvic_disable_irq(irq_num);
}

void nvic_set_pending(uint8_t irq_num) {
    cortex_m4_nvic_set_pending(irq_num);
}

void nvic_clear_pending(uint8_t irq_num) {
    cortex_m4_nvic_clear_pending(irq_num);
}

bool nvic_get_pending(uint8_t irq_num) {
    return cortex_m4_nvic_is_pending(irq_num);
}

bool nvic_get_active(uint8_t irq_num) {
    return cortex_m4_nvic_is_active(irq_num);
}

void nvic_set_priority(uint8_t irq_num, uint8_t priority) {
    cortex_m4_nvic_set_priority(irq_num, priority);
}

uint8_t nvic_get_priority(uint8_t irq_num) {
    return cortex_m4_nvic_get_priority(irq_num);
}

bool nvic_register_handler(uint8_t irq_num, irq_handler_t handler,
    uint8_t priority)
{
    if (irq_num >= NVIC_MAX_IRQ) return false;
    nvic_set_priority(irq_num, priority);
    nvic_enable_irq(irq_num);
    return true;
}

bool nvic_unregister_handler(uint8_t irq_num) {
    if (irq_num >= NVIC_MAX_IRQ) return false;
    nvic_disable_irq(irq_num);
    return true;
}

irq_handler_t nvic_get_handler(uint8_t irq_num) {
    (void)irq_num;
    return NULL;
}

void nvic_set_tail_chaining(uint8_t irq_num, bool enable) {
    (void)irq_num;
    (void)enable;
}

void nvic_set_late_arrival(uint8_t irq_num, bool enable) {
    (void)irq_num;
    (void)enable;
}

void nvic_enable_all_interrupts(void) {
    cortex_m4_enable_interrupts();
}

void nvic_disable_all_interrupts(void) {
    cortex_m4_disable_interrupts();
}

void nvic_system_reset(void) {
    cortex_m4_scb_system_reset();
}

uint32_t nvic_get_active_vector(void) {
    return SCB_ICSR & ICSR_VECTACTIVE_MASK;
}

uint32_t nvic_get_pending_vector(void) {
    return (SCB_ICSR & ICSR_VECTPENDING_MASK) >> 12;
}

void dma_init(uint32_t dma_base) {
    memset(dma_callbacks, 0, sizeof(dma_callbacks));
    for (uint8_t s = 0; s < DMA_MAX_STREAMS; s++) {
        volatile uint32_t *cr = (volatile uint32_t *)(dma_base + DMA_SxCR_OFFSET(s));
        *cr = 0;
    }
    (void)dma_base;
}

void dma_configure_stream(uint32_t dma_base, const dma_config_t *config) {
    uint8_t s = config->stream;
    volatile uint32_t *cr = (volatile uint32_t *)(dma_base + DMA_SxCR_OFFSET(s));
    volatile uint32_t *fcr = (volatile uint32_t *)(dma_base + DMA_SxFCR_OFFSET(s));
    *cr = 0;
    uint32_t reg = 0;
    reg |= ((uint32_t)config->channel << DMA_SxCR_CHSEL_POS);
    if (config->direction == DMA_DIR_MEM_TO_PERIPH) {
        reg |= (1UL << DMA_SxCR_DIR_POS);
    }
    if (config->direction == DMA_DIR_MEM_TO_MEM) {
        reg |= (2UL << DMA_SxCR_DIR_POS);
    }
    if (config->periph_inc)  reg |= DMA_SxCR_PINC;
    if (config->mem_inc)     reg |= DMA_SxCR_MINC;
    if (config->circular_mode) reg |= DMA_SxCR_CIRC;
    if (config->double_buffer) reg |= DMA_SxCR_DBM;
    reg |= ((uint32_t)config->periph_data_size << DMA_SxCR_PSIZE_POS);
    reg |= ((uint32_t)config->mem_data_size << DMA_SxCR_MSIZE_POS);
    reg |= ((uint32_t)config->periph_burst_size << DMA_SxCR_PBURST_POS);
    reg |= ((uint32_t)config->mem_burst_size << DMA_SxCR_MBURST_POS);
    *cr = reg;
    uint32_t fcr_val = (uint32_t)config->fifo_threshold & 0x3U;
    if (!config->use_scatter_gather) {
        fcr_val |= DMA_SxFCR_DMDIS;
    }
    *fcr = fcr_val;
}

void dma_set_source(uint32_t dma_base, uint8_t stream, uint32_t addr) {
    volatile uint32_t *par = (volatile uint32_t *)(dma_base + DMA_SxPAR_OFFSET(stream));
    *par = addr;
}

void dma_set_destination(uint32_t dma_base, uint8_t stream, uint32_t addr) {
    volatile uint32_t *m0ar = (volatile uint32_t *)(dma_base + DMA_SxM0AR_OFFSET(stream));
    *m0ar = addr;
}

void dma_set_transfer_count(uint32_t dma_base, uint8_t stream, uint32_t count) {
    volatile uint32_t *ndtr = (volatile uint32_t *)(dma_base + DMA_SxNDTR_OFFSET(stream));
    *ndtr = count;
}

void dma_enable_stream(uint32_t dma_base, uint8_t stream) {
    volatile uint32_t *cr = (volatile uint32_t *)(dma_base + DMA_SxCR_OFFSET(stream));
    *cr |= DMA_SxCR_EN;
}

void dma_disable_stream(uint32_t dma_base, uint8_t stream) {
    volatile uint32_t *cr = (volatile uint32_t *)(dma_base + DMA_SxCR_OFFSET(stream));
    *cr &= ~DMA_SxCR_EN;
}

bool dma_is_stream_enabled(uint32_t dma_base, uint8_t stream) {
    volatile uint32_t *cr = (volatile uint32_t *)(dma_base + DMA_SxCR_OFFSET(stream));
    return (*cr & DMA_SxCR_EN) != 0;
}

void dma_start_transfer(uint32_t dma_base, dma_transfer_t *transfer) {
    dma_set_source(dma_base, transfer->stream, transfer->config.src_addr);
    dma_set_destination(dma_base, transfer->stream, transfer->config.dest_addr);
    dma_set_transfer_count(dma_base, transfer->stream, transfer->remaining);
    dma_enable_stream(dma_base, transfer->stream);
    transfer->is_active = true;
}

void dma_stop_transfer(uint32_t dma_base, uint8_t stream) {
    dma_disable_stream(dma_base, stream);
}

bool dma_wait_for_completion(uint32_t dma_base, uint8_t stream,
    uint32_t timeout_ms)
{
    volatile uint32_t *isr_addr;
    uint32_t tcif_mask;
    if (stream < 4) {
        isr_addr = (volatile uint32_t *)(dma_base + DMA_LISR_OFFSET);
    } else {
        isr_addr = (volatile uint32_t *)(dma_base + DMA_HISR_OFFSET);
    }
    tcif_mask = DMA_IT_TCIF << ((stream % 4) * 6);
    uint32_t elapsed = 0;
    while (!(*isr_addr & tcif_mask) && elapsed < timeout_ms) {
        elapsed++;
    }
    return elapsed < timeout_ms;
}

void dma_enable_interrupt(uint32_t dma_base, uint8_t stream, uint32_t flags) {
    volatile uint32_t *cr = (volatile uint32_t *)(dma_base + DMA_SxCR_OFFSET(stream));
    *cr |= flags;
}

void dma_disable_interrupt(uint32_t dma_base, uint8_t stream, uint32_t flags) {
    volatile uint32_t *cr = (volatile uint32_t *)(dma_base + DMA_SxCR_OFFSET(stream));
    *cr &= ~flags;
}

void dma_register_callback(uint32_t dma_base, uint8_t stream,
    dma_callback_t callback)
{
    uint32_t index = (dma_base == DMA2_BASE) ? 1 : 0;
    dma_callbacks[index][stream] = callback;
}

void dma_clear_interrupt_flags(uint32_t dma_base, uint8_t stream,
    uint32_t flags)
{
    volatile uint32_t *ifcr;
    if (stream < 4) {
        ifcr = (volatile uint32_t *)(dma_base + DMA_LIFCR_OFFSET);
    } else {
        ifcr = (volatile uint32_t *)(dma_base + DMA_HIFCR_OFFSET);
    }
    *ifcr = flags << ((stream % 4) * 6);
}

uint32_t dma_get_transfer_count(uint32_t dma_base, uint8_t stream) {
    volatile uint32_t *ndtr = (volatile uint32_t *)(dma_base + DMA_SxNDTR_OFFSET(stream));
    return *ndtr;
}

bool dma_is_fifo_error(uint32_t dma_base, uint8_t stream) {
    volatile uint32_t *isr_addr;
    uint32_t feif_mask;
    if (stream < 4) {
        isr_addr = (volatile uint32_t *)(dma_base + DMA_LISR_OFFSET);
    } else {
        isr_addr = (volatile uint32_t *)(dma_base + DMA_HISR_OFFSET);
    }
    feif_mask = DMA_IT_FEIF << ((stream % 4) * 6);
    return (*isr_addr & feif_mask) != 0;
}

void dma_sg_configure(uint32_t dma_base, uint8_t stream,
    dma_sg_descriptor_t *descriptors, uint32_t count)
{
    if (!descriptors || count == 0) return;
    volatile uint32_t *m0ar = (volatile uint32_t *)(dma_base + DMA_SxM0AR_OFFSET(stream));
    *m0ar = (uint32_t)(uintptr_t)descriptors;
}

void dma_sg_start(uint32_t dma_base, uint8_t stream) {
    dma_enable_stream(dma_base, stream);
}

dma_sg_descriptor_t *dma_sg_allocate_list(uint32_t count) {
    return (dma_sg_descriptor_t *)calloc(count, sizeof(dma_sg_descriptor_t));
}

void dma_sg_free_list(dma_sg_descriptor_t *list) {
    free(list);
}

void dma_sg_set_descriptor(dma_sg_descriptor_t *desc,
    uint32_t src_addr, uint32_t dest_addr,
    dma_sg_descriptor_t *next, uint32_t transfer_size)
{
    desc->src_addr = src_addr;
    desc->dest_addr = dest_addr;
    desc->next_descriptor = (next) ? (uint32_t)(uintptr_t)next : 0;
    desc->control = transfer_size;
}
