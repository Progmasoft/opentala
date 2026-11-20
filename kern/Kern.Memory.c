/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kern/Kern.Memory.h"
#include "kern/Kern.PageFrames.h"
#include "kern/Kern.Print.h"

/*
 * Each page of the heap starts with a header: its class, how many of its blocks are in use, the
 * first of its free blocks, and its neighbours in the list of its class's pages that have a free
 * block. The page frames of the heap are mapped one to one in the kernel's GiB, so a frame's
 * address is its own, and a block's page is its address rounded down to the page.
 */

#define KERN_MEMORY_PAGE    KERN_PAGE_FRAMES_SIZE
#define KERN_MEMORY_HEADER  32U
#define KERN_MEMORY_CLASSES 8U
/** How many blocks the check at boot takes. */
#define KERN_MEMORY_CHECKED 40U
/** A word in each free block, so freeing a block twice is caught. */
#define KERN_MEMORY_FREE_MARK 0x4F70656E4672656EULL

typedef struct Kern_Memory_Page Kern_Memory_tPage;

/** A free block: the next free block of its page, and the mark. */
typedef struct Kern_Memory_FreeBlock
{
        struct Kern_Memory_FreeBlock* pNext;
        uint64_t                      mark;
} Kern_Memory_tFreeBlock;

struct Kern_Memory_Page
{
        Kern_Memory_tPage*      pNext;
        Kern_Memory_tPage*      pPrevious;
        Kern_Memory_tFreeBlock* pFree;
        uint32_t                classIndex;
        uint32_t                used;
};

_Static_assert(sizeof(Kern_Memory_tPage) <= KERN_MEMORY_HEADER, "the page header outgrew its room");

/** The block sizes: 32 to 2048 bytes, and the rest of a page. */
static const uint32_t gsSizes[ KERN_MEMORY_CLASSES ] = {
        32, 64, 128, 256, 512, 1024, 2048, KERN_MEMORY_LARGEST,
};

/** Per class, the pages with a free block. */
static Kern_Memory_tPage* gsPartial[ KERN_MEMORY_CLASSES ];
static uint64_t           gsInUse;

/**
 * @brief The smallest class whose blocks hold a size.
 *
 * @param[in] aSize The size.
 * @return The class, or KERN_MEMORY_CLASSES when no block is that large.
 */
static unsigned
ClassOf(size_t aSize)
{
        for(unsigned index = 0; index < KERN_MEMORY_CLASSES; index++)
                if(aSize <= gsSizes[ index ])
                        return (index);
        return (KERN_MEMORY_CLASSES);
}

/**
 * @brief Takes a page out of the list of its class's pages that have a free block.
 *
 * @param[in,out] aInOut_pPage The page.
 */
static void
Unlink(Kern_Memory_tPage* aInOut_pPage)
{
        if(aInOut_pPage->pPrevious != NULL)
                aInOut_pPage->pPrevious->pNext = aInOut_pPage->pNext;
        else
                gsPartial[ aInOut_pPage->classIndex ] = aInOut_pPage->pNext;
        if(aInOut_pPage->pNext != NULL)
                aInOut_pPage->pNext->pPrevious = aInOut_pPage->pPrevious;
        aInOut_pPage->pNext     = NULL;
        aInOut_pPage->pPrevious = NULL;
}

/**
 * @brief Puts a page at the head of the list of its class's pages that have a free block.
 *
 * @param[in,out] aInOut_pPage The page.
 */
static void
Push(Kern_Memory_tPage* aInOut_pPage)
{
        aInOut_pPage->pPrevious = NULL;
        aInOut_pPage->pNext     = gsPartial[ aInOut_pPage->classIndex ];
        if(aInOut_pPage->pNext != NULL)
                aInOut_pPage->pNext->pPrevious = aInOut_pPage;
        gsPartial[ aInOut_pPage->classIndex ] = aInOut_pPage;
}

/**
 * @brief Takes a frame for a page of one class, every block free, at the head of its class's list.
 *
 * @param[in] aClassIndex The class.
 * @return The page, or NULL when no frame is free.
 */
static Kern_Memory_tPage*
NewPage(unsigned aClassIndex)
{
        uint64_t frame = 0;

        if(!Kern_PageFrames_Allocate(&frame))
                return (NULL);
        Kern_Memory_tPage* pPage = (Kern_Memory_tPage*) (uintptr_t) frame;
        const uint32_t     size  = gsSizes[ aClassIndex ];

        pPage->classIndex = aClassIndex;
        pPage->used       = 0;
        pPage->pFree      = NULL;
        /* The blocks are threaded from the last, so the first block is handed out first. */
        for(uint32_t offset = KERN_MEMORY_HEADER
                    + ((KERN_MEMORY_PAGE - KERN_MEMORY_HEADER) / size - 1) * size;
            offset >= KERN_MEMORY_HEADER;
            offset -= size)
        {
                Kern_Memory_tFreeBlock* pBlock = (Kern_Memory_tFreeBlock*) ((uint8_t*) pPage
                                                                            + offset);

                pBlock->pNext = pPage->pFree;
                pBlock->mark  = KERN_MEMORY_FREE_MARK;
                pPage->pFree  = pBlock;
                if(offset < KERN_MEMORY_HEADER + size)
                        break;
        }
        Push(pPage);
        return (pPage);
}

