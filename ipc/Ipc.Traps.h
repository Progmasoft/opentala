/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "kern/Kern.SystemCalls.h"

/*
 * The Mach traps of IPC, with XNU's numbers in its mach_trap_table: the port calls that libSystem
 * makes as traps instead of MIG messages, the traps that name the task's, thread's and host's
 * ports, and mach_msg. Each reads the arguments of the trap and gives its kern_return_t or
 * mach_msg_return_t as the first value. The target task of a port call must be the caller's own;
 * any other is MACH_SEND_INVALID_DEST, as XNU answers.
 */

#define IPC_TRAPS_PORT_ALLOCATE     16U
#define IPC_TRAPS_PORT_DEALLOCATE   18U
#define IPC_TRAPS_PORT_MOD_REFS     19U
#define IPC_TRAPS_PORT_INSERT_RIGHT 21U
#define IPC_TRAPS_REPLY_PORT        26U
#define IPC_TRAPS_THREAD_SELF       27U
#define IPC_TRAPS_TASK_SELF         28U
#define IPC_TRAPS_HOST_SELF         29U
#define IPC_TRAPS_MSG               31U

/**
 * @brief _kernelrpc_mach_port_allocate_trap(target, right, name): a receive right, written to name.
 *
 * @param[in] aIn_pArguments The arguments.
 * @return The result.
 */
Kern_SystemCalls_tResult
Ipc_Traps_PortAllocate(const uint64_t* aIn_pArguments);

/**
 * @brief _kernelrpc_mach_port_deallocate_trap(target, name).
 *
 * @param[in] aIn_pArguments The arguments.
 * @return The result.
 */
Kern_SystemCalls_tResult
Ipc_Traps_PortDeallocate(const uint64_t* aIn_pArguments);

/**
 * @brief _kernelrpc_mach_port_mod_refs_trap(target, name, right, delta).
 *
 * @param[in] aIn_pArguments The arguments.
 * @return The result.
 */
Kern_SystemCalls_tResult
Ipc_Traps_PortModRefs(const uint64_t* aIn_pArguments);

/**
 * @brief _kernelrpc_mach_port_insert_right_trap(target, name, poly, polyPoly): the right poly names
 * in the caller's space, by the disposition polyPoly, under name.
 *
 * @param[in] aIn_pArguments The arguments.
 * @return The result.
 */
Kern_SystemCalls_tResult
Ipc_Traps_PortInsertRight(const uint64_t* aIn_pArguments);

/**
 * @brief mach_reply_port(): a new receive right, or MACH_PORT_NULL.
 *
 * @param[in] aIn_pArguments The arguments, none.
 * @return The result.
 */
Kern_SystemCalls_tResult
Ipc_Traps_ReplyPort(const uint64_t* aIn_pArguments);

/**
 * @brief thread_self_trap(): a send right to the thread's port.
 *
 * @param[in] aIn_pArguments The arguments, none.
 * @return The result.
 */
Kern_SystemCalls_tResult
Ipc_Traps_ThreadSelf(const uint64_t* aIn_pArguments);

/**
 * @brief task_self_trap(): a send right to the task's port.
 *
 * @param[in] aIn_pArguments The arguments, none.
 * @return The result.
 */
Kern_SystemCalls_tResult
Ipc_Traps_TaskSelf(const uint64_t* aIn_pArguments);

/**
 * @brief host_self_trap(): a send right to the host's port.
 *
 * @param[in] aIn_pArguments The arguments, none.
 * @return The result.
 */
Kern_SystemCalls_tResult
Ipc_Traps_HostSelf(const uint64_t* aIn_pArguments);

/**
 * @brief mach_msg_trap(msg, option, send_size, rcv_size, rcv_name, timeout): sends, receives, or
 * both, in that order. The timeout is not read: a task has one thread, so nothing ever waits.
 *
 * @param[in] aIn_pArguments The arguments.
 * @return The result.
 */
Kern_SystemCalls_tResult
Ipc_Traps_Msg(const uint64_t* aIn_pArguments);
