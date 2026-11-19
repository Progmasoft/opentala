/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdint.h>

#include "arm64/Arm64.Mmio.h"
#include "arm64/Arm64.Serial.h"
#include "kern/Kern.Machine.h"

/** The PL011 of QEMU's virt machine. */
#define ARM64_SERIAL_BASE 0x09000000ULL

#define ARM64_SERIAL_DATA       0x000
#define ARM64_SERIAL_FLAGS      0x018
#define ARM64_SERIAL_LINE       0x02C
#define ARM64_SERIAL_CONTROL    0x030
#define ARM64_SERIAL_INTERRUPTS 0x038
/** FR.TXFF: the transmit FIFO is full. */
#define ARM64_SERIAL_FULL 0x20U
/** LCR_H: 8 data bits, FIFOs on. */
#define ARM64_SERIAL_8N1_FIFO 0x70U
/** CR: the UART, its transmitter and its receiver on. */
#define ARM64_SERIAL_ON 0x301U

void
Arm64_Serial_Init(void)
{
        Arm64_Mmio_Write32(ARM64_SERIAL_BASE + ARM64_SERIAL_CONTROL, 0);
        Arm64_Mmio_Write32(ARM64_SERIAL_BASE + ARM64_SERIAL_INTERRUPTS, 0);
        Arm64_Mmio_Write32(ARM64_SERIAL_BASE + ARM64_SERIAL_LINE, ARM64_SERIAL_8N1_FIFO);
        Arm64_Mmio_Write32(ARM64_SERIAL_BASE + ARM64_SERIAL_CONTROL, ARM64_SERIAL_ON);
}

static void
WriteByte(uint8_t aByte)
{
        while((Arm64_Mmio_Read32(ARM64_SERIAL_BASE + ARM64_SERIAL_FLAGS) & ARM64_SERIAL_FULL) != 0)
                __asm__ volatile("yield");
        Arm64_Mmio_Write32(ARM64_SERIAL_BASE + ARM64_SERIAL_DATA, aByte);
}

/**
 * @brief The console of Kern.Machine.h: the PL011, bytes as they are, null characters too.
 *
 * @pre Arm64_Serial_Init() has run.
 */
void
Kern_Machine_ConsoleWrite(const char* aIn_pBytes, size_t aCount)
{
        for(size_t index = 0; index < aCount; index++)
        {
                if(aIn_pBytes[ index ] == '\n')
                        WriteByte('\r');
                WriteByte((uint8_t) aIn_pBytes[ index ]);
        }
}
