/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdarg.h>

/**
 * @brief Writes formatted text to the kernel console, the first serial port.
 *
 * The conversions are a small part of printf's: `%s`, `%c`, `%d`, `%u`, `%x` and `%p`, with the
 * length modifiers `l` and `ll`, a field width, and the `0` flag. `%%` writes a percent sign.
 * Anything else is written as it is, so a mistake shows instead of reading past the arguments.
 *
 * @param[in] aIn_pFormat The format.
 */
__attribute__((format(printf, 1, 2))) void
Kern_Print(const char* aIn_pFormat, ...);

/**
 * @brief Kern_Print() with its arguments in a va_list.
 *
 * @param[in] aIn_pFormat The format.
 * @param[in] aArguments  The arguments.
 */
void
Kern_PrintArguments(const char* aIn_pFormat, va_list aArguments);

/**
 * @brief Writes "panic: " and the message, and stops this processor for good.
 *
 * @param[in] aIn_pFormat The format of the message, as Kern_Print() takes it.
 */
__attribute__((noreturn, format(printf, 1, 2))) void
Kern_Panic(const char* aIn_pFormat, ...);

/**
 * @brief Stops this processor for good, with interrupts off.
 */
__attribute__((noreturn)) void
Kern_Halt(void);
