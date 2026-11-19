/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "arm64/Arm64.Trap.h"

/*
 * User mode on arm64. A process enters the kernel with `svc #0x80`, Darwin's way: the number in X16
 * (positive for a BSD system call, negative for a Mach trap, 0x80000000 for a machine-dependent
 * call), the arguments in X0 to X7. An error of a BSD call sets the carry flag and puts the errno
 * value in X0. The return is ERET from the vector's frame.
 */

/** The immediate of Darwin's system call instruction. */
#define ARM64_USER_SVC_DARWIN 0x80U

/**
 * @brief Called for an SVC from EL0: runs the system call and writes its result into the frame.
 *
 * @param[in,out] aInOut_pFrame The frame.
 * @param[in]     aImmediate    The SVC's immediate; anything but ARM64_USER_SVC_DARWIN kills the
 *                              process with SIGSYS.
 */
void
Arm64_User_SystemCall(Arm64_Trap_tFrame* aInOut_pFrame, uint32_t aImmediate);
