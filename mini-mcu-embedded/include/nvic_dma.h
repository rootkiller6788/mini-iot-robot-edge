#ifndef NVIC_DMA_H
#define NVIC_DMA_H

#include <stdbool.h>
#include <stdint.h>

/* ── NVIC IRQ numbers (Cortex-M4 specific subset) ────── */
#define NVIC_IRQ_WWDG          0
#define NVIC_IRQ_PVD           1
#define NVIC_IRQ_TAMP_STAMP    2
#define NVIC_IRQ_RTC_WKUP      3
#define NVIC_IRQ_FLASH         4
#define NVIC_IRQ_RCC           5
#define NVIC_IRQ_EXTI0         6
#define NVIC_IRQ_EXTI1         7
#define NVIC_IRQ_EXTI2         8
#define NVIC_IRQ_EXTI3         9
#define NVIC_IRQ_EXTI4         10
#define NVIC_IRQ_DMA1_STREAM0  11
#define NVIC_IRQ_DMA1_STREAM1  12
#define NVIC_IRQ_DMA1_STREAM2  13
#define NVIC_IRQ_DMA1_STREAM3  14
#define NVIC_IRQ_DMA1_STREAM4  15
#define NVIC_IRQ_DMA1_STREAM5  16
#define NVIC_IRQ_DMA1_STREAM6  17
#define NVIC_IRQ_ADC           18
#define NVIC_IRQ_EXTI9_5       23
#define NVIC_IRQ_TIM1_BRK_TIM9 24
#define NVIC_IRQ_TIM1_UP_TIM10 25
#define NVIC_IRQ_TIM1_TRG_COM_TIM11 26
#define NVIC_IRQ_TIM1_CC       27
#define NVIC_IRQ_TIM2          28
#define NVIC_IRQ_TIM3          29
#define NVIC_IRQ_TIM4          30
#define NVIC_IRQ_I2C1_EV       31
#define NVIC_IRQ_I2C1_ER       32
#define NVIC_IRQ_I2C2_EV       33
#define NVIC_IRQ_I2C2_ER       34
#define NVIC_IRQ_SPI1          35
#define NVIC_IRQ_SPI2          36
#define NVIC_IRQ_USART1        37
#define NVIC_IRQ_USART2        38
#define NVIC_IRQ_USART3        39
#define NVIC_IRQ_EXTI15_10     40
#define NVIC_IRQ_RTC_ALARM     41
#define NVIC_IRQ_DMA2_STREAM0  56
#define NVIC_IRQ_DMA2_STREAM1  57
#define NVIC_IRQ_DMA2_STREAM2  58
#define NVIC_IRQ_DMA2_STREAM3  59
#define NVIC_IRQ_DMA2_STREAM4  60
#define NVIC_IRQ_DMA2_STREAM5  68
#define NVIC_IRQ_DMA2_STREAM6  69
#define NVIC_IRQ_DMA2_STREAM7  70
#define NVIC_IRQ_USART6        71
#define NVIC_IRQ_I2C3_EV       72
#define NVIC_IRQ_I2C3_ER       73
#define NVIC_IRQ_FPU           81
#define NVIC_IRQ_SPI4          84

/* ── NVIC priority configuration ─────────────────────── */
#define NVIC_PRIORITY_HIGHEST  0x00U
#define NVIC_PRIORITY_HIGH     0x40U
#define NVIC_PRIORITY_MEDIUM   0x80U
#define NVIC_PRIORITY_LOW      0xC0U
#define NVIC_PRIORITY_LOWEST   0xF0U

#define NVIC_SUBPRIORITY_0     0x00U
#define NVIC_SUBPRIORITY_1     0x10U
#define NVIC_SUBPRIORITY_2     0x20U
#define NVIC_SUBPRIORITY_3     0x30U

/* ── Interrupt handler function type ─────────────────── */
typedef void (*irq_handler_t)(void);

