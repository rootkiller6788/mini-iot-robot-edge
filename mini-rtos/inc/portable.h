/*
 * portable.h -- Platform Abstraction for mini-rtos
 *
 * Provides macros and stubs for platform-specific operations:
 *   - Critical section enter/exit
 *   - Context switch trigger
 *   - Idle instruction
 *   - NOP for busy-wait
 *
 * When compiled for ARM (__arm__ defined), uses ARM Cortex-M
 * inline assembly. When compiled on host (tests/benches), uses
 * empty or OS-compatible stubs.
 */
#ifndef PORTABLE_H
#define PORTABLE_H

#include <stdint.h>

#ifdef __arm__

/* ARM Cortex-M specific */
#define PORT_FAULT_MASK()       __asm volatile("cpsid f" ::: "memory")
#define PORT_UNFAULT_MASK()     __asm volatile("cpsie f" ::: "memory")
#define PORT_WFI()              __asm volatile("wfi")
#define PORT_NOP()              __asm volatile("nop")
#define PORT_PENDSV_TRIGGER()   (*(volatile uint32_t *)0xE000ED04 = (1U << 28))

#else

/* Host (x86/amd64) stubs for testing */
#define PORT_FAULT_MASK()       do { /* no-op on host */ } while(0)
#define PORT_UNFAULT_MASK()     do { /* no-op on host */ } while(0)
#define PORT_WFI()              do { /* no-op on host */ } while(0)
#define PORT_NOP()              do { /* no-op on host */ } while(0)
#define PORT_PENDSV_TRIGGER()   do { /* no-op on host */ } while(0)

#endif /* __arm__ */

#endif /* PORTABLE_H */
