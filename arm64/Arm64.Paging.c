/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arm64/Arm64.Paging.h"
#include "kern/Kern.Machine.h"
#include "kern/Kern.PageFrames.h"
#include "kern/Kern.Print.h"

#define ARM64_PAGING_ENTRIES 512U
#define ARM64_PAGING_PAGE    4096ULL
#define ARM64_PAGING_GIB     0x40000000ULL

/* Descriptor bits. */
#define ARM64_PAGING_VALID      0x1ULL
#define ARM64_PAGING_TYPE_MASK  0x3ULL
#define ARM64_PAGING_TABLE      0x3ULL
#define ARM64_PAGING_BLOCK      0x1ULL
#define ARM64_PAGING_PAGE_ENTRY 0x3ULL
#define ARM64_PAGING_DEVICE     (0ULL << 2)
#define ARM64_PAGING_NORMAL     (1ULL << 2)
#define ARM64_PAGING_EL0        (1ULL << 6)
#define ARM64_PAGING_READ_ONLY  (1ULL << 7)
#define ARM64_PAGING_INNER      (3ULL << 8)
#define ARM64_PAGING_ACCESSED   (1ULL << 10)
#define ARM64_PAGING_NOT_GLOBAL (1ULL << 11)
#define ARM64_PAGING_PXN        (1ULL << 53)
#define ARM64_PAGING_UXN        (1ULL << 54)
#define ARM64_PAGING_ADDRESS    0x0000FFFFFFFFF000ULL

/** MAIR_EL1: attribute 0 Device-nGnRE, attribute 1 normal write-back, read and write allocate. */
#define ARM64_PAGING_MAIR 0xFF04ULL
/** TCR_EL1: T0SZ 16 (48 bits), write-back inner shareable walks, 4 KiB granule, TTBR1 walks off. */
#define ARM64_PAGING_TCR ((16ULL) | (1ULL << 8) | (1ULL << 10) | (3ULL << 12) | (1ULL << 23))
#define ARM64_PAGING_TCR_IPS_SHIFT 32
/** SCTLR_EL1: the MMU, alignment checks of the stack, the data and instruction caches. */
#define ARM64_PAGING_SCTLR_ON ((1ULL << 0) | (1ULL << 2) | (1ULL << 3) | (1ULL << 12))
/** SCTLR_EL1.SPAN: when clear, PSTATE.PAN is set on every exception into EL1. */
#define ARM64_PAGING_SCTLR_SPAN (1ULL << 23)

/** The kernel's level 0 table and the level 1 table of its first 512 GiB. */
static uint64_t gsLevel0[ ARM64_PAGING_ENTRIES ] __attribute__((aligned(4096)));
static uint64_t gsLevel1[ ARM64_PAGING_ENTRIES ] __attribute__((aligned(4096)));
static bool     gsHasPan;

void
Arm64_Paging_EnableMmu(void)
{
        uint64_t features = 0;
        uint64_t sctlr    = 0;

        gsLevel0[ 0 ] = (uint64_t) (uintptr_t) gsLevel1 | ARM64_PAGING_TABLE;
        /* The devices: never executed, not reachable from EL0. */
        gsLevel1[ 0 ] = 0 | ARM64_PAGING_BLOCK | ARM64_PAGING_DEVICE | ARM64_PAGING_ACCESSED
                | ARM64_PAGING_PXN | ARM64_PAGING_UXN;
        /* The RAM: the kernel's, which EL0 can neither reach nor execute. */
        gsLevel1[ 1 ] = ARM64_PAGING_GIB | ARM64_PAGING_BLOCK | ARM64_PAGING_NORMAL
                | ARM64_PAGING_INNER | ARM64_PAGING_ACCESSED | ARM64_PAGING_UXN;

        __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(features));
        __asm__ volatile("msr mair_el1, %0" : : "r"(ARM64_PAGING_MAIR));
        __asm__ volatile(
                "msr tcr_el1, %0"
                :
                : "r"(ARM64_PAGING_TCR | ((features & 0x7) << ARM64_PAGING_TCR_IPS_SHIFT)));
        __asm__ volatile("msr ttbr0_el1, %0" : : "r"((uint64_t) (uintptr_t) gsLevel0));
        __asm__ volatile("dsb sy\n\ttlbi vmalle1\n\tdsb sy\n\tisb" : : : "memory");

        __asm__ volatile("mrs %0, id_aa64mmfr1_el1" : "=r"(features));
        gsHasPan = ((features >> 20) & 0xF) != 0;
        __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
        sctlr |= ARM64_PAGING_SCTLR_ON;
        if(gsHasPan)
                sctlr &= ~ARM64_PAGING_SCTLR_SPAN;
        __asm__ volatile("msr sctlr_el1, %0\n\tisb" : : "r"(sctlr) : "memory");
}

