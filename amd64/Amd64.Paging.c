/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kern/Kern.Machine.h"
#include "kern/Kern.PageFrames.h"
#include "kern/Kern.Print.h"

/*
 * The user half of the address space on amd64, on top of the boot code's page tables: the boot
 * code maps the first GiB one to one, for the kernel only, and user pages go from
 * KERN_MACHINE_USER_START to KERN_MACHINE_USER_END in 4 KiB pages whose tables come from the page
 * frame allocator. A user page is never writable and executable at once, and the kernel's GiB is
 * never open to user mode.
 * No-execute pages, SMEP and SMAP are turned on where the processor has them; the kernel reads user
 * memory only in Kern_Machine_CopyIn(), between STAC and CLAC.
 */

#define AMD64_PAGING_PRESENT 0x001ULL
#define AMD64_PAGING_WRITE   0x002ULL
#define AMD64_PAGING_USER    0x004ULL
#define AMD64_PAGING_LARGE   0x080ULL
#define AMD64_PAGING_NO_EXEC 0x8000000000000000ULL
#define AMD64_PAGING_ADDRESS 0x000FFFFFFFFFF000ULL
#define AMD64_PAGING_ENTRIES 512U
#define AMD64_PAGING_PAGE    4096ULL
/** The PML4 entries of the lower half, where user pages are. */
#define AMD64_PAGING_LOWER_HALF 256U

#define AMD64_PAGING_EFER     0xC0000080U
#define AMD64_PAGING_EFER_NXE (1ULL << 11)
#define AMD64_PAGING_CR4_SMEP (1ULL << 20)
#define AMD64_PAGING_CR4_SMAP (1ULL << 21)

/** CPUID 0x80000001, EDX: no-execute pages. */
#define AMD64_PAGING_CPUID_NX (1U << 20)
/** CPUID 7, EBX: SMEP and SMAP. */
#define AMD64_PAGING_CPUID_SMEP (1U << 7)
#define AMD64_PAGING_CPUID_SMAP (1U << 20)

/** The PML4 of the boot code; every table is in the first GiB, so its address is its own. */
static uint64_t* gsPml4;
static uint64_t  gsNoExecute;
static bool      gsHasSmap;

static void
Cpuid(uint32_t aLeaf, uint32_t aOut_pRegisters[ 4 ])
{
        __asm__ volatile("cpuid"
                         : "=a"(aOut_pRegisters[0]),
                           "=b"(aOut_pRegisters[1]),
                           "=c"(aOut_pRegisters[2]),
                           "=d"(aOut_pRegisters[3])
                         : "a"(aLeaf), "c"(0));
}

static uint64_t
ReadMsr(uint32_t aMsr)
{
        uint32_t low  = 0;
        uint32_t high = 0;

        __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(aMsr));
        return (((uint64_t) high << 32) | low);
}

static void
WriteMsr(uint32_t aMsr, uint64_t aValue)
{
        __asm__ volatile("wrmsr"
                         :
                         : "c"(aMsr), "a"((uint32_t) aValue), "d"((uint32_t) (aValue >> 32)));
}

void
Kern_Machine_InitPaging(void)
{
        uint32_t extended[ 4 ];
        uint32_t features[ 4 ];
        uint64_t cr3 = 0;
        uint64_t cr4 = 0;

        __asm__ volatile("movq %%cr3, %0" : "=r"(cr3));
        gsPml4 = (uint64_t*) (uintptr_t) (cr3 & AMD64_PAGING_ADDRESS);
        Cpuid(0x80000001U, extended);
        if((extended[ 3 ] & AMD64_PAGING_CPUID_NX) != 0)
        {
                WriteMsr(AMD64_PAGING_EFER, ReadMsr(AMD64_PAGING_EFER) | AMD64_PAGING_EFER_NXE);
                gsNoExecute = AMD64_PAGING_NO_EXEC;
        }
        Cpuid(7, features);
        __asm__ volatile("movq %%cr4, %0" : "=r"(cr4));
        if((features[ 1 ] & AMD64_PAGING_CPUID_SMEP) != 0)
                cr4 |= AMD64_PAGING_CR4_SMEP;
        gsHasSmap = (features[ 1 ] & AMD64_PAGING_CPUID_SMAP) != 0;
        if(gsHasSmap)
                cr4 |= AMD64_PAGING_CR4_SMAP;
        __asm__ volatile("movq %0, %%cr4" : : "r"(cr4) : "memory");
        Kern_Print("paging: no-execute %s, SMEP %s, SMAP %s\n",
                   gsNoExecute != 0 ? "on" : "off",
                   (cr4 & AMD64_PAGING_CR4_SMEP) != 0 ? "on" : "off",
                   gsHasSmap ? "on" : "off");
}

