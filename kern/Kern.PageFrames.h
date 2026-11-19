/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** The size of a page frame. */
#define KERN_PAGE_FRAMES_SIZE 4096U

/**
 * The physical memory the allocator covers, from KERN_PAGE_FRAMES_BASE: the first GiB of RAM, which
 * the boot code maps one to one. RAM above it waits for the kernel's own page tables.
 */
#if defined(__aarch64__)
/** RAM starts at 1 GiB on QEMU's virt machine. */
#    define KERN_PAGE_FRAMES_BASE 0x40000000ULL
#else
#    define KERN_PAGE_FRAMES_BASE 0x0ULL
#endif
#define KERN_PAGE_FRAMES_LIMIT 0x40000000ULL

/**
 * @brief Starts the allocator with every frame in use; Kern_PageFrames_AddFree() then gives it
 * the free RAM, and Kern_PageFrames_Reserve() takes back what the kernel and the loader hold.
 */
void
Kern_PageFrames_Init(void);

/**
 * @brief Marks the whole frames inside [aStart, aStart + aLength) free, up to
 * KERN_PAGE_FRAMES_LIMIT.
 *
 * @param[in] aStart  The physical address.
 * @param[in] aLength The length in bytes.
 */
void
Kern_PageFrames_AddFree(uint64_t aStart, uint64_t aLength);

/**
 * @brief Marks every frame that [aStart, aStart + aLength) touches in use.
 *
 * @param[in] aStart  The physical address.
 * @param[in] aLength The length in bytes.
 */
void
Kern_PageFrames_Reserve(uint64_t aStart, uint64_t aLength);

/**
 * @brief Takes a free frame.
 *
 * @param[out] aOut_pAddress Its physical address.
 * @return false when no frame is free.
 */
bool
Kern_PageFrames_Allocate(uint64_t* aOut_pAddress);

/**
 * @brief Gives a frame back. Freeing a frame that is not in use, or an address that is not a
 * frame of the allocator, is a kernel bug and panics: a double free would hand one frame to two
 * owners.
 *
 * @param[in] aAddress The physical address of the frame.
 */
void
Kern_PageFrames_Free(uint64_t aAddress);

/**
 * @brief How many frames are free.
 *
 * @return The count.
 */
uint64_t
Kern_PageFrames_FreeCount(void);
