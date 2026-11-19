/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdint.h>

#include "amd64/Amd64.Cpu.h"
#include "amd64/Amd64.Pic.h"
#include "amd64/Amd64.Trap.h"
#include "kern/Kern.Print.h"

#define AMD64_TRAP_BREAKPOINT 3
#define AMD64_TRAP_PAGE_FAULT 14
#define AMD64_TRAP_IRQS       16

/** The names of the exceptions, by vector; reserved vectors have none. */
static const char* const gskExceptionNames[ 32 ] = {
        "divide error",
        "debug",
        "non-maskable interrupt",
        "breakpoint",
        "overflow",
        "bound range exceeded",
        "invalid opcode",
        "device not available",
        "double fault",
        "coprocessor segment overrun",
        "invalid TSS",
        "segment not present",
        "stack-segment fault",
        "general protection fault",
        "page fault",
        NULL,
        "x87 floating-point exception",
        "alignment check",
        "machine check",
        "SIMD floating-point exception",
        "virtualization exception",
        "control protection exception",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "hypervisor injection exception",
        "VMM communication exception",
        "security exception",
        NULL,
};

static Amd64_Trap_IrqHandler gsIrqHandlers[ AMD64_TRAP_IRQS ];
static uint64_t              gsBreakpoints;

void
Amd64_Trap_SetIrqHandler(unsigned aIrq, Amd64_Trap_IrqHandler fHandler)
{
        if(aIrq >= AMD64_TRAP_IRQS)
                Kern_Panic("IRQ %u does not exist", aIrq);
        gsIrqHandlers[ aIrq ] = fHandler;
}

uint64_t
Amd64_Trap_Breakpoints(void)
{
        return (gsBreakpoints);
}

/**
 * @brief Writes the registers of a frame, four to a line.
 *
 * @param[in] aIn_pFrame The frame.
 */
static void
WriteRegisters(const Amd64_Trap_Frame* aIn_pFrame)
{
        Kern_Print("  rax %016lx rbx %016lx rcx %016lx rdx %016lx\n",
                   (unsigned long) aIn_pFrame->rax,
                   (unsigned long) aIn_pFrame->rbx,
                   (unsigned long) aIn_pFrame->rcx,
                   (unsigned long) aIn_pFrame->rdx);
        Kern_Print("  rsi %016lx rdi %016lx rbp %016lx rsp %016lx\n",
                   (unsigned long) aIn_pFrame->rsi,
                   (unsigned long) aIn_pFrame->rdi,
                   (unsigned long) aIn_pFrame->rbp,
                   (unsigned long) aIn_pFrame->rsp);
        Kern_Print("  r8  %016lx r9  %016lx r10 %016lx r11 %016lx\n",
                   (unsigned long) aIn_pFrame->r8,
                   (unsigned long) aIn_pFrame->r9,
                   (unsigned long) aIn_pFrame->r10,
                   (unsigned long) aIn_pFrame->r11);
        Kern_Print("  r12 %016lx r13 %016lx r14 %016lx r15 %016lx\n",
                   (unsigned long) aIn_pFrame->r12,
                   (unsigned long) aIn_pFrame->r13,
                   (unsigned long) aIn_pFrame->r14,
                   (unsigned long) aIn_pFrame->r15);
        Kern_Print("  rip %016lx cs %04lx rflags %08lx ss %04lx\n",
                   (unsigned long) aIn_pFrame->rip,
                   (unsigned long) aIn_pFrame->cs,
                   (unsigned long) aIn_pFrame->rflags,
                   (unsigned long) aIn_pFrame->ss);
}

/**
 * @brief Writes what an exception was and where, and panics.
 *
 * @param[in] aIn_pFrame The frame of the exception.
 */
__attribute__((noreturn)) static void
Fatal(const Amd64_Trap_Frame* aIn_pFrame)
{
        const char* pName = aIn_pFrame->vector < 32 ? gskExceptionNames[ aIn_pFrame->vector ]
                                                    : NULL;

        WriteRegisters(aIn_pFrame);
        if(aIn_pFrame->vector == AMD64_TRAP_PAGE_FAULT)
                Kern_Panic("page fault at %p, address %p, error %lx",
                           (void*) aIn_pFrame->rip,
                           (void*) Amd64_Cpu_ReadCr2(),
                           (unsigned long) aIn_pFrame->errorCode);
        Kern_Panic("%s (vector %lu) at %p, error %lx",
                   pName != NULL ? pName : "reserved exception",
                   (unsigned long) aIn_pFrame->vector,
                   (void*) aIn_pFrame->rip,
                   (unsigned long) aIn_pFrame->errorCode);
}

void
Amd64_Trap_Dispatch(Amd64_Trap_Frame* aIn_pFrame)
{
        const uint64_t vector = aIn_pFrame->vector;

        if(vector == AMD64_TRAP_BREAKPOINT)
        {
                gsBreakpoints++;
                return;
        }
        if(vector >= AMD64_TRAP_IRQ_BASE && vector < AMD64_TRAP_IRQ_BASE + AMD64_TRAP_IRQS)
        {
                const unsigned irq = (unsigned) (vector - AMD64_TRAP_IRQ_BASE);

                /* A spurious IRQ 7 or 15 is not acknowledged; the PIC did not raise it. */
                if(Amd64_Pic_IsSpurious(irq))
                        return;
                if(gsIrqHandlers[ irq ] != NULL)
                        gsIrqHandlers[ irq ](aIn_pFrame);
                Amd64_Pic_Acknowledge(irq);
                return;
        }
        Fatal(aIn_pFrame);
}