void
Kern_Machine_InitPaging(void)
{
        /* With PAN, the kernel cannot touch memory EL0 can reach; it reads it with LDTR only. */
        if(gsHasPan)
                __asm__ volatile("msr pan, #1" : : : "memory");
        Kern_Print("paging: execute-never, PAN %s\n", gsHasPan ? "on" : "off");
}

/**
 * @brief The table an entry points to, made and zeroed when it is not there.
 *
 * @return The table, or NULL when the entry maps a block or no frame is free.
 */
static uint64_t*
Table(uint64_t* aInOut_pEntry)
{
        if((*aInOut_pEntry & ARM64_PAGING_VALID) == 0)
        {
                uint64_t frame = 0;

                if(!Kern_PageFrames_Allocate(&frame))
                        return (NULL);
                uint64_t* pTable = (uint64_t*) (uintptr_t) frame;

                for(unsigned index = 0; index < ARM64_PAGING_ENTRIES; index++)
                        pTable[ index ] = 0;
                __asm__ volatile("dsb ishst" : : : "memory");
                *aInOut_pEntry = frame | ARM64_PAGING_TABLE;
        }
        if((*aInOut_pEntry & ARM64_PAGING_TYPE_MASK) != ARM64_PAGING_TABLE)
                return (NULL);
        return ((uint64_t*) (uintptr_t) (*aInOut_pEntry & ARM64_PAGING_ADDRESS));
}

static unsigned
IndexOf(uint64_t aVirtual, unsigned aShift)
{
        return ((unsigned) (aVirtual >> aShift) & (ARM64_PAGING_ENTRIES - 1));
}

/** The level 3 entry of a user address, with the tables above it made where missing. */
static uint64_t*
Leaf(uint64_t aVirtual)
{
        uint64_t* pTable = gsLevel0;

        for(unsigned shift = 39; shift > 12 && pTable != NULL; shift -= 9)
                pTable = Table(&pTable[ IndexOf(aVirtual, shift) ]);
        return (pTable != NULL ? &pTable[ IndexOf(aVirtual, 12) ] : NULL);
}

/** Whether a user page at aVirtual of frame aPhysical may be mapped with these permissions. */
static bool
IsMappable(uint64_t aVirtual, uint64_t aPhysical, bool isWritable, bool isExecutable)
{
        return (aVirtual >= KERN_MACHINE_USER_START && aVirtual < KERN_MACHINE_USER_END
                && aVirtual % ARM64_PAGING_PAGE == 0 && aPhysical % ARM64_PAGING_PAGE == 0
                && !(isWritable && isExecutable));
}

bool
Kern_Machine_MapUser(uint64_t aVirtual, uint64_t aPhysical, unsigned aFlags)
{
        const bool isWritable   = (aFlags & KERN_MACHINE_WRITABLE) != 0;
        const bool isExecutable = (aFlags & KERN_MACHINE_EXECUTABLE) != 0;

        if(!IsMappable(aVirtual, aPhysical, isWritable, isExecutable))
                return (false);
        uint64_t* pLeaf = Leaf(aVirtual);

        if(pLeaf == NULL || (*pLeaf & ARM64_PAGING_VALID) != 0)
                return (false);
        /* EL0 reaches it; the kernel never executes it. */
        *pLeaf = aPhysical | ARM64_PAGING_PAGE_ENTRY | ARM64_PAGING_NORMAL | ARM64_PAGING_INNER
                | ARM64_PAGING_ACCESSED | ARM64_PAGING_NOT_GLOBAL | ARM64_PAGING_EL0
                | ARM64_PAGING_PXN | (isWritable ? 0 : ARM64_PAGING_READ_ONLY)
                | (isExecutable ? 0 : ARM64_PAGING_UXN);
        __asm__ volatile("dsb ishst\n\tisb" : : : "memory");
        return (true);
}

