/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "arm64/Arm64.Gic.h"
#include "arm64/Arm64.Paging.h"
#include "arm64/Arm64.Serial.h"
#include "arm64/Arm64.Trap.h"
#include "kern/Kern.Boot.h"
#include "kern/Kern.DeviceTree.h"
#include "kern/Kern.Machine.h"
#include "kern/Kern.Print.h"

/*
 * The arm64 side of Kern.Machine.h, and the entry from the boot code: it turns the MMU on and reads
 * the device tree into the shape Kern_Main() takes.
 */

static Kern_Boot_tInformation gsInformation;

/**
 * @brief The entry from Arm64.Boot.S at EL1, with the MMU off.
 *
 * @param[in] aDeviceTree The physical address of the device tree, from the loader in X0.
 */
__attribute__((noreturn)) void
Arm64_Main(uint64_t aDeviceTree)
{
        Arm64_Serial_Init();
        Kern_Print("OpenTala: arm64, EL1\n");
        Arm64_Paging_EnableMmu();
        /* The kernel maps virt's RAM, from 1 GiB to 2 GiB; the tree must be in it. */
        if(aDeviceTree < 0x40000000ULL || aDeviceTree >= 0x80000000ULL || aDeviceTree % 8 != 0)
                Kern_Panic("the device tree at %p is outside the kernel's RAM",
                           (void*) aDeviceTree);
        gsInformation.pLoader = "a device tree";
        if(!Kern_DeviceTree_Read((const void*) (uintptr_t) aDeviceTree, &gsInformation))
                Kern_Panic("the device tree at %p is not one OpenTala reads", (void*) aDeviceTree);
        Kern_Main(&gsInformation);
}

void
Kern_Machine_InitTraps(void)
{
        Arm64_Trap_Init();
}

uint64_t
Kern_Machine_Breakpoint(void)
{
        __asm__ volatile("brk #0" : : : "memory");
        return (Arm64_Trap_Breakpoints());
}

void
Kern_Machine_StartTimer(void)
{
        Arm64_Gic_StartTimer();
}

uint64_t
Kern_Machine_Ticks(void)
{
        return (Arm64_Gic_Ticks());
}

void
Kern_Machine_WaitForInterrupt(void)
{
        /*
         * WFI wakes on a pending interrupt even while IRQs are masked; unmasking after it takes the
         * interrupt then, so none is lost between the check and the wait.
         */
        __asm__ volatile("wfi\n\tmsr daifclr, #2\n\tisb\n\tmsr daifset, #2" : : : "memory");
}

void
Kern_Machine_RunTests(const char* aIn_pCommandLine)
{
        (void) aIn_pCommandLine;
}

void
Kern_Machine_Halt(void)
{
        for(;;)
                __asm__ volatile("msr daifset, #0xF\n\twfi");
}
