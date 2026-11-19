/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * What the machine-independent kernel asks of each architecture. amd64/ and arm64/ each implement
 * every function here, most in Amd64.Machine.c and Arm64.Machine.c; nothing in kern/ includes a
 * header of an architecture.
 */

#if defined(__x86_64__)
/** An address the kernel does not map, for the page fault test. */
#    define KERN_MACHINE_UNMAPPED 0x0000000040000000ULL
#elif defined(__aarch64__)
#    define KERN_MACHINE_UNMAPPED 0x0000000080000000ULL
#else
#    error "OpenTala supports amd64 and aarch64"
#endif

/** The lowest user address: 4 GiB, where Darwin puts a program after its __PAGEZERO. */
#define KERN_MACHINE_USER_START 0x0000000100000000ULL
/**
 * The address after the highest user address: the end of the lower half (48-bit addresses on both
 * architectures), less one page, which stays unmapped so that a system call at its very end cannot
 * leave a return address outside the lower half.
 */
#define KERN_MACHINE_USER_END 0x00007FFFFFFFF000ULL

/** The page may be written; without it the page is read-only. */
#define KERN_MACHINE_WRITABLE 0x1U
/** The page may be executed; without it the page is never executed. A page is not both. */
#define KERN_MACHINE_EXECUTABLE 0x2U

/** The timer's rate. */
#define KERN_MACHINE_TIMER_HZ 100U

/** The kernel's callee-saved registers where it went into user mode, for the way back. */
typedef struct Kern_Machine_Context
{
        uint64_t registers[ 14 ];
} Kern_Machine_tContext;

/**
 * @brief Writes bytes to the console as they are, a carriage return before each line feed.
 *
 * @param[in] aIn_pBytes The bytes.
 * @param[in] aCount     How many.
 */
void
Kern_Machine_ConsoleWrite(const char* aIn_pBytes, size_t aCount);

/**
 * @brief Sets up the processor's exception and interrupt handling.
 */
void
Kern_Machine_InitTraps(void);

/**
 * @brief Takes a breakpoint in the kernel, which the handler counts and resumes after.
 *
 * @return How many breakpoints the kernel has taken.
 */
uint64_t
Kern_Machine_Breakpoint(void);

/**
 * @brief Starts the timer at KERN_MACHINE_TIMER_HZ and its interrupt.
 */
void
Kern_Machine_StartTimer(void);

/**
 * @brief How many times the timer has ticked.
 *
 * @return The count.
 */
uint64_t
Kern_Machine_Ticks(void);

/**
 * @brief Waits with interrupts on until the next interrupt, then turns them off again.
 */
void
Kern_Machine_WaitForInterrupt(void);

/**
 * @brief Turns on what keeps the kernel and user mode apart in the page tables, and writes what.
 */
void
Kern_Machine_InitPaging(void);

/**
 * @brief Maps one user page.
 *
 * @param[in] aVirtual  The user address, page-aligned, in [KERN_MACHINE_USER_START,
 *                      KERN_MACHINE_USER_END).
 * @param[in] aPhysical The frame, from the page frame allocator.
 * @param[in] aFlags    KERN_MACHINE_WRITABLE or KERN_MACHINE_EXECUTABLE, or neither.
 * @return false when the address is not a user page, the page is mapped already, the flags ask for
 *         a page both writable and executable, or no frame is left for a table.
 */
bool
Kern_Machine_MapUser(uint64_t aVirtual, uint64_t aPhysical, unsigned aFlags);

/**
 * @brief Copies from user memory, after checking that every byte is readable from user mode.
 *
 * @param[out] aOut_pDestination The kernel buffer.
 * @param[in]  aSource           The user address.
 * @param[in]  aLength           How many bytes.
 * @return false, with nothing copied, when part of the range is not readable from user mode.
 */
bool
Kern_Machine_CopyIn(void* aOut_pDestination, uint64_t aSource, size_t aLength);

/**
 * @brief Unmaps every user page and gives its frame and the tables' frames back.
 */
void
Kern_Machine_ClearUser(void);

/**
 * @brief Sets up the way into the kernel from user mode, system calls among them, and writes how.
 */
void
Kern_Machine_InitUser(void);

/**
 * @brief Goes into user mode at aEntry with the stack at aStack and aArgument as the first
 * argument; every other register is zero. Returns when Kern_Machine_Leave() is called.
 *
 * @param[out] aOut_pContext Where the kernel's registers are kept.
 * @param[in]  aEntry        The user address to start at.
 * @param[in]  aStack        The user stack pointer.
 * @param[in]  aArgument     The first argument.
 * @return The value given to Kern_Machine_Leave().
 */
int64_t
Kern_Machine_Enter(Kern_Machine_tContext* aOut_pContext,
                   uint64_t               aEntry,
                   uint64_t               aStack,
                   uint64_t               aArgument);

/**
 * @brief Goes back to where Kern_Machine_Enter() was called, which returns aValue.
 *
 * @param[in] aIn_pContext The context Kern_Machine_Enter() filled.
 * @param[in] aValue       What it returns.
 */
__attribute__((noreturn)) void
Kern_Machine_Leave(const Kern_Machine_tContext* aIn_pContext, int64_t aValue);

/**
 * @brief Runs a test of the architecture's own that the command line names, as amd64's double
 * fault; returns when the command line names none.
 *
 * @param[in] aIn_pCommandLine The command line, or NULL.
 */
void
Kern_Machine_RunTests(const char* aIn_pCommandLine);

/**
 * @brief Stops this processor for good, with interrupts off.
 */
__attribute__((noreturn)) void
Kern_Machine_Halt(void);