void*
Kern_Memory_Allocate(size_t aSize)
{
        const unsigned classIndex = ClassOf(aSize);

        if(aSize == 0 || classIndex == KERN_MEMORY_CLASSES)
                Kern_Panic("the heap was asked for %lu bytes", (unsigned long) aSize);
        Kern_Memory_tPage* pPage = gsPartial[ classIndex ];

        if(pPage == NULL)
                pPage = NewPage(classIndex);
        if(pPage == NULL)
                return (NULL);
        Kern_Memory_tFreeBlock* pBlock = pPage->pFree;

        pPage->pFree = pBlock->pNext;
        pPage->used++;
        if(pPage->pFree == NULL)
                Unlink(pPage);
        gsInUse++;
        uint8_t* pBytes = (uint8_t*) pBlock;

        for(uint32_t index = 0; index < gsSizes[ classIndex ]; index++)
                pBytes[ index ] = 0;
        return (pBytes);
}

void
Kern_Memory_Free(void* aIn_pBlock)
{
        if(aIn_pBlock == NULL)
                return;
        const uintptr_t    address = (uintptr_t) aIn_pBlock;
        Kern_Memory_tPage* pPage   = (Kern_Memory_tPage*) (address
                                                           & ~(uintptr_t) (KERN_MEMORY_PAGE - 1));

        if(pPage->classIndex >= KERN_MEMORY_CLASSES)
                Kern_Panic("%p is not a block of the heap", aIn_pBlock);
        const uint32_t size   = gsSizes[ pPage->classIndex ];
        const uint32_t offset = (uint32_t) (address - (uintptr_t) pPage);

        if(offset < KERN_MEMORY_HEADER || (offset - KERN_MEMORY_HEADER) % size != 0)
                Kern_Panic("%p is not the start of a block", aIn_pBlock);
        Kern_Memory_tFreeBlock* pBlock = aIn_pBlock;

        if(pBlock->mark == KERN_MEMORY_FREE_MARK || pPage->used == 0)
                Kern_Panic("the block %p was freed twice", aIn_pBlock);
        const bool wasFull = pPage->pFree == NULL;

        pBlock->mark  = KERN_MEMORY_FREE_MARK;
        pBlock->pNext = pPage->pFree;
        pPage->pFree  = pBlock;
        pPage->used--;
        gsInUse--;
        if(wasFull)
                Push(pPage);
        if(pPage->used == 0)
        {
                Unlink(pPage);
                pPage->classIndex = KERN_MEMORY_CLASSES;
                Kern_PageFrames_Free((uint64_t) (uintptr_t) pPage);
        }
}

uint64_t
Kern_Memory_InUse(void)
{
        return (gsInUse);
}

void
Kern_Memory_Check(void)
{
        const uint64_t frames = Kern_PageFrames_FreeCount();
        void*          pBlocks[ KERN_MEMORY_CHECKED ];

        for(unsigned index = 0; index < KERN_MEMORY_CHECKED; index++)
        {
                const size_t size = (size_t) gsSizes[ index % KERN_MEMORY_CLASSES ] - index % 3;

                pBlocks[ index ] = Kern_Memory_Allocate(size);
                if(pBlocks[ index ] == NULL)
                        Kern_Panic("the heap found no page for %lu bytes", (unsigned long) size);
                if(((uintptr_t) pBlocks[ index ] & 15U) != 0)
                        Kern_Panic("the heap gave %p, which is not aligned to 16 bytes",
                                   pBlocks[ index ]);
                for(unsigned earlier = 0; earlier < index; earlier++)
                        if(pBlocks[ earlier ] == pBlocks[ index ])
                                Kern_Panic("the heap gave %p twice", pBlocks[ index ]);
                ((uint8_t*) pBlocks[ index ])[ size - 1 ] = 0xA5;
        }
        for(unsigned index = 0; index < KERN_MEMORY_CHECKED; index++)
                Kern_Memory_Free(pBlocks[ KERN_MEMORY_CHECKED - 1 - index ]);
        if(Kern_Memory_InUse() != 0 || Kern_PageFrames_FreeCount() != frames)
                Kern_Panic("the heap kept %lu blocks and %ld page frames",
                           (unsigned long) Kern_Memory_InUse(),
                           (long) (frames - Kern_PageFrames_FreeCount()));
        Kern_Print("memory: heap checked\n");
}
