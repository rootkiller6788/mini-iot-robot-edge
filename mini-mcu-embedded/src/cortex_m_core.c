#include "cortex_m_core.h"
#include <stddef.h>

static volatile uint32_t *nvic_iser(uint8_t irq_num) {
    return (volatile uint32_t *)(NVIC_ISER_BASE + ((irq_num / 32UL) * 4UL));
}

static volatile uint32_t *nvic_icer(uint8_t irq_num) {
    return (volatile uint32_t *)(NVIC_ICER_BASE + ((irq_num / 32UL) * 4UL));
}

static volatile uint32_t *nvic_ispr(uint8_t irq_num) {
    return (volatile uint32_t *)(NVIC_ISPR_BASE + ((irq_num / 32UL) * 4UL));
}

static volatile uint32_t *nvic_icpr(uint8_t irq_num) {
    return (volatile uint32_t *)(NVIC_ICPR_BASE + ((irq_num / 32UL) * 4UL));
}

static volatile uint32_t *nvic_iabr(uint8_t irq_num) {
    return (volatile uint32_t *)(NVIC_IABR_BASE + ((irq_num / 32UL) * 4UL));
}

static volatile uint8_t *nvic_ipr(uint8_t irq_num) {
    return (volatile uint8_t *)(NVIC_IPR_BASE + irq_num);
}

void cortex_m4_nvic_enable_irq(uint8_t irq_num) {
    if (irq_num >= NVIC_MAX_IRQ) return;
    volatile uint32_t *reg = nvic_iser(irq_num);
    *reg = (1UL << (irq_num & 0x1FUL));
    __asm__ volatile("dsb");
    __asm__ volatile("isb");
}

void cortex_m4_nvic_disable_irq(uint8_t irq_num) {
    if (irq_num >= NVIC_MAX_IRQ) return;
    volatile uint32_t *reg = nvic_icer(irq_num);
    *reg = (1UL << (irq_num & 0x1FUL));
    __asm__ volatile("dsb");
    __asm__ volatile("isb");
}

void cortex_m4_nvic_set_pending(uint8_t irq_num) {
    if (irq_num >= NVIC_MAX_IRQ) return;
    volatile uint32_t *reg = nvic_ispr(irq_num);
    *reg = (1UL << (irq_num & 0x1FUL));
}

void cortex_m4_nvic_clear_pending(uint8_t irq_num) {
    if (irq_num >= NVIC_MAX_IRQ) return;
    volatile uint32_t *reg = nvic_icpr(irq_num);
    *reg = (1UL << (irq_num & 0x1FUL));
}

bool cortex_m4_nvic_is_pending(uint8_t irq_num) {
    if (irq_num >= NVIC_MAX_IRQ) return false;
    volatile uint32_t *reg = nvic_ispr(irq_num);
    return (*reg & (1UL << (irq_num & 0x1FUL))) != 0;
}

bool cortex_m4_nvic_is_active(uint8_t irq_num) {
    if (irq_num >= NVIC_MAX_IRQ) return false;
    volatile uint32_t *reg = nvic_iabr(irq_num);
    return (*reg & (1UL << (irq_num & 0x1FUL))) != 0;
}

void cortex_m4_nvic_set_priority(uint8_t irq_num, uint8_t priority) {
    if (irq_num >= NVIC_MAX_IRQ) return;
    volatile uint8_t *reg = nvic_ipr(irq_num);
    *reg = (priority << 4) & NVIC_PRIORITY_MASK;
}

uint8_t cortex_m4_nvic_get_priority(uint8_t irq_num) {
    if (irq_num >= NVIC_MAX_IRQ) return 0;
    volatile uint8_t *reg = nvic_ipr(irq_num);
    return (*reg >> 4) & 0x0FU;
}

void cortex_m4_nvic_trigger_sw_interrupt(uint8_t irq_num) {
    NVIC_STIR = (uint32_t)irq_num;
}

void cortex_m4_systick_init(uint32_t reload_value) {
    SYSTICK_CVR = 0;
    SYSTICK_RVR = reload_value & 0x00FFFFFFUL;
}

void cortex_m4_systick_enable(void) {
    SYSTICK_CSR |= SYSTICK_CSR_ENABLE;
}

void cortex_m4_systick_disable(void) {
    SYSTICK_CSR &= ~SYSTICK_CSR_ENABLE;
}

void cortex_m4_systick_enable_interrupt(void) {
    SYSTICK_CSR |= SYSTICK_CSR_TICKINT;
}

void cortex_m4_systick_disable_interrupt(void) {
    SYSTICK_CSR &= ~SYSTICK_CSR_TICKINT;
}

bool cortex_m4_systick_has_expired(void) {
    return (SYSTICK_CSR & SYSTICK_CSR_COUNTFLAG) != 0;
}

uint32_t cortex_m4_systick_get_current(void) {
    return SYSTICK_CVR & 0x00FFFFFFUL;
}

uint32_t cortex_m4_systick_get_calibration(void) {
    return SYSTICK_CALIB;
}

void cortex_m4_scb_set_priority_grouping(priority_grouping_t grouping) {
    uint32_t reg_value = SCB_AIRCR;
    reg_value &= ~(AIRCR_VECTKEY_MASK | AIRCR_PRIGROUP_MASK);
    reg_value |= AIRCR_VECTKEY_VALUE;
    reg_value |= ((uint32_t)grouping << AIRCR_PRIGROUP_POS) & AIRCR_PRIGROUP_MASK;
    SCB_AIRCR = reg_value;
    __asm__ volatile("dsb");
    __asm__ volatile("isb");
}