/**
 * @brief The table an entry points to, made, zeroed and opened to user mode when it is not there.
 *
 * @param[in,out] aInOut_pEntry The entry.
 * @return The table, or NULL when the entry maps a large page or no frame is free.
 */
static uint64_t*
Table(uint64_t* aInOut_pEntry)
{
        if((*aInOut_pEntry & AMD64_PAGING_PRESENT) == 0)
        {
                uint64_t frame = 0;

                if(!Kern_PageFrames_Allocate(&frame))
                        return (NULL);
                uint64_t* pTable = (uint64_t*) (uintptr_t) frame;

                for(unsigned index = 0; index < AMD64_PAGING_ENTRIES; index++)
                        pTable[ index ] = 0;
                *aInOut_pEntry = frame | AMD64_PAGING_PRESENT | AMD64_PAGING_WRITE;
        }
        if((*aInOut_pEntry & AMD64_PAGING_LARGE) != 0)
                return (NULL);
        /* A page is open to user mode only when every level says so; the leaf decides the rest. */
        *aInOut_pEntry |= AMD64_PAGING_USER;
        return ((uint64_t*) (uintptr_t) (*aInOut_pEntry & AMD64_PAGING_ADDRESS));
}

static unsigned
IndexOf(uint64_t aVirtual, unsigned aShift)
{
        return ((unsigned) (aVirtual >> aShift) & (AMD64_PAGING_ENTRIES - 1));
}

static bool
IsUserPage(uint64_t aVirtual)
{
        return (aVirtual >= KERN_MACHINE_USER_START && aVirtual < KERN_MACHINE_USER_END
                && aVirtual % AMD64_PAGING_PAGE == 0);
}

/**
 * @brief The page table entry of a user address, with the tables above it made where missing.
 *
 * @return The entry, or NULL when no frame is left for a table.
 */
static uint64_t*
Leaf(uint64_t aVirtual)
{
        uint64_t* pTable = gsPml4;

        for(unsigned shift = 39; shift > 12 && pTable != NULL; shift -= 9)
                pTable = Table(&pTable[ IndexOf(aVirtual, shift) ]);
        return (pTable != NULL ? &pTable[ IndexOf(aVirtual, 12) ] : NULL);
}

bool
Kern_Machine_MapUser(uint64_t aVirtual, uint64_t aPhysical, unsigned aFlags)
{
        const bool isWritable   = (aFlags & KERN_MACHINE_WRITABLE) != 0;
        const bool isExecutable = (aFlags & KERN_MACHINE_EXECUTABLE) != 0;

        if(!IsUserPage(aVirtual) || aPhysical % AMD64_PAGING_PAGE != 0
           || (isWritable && isExecutable))
                return (false);
        uint64_t* pLeaf = Leaf(aVirtual);

        if(pLeaf == NULL || (*pLeaf & AMD64_PAGING_PRESENT) != 0)
                return (false);
        *pLeaf = aPhysical | AMD64_PAGING_PRESENT | AMD64_PAGING_USER
                | (isWritable ? AMD64_PAGING_WRITE : 0) | (isExecutable ? 0 : gsNoExecute);
        return (true);
}

/**
 * @brief Whether user mode may read the page of aVirtual: present and user at every level.
 */
static bool
IsUserReadable(uint64_t aVirtual)
{
        const uint64_t* pTable = gsPml4;

        for(unsigned shift = 39;; shift -= 9)
        {
                const uint64_t entry = pTable[ IndexOf(aVirtual, shift) ];

                if((entry & (AMD64_PAGING_PRESENT | AMD64_PAGING_USER))
                   != (AMD64_PAGING_PRESENT | AMD64_PAGING_USER))
                        return (false);
                if(shift == 12)
                        return (true);
                if((entry & AMD64_PAGING_LARGE) != 0)
                        return (false);
                pTable = (const uint64_t*) (uintptr_t) (entry & AMD64_PAGING_ADDRESS);
        }
}

/**
 * @brief Whether a user page is present and writable from user mode: every level must allow both.
 *
 * @param[in] aVirtual The page's address.
 * @return Whether it is.
 */
static bool
IsUserWritable(uint64_t aVirtual)
{
        const uint64_t* pTable = gsPml4;
        const uint64_t  needs  = AMD64_PAGING_PRESENT | AMD64_PAGING_USER | AMD64_PAGING_WRITE;

        for(unsigned shift = 39;; shift -= 9)
        {
                const uint64_t entry = pTable[ IndexOf(aVirtual, shift) ];

                if((entry & needs) != needs)
                        return (false);
                if(shift == 12)
                        return (true);
                if((entry & AMD64_PAGING_LARGE) != 0)
                        return (false);
                pTable = (const uint64_t*) (uintptr_t) (entry & AMD64_PAGING_ADDRESS);
        }
}

