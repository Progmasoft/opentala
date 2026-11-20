/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * The kernel's heap: blocks of 32 bytes to a little less than a page, in size classes of powers of
 * two, carved out of page frames. A page holds blocks of one class; when its last block is freed,
 * the frame goes back to the page frame allocator, so the heap holds nothing it does not use.
 */

/** The largest block: a page less the header that names its class. */
#define KERN_MEMORY_LARGEST 4064U

/**
 * @brief Allocates a block of at least aSize bytes, filled with zeros.
 *
 * @param[in] aSize The size, 1 to KERN_MEMORY_LARGEST.
 * @return The block, aligned to 16 bytes, or NULL when no page frame is free.
 */
void*
Kern_Memory_Allocate(size_t aSize);

/**
 * @brief Gives a block back. Freeing a block that is not in use panics, since it would give one
 * block to two owners.
 *
 * @param[in] aIn_pBlock The block, or NULL, which does nothing.
 */
void
Kern_Memory_Free(void* aIn_pBlock);

/**
 * @brief How many blocks are in use, for the checks that nothing is kept.
 *
 * @return The count.
 */
uint64_t
Kern_Memory_InUse(void);

/**
 * @brief Allocates and frees blocks of every class, and checks that every page comes back.
 */
void
Kern_Memory_Check(void);
