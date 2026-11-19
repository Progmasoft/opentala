/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/** The first vector of the legacy IRQs, where Amd64_Pic_Init() moves the PICs. */
#define AMD64_TRAP_IRQ_BASE 32

/**
 * @brief What the stubs of Amd64.Traps.S leave on the stack, from the lowest address up: the
 * general registers, the vector, the error code (0 where the processor gives none), and what the
 * processor pushed.
 */
typedef struct Amd64_Trap_Frame
{
        uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
        uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
        uint64_t vector;
        uint64_t errorCode;
        uint64_t rip, cs, rflags, rsp, ss;
} Amd64_Trap_Frame;

/**
 * @brief A handler of one legacy IRQ, called with interrupts off.
 *
 * @param[in,out] aIn_pFrame The frame of the interrupted code.
 */
typedef void (*Amd64_Trap_IrqHandler)(Amd64_Trap_Frame* aIn_pFrame);

/**
 * @brief Sets the handler of IRQ aIrq, 0 to 15, or removes it with a null handler.
 *
 * @param[in] aIrq     The IRQ.
 * @param[in] fHandler The handler.
 */
void
Amd64_Trap_SetIrqHandler(unsigned aIrq, Amd64_Trap_IrqHandler fHandler);

/**
 * @brief How many breakpoints (int3) the kernel took and resumed after.
 *
 * @return The count.
 */
uint64_t
Amd64_Trap_Breakpoints(void);

/**
 * @brief Called by the stubs for every exception and interrupt.
 *
 * A breakpoint is counted and resumed after; an IRQ goes to its handler and is acknowledged; any
 * other exception is a kernel bug at this stage, so the registers are written out and the kernel
 * panics, the address of a page fault included.
 *
 * @param[in,out] aIn_pFrame The frame.
 */
void
Amd64_Trap_Dispatch(Amd64_Trap_Frame* aIn_pFrame);