/** Whether EL0 may read the page of aVirtual. */
static bool
IsUserReadable(uint64_t aVirtual)
{
        const uint64_t* pTable = gsLevel0;

        for(unsigned shift = 39;; shift -= 9)
        {
                const uint64_t entry = pTable[ IndexOf(aVirtual, shift) ];

                if(shift == 12)
                        return ((entry & ARM64_PAGING_TYPE_MASK) == ARM64_PAGING_PAGE_ENTRY
                                && (entry & ARM64_PAGING_EL0) != 0);
                if((entry & ARM64_PAGING_TYPE_MASK) != ARM64_PAGING_TABLE)
                        return (false);
                pTable = (const uint64_t*) (uintptr_t) (entry & ARM64_PAGING_ADDRESS);
        }
}

bool
Kern_Machine_CopyIn(void* aOut_pDestination, uint64_t aSource, size_t aLength)
{
        uint8_t* pDestination = aOut_pDestination;

        if(aLength == 0)
                return (true);
        if(aSource < KERN_MACHINE_USER_START || aSource >= KERN_MACHINE_USER_END
           || aLength > KERN_MACHINE_USER_END - aSource)
                return (false);
        for(uint64_t page = aSource & ~(ARM64_PAGING_PAGE - 1); page < aSource + aLength;
            page         += ARM64_PAGING_PAGE)
                if(!IsUserReadable(page))
                        return (false);
        /* LDTRB loads with EL0's permissions: PAN stays on, and the MMU checks it again. */
        for(size_t index = 0; index < aLength; index++)
        {
                uint32_t byte = 0;

                __asm__ volatile("ldtrb %w0, [%1]" : "=r"(byte) : "r"(aSource + index) : "memory");
                pDestination[ index ] = (uint8_t) byte;
        }
        return (true);
}

/** Frees the tables and pages below a table; level 1 is a page table, whose entries are pages. */
static void
FreeTable(uint64_t* aInOut_pTable, unsigned aLevel)
{
        for(unsigned index = 0; index < ARM64_PAGING_ENTRIES; index++)
        {
                const uint64_t entry = aInOut_pTable[ index ];

                if((entry & ARM64_PAGING_VALID) == 0)
                        continue;
                if(aLevel > 1)
                        FreeTable((uint64_t*) (uintptr_t) (entry & ARM64_PAGING_ADDRESS),
                                  aLevel - 1);
                Kern_PageFrames_Free(entry & ARM64_PAGING_ADDRESS);
                aInOut_pTable[ index ] = 0;
        }
}

void
Kern_Machine_ClearUser(void)
{
        /* The kernel's two GiB stay; the user's tables in the first 512 GiB start at its 4 GiB. */
        for(unsigned index = 2; index < ARM64_PAGING_ENTRIES; index++)
        {
                if((gsLevel1[ index ] & ARM64_PAGING_TYPE_MASK) != ARM64_PAGING_TABLE)
                        continue;
                FreeTable((uint64_t*) (uintptr_t) (gsLevel1[ index ] & ARM64_PAGING_ADDRESS), 2);
                Kern_PageFrames_Free(gsLevel1[ index ] & ARM64_PAGING_ADDRESS);
                gsLevel1[ index ] = 0;
        }
        for(unsigned index = 1; index < ARM64_PAGING_ENTRIES / 2; index++)
        {
                if((gsLevel0[ index ] & ARM64_PAGING_VALID) == 0)
                        continue;
                FreeTable((uint64_t*) (uintptr_t) (gsLevel0[ index ] & ARM64_PAGING_ADDRESS), 3);
                Kern_PageFrames_Free(gsLevel0[ index ] & ARM64_PAGING_ADDRESS);
                gsLevel0[ index ] = 0;
        }
        __asm__ volatile("dsb ishst\n\ttlbi vmalle1\n\tdsb ish\n\tisb" : : : "memory");
}
