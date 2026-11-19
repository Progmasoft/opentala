/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "arm64/Arm64.Gic.h"
#include "arm64/Arm64.Mmio.h"
#include "kern/Kern.Machine.h"
#include "kern/Kern.Print.h"

#define ARM64_GIC_DISTRIBUTOR 0x08000000ULL
#define ARM64_GIC_CPU         0x08010000ULL

#define ARM64_GIC_D_CONTROL     0x000
#define ARM64_GIC_D_SET_ENABLE  0x100
#define ARM64_GIC_D_PRIORITY    0x400
#define ARM64_GIC_C_CONTROL     0x000
#define ARM64_GIC_C_PRIORITY    0x004
#define ARM64_GIC_C_ACKNOWLEDGE 0x00C
#define ARM64_GIC_C_END         0x010

/** The EL1 physical timer's interrupt. */
#define ARM64_GIC_TIMER 30U
/** Interrupt numbers from 1020 on are special; 1023 is "none pending". */
#define ARM64_GIC_SPECIAL 1020U
#define ARM64_GIC_ID_MASK 0x3FFU
/** CNTP_CTL_EL0.ENABLE, with IMASK clear. */
#define ARM64_GIC_TIMER_ON 1U

static uint64_t gsTicks;
static uint64_t gsInterval;

static void
SetTimer(void)
{
        __asm__ volatile("msr cntp_tval_el0, %0\n\tisb" : : "r"(gsInterval));
}

void
Arm64_Gic_StartTimer(void)
{
        uint64_t frequency = 0;
        uint64_t control   = ARM64_GIC_TIMER_ON;

        __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frequency));
        if(frequency == 0)
                Kern_Panic("the generic timer has no frequency");
        gsInterval = frequency / KERN_MACHINE_TIMER_HZ;

        /* A priority in the middle, the timer's interrupt on, and the distributor on. */
        Arm64_Mmio_Write32(ARM64_GIC_DISTRIBUTOR + ARM64_GIC_D_PRIORITY + (ARM64_GIC_TIMER / 4) * 4,
                           0x80U << (8 * (ARM64_GIC_TIMER % 4)));
        Arm64_Mmio_Write32(ARM64_GIC_DISTRIBUTOR + ARM64_GIC_D_SET_ENABLE
                                   + (ARM64_GIC_TIMER / 32) * 4,
                           1U << (ARM64_GIC_TIMER % 32));
        Arm64_Mmio_Write32(ARM64_GIC_DISTRIBUTOR + ARM64_GIC_D_CONTROL, 1);
        /* Every priority passes the CPU interface, which is on. */
        Arm64_Mmio_Write32(ARM64_GIC_CPU + ARM64_GIC_C_PRIORITY, 0xFF);
        Arm64_Mmio_Write32(ARM64_GIC_CPU + ARM64_GIC_C_CONTROL, 1);

        SetTimer();
        __asm__ volatile("msr cntp_ctl_el0, %0\n\tisb" : : "r"(control));
}

void
Arm64_Gic_HandleInterrupt(void)
{
        const uint32_t acknowledged = Arm64_Mmio_Read32(ARM64_GIC_CPU + ARM64_GIC_C_ACKNOWLEDGE);
        const uint32_t id           = acknowledged & ARM64_GIC_ID_MASK;

        if(id >= ARM64_GIC_SPECIAL)
                return;
        if(id == ARM64_GIC_TIMER)
        {
                gsTicks++;
                SetTimer();
        }
        Arm64_Mmio_Write32(ARM64_GIC_CPU + ARM64_GIC_C_END, acknowledged);
}

uint64_t
Arm64_Gic_Ticks(void)
{
        return (gsTicks);
}
