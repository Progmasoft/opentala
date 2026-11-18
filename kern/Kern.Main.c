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
#include "kern/Kern.Main.h"
#include "kern/Kern.PageFrames.h"
#include "kern/Kern.Print.h"

/** The longest command line the kernel reads; the rest is ignored. */
#define KERN_MAIN_COMMAND_LINE_MAX 4096
/** How many ticks the timer check waits for. */
#define KERN_MAIN_TICKS 10
/** The first address the boot code leaves unmapped, which the page fault test writes to. */
#define KERN_MAIN_UNMAPPED 0x40000000UL

/* The ABI of long double: x87 80-bit extended on amd64, IEEE 754 binary64 on aarch64. */
#if defined(__x86_64__)
_Static_assert(__LDBL_MANT_DIG__ == 64 && __SIZEOF_LONG_DOUBLE__ == 16,
               "long double on amd64 is the x87 80-bit extended format");
#elif defined(__aarch64__)
_Static_assert(__LDBL_MANT_DIG__ == 53 && __SIZEOF_LONG_DOUBLE__ == 8,
               "long double on aarch64 is IEEE 754 binary64");
#else
#    error "OpenTala supports amd64 and aarch64"
#endif

/* Where the linker put the kernel: from the start of __TEXT to the end of __DATA, its bss too. */
extern char gKern_ImageStart[] __asm__("segment$start$__TEXT");
extern char gKern_ImageEnd[] __asm__("segment$end$__DATA");

/**
 * @brief The length of a string of at most aMaximum bytes.
 *
 * @param[in] aIn_pText The string.
 * @param[in] aMaximum  The most bytes read.
 * @return The length, or aMaximum when no null character came before it.
 */
static size_t
BoundedLength(const char* aIn_pText, size_t aMaximum)
{
        size_t length = 0;

        while(length < aMaximum && aIn_pText[ length ] != '\0')
                length++;
        return (length);
}

/**
 * @brief Whether the command line has aIn_pWord as one of its space-separated words.
 *
 * @param[in] aIn_pLine The command line, or NULL.
 * @param[in] aIn_pWord The word.
 * @return true when it does.
 */
static bool
HasWord(const char* aIn_pLine, const char* aIn_pWord)
{
        if(aIn_pLine == NULL)
                return (false);
        const size_t lineLength = BoundedLength(aIn_pLine, KERN_MAIN_COMMAND_LINE_MAX);
        const size_t wordLength = BoundedLength(aIn_pWord, KERN_MAIN_COMMAND_LINE_MAX);

        for(size_t start = 0; start + wordLength <= lineLength; start++)
        {
                bool isSame = start == 0 || aIn_pLine[ start - 1 ] == ' ';

                for(size_t index = 0; isSame && index < wordLength; index++)
                        isSame = aIn_pLine[ start + index ] == aIn_pWord[ index ];
                if(isSame
                   && (start + wordLength == lineLength || aIn_pLine[ start + wordLength ] == ' '))
                        return (true);
        }
        return (false);
}

/**
 * @brief Gives the allocator the RAM of the loader's memory map, and takes back what the kernel,
 * the first MiB and the loader's own structures hold.
 *
 * @param[in] aIn_pInformation The loader's information.
 */
static void
ReadMemoryMap(const Amd64_Multiboot_Information* aIn_pInformation)
{
        if((aIn_pInformation->flags & AMD64_MULTIBOOT_HAS_MEMORY_MAP) == 0)
                Kern_Panic("the loader gave no memory map");

        const uint8_t* pCursor = (const uint8_t*) (uintptr_t) aIn_pInformation->memoryMapAddress;
        const uint8_t* pEnd    = pCursor + aIn_pInformation->memoryMapLength;
        uint64_t       usable  = 0;

        Kern_PageFrames_Init();
        while(pCursor + sizeof(Amd64_Multiboot_MemoryRange) <= pEnd)
        {
                const Amd64_Multiboot_MemoryRange* pRange = (const Amd64_Multiboot_MemoryRange*)
                        pCursor;

                if(pRange->size < sizeof(*pRange) - sizeof(pRange->size))
                        Kern_Panic("a memory map entry of %u bytes is too short", pRange->size);
                if(pRange->type == AMD64_MULTIBOOT_MEMORY_AVAILABLE)
                {
                        Kern_PageFrames_AddFree(pRange->address, pRange->length);
                        usable += pRange->length;
                }
                pCursor += pRange->size + sizeof(pRange->size);
        }
        /* Real-mode structures, the BIOS and devices live below 1 MiB. */
        Kern_PageFrames_Reserve(0, 0x100000);
        Kern_PageFrames_Reserve((uintptr_t) gKern_ImageStart,
                                (uint64_t) (gKern_ImageEnd - gKern_ImageStart));
        Kern_PageFrames_Reserve((uintptr_t) aIn_pInformation, sizeof(*aIn_pInformation));
        Kern_PageFrames_Reserve(aIn_pInformation->memoryMapAddress,
                                aIn_pInformation->memoryMapLength);
        if((aIn_pInformation->flags & AMD64_MULTIBOOT_HAS_COMMAND_LINE) != 0)
                Kern_PageFrames_Reserve(aIn_pInformation->commandLine, KERN_MAIN_COMMAND_LINE_MAX);
        Kern_Print("memory: %lu MiB usable, %lu page frames free below 1 GiB\n",
                   (unsigned long) (usable >> 20),
                   (unsigned long) Kern_PageFrames_FreeCount());
}

