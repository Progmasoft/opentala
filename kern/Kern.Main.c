/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kern/Kern.Boot.h"
#include "kern/Kern.Machine.h"
#include "kern/Kern.PageFrames.h"
#include "kern/Kern.Print.h"
#include "kern/Kern.Process.h"

/** The longest command line the kernel reads; the rest is ignored. */
#define KERN_MAIN_COMMAND_LINE_MAX 4096
/** How many ticks the timer check waits for. */
#define KERN_MAIN_TICKS 10

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

/* The test program of the architecture's UserProgram.S, which the kernel copies into a process. */
extern const char gKern_UserProgramStart[] __asm__("Machine_UserProgram_Start");
extern const char gKern_UserProgramEnd[] __asm__("Machine_UserProgram_End");

void
Kern_Boot_AddRange(Kern_Boot_tRange* aInOut_pRanges,
                   unsigned*         aInOut_pCount,
                   uint64_t          aStart,
                   uint64_t          aLength)
{
        if(*aInOut_pCount >= KERN_BOOT_RANGES)
        {
                Kern_Print("boot: a range at %p is dropped, %u are kept\n",
                           (void*) aStart,
                           KERN_BOOT_RANGES);
                return;
        }
        aInOut_pRanges[ *aInOut_pCount ].start  = aStart;
        aInOut_pRanges[ *aInOut_pCount ].length = aLength;
        (*aInOut_pCount)++;
}

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

bool
Kern_Main_HasWord(const char* aIn_pLine, const char* aIn_pWord)
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
 * @brief Gives the allocator the loader's RAM, and takes back the kernel and what the loader
 * reserved.
 *
 * @param[in] aIn_pInformation What the loader gave.
 */
static void
ReadMemory(const Kern_Boot_tInformation* aIn_pInformation)
{
        uint64_t usable = 0;

        if(aIn_pInformation->memoryCount == 0)
                Kern_Panic("the loader gave no memory");
        Kern_PageFrames_Init();
        for(unsigned index = 0; index < aIn_pInformation->memoryCount; index++)
        {
                Kern_PageFrames_AddFree(aIn_pInformation->memory[ index ].start,
                                        aIn_pInformation->memory[ index ].length);
                usable += aIn_pInformation->memory[ index ].length;
        }
        Kern_PageFrames_Reserve((uintptr_t) gKern_ImageStart,
                                (uint64_t) (gKern_ImageEnd - gKern_ImageStart));
        for(unsigned index = 0; index < aIn_pInformation->reservedCount; index++)
                Kern_PageFrames_Reserve(aIn_pInformation->reserved[ index ].start,
                                        aIn_pInformation->reserved[ index ].length);
        Kern_Print("memory: %lu MiB usable, %lu page frames free\n",
                   (unsigned long) (usable >> 20),
                   (unsigned long) Kern_PageFrames_FreeCount());
}

/**
 * @brief Panics unless frame aIn_pFrames[ aIndex ] is one the allocator may give: aligned, in its
 * range, outside the kernel image, and none of the frames before it.
 *
 * @param[in] aIn_pFrames The frames allocated so far.
 * @param[in] aIndex      The one to check.
 */
static void
CheckAllocatedFrame(const uint64_t* aIn_pFrames, unsigned aIndex)
{
        const uint64_t frame      = aIn_pFrames[ aIndex ];
        const uint64_t imageStart = (uintptr_t) gKern_ImageStart;
        const uint64_t imageEnd   = (uintptr_t) gKern_ImageEnd;
        const bool     isInImage  = frame + KERN_PAGE_FRAMES_SIZE > imageStart && frame < imageEnd;
        const bool     isOutside  = frame < KERN_PAGE_FRAMES_BASE
                || frame >= KERN_PAGE_FRAMES_BASE + KERN_PAGE_FRAMES_LIMIT;

        if(frame % KERN_PAGE_FRAMES_SIZE != 0 || isOutside || isInImage)
                Kern_Panic("the allocator gave %p, which it holds back", (void*) frame);
        for(unsigned earlier = 0; earlier < aIndex; earlier++)
                if(aIn_pFrames[ earlier ] == frame)
                        Kern_Panic("the allocator gave %p twice", (void*) frame);
}

