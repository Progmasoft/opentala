/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "amd64/Amd64.Pic.h"
#include "amd64/Amd64.Port.h"
#include "amd64/Amd64.Timer.h"
#include "amd64/Amd64.Trap.h"

#define AMD64_TIMER_CHANNEL0 0x40
#define AMD64_TIMER_COMMAND  0x43
/* Channel 0, low byte then high byte, mode 2 (rate generator), binary. */
#define AMD64_TIMER_RATE_GENERATOR 0x34
/* The input clock of the timer, in Hz. */
#define AMD64_TIMER_INPUT_HZ 1193182U
#define AMD64_TIMER_IRQ      0

/* Written by the interrupt, read by the kernel with interrupts on. */
static volatile uint64_t gsTicks;

/**
 * @brief Counts one tick.
 *
 * @param[in,out] aIn_pFrame The interrupted code's frame.
 */
static void
Tick(Amd64_Trap_Frame* aIn_pFrame)
{
        (void) aIn_pFrame;
        gsTicks++;
}

void
Amd64_Timer_Init(void)
{
        const uint32_t divisor = (AMD64_TIMER_INPUT_HZ + AMD64_TIMER_HZ / 2) / AMD64_TIMER_HZ;

        gsTicks = 0;
        Amd64_Trap_SetIrqHandler(AMD64_TIMER_IRQ, Tick);
        Amd64_Port_Write8(AMD64_TIMER_COMMAND, AMD64_TIMER_RATE_GENERATOR);
        Amd64_Port_Write8(AMD64_TIMER_CHANNEL0, (uint8_t) divisor);
        Amd64_Port_Write8(AMD64_TIMER_CHANNEL0, (uint8_t) (divisor >> 8));
        Amd64_Pic_SetEnabled(AMD64_TIMER_IRQ, true);
}

uint64_t
Amd64_Timer_Ticks(void)
{
        return (gsTicks);
}