/**
 * @brief Takes three frames and gives them back, and checks what the allocator answers.
 */
static void
CheckPageFrames(void)
{
        const uint64_t imageStart = (uintptr_t) gKern_ImageStart;
        const uint64_t imageEnd   = (uintptr_t) gKern_ImageEnd;
        const uint64_t before     = Kern_PageFrames_FreeCount();
        uint64_t       frames[ 3 ];

        for(unsigned index = 0; index < 3; index++)
        {
                if(!Kern_PageFrames_Allocate(&frames[ index ]))
                        Kern_Panic("no page frame is free");
                const uint64_t frame = frames[ index ];

                if(frame % KERN_PAGE_FRAMES_SIZE != 0 || frame < 0x100000
                   || (frame + KERN_PAGE_FRAMES_SIZE > imageStart && frame < imageEnd))
                        Kern_Panic("the allocator gave %p, which it holds back", (void*) frame);
                for(unsigned earlier = 0; earlier < index; earlier++)
                        if(frames[ earlier ] == frame)
                                Kern_Panic("the allocator gave %p twice", (void*) frame);
        }
        if(Kern_PageFrames_FreeCount() != before - 3)
                Kern_Panic("three allocations took %lu frames",
                           (unsigned long) (before - Kern_PageFrames_FreeCount()));
        for(unsigned index = 0; index < 3; index++)
                Kern_PageFrames_Free(frames[ index ]);
        if(Kern_PageFrames_FreeCount() != before)
                Kern_Panic("the frames did not all come back");
        Kern_Print("memory: allocator checked\n");
}

/**
 * @brief Turns interrupts on until the timer has ticked KERN_MAIN_TICKS times.
 */
static void
CheckTimer(void)
{
        Amd64_Pic_Init();
        Amd64_Timer_Init();
        __asm__ volatile("sti");
        while(Amd64_Timer_Ticks() < KERN_MAIN_TICKS)
                __asm__ volatile("hlt");
        __asm__ volatile("cli");
        Kern_Print("timer: %u ticks at %u Hz\n", KERN_MAIN_TICKS, AMD64_TIMER_HZ);
}

/**
 * @brief The first C function of the kernel, called in 64-bit mode by the boot code.
 *
 * It sets up the processor's tables, checks that an exception comes back, reads the memory map,
 * checks the page frame allocator and the timer, and halts. With `opentala.test=page-fault` on the
 * command line, it writes to an unmapped address first, and with `opentala.test=double-fault` it
 * pushes to an unmapped stack; either must end in a panic that names the fault.
 *
 * @param[in] aBootMagic       The value the loader left in EAX; 0x2BADB002 for Multiboot.
 * @param[in] aBootInformation The physical address of the loader's information structure.
 */
__attribute__((noreturn)) void
Kern_Main(uint32_t aBootMagic, uint32_t aBootInformation)
{
        Amd64_Serial_Init();
        Kern_Print("OpenTala: amd64, 64-bit mode\n");
        if(aBootMagic != AMD64_MULTIBOOT_LOADER_MAGIC)
                Kern_Panic("loaded by an unknown loader (magic %x)", aBootMagic);
        Kern_Print("OpenTala: loaded by Multiboot\n");

        const Amd64_Multiboot_Information* pInformation
                = (const Amd64_Multiboot_Information*) (uintptr_t) aBootInformation;
        const char* pCommandLine = (pInformation->flags & AMD64_MULTIBOOT_HAS_COMMAND_LINE) != 0
                ? (const char*) (uintptr_t) pInformation->commandLine
                : NULL;

        Amd64_Cpu_Init();
        __asm__ volatile("int3");
        if(Amd64_Trap_Breakpoints() != 1)
                Kern_Panic("the breakpoint did not reach its handler");
        Kern_Print("traps: breakpoint handled and resumed\n");

        ReadMemoryMap(pInformation);
        CheckPageFrames();
        CheckTimer();

        if(HasWord(pCommandLine, "opentala.test=page-fault"))
        {
                Kern_Print("test: writing to %p\n", (void*) KERN_MAIN_UNMAPPED);
                *(volatile uint32_t*) KERN_MAIN_UNMAPPED = 1;
                Kern_Panic("the write to an unmapped address did not fault");
        }
        if(HasWord(pCommandLine, "opentala.test=double-fault"))
        {
                /*
                 * The stack pointer goes to an unmapped address: the push faults, the processor
                 * cannot push the page fault's frame there either, and the double fault that
                 * follows must run on its own stack to be reported at all.
                 */
                Kern_Print("test: pushing to a stack at %p\n", (void*) KERN_MAIN_UNMAPPED);
                __asm__ volatile("movq %0, %%rsp\n\t"
                                 "pushq $0"
                                 :
                                 : "r"(KERN_MAIN_UNMAPPED + KERN_PAGE_FRAMES_SIZE)
                                 : "memory");
                Kern_Panic("the push to an unmapped stack did not fault");
        }
        Kern_Print("OpenTala: halting\n");
        Kern_Halt();
}
