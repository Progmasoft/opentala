/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/**
 * @brief The first C function of the kernel, called in 64-bit mode by the boot code.
 *
 * It reports on the serial port that the kernel runs and how it was loaded, and halts.
 *
 * @param[in] aBootMagic       The value the loader left in EAX; 0x2BADB002 for Multiboot.
 * @param[in] aBootInformation The physical address of the loader's information structure.
 */
__attribute__((noreturn)) void
Kern_Main(uint32_t aBootMagic, uint32_t aBootInformation);
