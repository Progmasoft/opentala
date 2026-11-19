/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/** The size of Arm64_Trap_tFrame, a multiple of 16 so the stack stays aligned. */
#define ARM64_TRAP_FRAME_SIZE 288

/**
 * @brief What the vectors of Arm64.Vectors.S leave on the stack: the general registers, the stack
 * pointer of EL0, the return address and state, and which of the 16 vectors was taken.
 */
typedef struct Arm64_Trap_Frame
{
        uint64_t x[ 31 ];
        uint64_t sp;
        uint64_t elr;
        uint64_t spsr;
        /**
         * The vector: 4 times where it came from (0 EL1 with SP_EL0, 1 EL1, 2 EL0 in AArch64,
         * 3 EL0 in AArch32), plus its kind (0 synchronous, 1 IRQ, 2 FIQ, 3 SError).
         */
        uint64_t vector;
        uint64_t pad;
} Arm64_Trap_tFrame;

_Static_assert(sizeof(Arm64_Trap_tFrame) == ARM64_TRAP_FRAME_SIZE, "the frame of Arm64.Vectors.S");

/**
 * @brief Sets VBAR_EL1 to the vectors of Arm64.Vectors.S.
 */
void
Arm64_Trap_Init(void);

/**
 * @brief How many breakpoints (BRK) the kernel took and resumed after.
 *
 * @return The count.
 */
uint64_t
Arm64_Trap_Breakpoints(void);

/**
 * @brief Called by the vectors for every exception and interrupt.
 *
 * An IRQ goes to the GIC; a BRK in the kernel is counted and resumed after; a synchronous exception
 * from EL0 is a system call or the process's fault; anything else is a kernel bug at this stage, so
 * the registers are written out and the kernel panics.
 *
 * @param[in,out] aInOut_pFrame The frame.
 */
void
Arm64_Trap_Dispatch(Arm64_Trap_tFrame* aInOut_pFrame);
