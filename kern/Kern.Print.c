/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "amd64/Amd64.Serial.h"
#include "kern/Kern.Print.h"

/** The digits of the widest number, 64 bits in octal, and room for the sign. */
#define KERN_PRINT_DIGITS 24

/** The length modifier of one conversion. */
typedef enum
{
        kLengthInt,
        kLengthLong,
        kLengthLongLong,
} Length;

/** What one conversion says besides its letter. */
typedef struct
{
        bool   isZeroPadded;
        int    width;
        Length length;
} Spec;

/**
 * @brief Writes [aIn_pText, aIn_pText + aCount) to the console.
 *
 * @param[in] aIn_pText The bytes.
 * @param[in] aCount    How many there are.
 */
static void
WriteBytes(const char* aIn_pText, int aCount)
{
        char piece[ 2 ] = { 0, 0 };

        for(int index = 0; index < aCount; index++)
        {
                piece[ 0 ] = aIn_pText[ index ];
                Amd64_Serial_Write(piece);
        }
}

/**
 * @brief Writes [aIn_pText, aIn_pText + aCount) right-aligned in the width of aIn_pSpec.
 *
 * @param[in] aIn_pSpec  The width and the padding.
 * @param[in] aIn_pText  The text.
 * @param[in] aCount     Its length.
 * @param[in] isNegative Whether a minus sign goes before it, after zero padding is counted.
 */
static void
WritePadded(const Spec* aIn_pSpec, const char* aIn_pText, int aCount, bool isNegative)
{
        int padding = aIn_pSpec->width - aCount - (isNegative ? 1 : 0);

        if(isNegative && aIn_pSpec->isZeroPadded)
                WriteBytes("-", 1);
        for(; padding > 0; padding--)
                WriteBytes(aIn_pSpec->isZeroPadded ? "0" : " ", 1);
        if(isNegative && !aIn_pSpec->isZeroPadded)
                WriteBytes("-", 1);
        WriteBytes(aIn_pText, aCount);
}

/**
 * @brief Writes aValue in base aBase, as the width and padding of aIn_pSpec say.
 *
 * @param[in] aIn_pSpec  The width and the padding.
 * @param[in] aValue     The magnitude.
 * @param[in] aBase      10 or 16.
 * @param[in] isNegative Whether the number is negative.
 */
static void
WriteNumber(const Spec* aIn_pSpec, unsigned long long aValue, unsigned aBase, bool isNegative)
{
        char digits[ KERN_PRINT_DIGITS ];
        int  start = KERN_PRINT_DIGITS;

        do
        {
                digits[ --start ] = "0123456789abcdef"[ aValue % aBase ];
                aValue           /= aBase;
        } while(aValue != 0);
        WritePadded(aIn_pSpec, digits + start, KERN_PRINT_DIGITS - start, isNegative);
}

/**
 * @brief Takes the next signed argument as long as the length modifier says.
 *
 * @param[in]     aLength     The length modifier.
 * @param[in,out] aIn_pValues The arguments.
 * @return The argument, widened.
 */
static long long
NextSigned(Length aLength, va_list* aIn_pValues)
{
        switch(aLength)
        {
        case kLengthLongLong:
                return (va_arg(*aIn_pValues, long long));
        case kLengthLong:
                return (va_arg(*aIn_pValues, long));
        default:
                return (va_arg(*aIn_pValues, int));
        }
}

/**
 * @brief Takes the next unsigned argument as long as the length modifier says.
 *
 * @param[in]     aLength     The length modifier.
 * @param[in,out] aIn_pValues The arguments.
 * @return The argument, widened.
 */
static unsigned long long
NextUnsigned(Length aLength, va_list* aIn_pValues)
{
        switch(aLength)
        {
        case kLengthLongLong:
                return (va_arg(*aIn_pValues, unsigned long long));
        case kLengthLong:
                return (va_arg(*aIn_pValues, unsigned long));
        default:
                return (va_arg(*aIn_pValues, unsigned int));
        }
}

/**
 * @brief Reads the flags, width and length modifier after a percent sign.
 *
 * @param[in]  aIn_pFormat The text after the percent sign.
 * @param[out] aOut_pSpec  What it says.
 * @return Where the conversion letter is.
 */
static const char*
ReadSpec(const char* aIn_pFormat, Spec* aOut_pSpec)
{
        const char* pCursor = aIn_pFormat;

        aOut_pSpec->isZeroPadded = false;
        aOut_pSpec->width        = 0;
        aOut_pSpec->length       = kLengthInt;
        if(*pCursor == '0')
        {
                aOut_pSpec->isZeroPadded = true;
                pCursor++;
        }
        while(*pCursor >= '0' && *pCursor <= '9' && aOut_pSpec->width < 64)
                aOut_pSpec->width = aOut_pSpec->width * 10 + (*pCursor++ - '0');
        if(*pCursor == 'l')
        {
                aOut_pSpec->length = kLengthLong;
                if(*++pCursor == 'l')
                {
                        aOut_pSpec->length = kLengthLongLong;
                        pCursor++;
                }
        }
        return (pCursor);
}

