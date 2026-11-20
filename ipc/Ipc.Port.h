/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * Ports. A port is a queue of messages with one receive right. It is kept while something refers to
 * it: each name in a space, each right in a queued message, and its receive right. When the receive
 * right is destroyed, the port dies: its queue is destroyed, and a send right to it becomes a dead
 * name when its task looks it up. A kernel port stands for the kernel's object of a task, a thread
 * or the host; no task holds its receive right.
 */

/** What a kernel port stands for; an ordinary port stands for nothing. */
typedef enum Ipc_Port_Kernel
{
        Ipc_Port_kNone,
        Ipc_Port_kTask,
        Ipc_Port_kThread,
        Ipc_Port_kHost,
} Ipc_Port_tKernel;

typedef struct Ipc_Port_Port
{
        /** Names, rights in messages and the receive right that refer to the port. */
        uint32_t references;
        /** Send rights and send-once rights, wherever they are. */
        uint32_t sendRights;
        uint32_t sendOnceRights;
        /** The queued messages, the first to be received first, and how many. */
        struct Ipc_Message_Message* pFirst;
        struct Ipc_Message_Message* pLast;
        uint32_t                    queued;
        /** The space with the receive right; NULL for a kernel port and for a dead one. */
        struct Ipc_Space_Space* pReceiver;
        Ipc_Port_tKernel        kernel;
        bool                    isDead;
} Ipc_Port_tPort;

/** A right on its way: a port, and the type it has once received, IPC_MACH_PORT_*. */
typedef struct Ipc_Port_Right
{
        Ipc_Port_tPort* pPort;
        uint32_t        type;
} Ipc_Port_tRight;

/**
 * @brief Creates a port that nothing refers to yet.
 *
 * @param[in] aKernel What it stands for, or Ipc_Port_kNone.
 * @return The port, or NULL when the heap is full.
 */
Ipc_Port_tPort*
Ipc_Port_Create(Ipc_Port_tKernel aKernel);

/**
 * @brief The host's port, the one port that lives as long as the kernel.
 *
 * @return The port.
 */
Ipc_Port_tPort*
Ipc_Port_Host(void);

/**
 * @brief Drops one reference of a port, and frees it once it is dead and nothing refers to it.
 *
 * @param[in,out] aInOut_pPort The port.
 */
void
Ipc_Port_Release(Ipc_Port_tPort* aInOut_pPort);

/**
 * @brief Destroys a port's receive right: the port dies, its queued messages are destroyed, and the
 * receive right's reference is dropped.
 *
 * @param[in,out] aInOut_pPort The port.
 */
void
Ipc_Port_Kill(Ipc_Port_tPort* aInOut_pPort);

/**
 * @brief Gives back a send or send-once right that goes nowhere, and its reference.
 *
 * @param[in] aIn_Right The right; one with no port does nothing.
 */
void
Ipc_Port_ReleaseRight(Ipc_Port_tRight aIn_Right);

/**
 * @brief How many ports the heap holds, for the checks that a destroyed space keeps nothing.
 *
 * @return The count.
 */
uint64_t
Ipc_Port_Alive(void);
