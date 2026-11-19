/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

/**
 * @brief Sets up the PL011 UART of QEMU's virt machine at 0x09000000 for 8 data bits, no parity,
 * 1 stop bit, with its FIFOs; the console of Kern_Machine_ConsoleWrite().
 */
void
Arm64_Serial_Init(void);