/**
 * @brief Writes the next argument as a signed decimal number.
 *
 * @param[in]     aIn_pSpec   The flags, width and length.
 * @param[in,out] aIn_pValues The arguments.
 */
static void
WriteSigned(const Spec* aIn_pSpec, va_list* aIn_pValues)
{
        const long long          value     = NextSigned(aIn_pSpec->length, aIn_pValues);
        const unsigned long long magnitude = value < 0 ? 0ULL - (unsigned long long) value
                                                       : (unsigned long long) value;

        WriteNumber(aIn_pSpec, magnitude, 10, value < 0);
}

/**
 * @brief Writes the next argument as a string, "(null)" for a null pointer.
 *
 * @param[in]     aIn_pSpec   The width.
 * @param[in,out] aIn_pValues The arguments.
 */
static void
WriteString(const Spec* aIn_pSpec, va_list* aIn_pValues)
{
        const char* pText  = va_arg(*aIn_pValues, const char*);
        int         length = 0;

        if(pText == 0)
                pText = "(null)";
        while(pText[ length ] != '\0')
                length++;
        WritePadded(aIn_pSpec, pText, length, false);
}

/**
 * @brief Writes the next argument as a pointer: 0x and sixteen hexadecimal digits.
 *
 * @param[in,out] aIn_pValues The arguments.
 */
static void
WritePointer(va_list* aIn_pValues)
{
        const Spec pointer = { true, 16, kLengthLong };

        WriteBytes("0x", 2);
        WriteNumber(&pointer, (uintptr_t) va_arg(*aIn_pValues, void*), 16, false);
}

/**
 * @brief Writes one conversion.
 *
 * @param[in]     aIn_pSpec   Its flags, width and length.
 * @param[in]     aConversion Its letter.
 * @param[in,out] aIn_pValues The arguments.
 * @return false when aConversion is not one Kern_Print() knows.
 */
static bool
WriteConversion(const Spec* aIn_pSpec, char aConversion, va_list* aIn_pValues)
{
        switch(aConversion)
        {
        case 'd':
                WriteSigned(aIn_pSpec, aIn_pValues);
                return (true);
        case 'u':
        case 'x':
                WriteNumber(aIn_pSpec,
                            NextUnsigned(aIn_pSpec->length, aIn_pValues),
                            aConversion == 'x' ? 16 : 10,
                            false);
                return (true);
        case 'p':
                WritePointer(aIn_pValues);
                return (true);
        case 's':
                WriteString(aIn_pSpec, aIn_pValues);
                return (true);
        case 'c':
        {
                const char character = (char) va_arg(*aIn_pValues, int);

                WritePadded(aIn_pSpec, &character, 1, false);
                return (true);
        }
        case '%':
                WriteBytes("%", 1);
                return (true);
        default:
                return (false);
        }
}

void
Kern_PrintArguments(const char* aIn_pFormat, va_list aArguments)
{
        va_list values;

        va_copy(values, aArguments);
        for(const char* pCursor = aIn_pFormat; *pCursor != '\0'; pCursor++)
        {
                const char* pStart = pCursor;
                Spec        spec;

                if(*pCursor != '%')
                {
                        WriteBytes(pCursor, 1);
                        continue;
                }
                pCursor = ReadSpec(pCursor + 1, &spec);
                if(*pCursor == '\0' || !WriteConversion(&spec, *pCursor, &values))
                {
                        WriteBytes(pStart, (int) (pCursor - pStart) + (*pCursor != '\0'));
                        if(*pCursor == '\0')
                                break;
                }
        }
        va_end(values);
}

void
Kern_Print(const char* aIn_pFormat, ...)
{
        va_list arguments;

        va_start(arguments, aIn_pFormat);
        Kern_PrintArguments(aIn_pFormat, arguments);
        va_end(arguments);
}

void
Kern_Halt(void)
{
        for(;;)
                __asm__ volatile("cli; hlt");
}

void
Kern_Panic(const char* aIn_pFormat, ...)
{
        va_list arguments;

        __asm__ volatile("cli");
        Kern_Print("panic: ");
        va_start(arguments, aIn_pFormat);
        Kern_PrintArguments(aIn_pFormat, arguments);
        va_end(arguments);
        Kern_Print("\n");
        Kern_Halt();
}