priority_grouping_t cortex_m4_scb_get_priority_grouping(void) {
    uint32_t air = SCB_AIRCR;
    return (priority_grouping_t)((air & AIRCR_PRIGROUP_MASK) >> AIRCR_PRIGROUP_POS);
}

void cortex_m4_scb_system_reset(void) {
    __asm__ volatile("dsb");
    SCB_AIRCR = AIRCR_VECTKEY_VALUE | AIRCR_SYSRESETREQ;
    __asm__ volatile("dsb");
    while (1) { __asm__ volatile("nop"); }
}

void cortex_m4_scb_set_vector_table_offset(uint32_t offset) {
    SCB_VTOR = offset & 0xFFFFFF00UL;
    __asm__ volatile("dsb");
}

uint32_t cortex_m4_scb_get_vector_table_offset(void) {
    return SCB_VTOR & 0xFFFFFF00UL;
}

uint32_t cortex_m4_scb_get_cpu_id(void) {
    return SCB_CPUID;
}

void cortex_m4_scb_enable_fault_handlers(void) {
    SCB_SHCSR |= (SHCSR_MEMFAULTENA | SHCSR_BUSFAULTENA | SHCSR_USGFAULTENA);
}

void cortex_m4_scb_set_sleep_on_exit(bool enable) {
    if (enable) {
        SCB_SCR |= SCR_SLEEPONEXIT;
    } else {
        SCB_SCR &= ~SCR_SLEEPONEXIT;
    }
}

void cortex_m4_scb_set_deep_sleep(bool enable) {
    if (enable) {
        SCB_SCR |= SCR_SLEEPDEEP;
    } else {
        SCB_SCR &= ~SCR_SLEEPDEEP;
    }
}

void cortex_m4_scb_set_sev_on_pend(bool enable) {
    if (enable) {
        SCB_SCR |= SCR_SEVEONPEND;
    } else {
        SCB_SCR &= ~SCR_SEVEONPEND;
    }
}

void cortex_m4_scb_set_div_by_zero_trap(bool enable) {
    if (enable) {
        SCB_CCR |= CCR_DIV_0_TRAP;
    } else {
        SCB_CCR &= ~CCR_DIV_0_TRAP;
    }
}

void cortex_m4_scb_set_unaligned_access_trap(bool enable) {
    if (enable) {
        SCB_CCR |= CCR_UNALIGN_TRAP;
    } else {
        SCB_CCR &= ~CCR_UNALIGN_TRAP;
    }
}

void cortex_m4_mpu_enable(void) {
    __asm__ volatile("dsb");
    __asm__ volatile("isb");
    MPU_CTRL |= MPU_CTRL_ENABLE;
    __asm__ volatile("dsb");
    __asm__ volatile("isb");
}

void cortex_m4_mpu_disable(void) {
    __asm__ volatile("dsb");
    __asm__ volatile("isb");
    MPU_CTRL &= ~MPU_CTRL_ENABLE;
    __asm__ volatile("dsb");
    __asm__ volatile("isb");
}

void cortex_m4_mpu_enable_hard_fault_nmi(bool enable) {
    if (enable) {
        MPU_CTRL |= MPU_CTRL_HFNMIENA;
    } else {
        MPU_CTRL &= ~MPU_CTRL_HFNMIENA;
    }
}

void cortex_m4_mpu_enable_default_map(bool enable) {
    if (enable) {
        MPU_CTRL |= MPU_CTRL_PRIVDEFENA;
    } else {
        MPU_CTRL &= ~MPU_CTRL_PRIVDEFENA;
    }
}

void cortex_m4_mpu_configure_region(uint8_t region, uint32_t base_addr,
    mpu_region_size_t size, mpu_access_permission_t access,
    uint8_t tex, bool cacheable, bool bufferable, bool shareable, bool execute_never)
{
    MPU_RNR = region;
    MPU_RBAR = base_addr | MPU_RBAR_VALID | (region & 0xFU);
    uint32_t rasr = MPU_RASR_ENABLE
                  | ((uint32_t)size << MPU_RASR_SIZE_POS)
                  | ((uint32_t)access << MPU_RASR_AP_POS)
                  | ((uint32_t)tex << MPU_RASR_TEX_POS);
    if (cacheable)  rasr |= (1UL << MPU_RASR_C_POS);
    if (bufferable) rasr |= (1UL << MPU_RASR_B_POS);
    if (shareable)  rasr |= (1UL << MPU_RASR_S_POS);
    if (execute_never) rasr |= (1UL << MPU_RASR_XN_POS);
    MPU_RASR = rasr;
}

void cortex_m4_mpu_enable_region(uint8_t region) {
    MPU_RNR = region;
    MPU_RASR |= MPU_RASR_ENABLE;
}

void cortex_m4_mpu_disable_region(uint8_t region) {
    MPU_RNR = region;
    MPU_RASR &= ~MPU_RASR_ENABLE;
}

processor_mode_t cortex_m4_get_processor_mode(void) {
    uint32_t ipsr;
    __asm__ volatile("mrs %0, ipsr" : "=r"(ipsr));
    return (ipsr == 0) ? PROCESSOR_MODE_THREAD : PROCESSOR_MODE_HANDLER;
}

void cortex_m4_wait_for_interrupt(void) {
    __asm__ volatile("wfi");
}

void cortex_m4_wait_for_event(void) {
    __asm__ volatile("wfe");
}

void cortex_m4_disable_interrupts(void) {
    __asm__ volatile("cpsid i");
    __asm__ volatile("dsb");
    __asm__ volatile("isb");
}

void cortex_m4_enable_interrupts(void) {
    __asm__ volatile("cpsie i");
    __asm__ volatile("dsb");
    __asm__ volatile("isb");
}
