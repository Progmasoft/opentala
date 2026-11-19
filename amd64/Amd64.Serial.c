/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "amd64/Amd64.Port.h"
#include "amd64/Amd64.Serial.h"

/* The registers of a 16550 UART, as offsets from the port base. */
#define AMD64_SERIAL_COM1         0x3F8
#define AMD64_SERIAL_DATA         0
#define AMD64_SERIAL_INTERRUPTS   1
#define AMD64_SERIAL_FIFO         2
#define AMD64_SERIAL_LINE_CONTROL 3
#define AMD64_SERIAL_MODEM        4
#define AMD64_SERIAL_LINE_STATUS  5

#define AMD64_SERIAL_DIVISOR_LATCH 0x80
#define AMD64_SERIAL_8N1           0x03
#define AMD64_SERIAL_FIFO_ON       0xC7
#define AMD64_SERIAL_DTR_RTS_OUT2  0x0B
#define AMD64_SERIAL_CAN_SEND      0x20

/**
 * @brief Writes one byte to COM1 once the transmitter can take it.
 *
 * @param[in] aByte The byte.
 */
static void
WriteByte(uint8_t aByte)
{
        while((Amd64_Port_Read8(AMD64_SERIAL_COM1 + AMD64_SERIAL_LINE_STATUS)
               & AMD64_SERIAL_CAN_SEND)
              == 0)
                __asm__ volatile("pause");
        Amd64_Port_Write8(AMD64_SERIAL_COM1 + AMD64_SERIAL_DATA, aByte);
}

/**
 * @brief Sets up the first serial port, COM1, for 115200 baud, 8 data bits, no parity, 1 stop bit.
 */
void
Amd64_Serial_Init(void)
{
        Amd64_Port_Write8(AMD64_SERIAL_COM1 + AMD64_SERIAL_INTERRUPTS, 0x00);
        Amd64_Port_Write8(AMD64_SERIAL_COM1 + AMD64_SERIAL_LINE_CONTROL,
                          AMD64_SERIAL_DIVISOR_LATCH);
        /* Divisor 1: 115200 baud. */
        Amd64_Port_Write8(AMD64_SERIAL_COM1 + AMD64_SERIAL_DATA, 0x01);
        Amd64_Port_Write8(AMD64_SERIAL_COM1 + AMD64_SERIAL_INTERRUPTS, 0x00);
        Amd64_Port_Write8(AMD64_SERIAL_COM1 + AMD64_SERIAL_LINE_CONTROL, AMD64_SERIAL_8N1);
        Amd64_Port_Write8(AMD64_SERIAL_COM1 + AMD64_SERIAL_FIFO, AMD64_SERIAL_FIFO_ON);
        Amd64_Port_Write8(AMD64_SERIAL_COM1 + AMD64_SERIAL_MODEM, AMD64_SERIAL_DTR_RTS_OUT2);
}

/**
 * @brief Writes a string to COM1, a carriage return before each line feed.
 *
 * @param[in] aIn_pText The string, ending in a null character.
 *
 * @pre Amd64_Serial_Init() has run.
 */
void
Amd64_Serial_Write(const char* aIn_pText)
{
        for(const char* pCharacter = aIn_pText; *pCharacter != '\0'; pCharacter++)
        {
                if(*pCharacter == '\n')
                        WriteByte('\r');
                WriteByte((uint8_t) *pCharacter);
        }
}

void
Amd64_Serial_WriteBytes(const char* aIn_pBytes, size_t aCount)
{
        for(size_t index = 0; index < aCount; index++)
        {
                if(aIn_pBytes[ index ] == '\n')
                        WriteByte('\r');
                WriteByte((uint8_t) aIn_pBytes[ index ]);
        }
}
