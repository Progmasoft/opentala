/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>

#include "amd64/Amd64.Pic.h"
#include "amd64/Amd64.Port.h"
#include "amd64/Amd64.Trap.h"

#define AMD64_PIC_PRIMARY_COMMAND   0x20
#define AMD64_PIC_PRIMARY_DATA      0x21
#define AMD64_PIC_SECONDARY_COMMAND 0xA0
#define AMD64_PIC_SECONDARY_DATA    0xA1

/* ICW1: initialization, ICW4 follows. ICW4: 8086 mode. */
#define AMD64_PIC_ICW1_INIT_ICW4 0x11
#define AMD64_PIC_ICW4_8086      0x01
/* The secondary PIC is on the primary's IRQ 2. */
#define AMD64_PIC_CASCADE_IRQ 2

#define AMD64_PIC_END_OF_INTERRUPT 0x20
#define AMD64_PIC_READ_ISR         0x0B

/** The masks of the primary (low byte) and secondary (high byte) PIC; a set bit masks an IRQ. */
static uint16_t gsMask = 0xFFFF;

/**
 * @brief Writes the masks to both PICs.
 */
static void
WriteMask(void)
{
        Amd64_Port_Write8(AMD64_PIC_PRIMARY_DATA, (uint8_t) gsMask);
        Amd64_Port_Write8(AMD64_PIC_SECONDARY_DATA, (uint8_t) (gsMask >> 8));
}

void
Amd64_Pic_Init(void)
{
        Amd64_Port_Write8(AMD64_PIC_PRIMARY_COMMAND, AMD64_PIC_ICW1_INIT_ICW4);
        Amd64_Port_Write8(AMD64_PIC_SECONDARY_COMMAND, AMD64_PIC_ICW1_INIT_ICW4);
        Amd64_Port_Write8(AMD64_PIC_PRIMARY_DATA, AMD64_TRAP_IRQ_BASE);
        Amd64_Port_Write8(AMD64_PIC_SECONDARY_DATA, AMD64_TRAP_IRQ_BASE + 8);
        Amd64_Port_Write8(AMD64_PIC_PRIMARY_DATA, 1U << AMD64_PIC_CASCADE_IRQ);
        Amd64_Port_Write8(AMD64_PIC_SECONDARY_DATA, AMD64_PIC_CASCADE_IRQ);
        Amd64_Port_Write8(AMD64_PIC_PRIMARY_DATA, AMD64_PIC_ICW4_8086);
        Amd64_Port_Write8(AMD64_PIC_SECONDARY_DATA, AMD64_PIC_ICW4_8086);
        /* Every IRQ masked, the cascade line included until a secondary IRQ is enabled. */
        gsMask = 0xFFFF;
        WriteMask();
}

void
Amd64_Pic_SetEnabled(unsigned aIrq, bool isEnabled)
{
        if(aIrq > 15)
                return;
        if(isEnabled)
                gsMask &= (uint16_t) ~(1U << aIrq);
        else
                gsMask |= (uint16_t) (1U << aIrq);
        /* An IRQ of the secondary PIC reaches the processor through the cascade line. */
        if((gsMask & 0xFF00) != 0xFF00)
                gsMask &= (uint16_t) ~(1U << AMD64_PIC_CASCADE_IRQ);
        else
                gsMask |= (uint16_t) (1U << AMD64_PIC_CASCADE_IRQ);
        WriteMask();
}

bool
Amd64_Pic_IsSpurious(unsigned aIrq)
{
        if(aIrq != 7 && aIrq != 15)
                return (false);
        const uint16_t command = aIrq == 7 ? AMD64_PIC_PRIMARY_COMMAND
                                           : AMD64_PIC_SECONDARY_COMMAND;

        Amd64_Port_Write8(command, AMD64_PIC_READ_ISR);
        if((Amd64_Port_Read8(command) & 0x80) != 0)
                return (false);
        /* The primary PIC did see the cascade line of a spurious secondary IRQ. */
        if(aIrq == 15)
                Amd64_Port_Write8(AMD64_PIC_PRIMARY_COMMAND, AMD64_PIC_END_OF_INTERRUPT);
        return (true);
}

void
Amd64_Pic_Acknowledge(unsigned aIrq)
{
        if(aIrq >= 8)
                Amd64_Port_Write8(AMD64_PIC_SECONDARY_COMMAND, AMD64_PIC_END_OF_INTERRUPT);
        Amd64_Port_Write8(AMD64_PIC_PRIMARY_COMMAND, AMD64_PIC_END_OF_INTERRUPT);
}
