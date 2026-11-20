/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ipc/Ipc.Mach.h"
#include "ipc/Ipc.Port.h"

/*
 * Spaces: a task's table of names. Each name names one port and the rights the task holds to it: a
 * receive right, a send right counted by user references, a send-once right, or a dead name. A task
 * has one name for its receive and send rights to a port, and one more name for each send-once
 * right.
 */

/** The rights of an entry, as bits. */
#define IPC_SPACE_RECEIVE   0x1U
#define IPC_SPACE_SEND      0x2U
#define IPC_SPACE_SEND_ONCE 0x4U
#define IPC_SPACE_DEAD      0x8U

/** How many names a space has; index 0 is never used, so a name is never MACH_PORT_NULL. */
#define IPC_SPACE_ENTRIES 128U

typedef struct Ipc_Space_Entry
{
        /** The port; NULL for a dead name and for a free entry. */
        Ipc_Port_tPort* pPort;
        uint32_t        rights;
        /** The user references of a send right or a dead name. */
        uint32_t urefs;
        /** The low 8 bits of the name; changed each time the entry is freed. */
        uint32_t generation;
} Ipc_Space_tEntry;

typedef struct Ipc_Space_Space
{
        Ipc_Space_tEntry entries[ IPC_SPACE_ENTRIES ];
        /** The task's own port and its thread's, which task_self_trap and thread_self_trap name. */
        Ipc_Port_tPort* pTaskPort;
        Ipc_Port_tPort* pThreadPort;
} Ipc_Space_tSpace;

/**
 * @brief Creates a space, with a task port and a thread port of its own.
 *
 * @return The space, which the caller owns, or NULL when the heap is full.
 */
Ipc_Space_tSpace*
Ipc_Space_Create(void);

/**
 * @brief Destroys a space: its receive rights, so that their ports die, every other right it holds,
 * its kernel ports and its memory.
 *
 * @param[in] aIn_pSpace The space.
 */
void
Ipc_Space_Destroy(Ipc_Space_tSpace* aIn_pSpace);

/**
 * @brief Finds the entry a name names. A send or send-once right to a dead port is turned into a
 * dead name first.
 *
 * @param[in,out] aInOut_pSpace The space.
 * @param[in]     aName         The name.
 * @return The entry, or NULL when the name names nothing.
 */
Ipc_Space_tEntry*
Ipc_Space_Find(Ipc_Space_tSpace* aInOut_pSpace, Ipc_Mach_tName aName);

/**
 * @brief Creates a port whose receive right the space holds.
 *
 * @param[in,out] aInOut_pSpace The space.
 * @param[out]    aOut_pName    The name of the receive right.
 * @return IPC_MACH_KERN_SUCCESS, or IPC_MACH_KERN_RESOURCE_SHORTAGE when the space or the heap is
 * full.
 */
int
Ipc_Space_AllocateReceive(Ipc_Space_tSpace* aInOut_pSpace, Ipc_Mach_tName* aOut_pName);

/**
 * @brief Takes a right out of a space as a message carries it, by its disposition: COPY_SEND,
 * MOVE_SEND, MAKE_SEND, MAKE_SEND_ONCE or MOVE_SEND_ONCE. The right holds a reference to its port.
 *
 * @param[in,out] aInOut_pSpace  The space.
 * @param[in]     aName          The name.
 * @param[in]     aDisposition   The disposition.
 * @param[out]    aOut_pRight    The right.
 * @return IPC_MACH_KERN_SUCCESS; IPC_MACH_KERN_INVALID_NAME for a name that names nothing or a dead
 * name; IPC_MACH_KERN_INVALID_RIGHT when the name lacks the right the disposition needs;
 * IPC_MACH_KERN_INVALID_VALUE for any other disposition.
 */
int
Ipc_Space_CopyIn(Ipc_Space_tSpace* aInOut_pSpace,
                 Ipc_Mach_tName    aName,
                 uint32_t          aDisposition,
                 Ipc_Port_tRight*  aOut_pRight);

