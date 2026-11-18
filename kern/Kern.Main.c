/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "amd64/Amd64.Serial.h"
#include "kern/Kern.Main.h"

#define KERN_MAIN_MULTIBOOT_MAGIC 0x2BADB002U

/**
 * @brief Stops the processor for good, with interrupts off.
 */
__attribute__((noreturn)) static void
Halt(void)
{
        for(;;)
                __asm__ volatile("cli; hlt");
}

/**
 * @brief The first C function of the kernel, called in 64-bit mode by the boot code.
 *
 * It reports on the serial port that the kernel runs and how it was loaded, and halts.
 *
 * @param[in] aBootMagic       The value the loader left in EAX; 0x2BADB002 for Multiboot.
 * @param[in] aBootInformation The physical address of the loader's information structure.
 */
__attribute__((noreturn)) void
Kern_Main(uint32_t aBootMagic, uint32_t aBootInformation)
{
        (void) aBootInformation;

        Amd64_Serial_Init();
        Amd64_Serial_Write("OpenTala: amd64, 64-bit mode\n");
        if(aBootMagic == KERN_MAIN_MULTIBOOT_MAGIC)
                Amd64_Serial_Write("OpenTala: loaded by Multiboot\n");
        else
                Amd64_Serial_Write("OpenTala: loaded by an unknown loader\n");
        Amd64_Serial_Write("OpenTala: halting\n");
        Halt();
}
