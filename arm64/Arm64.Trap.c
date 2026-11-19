/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "arm64/Arm64.Gic.h"
#include "arm64/Arm64.Trap.h"
#include "arm64/Arm64.User.h"
#include "kern/Kern.Print.h"
#include "kern/Kern.Process.h"

#define ARM64_TRAP_FROM_EL1 1U
#define ARM64_TRAP_FROM_EL0 2U
#define ARM64_TRAP_SYNC     0U
#define ARM64_TRAP_IRQ      1U

/** ESR_EL1's exception classes, bits 26 to 31. */
#define ARM64_TRAP_CLASS_UNKNOWN         0x00U
#define ARM64_TRAP_CLASS_SVC             0x15U
#define ARM64_TRAP_CLASS_INSTRUCTION_EL0 0x20U
#define ARM64_TRAP_CLASS_PC_ALIGNMENT    0x22U
#define ARM64_TRAP_CLASS_DATA_EL0        0x24U
#define ARM64_TRAP_CLASS_DATA_EL1        0x25U
#define ARM64_TRAP_CLASS_SP_ALIGNMENT    0x26U
#define ARM64_TRAP_CLASS_BRK             0x3CU
/** A data abort's status code for an alignment fault. */
#define ARM64_TRAP_ALIGNMENT_FAULT 0x21U

/** The vectors; their address goes into VBAR_EL1. */
extern char gArm64_Vectors[] __asm__("Arm64_Vectors");

static uint64_t gsBreakpoints;

void
Arm64_Trap_Init(void)
{
        __asm__ volatile("msr vbar_el1, %0\n\tisb" : : "r"(gArm64_Vectors) : "memory");
}

uint64_t
Arm64_Trap_Breakpoints(void)
{
        return (gsBreakpoints);
}

static uint64_t
ReadEsr(void)
{
        uint64_t value = 0;

        __asm__ volatile("mrs %0, esr_el1" : "=r"(value));
        return (value);
}

static uint64_t
ReadFar(void)
{
        uint64_t value = 0;

        __asm__ volatile("mrs %0, far_el1" : "=r"(value));
        return (value);
}

/**
 * @brief Writes the registers of a frame, four to a line, and the syndrome.
 *
 * @param[in] aIn_pFrame The frame.
 */
static void
WriteRegisters(const Arm64_Trap_tFrame* aIn_pFrame)
{
        for(unsigned index = 0; index < 31; index += 4)
        {
                Kern_Print(" ");
                for(unsigned column = index; column < index + 4 && column < 31; column++)
                        Kern_Print(" x%02u %016lx",
                                   column,
                                   (unsigned long) aIn_pFrame->x[ column ]);
                Kern_Print("\n");
        }
        Kern_Print("  sp  %016lx\n", (unsigned long) aIn_pFrame->sp);
        Kern_Print("  elr %016lx spsr %08lx esr %08lx far %016lx\n",
                   (unsigned long) aIn_pFrame->elr,
                   (unsigned long) aIn_pFrame->spsr,
                   (unsigned long) ReadEsr(),
                   (unsigned long) ReadFar());
}

/**
 * @brief Writes what an exception in the kernel was and where, and panics.
 *
 * @param[in] aIn_pFrame The frame of the exception.
 */
__attribute__((noreturn)) static void
Fatal(const Arm64_Trap_tFrame* aIn_pFrame)
{
        const uint64_t esr = ReadEsr();

        WriteRegisters(aIn_pFrame);
        if((esr >> 26) == ARM64_TRAP_CLASS_DATA_EL1)
                Kern_Panic("data abort at %p, address %p, ESR %x",
                           (void*) aIn_pFrame->elr,
                           (void*) ReadFar(),
                           (unsigned) esr);
        Kern_Panic("exception %lu, class %x, at %p, ESR %x",
                   (unsigned long) aIn_pFrame->vector,
                   (unsigned) (esr >> 26),
                   (void*) aIn_pFrame->elr,
                   (unsigned) esr);
}

/**
 * @brief The signal a synchronous exception of a process sends it, as XNU maps the exception.
 *
 * @param[in] aClass The exception class.
 * @param[in] aEsr   The syndrome.
 * @return The signal.
 */
static int
SignalOf(uint64_t aClass, uint64_t aEsr)
{
        switch(aClass)
        {
        case ARM64_TRAP_CLASS_UNKNOWN:
                return (KERN_SIGNAL_ILL);
        case ARM64_TRAP_CLASS_BRK:
                return (KERN_SIGNAL_TRAP);
        case ARM64_TRAP_CLASS_PC_ALIGNMENT:
        case ARM64_TRAP_CLASS_SP_ALIGNMENT:
                return (KERN_SIGNAL_BUS);
        case ARM64_TRAP_CLASS_DATA_EL0:
                return ((aEsr & 0x3F) == ARM64_TRAP_ALIGNMENT_FAULT ? KERN_SIGNAL_BUS
                                                                    : KERN_SIGNAL_SEGV);
        case ARM64_TRAP_CLASS_INSTRUCTION_EL0:
                return (KERN_SIGNAL_SEGV);
        default:
                return (KERN_SIGNAL_ILL);
        }
}

/**
 * @brief A synchronous exception from EL0: Darwin's system call, or the process's fault, which is
 * written out and kills the process with its signal.
 *
 * @param[in,out] aInOut_pFrame The frame.
 */
static void
FromUser(Arm64_Trap_tFrame* aInOut_pFrame)
{
        const uint64_t esr   = ReadEsr();
        const uint64_t class = esr >> 26;

        if(class == ARM64_TRAP_CLASS_SVC)
        {
                Arm64_User_SystemCall(aInOut_pFrame, (uint32_t) (esr & 0xFFFF));
                return;
        }
        if(class == ARM64_TRAP_CLASS_DATA_EL0 || class == ARM64_TRAP_CLASS_INSTRUCTION_EL0)
                Kern_Print("process %u: %s abort at %p, address %p, ESR %x\n",
                           Kern_Process_Id(),
                           class == ARM64_TRAP_CLASS_DATA_EL0 ? "data" : "instruction",
                           (void*) aInOut_pFrame->elr,
                           (void*) ReadFar(),
                           (unsigned) esr);
        else
                Kern_Print("process %u: exception class %x at %p\n",
                           Kern_Process_Id(),
                           (unsigned) class,
                           (void*) aInOut_pFrame->elr);
        Kern_Process_Kill(SignalOf(class, esr));
}

void
Arm64_Trap_Dispatch(Arm64_Trap_tFrame* aInOut_pFrame)
{
        const uint64_t from = aInOut_pFrame->vector / 4;
        const uint64_t kind = aInOut_pFrame->vector % 4;

        if(kind == ARM64_TRAP_IRQ && (from == ARM64_TRAP_FROM_EL1 || from == ARM64_TRAP_FROM_EL0))
        {
                Arm64_Gic_HandleInterrupt();
                return;
        }
        if(kind == ARM64_TRAP_SYNC && from == ARM64_TRAP_FROM_EL0)
        {
                FromUser(aInOut_pFrame);
                return;
        }
        if(kind == ARM64_TRAP_SYNC && from == ARM64_TRAP_FROM_EL1
           && (ReadEsr() >> 26) == ARM64_TRAP_CLASS_BRK)
        {
                /* The return address is the BRK itself; going on means the instruction after it. */
                gsBreakpoints++;
                aInOut_pFrame->elr += 4;
                return;
        }
        Fatal(aInOut_pFrame);
}
