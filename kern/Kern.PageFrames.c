/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>

#include "kern/Kern.PageFrames.h"
#include "kern/Kern.Print.h"

#define KERN_PAGE_FRAMES_COUNT (KERN_PAGE_FRAMES_LIMIT / KERN_PAGE_FRAMES_SIZE)
#define KERN_PAGE_FRAMES_WORDS (KERN_PAGE_FRAMES_COUNT / 64)

/** One bit per frame, set while the frame is in use. */
static uint64_t gsInUse[ KERN_PAGE_FRAMES_WORDS ];
static uint64_t gsFreeCount;
/** The word the next search starts at: allocation goes on from where it stopped. */
static uint64_t gsNextWord;

/**
 * @brief Whether frame aFrame is in use.
 *
 * @param[in] aFrame The frame number.
 * @return true when it is.
 */
static bool
IsInUse(uint64_t aFrame)
{
        return ((gsInUse[ aFrame / 64 ] >> (aFrame % 64)) & 1) != 0;
}

/**
 * @brief Marks frame aFrame in use or free, keeping the count of free frames.
 *
 * @param[in] aFrame  The frame number.
 * @param[in] isInUse Whether it becomes in use.
 */
static void
SetInUse(uint64_t aFrame, bool isInUse)
{
        const uint64_t bit = 1ULL << (aFrame % 64);

        if(isInUse == IsInUse(aFrame))
                return;
        if(isInUse)
        {
                gsInUse[ aFrame / 64 ] |= bit;
                gsFreeCount--;
        } else
        {
                gsInUse[ aFrame / 64 ] &= ~bit;
                gsFreeCount++;
        }
}

void
Kern_PageFrames_Init(void)
{
        for(uint64_t word = 0; word < KERN_PAGE_FRAMES_WORDS; word++)
                gsInUse[ word ] = ~0ULL;
        gsFreeCount = 0;
        gsNextWord  = 0;
}

void
Kern_PageFrames_AddFree(uint64_t aStart, uint64_t aLength)
{
        uint64_t first = (aStart + KERN_PAGE_FRAMES_SIZE - 1) / KERN_PAGE_FRAMES_SIZE;
        uint64_t end   = aLength > UINT64_MAX - aStart ? KERN_PAGE_FRAMES_COUNT
                                                       : (aStart + aLength) / KERN_PAGE_FRAMES_SIZE;

        if(end > KERN_PAGE_FRAMES_COUNT)
                end = KERN_PAGE_FRAMES_COUNT;
        for(uint64_t frame = first; frame < end; frame++)
                SetInUse(frame, false);
}

void
Kern_PageFrames_Reserve(uint64_t aStart, uint64_t aLength)
{
        if(aLength == 0)
                return;
        uint64_t first = aStart / KERN_PAGE_FRAMES_SIZE;
        uint64_t end   = aLength > UINT64_MAX - aStart
                ? KERN_PAGE_FRAMES_COUNT
                : (aStart + aLength + KERN_PAGE_FRAMES_SIZE - 1) / KERN_PAGE_FRAMES_SIZE;

        if(end > KERN_PAGE_FRAMES_COUNT)
                end = KERN_PAGE_FRAMES_COUNT;
        for(uint64_t frame = first; frame < end; frame++)
                SetInUse(frame, true);
}

bool
Kern_PageFrames_Allocate(uint64_t* aOut_pAddress)
{
        for(uint64_t step = 0; step < KERN_PAGE_FRAMES_WORDS; step++)
        {
                const uint64_t word = (gsNextWord + step) % KERN_PAGE_FRAMES_WORDS;

                if(gsInUse[ word ] == ~0ULL)
                        continue;
                const uint64_t frame = word * 64 + (uint64_t) __builtin_ctzll(~gsInUse[ word ]);

                SetInUse(frame, true);
                gsNextWord     = word;
                *aOut_pAddress = frame * KERN_PAGE_FRAMES_SIZE;
                return (true);
        }
        return (false);
}

void
Kern_PageFrames_Free(uint64_t aAddress)
{
        const uint64_t frame = aAddress / KERN_PAGE_FRAMES_SIZE;

        if(aAddress % KERN_PAGE_FRAMES_SIZE != 0 || frame >= KERN_PAGE_FRAMES_COUNT)
                Kern_Panic("freeing %p, which is not a page frame", (void*) aAddress);
        if(!IsInUse(frame))
                Kern_Panic("freeing the page frame %p twice", (void*) aAddress);
        SetInUse(frame, false);
}

uint64_t
Kern_PageFrames_FreeCount(void)
{
        return (gsFreeCount);
}
