#ifndef CORTEX_M_CORE_H
#define CORTEX_M_CORE_H

#include <stdbool.h>
#include <stdint.h>

#define NVIC_BASE             0xE000E100UL
#define SCB_BASE              0xE000ED00UL
#define SYSTICK_BASE          0xE000E010UL
#define MPU_BASE              0xE000ED90UL
#define ITM_BASE              0xE0000000UL
#define DWT_BASE              0xE0001000UL
#define FPB_BASE              0xE0002000UL
#define TPIU_BASE             0xE0040000UL

/* ── NVIC registers ──────────────────────────────────── */
#define NVIC_ISER_BASE        (NVIC_BASE + 0x000UL) /* Interrupt Set-Enable   */
#define NVIC_ICER_BASE        (NVIC_BASE + 0x080UL) /* Interrupt Clear-Enable */
#define NVIC_ISPR_BASE        (NVIC_BASE + 0x100UL) /* Interrupt Set-Pending  */
#define NVIC_ICPR_BASE        (NVIC_BASE + 0x180UL) /* Interrupt Clear-Pending*/
#define NVIC_IABR_BASE        (NVIC_BASE + 0x200UL) /* Interrupt Active Bit   */
#define NVIC_IPR_BASE         (NVIC_BASE + 0x300UL) /* Interrupt Priority     */
#define NVIC_STIR             (*(volatile uint32_t *)(NVIC_BASE + 0xE00UL))

/* ── SysTick registers ────────────────────────────────── */
#define SYSTICK_CSR           (*(volatile uint32_t *)(SYSTICK_BASE + 0x00UL))
#define SYSTICK_RVR           (*(volatile uint32_t *)(SYSTICK_BASE + 0x04UL))
#define SYSTICK_CVR           (*(volatile uint32_t *)(SYSTICK_BASE + 0x08UL))
#define SYSTICK_CALIB         (*(volatile uint32_t *)(SYSTICK_BASE + 0x0CUL))

/* ── SCB registers ───────────────────────────────────── */
#define SCB_CPUID             (*(volatile const uint32_t *)(SCB_BASE + 0x00UL))
#define SCB_ICSR              (*(volatile uint32_t *)(SCB_BASE + 0x04UL))
#define SCB_VTOR              (*(volatile uint32_t *)(SCB_BASE + 0x08UL))
#define SCB_AIRCR             (*(volatile uint32_t *)(SCB_BASE + 0x0CUL))
#define SCB_SCR               (*(volatile uint32_t *)(SCB_BASE + 0x10UL))
#define SCB_CCR               (*(volatile uint32_t *)(SCB_BASE + 0x14UL))
#define SCB_SHPR1             (*(volatile uint32_t *)(SCB_BASE + 0x18UL))
#define SCB_SHPR2             (*(volatile uint32_t *)(SCB_BASE + 0x1CUL))
#define SCB_SHPR3             (*(volatile uint32_t *)(SCB_BASE + 0x20UL))
#define SCB_SHCSR             (*(volatile uint32_t *)(SCB_BASE + 0x24UL))
#define SCB_CFSR              (*(volatile uint32_t *)(SCB_BASE + 0x28UL))
#define SCB_HFSR              (*(volatile uint32_t *)(SCB_BASE + 0x2CUL))
#define SCB_DFSR              (*(volatile uint32_t *)(SCB_BASE + 0x30UL))
#define SCB_MMFAR             (*(volatile uint32_t *)(SCB_BASE + 0x34UL))
#define SCB_BFAR              (*(volatile uint32_t *)(SCB_BASE + 0x38UL))
#define SCB_AFSR              (*(volatile uint32_t *)(SCB_BASE + 0x3CUL))

/* ── MPU registers ───────────────────────────────────── */
#define MPU_TYPE              (*(volatile const uint32_t *)(MPU_BASE + 0x00UL))
#define MPU_CTRL              (*(volatile uint32_t *)(MPU_BASE + 0x04UL))
#define MPU_RNR               (*(volatile uint32_t *)(MPU_BASE + 0x08UL))
#define MPU_RBAR              (*(volatile uint32_t *)(MPU_BASE + 0x0CUL))
#define MPU_RASR              (*(volatile uint32_t *)(MPU_BASE + 0x10UL))

/* ── ICSR bit definitions ────────────────────────────── */
#define ICSR_VECTACTIVE_MASK   0x000001FFUL
#define ICSR_VECTPENDING_MASK  0x003FF000UL
#define ICSR_RETTOBASE         (1UL << 11)
#define ICSR_ISRPENDING        (1UL << 22)
#define ICSR_PENDSTCLR          (1UL << 25)
#define ICSR_PENDSTSET          (1UL << 26)
#define ICSR_PENDSVCLR          (1UL << 27)
#define ICSR_PENDSVSET          (1UL << 28)
#define ICSR_NMIPENDSET         (1UL << 31)

