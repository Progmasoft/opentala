/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "amd64/Amd64.Cpu.h"
#include "amd64/Amd64.Multiboot.h"
#include "amd64/Amd64.Pic.h"
#include "amd64/Amd64.Serial.h"
#include "amd64/Amd64.Timer.h"
#include "amd64/Amd64.Trap.h"
#include "kern/Kern.Boot.h"
#include "kern/Kern.Machine.h"
#include "kern/Kern.PageFrames.h"
#include "kern/Kern.Print.h"

/*
 * The amd64 side of Kern.Machine.h, and the entry from the boot code: it reads the Multiboot
 * information into the shape Kern_Main() takes.
 */

/** The longest command line the kernel reads, held back with the loader's structures. */
#define AMD64_MACHINE_COMMAND_LINE_MAX 4096

static Kern_Boot_tInformation gsInformation;

/**
 * @brief Reads the loader's memory map: the available ranges as RAM, and the first MiB and the
 * loader's own structures as reserved.
 *
 * @param[in] aIn_pMultiboot The loader's information.
 */
static void
ReadMemoryMap(const Amd64_Multiboot_Information* aIn_pMultiboot)
{
        if((aIn_pMultiboot->flags & AMD64_MULTIBOOT_HAS_MEMORY_MAP) == 0)
                Kern_Panic("the loader gave no memory map");

        const uint8_t* pCursor = (const uint8_t*) (uintptr_t) aIn_pMultiboot->memoryMapAddress;
        const uint8_t* pEnd    = pCursor + aIn_pMultiboot->memoryMapLength;

        while(pCursor + sizeof(Amd64_Multiboot_MemoryRange) <= pEnd)
        {
                const Amd64_Multiboot_MemoryRange* pRange = (const Amd64_Multiboot_MemoryRange*)
                        pCursor;

                if(pRange->size < sizeof(*pRange) - sizeof(pRange->size))
                        Kern_Panic("a memory map entry of %u bytes is too short", pRange->size);
                if(pRange->type == AMD64_MULTIBOOT_MEMORY_AVAILABLE)
                        Kern_Boot_AddRange(gsInformation.memory,
                                           &gsInformation.memoryCount,
                                           pRange->address,
                                           pRange->length);
                pCursor += pRange->size + sizeof(pRange->size);
        }
        /* Real-mode structures, the BIOS and devices live below 1 MiB. */
        Kern_Boot_AddRange(gsInformation.reserved, &gsInformation.reservedCount, 0, 0x100000);
        Kern_Boot_AddRange(gsInformation.reserved,
                           &gsInformation.reservedCount,
                           (uintptr_t) aIn_pMultiboot,
                           sizeof(*aIn_pMultiboot));
        Kern_Boot_AddRange(gsInformation.reserved,
                           &gsInformation.reservedCount,
                           aIn_pMultiboot->memoryMapAddress,
                           aIn_pMultiboot->memoryMapLength);
}

/**
 * @brief The entry from Amd64.Boot.S in 64-bit mode.
 *
 * @param[in] aBootMagic       The value the loader left in EAX; 0x2BADB002 for Multiboot.
 * @param[in] aBootInformation The physical address of the loader's information structure.
 */
__attribute__((noreturn)) void
Amd64_Main(uint32_t aBootMagic, uint32_t aBootInformation)
{
        Amd64_Serial_Init();
        Kern_Print("OpenTala: amd64, 64-bit mode\n");
        if(aBootMagic != AMD64_MULTIBOOT_LOADER_MAGIC)
                Kern_Panic("loaded by an unknown loader (magic %x)", aBootMagic);

        const Amd64_Multiboot_Information* pMultiboot
                = (const Amd64_Multiboot_Information*) (uintptr_t) aBootInformation;

        gsInformation.pLoader = "Multiboot";
        if((pMultiboot->flags & AMD64_MULTIBOOT_HAS_COMMAND_LINE) != 0)
        {
                gsInformation.pCommandLine = (const char*) (uintptr_t) pMultiboot->commandLine;
                Kern_Boot_AddRange(gsInformation.reserved,
                                   &gsInformation.reservedCount,
                                   pMultiboot->commandLine,
                                   AMD64_MACHINE_COMMAND_LINE_MAX);
        }
        ReadMemoryMap(pMultiboot);
        Kern_Main(&gsInformation);
}

void
Kern_Machine_InitTraps(void)
{
        Amd64_Cpu_Init();
}

uint64_t
Kern_Machine_Breakpoint(void)
{
        __asm__ volatile("int3");
        return (Amd64_Trap_Breakpoints());
}

void
Kern_Machine_StartTimer(void)
{
        Amd64_Pic_Init();
        Amd64_Timer_Init();
}

uint64_t
Kern_Machine_Ticks(void)
{
        return (Amd64_Timer_Ticks());
}

void
Kern_Machine_WaitForInterrupt(void)
{
        /* STI holds interrupts off for one more instruction, so none comes between it and HLT. */
        __asm__ volatile("sti\n\thlt\n\tcli" : : : "memory");
}

void
Kern_Machine_RunTests(const char* aIn_pCommandLine)
{
        if(!Kern_Main_HasWord(aIn_pCommandLine, "opentala.test=double-fault"))
                return;
        /*
         * The stack pointer goes to an unmapped address: the push faults, the processor cannot push
         * the page fault's frame there either, and the double fault that follows must run on its
         * own stack to be reported at all.
         */
        Kern_Print("test: pushing to a stack at %p\n", (void*) KERN_MACHINE_UNMAPPED);
        __asm__ volatile("movq %0, %%rsp\n\t"
                         "pushq $0"
                         :
                         : "r"(KERN_MACHINE_UNMAPPED + KERN_PAGE_FRAMES_SIZE)
                         : "memory");
        Kern_Panic("the push to an unmapped stack did not fault");
}

void
Kern_Machine_Halt(void)
{
        for(;;)
                __asm__ volatile("cli; hlt");
}
