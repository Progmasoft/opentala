/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdint.h>

#include "amd64/Amd64.Paging.h"
#include "amd64/Amd64.User.h"
#include "kern/Kern.PageFrames.h"
#include "kern/Kern.Print.h"
#include "kern/Kern.Process.h"

/** What Amd64_User_Leave() gives back for a killed process: this bit and the signal. */
#define KERN_PROCESS_KILLED 0x100

/** Where Kern_Process_Run() went into user mode, for the way back. */
static Amd64_User_tContext gsContext;
static uint32_t            gsRunning;
static uint32_t            gsLastId;

/**
 * @brief Takes a frame and maps it at aVirtual.
 *
 * @return The frame's address, which the kernel reaches one to one, or 0 when none is left.
 */
static uint64_t
MapNewPage(uint64_t aVirtual, unsigned aFlags)
{
        uint64_t frame = 0;

        if(!Kern_PageFrames_Allocate(&frame))
                return (0);
        uint8_t* pBytes = (uint8_t*) (uintptr_t) frame;

        /* A fresh page shows the process nothing of what the frame held before. */
        for(unsigned index = 0; index < KERN_PAGE_FRAMES_SIZE; index++)
                pBytes[ index ] = 0;
        if(!Amd64_Paging_MapUser(aVirtual, frame, aFlags))
        {
                Kern_PageFrames_Free(frame);
                return (0);
        }
        return (frame);
}

/** Maps the program and its stack; panics without the memory, as nothing runs without it. */
static void
MapProgram(const void* aIn_pProgram, size_t aSize)
{
        if(aSize > KERN_PAGE_FRAMES_SIZE)
                Kern_Panic("a program of %lu bytes is over one page", (unsigned long) aSize);
        const uint64_t text = MapNewPage(KERN_PROCESS_TEXT, AMD64_PAGING_EXECUTABLE);

        if(text == 0)
                Kern_Panic("no memory for a program");
        /* The kernel reaches the frame one to one; the process sees the same bytes at its address. */
        uint8_t*       pPage  = (uint8_t*) (uintptr_t) text;
        const uint8_t* pBytes = (const uint8_t*) aIn_pProgram;

        for(size_t index = 0; index < aSize; index++)
                pPage[ index ] = pBytes[ index ];
        for(unsigned page = 1; page <= KERN_PROCESS_STACK_PAGES; page++)
                if(MapNewPage(KERN_PROCESS_STACK_TOP - (uint64_t) page * KERN_PAGE_FRAMES_SIZE,
                              AMD64_PAGING_WRITABLE)
                   == 0)
                        Kern_Panic("no memory for a stack");
}

Kern_Process_tOutcome
Kern_Process_Run(const void* aIn_pProgram, size_t aSize, uint64_t aArgument)
{
        if(gsRunning != 0)
                Kern_Panic("process %u is running already", gsRunning);
        MapProgram(aIn_pProgram, aSize);
        gsRunning = ++gsLastId;

        const int64_t         ended   = Amd64_User_Enter(&gsContext,
                                                         KERN_PROCESS_TEXT,
                                                         KERN_PROCESS_STACK_TOP,
                                                         aArgument);
        Kern_Process_tOutcome outcome = { gsRunning, 0, 0 };

        if((ended & KERN_PROCESS_KILLED) != 0)
                outcome.signal = (int) (ended & 0xFF);
        else
                outcome.status = (int) (ended & 0xFF);
        gsRunning = 0;
        Amd64_Paging_ClearUser();
        return (outcome);
}

uint32_t
Kern_Process_Id(void)
{
        return (gsRunning);
}

void
Kern_Process_Exit(int aStatus)
{
        if(gsRunning == 0)
                Kern_Panic("exit without a process");
        Amd64_User_Leave(&gsContext, aStatus & 0xFF);
}

void
Kern_Process_Kill(int aSignal)
{
        if(gsRunning == 0)
                Kern_Panic("signal %d without a process", aSignal);
        Amd64_User_Leave(&gsContext, KERN_PROCESS_KILLED | (aSignal & 0xFF));
}

const char*
Kern_Process_SignalName(int aSignal)
{
        switch(aSignal)
        {
        case KERN_SIGNAL_ILL:
                return ("SIGILL");
        case KERN_SIGNAL_TRAP:
                return ("SIGTRAP");
        case KERN_SIGNAL_FPE:
                return ("SIGFPE");
        case KERN_SIGNAL_BUS:
                return ("SIGBUS");
        case KERN_SIGNAL_SEGV:
                return ("SIGSEGV");
        case KERN_SIGNAL_SYS:
                return ("SIGSYS");
        default:
                return ("a signal");
        }
}
