/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>

/**
 * @brief Sets up the first serial port, COM1, for 115200 baud, 8 data bits, no parity, 1 stop bit.
 */
void
Amd64_Serial_Init(void);

/**
 * @brief Writes a string to COM1, a carriage return before each line feed.
 *
 * @param[in] aIn_pText The string, ending in a null character.
 *
 * @pre Amd64_Serial_Init() has run.
 */
void
Amd64_Serial_Write(const char* aIn_pText);

/**
 * @brief Writes bytes to COM1 as they are, null characters too, a carriage return before each line
 * feed.
 *
 * @param[in] aIn_pBytes The bytes.
 * @param[in] aCount     How many.
 *
 * @pre Amd64_Serial_Init() has run.
 */
void
Amd64_Serial_WriteBytes(const char* aIn_pBytes, size_t aCount);