/**
 * @brief Takes three frames and gives them back, and checks what the allocator answers.
 */
static void
CheckPageFrames(void)
{
        const uint64_t before = Kern_PageFrames_FreeCount();
        uint64_t       frames[ 3 ];

        for(unsigned index = 0; index < 3; index++)
        {
                if(!Kern_PageFrames_Allocate(&frames[ index ]))
                        Kern_Panic("no page frame is free");
                CheckAllocatedFrame(frames, index);
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
 * @brief Starts the timer and waits until it has ticked KERN_MAIN_TICKS times.
 */
static void
CheckTimer(void)
{
        Kern_Machine_StartTimer();
        while(Kern_Machine_Ticks() < KERN_MAIN_TICKS)
                Kern_Machine_WaitForInterrupt();
        Kern_Print("timer: %u ticks at %u Hz\n", KERN_MAIN_TICKS, KERN_MACHINE_TIMER_HZ);
}

/**
 * @brief Runs the test program in user mode, and checks that its pages all come back.
 *
 * The command line picks what the program does: by default it writes a line and checks the errors
 * of write; `opentala.test=unknown-system-call` and `opentala.test=mach-trap` make a system call
 * OpenTala does not have, and `opentala.test=user-fault` reads kernel memory. Each must end the
 * process, and only the process.
 *
 * @param[in] aIn_pCommandLine The command line, or NULL.
 */
static void
RunUserProgram(const char* aIn_pCommandLine)
{
        static const char* const skTests[] = {
                "opentala.test=unknown-system-call",
                "opentala.test=mach-trap",
                "opentala.test=user-fault",
        };
        uint64_t argument = 0;

        for(unsigned index = 0; index < sizeof(skTests) / sizeof(skTests[ 0 ]); index++)
                if(Kern_Main_HasWord(aIn_pCommandLine, skTests[ index ]))
                        argument = index + 1;
        const uint64_t              before  = Kern_PageFrames_FreeCount();
        const Kern_Process_tOutcome outcome = Kern_Process_Run(
                gKern_UserProgramStart,
                (size_t) (gKern_UserProgramEnd - gKern_UserProgramStart),
                argument);

        if(outcome.signal != 0)
                Kern_Print("process %u: killed by %s\n",
                           outcome.id,
                           Kern_Process_SignalName(outcome.signal));
        else
                Kern_Print("process %u: exited with status %d\n", outcome.id, outcome.status);
        if(Kern_PageFrames_FreeCount() != before)
                Kern_Panic("the process kept %ld page frames",
                           (long) (before - Kern_PageFrames_FreeCount()));
        Kern_Print("process %u: every page given back\n", outcome.id);
}

void
Kern_Main(const Kern_Boot_tInformation* aIn_pInformation)
{
        const char* pCommandLine = aIn_pInformation->pCommandLine;

        Kern_Print("OpenTala: loaded by %s\n", aIn_pInformation->pLoader);
        Kern_Machine_InitTraps();
        if(Kern_Machine_Breakpoint() != 1)
                Kern_Panic("the breakpoint did not reach its handler");
        Kern_Print("traps: breakpoint handled and resumed\n");

        ReadMemory(aIn_pInformation);
        CheckPageFrames();
        CheckTimer();
        Kern_Machine_InitPaging();
        Kern_Machine_InitUser();
        RunUserProgram(pCommandLine);

        if(Kern_Main_HasWord(pCommandLine, "opentala.test=page-fault"))
        {
                Kern_Print("test: writing to %p\n", (void*) KERN_MACHINE_UNMAPPED);
                *(volatile uint32_t*) KERN_MACHINE_UNMAPPED = 1;
                Kern_Panic("the write to an unmapped address did not fault");
        }
        Kern_Machine_RunTests(pCommandLine);
        Kern_Print("OpenTala: halting\n");
        Kern_Machine_Halt();
}
