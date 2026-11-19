/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * Processes in user mode. There is one at a time yet: Kern_Process_Run() maps a program and a
 * stack, runs it until it exits or is killed, and takes its pages back.
 */

/** Darwin's signal numbers for what kills a process. */
#define KERN_SIGNAL_ILL  4
#define KERN_SIGNAL_TRAP 5
#define KERN_SIGNAL_FPE  8
#define KERN_SIGNAL_BUS  10
#define KERN_SIGNAL_SEGV 11
#define KERN_SIGNAL_SYS  12

/** Where a program is put: Darwin's first address after its __PAGEZERO. */
#define KERN_PROCESS_TEXT 0x0000000100000000ULL
/** The address after the top of the stack, where Darwin puts it on amd64. */
#define KERN_PROCESS_STACK_TOP 0x00007FF7C0000000ULL
/** The pages of the stack. */
#define KERN_PROCESS_STACK_PAGES 4U

/** How a process ended. */
typedef struct Kern_Process_Outcome
{
        /** The process's id. */
        uint32_t id;
        /** 0 when it exited, or the signal that killed it. */
        int signal;
        /** The status it exited with. */
        int status;
} Kern_Process_tOutcome;

/**
 * @brief Runs a program in user mode until it ends, then unmaps it.
 *
 * The program's bytes go into fresh pages at KERN_PROCESS_TEXT, read-only and executable; the stack
 * is KERN_PROCESS_STACK_PAGES writable, non-executable pages below KERN_PROCESS_STACK_TOP. The
 * program starts at its first byte with aArgument in its first argument register.
 *
 * @param[in] aIn_pProgram The program's bytes.
 * @param[in] aSize        How many, at most one page.
 * @param[in] aArgument    The argument.
 * @return How it ended.
 */
Kern_Process_tOutcome
Kern_Process_Run(const void* aIn_pProgram, size_t aSize, uint64_t aArgument);

/**
 * @brief The id of the process running, or 0 when there is none.
 *
 * @return The id.
 */
uint32_t
Kern_Process_Id(void);

/**
 * @brief Ends the process running with a status, back into Kern_Process_Run().
 *
 * @param[in] aStatus The status, 0 to 255.
 */
__attribute__((noreturn)) void
Kern_Process_Exit(int aStatus);

/**
 * @brief Ends the process running with a signal, back into Kern_Process_Run(). The caller has
 * written why.
 *
 * @param[in] aSignal The signal.
 */
__attribute__((noreturn)) void
Kern_Process_Kill(int aSignal);

/**
 * @brief The name of a signal, as "SIGSEGV".
 *
 * @param[in] aSignal The signal.
 * @return The name, or "a signal" for one without a name here.
 */
const char*
Kern_Process_SignalName(int aSignal);
