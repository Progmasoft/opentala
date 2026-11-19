/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The user half of the address space, on top of the boot code's page tables.
 *
 * The boot code maps the first GiB one to one, for the kernel only. User pages go elsewhere, from
 * AMD64_PAGING_USER_START (4 GiB, where Darwin puts a program after its __PAGEZERO) to
 * AMD64_PAGING_USER_END (the end of the lower half, less a page), in 4 KiB pages whose tables come
 * from the page frame allocator. A user page is never writable and executable at once, and the
 * kernel's GiB is never mapped for user mode.
 */

/** The lowest user address. */
#define AMD64_PAGING_USER_START 0x0000000100000000ULL
/**
 * The address after the highest user address: the end of the canonical lower half, less one page.
 * That page stays unmapped, so a `syscall` at its very end cannot leave a non-canonical return
 * address in RCX.
 */
#define AMD64_PAGING_USER_END 0x00007FFFFFFFF000ULL

/** The page may be written; without it the page is read-only. */
#define AMD64_PAGING_WRITABLE 0x1U
/** The page may be executed; without it the page is no-execute. A page is not both. */
#define AMD64_PAGING_EXECUTABLE 0x2U

/**
 * @brief Turns on what protects the kernel from user mode: no-execute pages, and SMEP and SMAP
 * where the processor has them (the kernel cannot run user code, nor read or write user memory
 * outside Amd64_Paging_CopyIn()). Writes what it turned on.
 */
void
Amd64_Paging_Init(void);

/**
 * @brief Maps one user page.
 *
 * @param[in] aVirtual  The user address, page-aligned, in [AMD64_PAGING_USER_START,
 *                      AMD64_PAGING_USER_END).
 * @param[in] aPhysical The frame, from the page frame allocator.
 * @param[in] aFlags    AMD64_PAGING_WRITABLE or AMD64_PAGING_EXECUTABLE, or neither.
 * @return false when the address is not a user page, the page is mapped already, the flags ask for
 *         a page both writable and executable, or no frame is left for a table.
 */
bool
Amd64_Paging_MapUser(uint64_t aVirtual, uint64_t aPhysical, unsigned aFlags);

/**
 * @brief Copies from user memory, after checking that every byte is in a present user page.
 *
 * @param[out] aOut_pDestination The kernel buffer.
 * @param[in]  aSource           The user address.
 * @param[in]  aLength           How many bytes.
 * @return false, with nothing copied, when part of the range is not readable from user mode.
 */
bool
Amd64_Paging_CopyIn(void* aOut_pDestination, uint64_t aSource, size_t aLength);

/**
 * @brief Unmaps every user page, gives their frames and the tables' frames back, and flushes the
 * TLB.
 */
void
Amd64_Paging_ClearUser(void);