/* ── AIRCR bit definitions ───────────────────────────── */
#define AIRCR_VECTKEY_MASK      0xFFFF0000UL
#define AIRCR_VECTKEY_VALUE     0x05FA0000UL
#define AIRCR_PRIGROUP_POS      8
#define AIRCR_PRIGROUP_MASK     0x00000700UL
#define AIRCR_ENDIANESS          (1UL << 15)
#define AIRCR_SYSRESETREQ        (1UL << 2)
#define AIRCR_VECTCLRACTIVE      (1UL << 1)

/* ── SCR bit definitions ─────────────────────────────── */
#define SCR_SLEEPONEXIT          (1UL << 1)
#define SCR_SLEEPDEEP            (1UL << 2)
#define SCR_SEVEONPEND           (1UL << 4)

/* ── CCR bit definitions ─────────────────────────────── */
#define CCR_DIV_0_TRAP          (1UL << 4)
#define CCR_UNALIGN_TRAP        (1UL << 3)
#define CCR_DATA_ENDIANESS      (1UL << 18)
#define CCR_STKALIGN            (1UL << 9)

/* ── SYSTICK_CSR bit definitions ─────────────────────── */
#define SYSTICK_CSR_ENABLE       (1UL << 0)
#define SYSTICK_CSR_TICKINT      (1UL << 1)
#define SYSTICK_CSR_CLKSOURCE    (1UL << 2)
#define SYSTICK_CSR_COUNTFLAG    (1UL << 16)

/* ── SHCSR bit definitions ───────────────────────────── */
#define SHCSR_MEMFAULTENA        (1UL << 16)
#define SHCSR_BUSFAULTENA        (1UL << 17)
#define SHCSR_USGFAULTENA        (1UL << 18)

/* ── MPU control ─────────────────────────────────────── */
#define MPU_CTRL_ENABLE          (1UL << 0)
#define MPU_CTRL_HFNMIENA        (1UL << 1)
#define MPU_CTRL_PRIVDEFENA      (1UL << 2)

#define MPU_RASR_ENABLE          (1UL << 0)
#define MPU_RASR_SRD_POS         8
#define MPU_RASR_SIZE_POS        1
#define MPU_RASR_AP_POS          24
#define MPU_RASR_XN_POS          28
#define MPU_RASR_TEX_POS         19
#define MPU_RASR_B_POS           22
#define MPU_RASR_C_POS           24
#define MPU_RASR_S_POS           18

#define MPU_RBAR_VALID           (1UL << 4)
#define MPU_RBAR_REGION_POS      0

/* ── Vector table indices ────────────────────────────── */
#define VECTOR_INITIAL_SP        0
#define VECTOR_RESET             1
#define VECTOR_NMI               2
#define VECTOR_HARD_FAULT        3
#define VECTOR_MEMMANAGE         4
#define VECTOR_BUS_FAULT         5
#define VECTOR_USAGE_FAULT       6
#define VECTOR_SV_CALL           11
#define VECTOR_DEBUG_MONITOR     12
#define VECTOR_PEND_SV           14
#define VECTOR_SYS_TICK          15
#define VECTOR_EXT_IRQ_BASE      16

#define NVIC_MAX_IRQ             240
#define NVIC_PRIORITY_BITS       4
#define NVIC_PRIORITY_MASK       0xF0U

/* ── Processor mode type ─────────────────────────────── */
typedef enum {
    PROCESSOR_MODE_HANDLER = 0,
    PROCESSOR_MODE_THREAD  = 1
} processor_mode_t;

typedef enum {
    PRIGROUP_DEFAULT     = 0,
    PRIGROUP_NVIC_16_0   = 0, /* 16 group priorities,  0 sub-priorities */
    PRIGROUP_NVIC_8_1    = 1, /*  8 group priorities,  1 sub-priority   */
    PRIGROUP_NVIC_4_2    = 2, /*  4 group priorities,  2 sub-priorities */
    PRIGROUP_NVIC_2_3    = 3, /*  2 group priorities,  3 sub-priorities */
    PRIGROUP_NVIC_1_4    = 4, /*  1 group priority,    4 sub-priorities */
    PRIGROUP_NVIC_0_5    = 5, /*  0 group,             5 sub-priorities */
    PRIGROUP_NVIC_0_6    = 6,
    PRIGROUP_NVIC_0_7    = 7
} priority_grouping_t;

typedef enum {
    MPU_REGION_SIZE_32B    = 4,
    MPU_REGION_SIZE_64B    = 5,
    MPU_REGION_SIZE_128B   = 6,
    MPU_REGION_SIZE_256B   = 7,
    MPU_REGION_SIZE_512B   = 8,
    MPU_REGION_SIZE_1K     = 9,
    MPU_REGION_SIZE_2K     = 10,
    MPU_REGION_SIZE_4K     = 11,
    MPU_REGION_SIZE_8K     = 12,
    MPU_REGION_SIZE_16K    = 13,
    MPU_REGION_SIZE_32K    = 14,
    MPU_REGION_SIZE_64K    = 15,
    MPU_REGION_SIZE_128K   = 16,
    MPU_REGION_SIZE_256K   = 17,
    MPU_REGION_SIZE_512K   = 18,
    MPU_REGION_SIZE_1M     = 19,
    MPU_REGION_SIZE_2M     = 20,
    MPU_REGION_SIZE_4M     = 21,
    MPU_REGION_SIZE_8M     = 22,
    MPU_REGION_SIZE_16M    = 23,
    MPU_REGION_SIZE_32M    = 24,
    MPU_REGION_SIZE_64M    = 25,
    MPU_REGION_SIZE_128M   = 26,
    MPU_REGION_SIZE_256M   = 27,
    MPU_REGION_SIZE_512M   = 28,
    MPU_REGION_SIZE_1G     = 29,
    MPU_REGION_SIZE_2G     = 30,
    MPU_REGION_SIZE_4G     = 31
} mpu_region_size_t;

