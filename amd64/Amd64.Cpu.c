/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "amd64/Amd64.Cpu.h"

/** Each stack of the interrupt stack table. */
#define AMD64_CPU_IST_STACK_SIZE 16384

/** The size of one stub of Amd64.Traps.S; the stubs follow each other at this distance. */
#define AMD64_CPU_STUB_SIZE 16

/** A 64-bit interrupt gate, present, of privilege level 0. */
#define AMD64_CPU_GATE_INTERRUPT 0x8E
/** The same of privilege level 3, which `int3` may raise from user mode. */
#define AMD64_CPU_GATE_INTERRUPT_USER 0xEE
/** An available 64-bit TSS, present. */
#define AMD64_CPU_DESCRIPTOR_TSS 0x89

#define AMD64_CPU_VECTOR_NMI           2
#define AMD64_CPU_VECTOR_BREAKPOINT    3
#define AMD64_CPU_VECTOR_DOUBLE_FAULT  8
#define AMD64_CPU_VECTOR_MACHINE_CHECK 18

/** The task state segment of amd64: in long mode it holds only stack pointers. */
typedef struct __attribute__((packed))
{
        uint32_t reserved0;
        uint64_t rsp[ 3 ];
        uint64_t reserved1;
        uint64_t ist[ 7 ];
        uint64_t reserved2;
        uint16_t reserved3;
        uint16_t ioMapBase;
} Tss;

/** One gate of the IDT. */
typedef struct __attribute__((packed))
{
        uint16_t offsetLow;
        uint16_t selector;
        uint8_t  ist;
        uint8_t  attributes;
        uint16_t offsetMiddle;
        uint32_t offsetHigh;
        uint32_t reserved;
} Gate;

/** The operand of LGDT and LIDT. */
typedef struct __attribute__((packed))
{
        uint16_t limit;
        uint64_t base;
} TableRegister;

/** The first stub; the stub of vector v is AMD64_CPU_STUB_SIZE * v bytes after it. */
extern char gAmd64_Traps_Stubs[] __asm__("Amd64_Traps_Stubs");

/*
 * Null, kernel code, kernel data, and the TSS, whose descriptor takes two entries. The code and
 * data descriptors are those of the boot code, with the accessed bit set.
 */
static uint64_t gsGdt[ 5 ] __attribute__((aligned(16))) = {
        0, 0x00AF9B000000FFFFULL, 0x00CF93000000FFFFULL, 0, 0,
};

static Tss  gsTss __attribute__((aligned(16)));
static Gate gsIdt[ AMD64_CPU_VECTORS ] __attribute__((aligned(16)));

static uint8_t gsIstStacks[ 3 ][ AMD64_CPU_IST_STACK_SIZE ] __attribute__((aligned(16)));

/**
 * @brief Writes the TSS descriptor into the GDT and loads the GDT, the segments and the TSS.
 */
static void
LoadGdt(void)
{
        const uint64_t      base  = (uint64_t) (uintptr_t) &gsTss;
        const uint64_t      limit = sizeof(gsTss) - 1;
        const TableRegister gdtr  = { sizeof(gsGdt) - 1, (uint64_t) (uintptr_t) gsGdt };

        gsGdt[ 3 ] = (limit & 0xFFFF) | ((base & 0xFFFFFF) << 16)
                | ((uint64_t) AMD64_CPU_DESCRIPTOR_TSS << 40) | (((limit >> 16) & 0xF) << 48)
                | (((base >> 24) & 0xFF) << 56);
        gsGdt[ 4 ] = base >> 32;
        __asm__ volatile("lgdt %0\n\t"
                         /* A far return loads CS: push the selector and the address after it. */
                         "pushq %1\n\t"
                         "leaq 1f(%%rip), %%rax\n\t"
                         "pushq %%rax\n\t"
                         "lretq\n"
                         "1:\n\t"
                         "movw %w2, %%ax\n\t"
                         "movw %%ax, %%ds\n\t"
                         "movw %%ax, %%es\n\t"
                         "movw %%ax, %%ss\n\t"
                         "ltr %w3"
                         :
                         : "m"(gdtr),
                           "i"(AMD64_CPU_SELECTOR_CODE),
                           "r"(AMD64_CPU_SELECTOR_DATA),
                           "r"(AMD64_CPU_SELECTOR_TSS)
                         : "rax", "memory");
}

/**
 * @brief The interrupt stack table entry of a vector, or 0 for the stack the processor is on.
 *
 * @param[in] aVector The vector.
 * @return The entry.
 */
static uint8_t
IstOf(unsigned aVector)
{
        switch(aVector)
        {
        case AMD64_CPU_VECTOR_DOUBLE_FAULT:
                return (AMD64_CPU_IST_DOUBLE_FAULT);
        case AMD64_CPU_VECTOR_NMI:
                return (AMD64_CPU_IST_NMI);
        case AMD64_CPU_VECTOR_MACHINE_CHECK:
                return (AMD64_CPU_IST_MACHINE_CHECK);
        default:
                return (0);
        }
}

/**
 * @brief Fills the IDT with the stubs and loads it.
 */
static void
LoadIdt(void)
{
        const TableRegister idtr = { sizeof(gsIdt) - 1, (uint64_t) (uintptr_t) gsIdt };

        for(unsigned vector = 0; vector < AMD64_CPU_VECTORS; vector++)
        {
                const uint64_t handler = (uint64_t) (uintptr_t) (gAmd64_Traps_Stubs
                                                                 + (uintptr_t) vector
                                                                         * AMD64_CPU_STUB_SIZE);
                Gate*          pGate   = &gsIdt[ vector ];

                pGate->offsetLow    = (uint16_t) handler;
                pGate->selector     = AMD64_CPU_SELECTOR_CODE;
                pGate->ist          = IstOf(vector);
                pGate->attributes   = vector == AMD64_CPU_VECTOR_BREAKPOINT
                        ? AMD64_CPU_GATE_INTERRUPT_USER
                        : AMD64_CPU_GATE_INTERRUPT;
                pGate->offsetMiddle = (uint16_t) (handler >> 16);
                pGate->offsetHigh   = (uint32_t) (handler >> 32);
                pGate->reserved     = 0;
        }
        __asm__ volatile("lidt %0" : : "m"(idtr) : "memory");
}

void
Amd64_Cpu_Init(void)
{
        for(unsigned index = 0; index < 3; index++)
                gsTss.ist[ index ] = (uint64_t) (uintptr_t) (gsIstStacks[ index ]
                                                             + AMD64_CPU_IST_STACK_SIZE);
        /* No I/O permission bitmap: the base points past the end of the segment. */
        gsTss.ioMapBase = sizeof(gsTss);
        LoadGdt();
        LoadIdt();
}
