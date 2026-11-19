/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/*
 * Device registers on arm64. The kernel maps the first GiB, where QEMU's virt machine puts its
 * devices, as Device-nGnRE memory, so each access reaches the device once and in order.
 */

/**
 * @brief Reads a 32-bit device register.
 *
 * @param[in] aAddress The register's physical address.
 * @return Its value.
 */
static inline uint32_t
Arm64_Mmio_Read32(uint64_t aAddress)
{
        return (*(volatile const uint32_t*) (uintptr_t) aAddress);
}

/**
 * @brief Writes a 32-bit device register.
 *
 * @param[in] aAddress The register's physical address.
 * @param[in] aValue   The value.
 */
static inline void
Arm64_Mmio_Write32(uint64_t aAddress, uint32_t aValue)
{
        *(volatile uint32_t*) (uintptr_t) aAddress = aValue;
}
