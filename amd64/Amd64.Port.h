/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/**
 * @brief Writes one byte to an I/O port.
 *
 * @param[in] aPort  The port.
 * @param[in] aValue The byte.
 */
static inline void
Amd64_Port_Write8(uint16_t aPort, uint8_t aValue)
{
        __asm__ volatile("outb %0, %1" : : "a"(aValue), "Nd"(aPort));
}

/**
 * @brief Reads one byte from an I/O port.
 *
 * @param[in] aPort The port.
 * @return The byte.
 */
static inline uint8_t
Amd64_Port_Read8(uint16_t aPort)
{
        uint8_t value = 0;

        __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(aPort));
        return (value);
}
