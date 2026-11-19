/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "amd64/Amd64.Trap.h"

/*
 * User mode on amd64: the SYSCALL entry and the way into and out of a process.
 *
 * A process enters the kernel with the `syscall` instruction, Darwin's way: the class in bits 24
 * to 31 of RAX (1 Mach, 2 BSD), the number below, the arguments in RDI, RSI, RDX, R10, R8 and R9.
 * The entry switches to the kernel stack, builds the same frame the trap stubs build, and returns
 * with IRETQ rather than SYSRET: SYSRET to a non-canonical address faults in kernel mode on the
 * user's stack on Intel processors (CVE-2012-0217), which IRETQ cannot.
 */

/** The vector the SYSCALL entry writes in its frame, past the IDT's. */
#define AMD64_USER_SYSCALL_VECTOR 256

/** The kernel's registers where it went into user mode, for the way back. */
typedef struct Amd64_User_Context
{
        uint64_t rbx, rbp, r12, r13, r14, r15, rsp;
} Amd64_User_tContext;

/**
 * @brief Turns on SYSCALL and points it at the entry, and gives user mode its kernel stack.
 */
void
Amd64_User_Init(void);

/**
 * @brief Goes into user mode at aEntry, with the stack at aStack and aArgument in RDI; every other
 * register is zero, so nothing of the kernel shows. Returns when Amd64_User_Leave() is called.
 *
 * @param[out] aOut_pContext Where the kernel's registers are kept.
 * @param[in]  aEntry        The user address to start at.
 * @param[in]  aStack        The user stack pointer.
 * @param[in]  aArgument     The first argument.
 * @return The value given to Amd64_User_Leave().
 */
int64_t
Amd64_User_Enter(Amd64_User_tContext* aOut_pContext,
                 uint64_t             aEntry,
                 uint64_t             aStack,
                 uint64_t             aArgument);

/**
 * @brief Goes back to where Amd64_User_Enter() was called, which returns aValue. The kernel stack
 * of user mode is left as it is: nothing on it is needed any more.
 *
 * @param[in] aIn_pContext The context Amd64_User_Enter() filled.
 * @param[in] aValue       What it returns.
 */
__attribute__((noreturn)) void
Amd64_User_Leave(const Amd64_User_tContext* aIn_pContext, int64_t aValue);

/**
 * @brief Called by the SYSCALL entry with the frame of the process: runs the system call and writes
 * its result into the frame.
 *
 * @param[in,out] aInOut_pFrame The frame.
 */
void
Amd64_User_SystemCall(Amd64_Trap_Frame* aInOut_pFrame);
