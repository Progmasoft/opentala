/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

/*
 * The page tables of arm64: 4 KiB pages, 48-bit addresses, all through TTBR0_EL1 (TTBR1_EL1's walks
 * are off). The first GiB holds QEMU virt's devices, mapped as Device-nGnRE, never executable;
 * the second GiB is its RAM, the kernel's, mapped one to one as normal write-back memory that EL0
 * cannot reach. User pages go from KERN_MACHINE_USER_START to KERN_MACHINE_USER_END in 4 KiB pages.
 */

/**
 * @brief Builds the kernel's map of the first two GiB and turns the MMU and the caches on. Called
 * by Arm64_Main() before anything else reads memory the device tree describes.
 */
void
Arm64_Paging_EnableMmu(void);