/* ── Tail-chaining & late-arriving configuration ─────── */
typedef struct {
    uint8_t irq_num;
    uint8_t preemption_priority;
    uint8_t subpriority;
    irq_handler_t handler;
    bool tail_chain_enabled;
    bool late_arrival_enabled;
} nvic_irq_config_t;

typedef struct {
    nvic_irq_config_t handlers[NVIC_MAX_IRQ];
    uint32_t handler_count;
    bool is_initialized;
} nvic_handler_table_t;

/* ── NVIC API ────────────────────────────────────────── */
void nvic_init(void);
void nvic_enable_irq(uint8_t irq_num);
void nvic_disable_irq(uint8_t irq_num);
void nvic_set_pending(uint8_t irq_num);
void nvic_clear_pending(uint8_t irq_num);
bool nvic_get_pending(uint8_t irq_num);
bool nvic_get_active(uint8_t irq_num);
void nvic_set_priority(uint8_t irq_num, uint8_t priority);
uint8_t nvic_get_priority(uint8_t irq_num);

bool nvic_register_handler(uint8_t irq_num, irq_handler_t handler,
    uint8_t priority);
bool nvic_unregister_handler(uint8_t irq_num);
irq_handler_t nvic_get_handler(uint8_t irq_num);
void nvic_set_tail_chaining(uint8_t irq_num, bool enable);
void nvic_set_late_arrival(uint8_t irq_num, bool enable);
void nvic_enable_all_interrupts(void);
void nvic_disable_all_interrupts(void);
void nvic_system_reset(void);
uint32_t nvic_get_active_vector(void);
uint32_t nvic_get_pending_vector(void);

/* ── DMA base addresses ──────────────────────────────── */
#define DMA1_BASE             0x40026000UL
#define DMA2_BASE             0x40026400UL

#define DMA_STREAM_0          0
#define DMA_STREAM_1          1
#define DMA_STREAM_2          2
#define DMA_STREAM_3          3
#define DMA_STREAM_4          4
#define DMA_STREAM_5          5
#define DMA_STREAM_6          6
#define DMA_STREAM_7          7

#define DMA_MAX_STREAMS       8

/* ── DMA channel selection ───────────────────────────── */
#define DMA_CHANNEL_0         0x0U
#define DMA_CHANNEL_1         0x1U
#define DMA_CHANNEL_2         0x2U
#define DMA_CHANNEL_3         0x3U
#define DMA_CHANNEL_4         0x4U
#define DMA_CHANNEL_5         0x5U
#define DMA_CHANNEL_6         0x6U
#define DMA_CHANNEL_7         0x7U

/* ── DMA transfer direction ──────────────────────────── */
#define DMA_DIR_PERIPH_TO_MEM  0x0U
#define DMA_DIR_MEM_TO_PERIPH  0x1U
#define DMA_DIR_MEM_TO_MEM     0x2U

/* ── DMA data size ───────────────────────────────────── */
#define DMA_DATA_SIZE_BYTE     0x0U
#define DMA_DATA_SIZE_HALFWORD 0x1U
#define DMA_DATA_SIZE_WORD     0x2U

/* ── DMA burst size ──────────────────────────────────── */
#define DMA_BURST_SINGLE       0x0U
#define DMA_BURST_INCR4        0x1U
#define DMA_BURST_INCR8        0x2U
#define DMA_BURST_INCR16       0x3U

/* ── DMA FIFO threshold ──────────────────────────────── */
#define DMA_FIFO_THRESHOLD_1_4   0x0U
#define DMA_FIFO_THRESHOLD_1_2   0x1U
#define DMA_FIFO_THRESHOLD_3_4   0x2U
#define DMA_FIFO_THRESHOLD_FULL  0x3U

/* ── DMA flow control ────────────────────────────────── */
#define DMA_FLOW_CTRL_DMA       0x0U
#define DMA_FLOW_CTRL_PERIPH    0x1U

/* ── DMA mode ────────────────────────────────────────── */
#define DMA_MODE_NORMAL         0x0U
#define DMA_MODE_CIRCULAR       0x1U

