/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * What the loader told the kernel, in one shape for every architecture: each architecture's entry
 * reads its loader's structures (Multiboot on amd64, the device tree on arm64) into it and calls
 * Kern_Main().
 */

/** How many ranges of each kind the kernel keeps; more are dropped, with a line on the console. */
#define KERN_BOOT_RANGES 32

/** A range of physical memory. */
typedef struct Kern_Boot_Range
{
        uint64_t start;
        uint64_t length;
} Kern_Boot_tRange;

/** What the loader gave. */
typedef struct Kern_Boot_Information
{
        /** The loader, for the console: "Multiboot", "a device tree". */
        const char* pLoader;
        /** The command line, or NULL. */
        const char* pCommandLine;
        /** The RAM the kernel may use. */
        Kern_Boot_tRange memory[ KERN_BOOT_RANGES ];
        unsigned         memoryCount;
        /** What inside it the kernel must not hand out: the loader's structures, the firmware's. */
        Kern_Boot_tRange reserved[ KERN_BOOT_RANGES ];
        unsigned         reservedCount;
} Kern_Boot_tInformation;

/**
 * @brief Adds a range to a list of KERN_BOOT_RANGES, or writes that it was dropped.
 *
 * @param[in,out] aInOut_pRanges The list.
 * @param[in,out] aInOut_pCount  How many it holds.
 * @param[in]     aStart         The range's start.
 * @param[in]     aLength        Its length.
 */
void
Kern_Boot_AddRange(Kern_Boot_tRange* aInOut_pRanges,
                   unsigned*         aInOut_pCount,
                   uint64_t          aStart,
                   uint64_t          aLength);

/**
 * @brief Whether the command line has aIn_pWord as one of its space-separated words.
 *
 * @param[in] aIn_pLine The command line, or NULL.
 * @param[in] aIn_pWord The word.
 * @return true when it does.
 */
bool
Kern_Main_HasWord(const char* aIn_pLine, const char* aIn_pWord);

/**
 * @brief The machine-independent kernel, called by each architecture's entry.
 *
 * It sets up the processor's traps, checks that a breakpoint comes back, gives the page frame
 * allocator the RAM, checks the allocator and the timer, runs a process in user mode, runs the
 * tests the command line names, and halts.
 *
 * @param[in] aIn_pInformation What the loader gave.
 */
__attribute__((noreturn)) void
Kern_Main(const Kern_Boot_tInformation* aIn_pInformation);