/**
 * @brief Puts a right into a space: a send right joins the name the space has for its port, a
 * send-once right gets a name of its own. The right's reference passes to the space.
 *
 * @param[in,out] aInOut_pSpace The space.
 * @param[in]     aIn_Right     The right.
 * @param[out]    aOut_pName    The name.
 * @return IPC_MACH_KERN_SUCCESS, or IPC_MACH_KERN_RESOURCE_SHORTAGE when the space is full, in which
 * case the right is given back.
 */
int
Ipc_Space_CopyOut(Ipc_Space_tSpace* aInOut_pSpace,
                  Ipc_Port_tRight   aIn_Right,
                  Ipc_Mach_tName*   aOut_pName);

/**
 * @brief Puts a right into a space under a name the caller picks, as mach_port_insert_right does.
 *
 * @param[in,out] aInOut_pSpace The space.
 * @param[in]     aName         The name.
 * @param[in]     aIn_Right     The right; on failure it is given back.
 * @return IPC_MACH_KERN_SUCCESS; IPC_MACH_KERN_NAME_EXISTS when the name names another port;
 * IPC_MACH_KERN_RIGHT_EXISTS when the space names the port under another name or holds the right
 * already; IPC_MACH_KERN_INVALID_VALUE for a name outside the table.
 */
int
Ipc_Space_Insert(Ipc_Space_tSpace* aInOut_pSpace, Ipc_Mach_tName aName, Ipc_Port_tRight aIn_Right);

/**
 * @brief Changes the user references of one right of a name, as mach_port_mod_refs does; a receive
 * right taken to 0 is destroyed, and its port dies.
 *
 * @param[in,out] aInOut_pSpace The space.
 * @param[in]     aName         The name.
 * @param[in]     aRight        The right, IPC_MACH_RIGHT_*.
 * @param[in]     aDelta        The change.
 * @return IPC_MACH_KERN_SUCCESS, IPC_MACH_KERN_INVALID_NAME, IPC_MACH_KERN_INVALID_RIGHT,
 * IPC_MACH_KERN_INVALID_VALUE or IPC_MACH_KERN_UREFS_OVERFLOW.
 */
int
Ipc_Space_ModRefs(Ipc_Space_tSpace* aInOut_pSpace,
                  Ipc_Mach_tName    aName,
                  uint32_t          aRight,
                  int32_t           aDelta);

/**
 * @brief Gives back one user reference of a send right, a send-once right or a dead name, as
 * mach_port_deallocate does; MACH_PORT_NULL and MACH_PORT_DEAD give back nothing.
 *
 * @param[in,out] aInOut_pSpace The space.
 * @param[in]     aName         The name.
 * @return IPC_MACH_KERN_SUCCESS, IPC_MACH_KERN_INVALID_NAME or IPC_MACH_KERN_INVALID_RIGHT.
 */
int
Ipc_Space_Deallocate(Ipc_Space_tSpace* aInOut_pSpace, Ipc_Mach_tName aName);

/**
 * @brief Names a send right to one of the kernel ports a task reaches, adding a user reference.
 *
 * @param[in,out] aInOut_pSpace The space.
 * @param[in]     aKernel       Ipc_Port_kTask, Ipc_Port_kThread or Ipc_Port_kHost.
 * @return The name, or MACH_PORT_NULL when the space is full.
 */
Ipc_Mach_tName
Ipc_Space_KernelPort(Ipc_Space_tSpace* aInOut_pSpace, Ipc_Port_tKernel aKernel);

/**
 * @brief Whether a name names a send right to the space's own task port: what the target task of a
 * port call must be.
 *
 * @param[in,out] aInOut_pSpace The space.
 * @param[in]     aName         The name.
 * @return Whether it does.
 */
bool
Ipc_Space_IsSelf(Ipc_Space_tSpace* aInOut_pSpace, Ipc_Mach_tName aName);
