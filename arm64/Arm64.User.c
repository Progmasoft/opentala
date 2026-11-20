/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "arm64/Arm64.User.h"
#include "kern/Kern.Machine.h"
#include "kern/Kern.Print.h"
#include "kern/Kern.Process.h"
#include "kern/Kern.SystemCalls.h"

#define ARM64_USER_STACK_SIZE 16384
/** SPSR's flags: N, Z, C and V; C is bit 29. */
#define ARM64_USER_FLAGS 0xF0000000ULL
#define ARM64_USER_CARRY (1ULL << 29)
/** X16 of a machine-dependent call. */
#define ARM64_USER_MACHINE_DEPENDENT 0x80000000LL
/** BSD numbers and Mach trap numbers fit in 24 bits, as on amd64. */
#define ARM64_USER_NUMBER_LIMIT 0x1000000LL

/** The kernel stack of user mode: exceptions and interrupts from EL0 arrive on it. */
static uint8_t gsKernelStack[ ARM64_USER_STACK_SIZE ] __attribute__((aligned(16)));

/* Kern_Machine_Enter() reads it by its assembly name. */
uint64_t gArm64_User_KernelStackTop __asm__("Arm64_User_KernelStackTop");

void
Kern_Machine_InitUser(void)
{
        gArm64_User_KernelStackTop = (uint64_t) (uintptr_t) (gsKernelStack + ARM64_USER_STACK_SIZE);
        Kern_Print("user: system calls through SVC #0x80, back through ERET\n");
}

/** The class and number of X16, as XNU reads it; false for a value that has neither. */
static int
Decode(int64_t aRegister, Kern_SystemCalls_tClass* aOut_pClass, uint32_t* aOut_pNumber)
{
        if(aRegister == ARM64_USER_MACHINE_DEPENDENT)
        {
                *aOut_pClass  = Kern_SystemCalls_kMachineDependent;
                *aOut_pNumber = 0;
                return (1);
        }
        if(aRegister > 0 && aRegister < ARM64_USER_NUMBER_LIMIT)
        {
                *aOut_pClass  = Kern_SystemCalls_kBsd;
                *aOut_pNumber = (uint32_t) aRegister;
                return (1);
        }
        if(aRegister < 0 && aRegister > -ARM64_USER_NUMBER_LIMIT)
        {
                *aOut_pClass  = Kern_SystemCalls_kMach;
                *aOut_pNumber = (uint32_t) -aRegister;
                return (1);
        }
        return (0);
}

/** Writes a result back: X0 and X1, and for a BSD call the carry flag. */
static void
WriteResult(Arm64_Trap_tFrame*              aInOut_pFrame,
            Kern_SystemCalls_tClass         aClass,
            const Kern_SystemCalls_tResult* aIn_pResult)
{
        /* EL0t with every interrupt open, and only the flags the process may hold. */
        uint64_t state = aInOut_pFrame->spsr & ARM64_USER_FLAGS;

        if(aClass == Kern_SystemCalls_kBsd)
        {
                state &= ~ARM64_USER_CARRY;
                if(aIn_pResult->error != 0)
                        state |= ARM64_USER_CARRY;
                aInOut_pFrame->x[ 0 ] = aIn_pResult->error != 0 ? (uint64_t) aIn_pResult->error
                                                                : aIn_pResult->values[ 0 ];
                aInOut_pFrame->x[ 1 ] = aIn_pResult->error != 0 ? 0 : aIn_pResult->values[ 1 ];
        } else
                aInOut_pFrame->x[ 0 ] = aIn_pResult->values[ 0 ];
        aInOut_pFrame->spsr = state;
}

void
Arm64_User_SystemCall(Arm64_Trap_tFrame* aInOut_pFrame, uint32_t aImmediate)
{
        Kern_SystemCalls_tClass kind                                     = Kern_SystemCalls_kBsd;
        uint32_t                number                                   = 0;
        const uint64_t          arguments[ KERN_SYSTEM_CALLS_ARGUMENTS ] = {
                aInOut_pFrame->x[ 0 ], aInOut_pFrame->x[ 1 ], aInOut_pFrame->x[ 2 ],
                aInOut_pFrame->x[ 3 ], aInOut_pFrame->x[ 4 ], aInOut_pFrame->x[ 5 ],
        };

        if(aImmediate != ARM64_USER_SVC_DARWIN
           || !Decode((int64_t) aInOut_pFrame->x[ 16 ], &kind, &number))
        {
                Kern_Print("process %u: svc #0x%x with x16 %lx is not a system call\n",
                           Kern_Process_Id(),
                           aImmediate,
                           (unsigned long) aInOut_pFrame->x[ 16 ]);
                Kern_Process_Kill(KERN_SIGNAL_SYS);
        }
        const Kern_SystemCalls_tResult result = Kern_SystemCalls_Call(kind, number, arguments);

        WriteResult(aInOut_pFrame, kind, &result);
        if(aInOut_pFrame->elr < KERN_MACHINE_USER_START
           || aInOut_pFrame->elr >= KERN_MACHINE_USER_END)
        {
                Kern_Print("process %u: returns to %p, outside user memory\n",
                           Kern_Process_Id(),
                           (void*) aInOut_pFrame->elr);
                Kern_Process_Kill(KERN_SIGNAL_SEGV);
        }
}
