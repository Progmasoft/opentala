/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/** The kernel's code segment selector in OpenTala's GDT. */
#define AMD64_CPU_SELECTOR_CODE 0x08
/** The kernel's data segment selector. */
#define AMD64_CPU_SELECTOR_DATA 0x10
/**
 * The user data and code selectors, privilege 3 in their low bits. They follow the kernel's in the
 * order SYSCALL and SYSRET need: user data at the base STAR names plus 8, user code plus 16.
 */
#define AMD64_CPU_SELECTOR_USER_DATA 0x1B
#define AMD64_CPU_SELECTOR_USER_CODE 0x23
/** The selector of the task state segment. */
#define AMD64_CPU_SELECTOR_TSS 0x28

/** The interrupt stack table entry, 1 to 7, of the double fault. */
#define AMD64_CPU_IST_DOUBLE_FAULT 1
/** Of the non-maskable interrupt. */
#define AMD64_CPU_IST_NMI 2
/** Of the machine check. */
#define AMD64_CPU_IST_MACHINE_CHECK 3

/** The number of vectors OpenTala installs: the 32 exceptions and the 16 legacy IRQs. */
#define AMD64_CPU_VECTORS 48

/**
 * @brief Loads OpenTala's own GDT, task state segment and IDT on this processor.
 *
 * The boot code's GDT has no TSS. This one does: the double fault, the NMI and the machine check
 * run on stacks of their own (the interrupt stack table), so that a fault on a broken kernel stack
 * still reaches its handler instead of resetting the machine. Every vector below
 * AMD64_CPU_VECTORS goes to the stubs of Amd64.Traps.S, as an interrupt gate (interrupts off on
 * entry) that only the kernel can raise with `int`, except the breakpoint, which `int3` raises.
 */
void
Amd64_Cpu_Init(void);

/**
 * @brief Sets the stack the processor switches to when an interrupt or exception comes from user
 * mode (RSP0 of the task state segment).
 *
 * @param[in] aTop The address after the top of the stack, 16-byte aligned.
 */
void
Amd64_Cpu_SetKernelStack(uint64_t aTop);

/**
 * @brief Reads CR2, the address a page fault was about.
 *
 * @return The address.
 */
static inline uint64_t
Amd64_Cpu_ReadCr2(void)
{
        uint64_t value = 0;

        __asm__ volatile("movq %%cr2, %0" : "=r"(value));
        return (value);
}