bool
Kern_Machine_CopyOut(uint64_t aDestination, const void* aIn_pSource, size_t aLength)
{
        if(aLength == 0)
                return (true);
        if(aDestination < KERN_MACHINE_USER_START || aDestination >= KERN_MACHINE_USER_END
           || aLength > KERN_MACHINE_USER_END - aDestination)
                return (false);
        for(uint64_t page = aDestination & ~(AMD64_PAGING_PAGE - 1); page < aDestination + aLength;
            page         += AMD64_PAGING_PAGE)
                if(!IsUserWritable(page))
                        return (false);
        if(gsHasSmap)
                __asm__ volatile("stac" : : : "memory");
        for(size_t index = 0; index < aLength; index++)
                ((volatile uint8_t*) (uintptr_t) aDestination)[ index ] = ((
                        const uint8_t*) aIn_pSource)[ index ];
        if(gsHasSmap)
                __asm__ volatile("clac" : : : "memory");
        return (true);
}

bool
Kern_Machine_CopyIn(void* aOut_pDestination, uint64_t aSource, size_t aLength)
{
        if(aLength == 0)
                return (true);
        if(aSource < KERN_MACHINE_USER_START || aSource >= KERN_MACHINE_USER_END
           || aLength > KERN_MACHINE_USER_END - aSource)
                return (false);
        for(uint64_t page = aSource & ~(AMD64_PAGING_PAGE - 1); page < aSource + aLength;
            page         += AMD64_PAGING_PAGE)
                if(!IsUserReadable(page))
                        return (false);
        /* SMAP lets the kernel touch user memory only between STAC and CLAC. */
        if(gsHasSmap)
                __asm__ volatile("stac" : : : "memory");
        for(size_t index = 0; index < aLength; index++)
                ((volatile uint8_t*) aOut_pDestination)[ index ] = ((
                        const volatile uint8_t*) (uintptr_t) aSource)[ index ];
        if(gsHasSmap)
                __asm__ volatile("clac" : : : "memory");
        return (true);
}

/**
 * @brief Frees the frames a table of the given level points to, and the tables below it; level 1
 * is a page table, whose entries are user pages.
 */
static void
FreeTable(uint64_t* aInOut_pTable, unsigned aLevel)
{
        for(unsigned index = 0; index < AMD64_PAGING_ENTRIES; index++)
        {
                const uint64_t entry = aInOut_pTable[ index ];

                if((entry & AMD64_PAGING_PRESENT) == 0)
                        continue;
                if(aLevel > 1)
                        FreeTable((uint64_t*) (uintptr_t) (entry & AMD64_PAGING_ADDRESS),
                                  aLevel - 1);
                Kern_PageFrames_Free(entry & AMD64_PAGING_ADDRESS);
                aInOut_pTable[ index ] = 0;
        }
}

void
Kern_Machine_ClearUser(void)
{
        for(unsigned index = 0; index < AMD64_PAGING_LOWER_HALF; index++)
        {
                uint64_t* pEntry = &gsPml4[ index ];

                if((*pEntry & (AMD64_PAGING_PRESENT | AMD64_PAGING_USER))
                   != (AMD64_PAGING_PRESENT | AMD64_PAGING_USER))
                        continue;
                uint64_t* pPdpt = (uint64_t*) (uintptr_t) (*pEntry & AMD64_PAGING_ADDRESS);

                if(index != 0)
                {
                        FreeTable(pPdpt, 3);
                        Kern_PageFrames_Free(*pEntry & AMD64_PAGING_ADDRESS);
                        *pEntry = 0;
                        continue;
                }
                /* PML4[ 0 ] also holds the kernel's GiB, PDPT[ 0 ], which stays. */
                for(unsigned pdpt = 1; pdpt < AMD64_PAGING_ENTRIES; pdpt++)
                {
                        if((pPdpt[ pdpt ] & AMD64_PAGING_PRESENT) == 0)
                                continue;
                        FreeTable((uint64_t*) (uintptr_t) (pPdpt[ pdpt ] & AMD64_PAGING_ADDRESS),
                                  2);
                        Kern_PageFrames_Free(pPdpt[ pdpt ] & AMD64_PAGING_ADDRESS);
                        pPdpt[ pdpt ] = 0;
                }
                *pEntry &= ~AMD64_PAGING_USER;
        }
        uint64_t cr3 = 0;

        __asm__ volatile("movq %%cr3, %0\n\tmovq %0, %%cr3" : "=r"(cr3) : : "memory");
}
