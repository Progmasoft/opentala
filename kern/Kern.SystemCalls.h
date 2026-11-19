/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/*
 * The system calls of OpenTala, with Darwin's numbers and conventions: the BSD system calls of
 * XNU's syscalls.master and the Mach traps of its trap table. The architecture's entry code splits
 * the number into its class and its number, passes the arguments and writes the result back the
 * way Darwin does (on amd64: the carry flag and the errno value for a BSD error; a kern_return_t
 * in RAX for a Mach trap).
 */

/** The class of a system call, the high byte of RAX on amd64. */
typedef enum Kern_SystemCalls_Class
{
        Kern_SystemCalls_Class_Mach             = 1,
        Kern_SystemCalls_Class_Bsd              = 2,
        Kern_SystemCalls_Class_MachineDependent = 3,
        Kern_SystemCalls_Class_Diagnostics      = 4,
} Kern_SystemCalls_tClass;

/** How many arguments come in registers. */
#define KERN_SYSTEM_CALLS_ARGUMENTS 6

/** Darwin's BSD system call numbers that OpenTala has. */
#define KERN_SYSTEM_CALLS_EXIT  1
#define KERN_SYSTEM_CALLS_WRITE 4

/** Darwin's errno values the system calls return. */
#define KERN_ERRNO_BADF  9
#define KERN_ERRNO_FAULT 14
#define KERN_ERRNO_INVAL 22
#define KERN_ERRNO_NOSYS 78

/** What a system call gives back. */
typedef struct Kern_SystemCalls_Result
{
        /** A BSD call's errno value, 0 on success; a Mach trap's kern_return_t is values[ 0 ]. */
        int error;
        /** The return values: RAX and RDX on amd64. */
        uint64_t values[ 2 ];
} Kern_SystemCalls_tResult;

/**
 * @brief Runs a system call of the current process.
 *
 * A class or number OpenTala does not implement kills the process with SIGSYS, as XNU's nosys and
 * its invalid Mach traps do; exit does not return either.
 *
 * @param[in] aClass        The class.
 * @param[in] aNumber       The number within the class.
 * @param[in] aIn_pArguments KERN_SYSTEM_CALLS_ARGUMENTS arguments.
 * @return The result.
 */
Kern_SystemCalls_tResult
Kern_SystemCalls_Call(Kern_SystemCalls_tClass aClass,
                      uint32_t                aNumber,
                      const uint64_t*         aIn_pArguments);
