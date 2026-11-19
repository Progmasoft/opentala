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

/**
 * @brief Called by the SYSCALL entry with the frame of the process: runs the system call and writes
 * its result into the frame.
 *
 * @param[in,out] aInOut_pFrame The frame.
 */
void
Amd64_User_SystemCall(Amd64_Trap_Frame* aInOut_pFrame);