typedef enum {
    MPU_ACCESS_NO_ACCESS           = 0x0,
    MPU_ACCESS_PRIVILEGED_RW       = 0x1,
    MPU_ACCESS_PRIVILEGED_RO       = 0x2,
    MPU_ACCESS_FULL                = 0x3,
    MPU_ACCESS_PRIVILEGED_RO_USER_RO = 0x5,
    MPU_ACCESS_RW                  = 0x6,
    MPU_ACCESS_RO                  = 0x7
} mpu_access_permission_t;

typedef struct {
    uint32_t initial_sp;
    void (*reset_handler)(void);
    void (*nmi_handler)(void);
    void (*hard_fault_handler)(void);
    void (*memmanage_handler)(void);
    void (*bus_fault_handler)(void);
    void (*usage_fault_handler)(void);
    void (*reserved_7_10)[4];
    void (*svcall_handler)(void);
    void (*debug_monitor_handler)(void);
    void (*reserved_13);
    void (*pend_sv_handler)(void);
    void (*systick_handler)(void);
    void (*ext_irq_handler[NVIC_MAX_IRQ])(void);
} vector_table_t;

/* ── NVIC API ────────────────────────────────────────── */
void cortex_m4_nvic_enable_irq(uint8_t irq_num);
void cortex_m4_nvic_disable_irq(uint8_t irq_num);
void cortex_m4_nvic_set_pending(uint8_t irq_num);
void cortex_m4_nvic_clear_pending(uint8_t irq_num);
bool cortex_m4_nvic_is_pending(uint8_t irq_num);
bool cortex_m4_nvic_is_active(uint8_t irq_num);
void cortex_m4_nvic_set_priority(uint8_t irq_num, uint8_t priority);
uint8_t cortex_m4_nvic_get_priority(uint8_t irq_num);
void cortex_m4_nvic_trigger_sw_interrupt(uint8_t irq_num);

/* ── SysTick API ─────────────────────────────────────── */
void cortex_m4_systick_init(uint32_t reload_value);
void cortex_m4_systick_enable(void);
void cortex_m4_systick_disable(void);
void cortex_m4_systick_enable_interrupt(void);
void cortex_m4_systick_disable_interrupt(void);
bool cortex_m4_systick_has_expired(void);
uint32_t cortex_m4_systick_get_current(void);
uint32_t cortex_m4_systick_get_calibration(void);

/* ── SCB API ─────────────────────────────────────────── */
void cortex_m4_scb_set_priority_grouping(priority_grouping_t grouping);
priority_grouping_t cortex_m4_scb_get_priority_grouping(void);
void cortex_m4_scb_system_reset(void);
void cortex_m4_scb_set_vector_table_offset(uint32_t offset);
uint32_t cortex_m4_scb_get_vector_table_offset(void);
uint32_t cortex_m4_scb_get_cpu_id(void);
void cortex_m4_scb_enable_fault_handlers(void);
void cortex_m4_scb_set_sleep_on_exit(bool enable);
void cortex_m4_scb_set_deep_sleep(bool enable);
void cortex_m4_scb_set_sev_on_pend(bool enable);
void cortex_m4_scb_set_div_by_zero_trap(bool enable);
void cortex_m4_scb_set_unaligned_access_trap(bool enable);

/* ── MPU API ─────────────────────────────────────────── */
void cortex_m4_mpu_enable(void);
void cortex_m4_mpu_disable(void);
void cortex_m4_mpu_enable_hard_fault_nmi(bool enable);
void cortex_m4_mpu_enable_default_map(bool enable);
void cortex_m4_mpu_configure_region(uint8_t region, uint32_t base_addr,
    mpu_region_size_t size, mpu_access_permission_t access,
    uint8_t tex, bool cacheable, bool bufferable, bool shareable, bool execute_never);
void cortex_m4_mpu_enable_region(uint8_t region);
void cortex_m4_mpu_disable_region(uint8_t region);

/* ── Processor utility ───────────────────────────────── */
processor_mode_t cortex_m4_get_processor_mode(void);
void cortex_m4_wait_for_interrupt(void);
void cortex_m4_wait_for_event(void);
void cortex_m4_disable_interrupts(void);
void cortex_m4_enable_interrupts(void);

#endif /* CORTEX_M_CORE_H */