/* ── DMA interrupt flags ─────────────────────────────── */
#define DMA_IT_TCIF             (1UL << 5)
#define DMA_IT_HTIF             (1UL << 4)
#define DMA_IT_TEIF             (1UL << 3)
#define DMA_IT_DMEIF            (1UL << 2)
#define DMA_IT_FEIF             (1UL << 0)

/* ── DMA priority ────────────────────────────────────── */
#define DMA_PRIORITY_LOW        0x0U
#define DMA_PRIORITY_MEDIUM     0x1U
#define DMA_PRIORITY_HIGH       0x2U
#define DMA_PRIORITY_VERY_HIGH  0x3U

/* ── Scatter-gather list descriptor ──────────────────── */
typedef struct __attribute__((aligned(16))) {
    uint32_t src_addr;
    uint32_t dest_addr;
    uint32_t next_descriptor;
    uint32_t control;
} dma_sg_descriptor_t;

typedef struct {
    uint32_t dma_base;
    uint8_t stream;
    uint8_t channel;
    uint8_t direction;
    bool periph_inc;
    bool mem_inc;
    uint8_t periph_data_size;
    uint8_t mem_data_size;
    uint8_t periph_burst_size;
    uint8_t mem_burst_size;
    bool circular_mode;
    bool double_buffer;
    uint8_t fifo_threshold;
    bool use_scatter_gather;
    dma_sg_descriptor_t *sg_list;
    uint32_t sg_count;
} dma_config_t;

typedef struct {
    uint8_t stream;
    dma_config_t config;
    bool is_active;
    uint32_t total_transferred;
    uint32_t remaining;
} dma_transfer_t;

/* ── DMA callback type ───────────────────────────────── */
typedef void (*dma_callback_t)(dma_transfer_t *transfer);

/* ── DMA API ─────────────────────────────────────────── */
void dma_init(uint32_t dma_base);
void dma_configure_stream(uint32_t dma_base, const dma_config_t *config);
void dma_set_source(uint32_t dma_base, uint8_t stream, uint32_t addr);
void dma_set_destination(uint32_t dma_base, uint8_t stream, uint32_t addr);
void dma_set_transfer_count(uint32_t dma_base, uint8_t stream, uint32_t count);
void dma_enable_stream(uint32_t dma_base, uint8_t stream);
void dma_disable_stream(uint32_t dma_base, uint8_t stream);
bool dma_is_stream_enabled(uint32_t dma_base, uint8_t stream);
void dma_start_transfer(uint32_t dma_base, dma_transfer_t *transfer);
void dma_stop_transfer(uint32_t dma_base, uint8_t stream);
bool dma_wait_for_completion(uint32_t dma_base, uint8_t stream,
    uint32_t timeout_ms);
void dma_enable_interrupt(uint32_t dma_base, uint8_t stream, uint32_t flags);
void dma_disable_interrupt(uint32_t dma_base, uint8_t stream, uint32_t flags);
void dma_register_callback(uint32_t dma_base, uint8_t stream,
    dma_callback_t callback);
void dma_clear_interrupt_flags(uint32_t dma_base, uint8_t stream,
    uint32_t flags);
uint32_t dma_get_transfer_count(uint32_t dma_base, uint8_t stream);
bool dma_is_fifo_error(uint32_t dma_base, uint8_t stream);

/* ── Scatter-gather API ──────────────────────────────── */
void dma_sg_configure(uint32_t dma_base, uint8_t stream,
    dma_sg_descriptor_t *descriptors, uint32_t count);
void dma_sg_start(uint32_t dma_base, uint8_t stream);
dma_sg_descriptor_t *dma_sg_allocate_list(uint32_t count);
void dma_sg_free_list(dma_sg_descriptor_t *list);
void dma_sg_set_descriptor(dma_sg_descriptor_t *desc,
    uint32_t src_addr, uint32_t dest_addr,
    dma_sg_descriptor_t *next, uint32_t transfer_size);

#endif /* NVIC_DMA_H */
