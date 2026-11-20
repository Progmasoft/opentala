/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ipc/Ipc.Mach.h"
#include "ipc/Ipc.Port.h"
#include "ipc/Ipc.Space.h"
#include "kern/Kern.Memory.h"
#include "kern/Kern.Print.h"

/**
 * @brief The name of an entry: its index and its generation.
 *
 * @param[in] aIndex      The index.
 * @param[in] aGeneration The generation.
 * @return The name.
 */
static Ipc_Mach_tName
NameOf(uint32_t aIndex, uint32_t aGeneration)
{
        return ((aIndex << 8) | (aGeneration & 0xFFU));
}

/**
 * @brief The index of a name, or 0 when the name is outside the table.
 *
 * @param[in] aName The name.
 * @return The index.
 */
static uint32_t
IndexOf(Ipc_Mach_tName aName)
{
        const uint32_t index = aName >> 8;

        return (index < IPC_SPACE_ENTRIES ? index : 0U);
}

/**
 * @brief Frees an entry, so its old name names nothing.
 *
 * @param[in,out] aInOut_pEntry The entry.
 */
static void
FreeEntry(Ipc_Space_tEntry* aInOut_pEntry)
{
        aInOut_pEntry->pPort      = NULL;
        aInOut_pEntry->rights     = 0U;
        aInOut_pEntry->urefs      = 0U;
        aInOut_pEntry->generation = (aInOut_pEntry->generation % 0xFFU) + 1U;
}

/**
 * @brief A free entry of a space.
 *
 * @param[in,out] aInOut_pSpace The space.
 * @param[out]    aOut_pIndex   Its index.
 * @return The entry, or NULL when the space is full.
 */
static Ipc_Space_tEntry*
FreeSlot(Ipc_Space_tSpace* aInOut_pSpace, uint32_t* aOut_pIndex)
{
        for(uint32_t index = 1U; index < IPC_SPACE_ENTRIES; index++)
        {
                if(aInOut_pSpace->entries[ index ].rights == 0U)
                {
                        *aOut_pIndex = index;
                        return (&aInOut_pSpace->entries[ index ]);
                }
        }
        return (NULL);
}

/**
 * @brief The name of the space's receive or send right to a port.
 *
 * @param[in] aIn_pSpace The space.
 * @param[in] aIn_pPort  The port.
 * @return The index of the entry, or 0 when the space has none.
 */
static uint32_t
IndexOfPort(const Ipc_Space_tSpace* aIn_pSpace, const Ipc_Port_tPort* aIn_pPort)
{
        for(uint32_t index = 1U; index < IPC_SPACE_ENTRIES; index++)
        {
                const Ipc_Space_tEntry* pEntry = &aIn_pSpace->entries[ index ];

                if(pEntry->pPort == aIn_pPort
                   && (pEntry->rights & (IPC_SPACE_RECEIVE | IPC_SPACE_SEND)) != 0U)
                        return (index);
        }
        return (0U);
}

/**
 * @brief Gives up an entry's send or send-once right and, when it holds nothing else, the entry.
 *
 * @param[in,out] aInOut_pEntry The entry.
 * @param[in]     aRight        IPC_SPACE_SEND or IPC_SPACE_SEND_ONCE.
 */
static void
DropRight(Ipc_Space_tEntry* aInOut_pEntry, uint32_t aRight)
{
        Ipc_Port_tPort* pPort = aInOut_pEntry->pPort;

        if(aRight == IPC_SPACE_SEND_ONCE)
                pPort->sendOnceRights--;
        else
                pPort->sendRights--;
        aInOut_pEntry->rights &= ~aRight;
        aInOut_pEntry->urefs   = 0U;
        if(aInOut_pEntry->rights == 0U)
        {
                FreeEntry(aInOut_pEntry);
                Ipc_Port_Release(pPort);
        }
}

/**
 * @brief Turns a send or send-once right to a dead port into a dead name, as the task sees it.
 *
 * @param[in,out] aInOut_pEntry The entry.
 */
static void
MakeDead(Ipc_Space_tEntry* aInOut_pEntry)
{
        Ipc_Port_tPort* pPort = aInOut_pEntry->pPort;
        const uint32_t urefs = (aInOut_pEntry->rights & IPC_SPACE_SEND) != 0U ? aInOut_pEntry->urefs
                                                                              : 1U;

        if((aInOut_pEntry->rights & IPC_SPACE_SEND) != 0U)
                pPort->sendRights--;
        if((aInOut_pEntry->rights & IPC_SPACE_SEND_ONCE) != 0U)
                pPort->sendOnceRights--;
        aInOut_pEntry->pPort  = NULL;
        aInOut_pEntry->rights = IPC_SPACE_DEAD;
        aInOut_pEntry->urefs  = urefs;
        Ipc_Port_Release(pPort);
}

Ipc_Space_tSpace*
Ipc_Space_Create(void)
{
        Ipc_Space_tSpace* pSpace = Kern_Memory_Allocate(sizeof(*pSpace));

        if(pSpace == NULL)
                return (NULL);
        pSpace->pTaskPort   = Ipc_Port_Create(Ipc_Port_kTask);
        pSpace->pThreadPort = Ipc_Port_Create(Ipc_Port_kThread);
        if(pSpace->pTaskPort == NULL || pSpace->pThreadPort == NULL)
        {
                Kern_Memory_Free(pSpace->pTaskPort);
                Kern_Memory_Free(pSpace->pThreadPort);
                Kern_Memory_Free(pSpace);
                return (NULL);
        }
        /* The space refers to its kernel ports until it is destroyed. */
        pSpace->pTaskPort->references   = 1U;
        pSpace->pThreadPort->references = 1U;
        for(uint32_t index = 0U; index < IPC_SPACE_ENTRIES; index++)
                pSpace->entries[ index ].generation = 1U;
        return (pSpace);
}

void
Ipc_Space_Destroy(Ipc_Space_tSpace* aIn_pSpace)
{
        for(uint32_t index = 1U; index < IPC_SPACE_ENTRIES; index++)
        {
                Ipc_Space_tEntry* pEntry = &aIn_pSpace->entries[ index ];
                Ipc_Port_tPort*   pPort  = pEntry->pPort;

                if(pEntry->rights == 0U)
                        continue;
                if((pEntry->rights & IPC_SPACE_RECEIVE) != 0U)
                        Ipc_Port_Kill(pPort);
                if((pEntry->rights & IPC_SPACE_SEND) != 0U)
                        pPort->sendRights--;
                if((pEntry->rights & IPC_SPACE_SEND_ONCE) != 0U)
                        pPort->sendOnceRights--;
                FreeEntry(pEntry);
                if(pPort != NULL)
                        Ipc_Port_Release(pPort);
        }
        /* The task's kernel ports die with it. */
        aIn_pSpace->pTaskPort->isDead   = true;
        aIn_pSpace->pThreadPort->isDead = true;
        Ipc_Port_Release(aIn_pSpace->pTaskPort);
        Ipc_Port_Release(aIn_pSpace->pThreadPort);
        Kern_Memory_Free(aIn_pSpace);
}

Ipc_Space_tEntry*
Ipc_Space_Find(Ipc_Space_tSpace* aInOut_pSpace, Ipc_Mach_tName aName)
{
        const uint32_t    index  = IndexOf(aName);
        Ipc_Space_tEntry* pEntry = &aInOut_pSpace->entries[ index ];

        if(index == 0U || pEntry->rights == 0U || NameOf(index, pEntry->generation) != aName)
                return (NULL);
        if((pEntry->rights & (IPC_SPACE_SEND | IPC_SPACE_SEND_ONCE)) != 0U && pEntry->pPort->isDead)
                MakeDead(pEntry);
        return (pEntry);
}

int
Ipc_Space_AllocateReceive(Ipc_Space_tSpace* aInOut_pSpace, Ipc_Mach_tName* aOut_pName)
{
        uint32_t          index  = 0U;
        Ipc_Space_tEntry* pEntry = FreeSlot(aInOut_pSpace, &index);

        if(pEntry == NULL)
                return (IPC_MACH_KERN_RESOURCE_SHORTAGE);
        Ipc_Port_tPort* pPort = Ipc_Port_Create(Ipc_Port_kNone);

        if(pPort == NULL)
                return (IPC_MACH_KERN_RESOURCE_SHORTAGE);
        /* One reference for the receive right, one for the name. */
        pPort->references = 2U;
        pPort->pReceiver  = aInOut_pSpace;
        pEntry->pPort     = pPort;
        pEntry->rights    = IPC_SPACE_RECEIVE;
        *aOut_pName       = NameOf(index, pEntry->generation);
        return (IPC_MACH_KERN_SUCCESS);
}

/**
 * @brief Takes a right that moves out of an entry: the entry gives up the right, and its reference
 * goes with it when the entry holds nothing else.
 *
 * @param[in,out] aInOut_pEntry The entry.
 * @param[in]     aRight        IPC_SPACE_SEND or IPC_SPACE_SEND_ONCE.
 */
static void
MoveOut(Ipc_Space_tEntry* aInOut_pEntry, uint32_t aRight)
{
        aInOut_pEntry->rights &= ~aRight;
        aInOut_pEntry->urefs   = 0U;
        if(aInOut_pEntry->rights == 0U)
                FreeEntry(aInOut_pEntry);
        else
                aInOut_pEntry->pPort->references++;
}

/**
 * @brief The right a name must hold for a disposition.
 *
 * @param[in] aDisposition MOVE_SEND, MOVE_SEND_ONCE, COPY_SEND, MAKE_SEND or MAKE_SEND_ONCE.
 * @return IPC_SPACE_RECEIVE, IPC_SPACE_SEND_ONCE or IPC_SPACE_SEND.
 */
static uint32_t
Needs(uint32_t aDisposition)
{
        switch(aDisposition)
        {
        case IPC_MACH_MAKE_SEND:
        case IPC_MACH_MAKE_SEND_ONCE:
                return (IPC_SPACE_RECEIVE);
        case IPC_MACH_MOVE_SEND_ONCE:
                return (IPC_SPACE_SEND_ONCE);
        default:
                return (IPC_SPACE_SEND);
        }
}

/**
 * @brief Makes the change a disposition makes to an entry and its port: a new right, or a right that
 * moves out of the entry.
 *
 * @param[in,out] aInOut_pEntry The entry, which holds the right the disposition needs.
 * @param[in]     aDisposition  MOVE_SEND, MOVE_SEND_ONCE, COPY_SEND, MAKE_SEND or MAKE_SEND_ONCE.
 */
static void
Take(Ipc_Space_tEntry* aInOut_pEntry, uint32_t aDisposition)
{
        Ipc_Port_tPort* pPort = aInOut_pEntry->pPort;

        switch(aDisposition)
        {
        case IPC_MACH_MOVE_SEND:
                if(aInOut_pEntry->urefs > 1U)
                {
                        aInOut_pEntry->urefs--;
                        pPort->sendRights++;
                        pPort->references++;
                } else
                {
                        MoveOut(aInOut_pEntry, IPC_SPACE_SEND);
                }
                break;
        case IPC_MACH_MOVE_SEND_ONCE:
                MoveOut(aInOut_pEntry, IPC_SPACE_SEND_ONCE);
                break;
        case IPC_MACH_MAKE_SEND_ONCE:
                pPort->sendOnceRights++;
                pPort->references++;
                break;
        default:
                /* COPY_SEND and MAKE_SEND: a new send right. */
                pPort->sendRights++;
                pPort->references++;
                break;
        }
}

int
Ipc_Space_CopyIn(Ipc_Space_tSpace* aInOut_pSpace,
                 Ipc_Mach_tName    aName,
                 uint32_t          aDisposition,
                 Ipc_Port_tRight*  aOut_pRight)
{
        Ipc_Space_tEntry* pEntry = Ipc_Space_Find(aInOut_pSpace, aName);

        if(pEntry == NULL || pEntry->rights == IPC_SPACE_DEAD)
                return (IPC_MACH_KERN_INVALID_NAME);
        Ipc_Port_tPort* pPort = pEntry->pPort;

        if(aDisposition < IPC_MACH_MOVE_SEND || aDisposition > IPC_MACH_MAKE_SEND_ONCE)
                return (IPC_MACH_KERN_INVALID_VALUE);
        if((pEntry->rights & Needs(aDisposition)) == 0U)
                return (IPC_MACH_KERN_INVALID_RIGHT);
        aOut_pRight->pPort = pPort;
        aOut_pRight->type  = aDisposition == IPC_MACH_MAKE_SEND_ONCE
                        || aDisposition == IPC_MACH_MOVE_SEND_ONCE
                ? IPC_MACH_PORT_SEND_ONCE
                : IPC_MACH_PORT_SEND;
        Take(pEntry, aDisposition);
        return (IPC_MACH_KERN_SUCCESS);
}

/**
 * @brief Adds a send right to an entry that names its port: one user reference more, and the right
 * merges into the entry's, so its own count and reference are dropped.
 *
 * @param[in,out] aInOut_pEntry The entry.
 * @param[in]     aIn_Right     The send right.
 * @return IPC_MACH_KERN_SUCCESS, or IPC_MACH_KERN_UREFS_OVERFLOW, in which case the right is given
 * back.
 */
static int
MergeSend(Ipc_Space_tEntry* aInOut_pEntry, Ipc_Port_tRight aIn_Right)
{
        if((aInOut_pEntry->rights & IPC_SPACE_SEND) != 0U
           && aInOut_pEntry->urefs >= IPC_MACH_UREFS_MAX)
        {
                Ipc_Port_ReleaseRight(aIn_Right);
                return (IPC_MACH_KERN_UREFS_OVERFLOW);
        }
        if((aInOut_pEntry->rights & IPC_SPACE_SEND) != 0U)
        {
                aInOut_pEntry->urefs++;
                Ipc_Port_ReleaseRight(aIn_Right);
                return (IPC_MACH_KERN_SUCCESS);
        }
        /* A receive right's name takes the send right; the entry's reference stays the only one. */
        aInOut_pEntry->rights |= IPC_SPACE_SEND;
        aInOut_pEntry->urefs   = 1U;
        Ipc_Port_Release(aIn_Right.pPort);
        return (IPC_MACH_KERN_SUCCESS);
}

int
Ipc_Space_CopyOut(Ipc_Space_tSpace* aInOut_pSpace,
                  Ipc_Port_tRight   aIn_Right,
                  Ipc_Mach_tName*   aOut_pName)
{
        const uint32_t named = aIn_Right.type == IPC_MACH_PORT_SEND
                ? IndexOfPort(aInOut_pSpace, aIn_Right.pPort)
                : 0U;

        if(named != 0U)
        {
                Ipc_Space_tEntry* pEntry = &aInOut_pSpace->entries[ named ];

                *aOut_pName = NameOf(named, pEntry->generation);
                return (MergeSend(pEntry, aIn_Right));
        }
        uint32_t          index  = 0U;
        Ipc_Space_tEntry* pEntry = FreeSlot(aInOut_pSpace, &index);

        if(pEntry == NULL)
        {
                Ipc_Port_ReleaseRight(aIn_Right);
                return (IPC_MACH_KERN_RESOURCE_SHORTAGE);
        }
        pEntry->pPort  = aIn_Right.pPort;
        pEntry->rights = aIn_Right.type == IPC_MACH_PORT_SEND ? IPC_SPACE_SEND
                                                              : IPC_SPACE_SEND_ONCE;
        pEntry->urefs  = 1U;
        *aOut_pName    = NameOf(index, pEntry->generation);
        return (IPC_MACH_KERN_SUCCESS);
}

/** What CanInsert() answers when a send right joins the entry of its name. */
#define IPC_SPACE_MERGE (-1)

/**
 * @brief Whether a right can go into a space under a name: the name must be in the table, and name
 * nothing or the right's port; a send right may join its port's name, and a port has one name for its
 * send rights.
 *
 * @param[in] aIn_pSpace The space.
 * @param[in] aName      The name.
 * @param[in] aIn_Right  The right.
 * @return IPC_MACH_KERN_SUCCESS for a free name, IPC_SPACE_MERGE when the send right joins the name's
 * entry, or why it cannot.
 */
static int
CanInsert(const Ipc_Space_tSpace* aIn_pSpace, Ipc_Mach_tName aName, Ipc_Port_tRight aIn_Right)
{
        const uint32_t          index  = IndexOf(aName);
        const Ipc_Space_tEntry* pEntry = &aIn_pSpace->entries[ index ];
        const bool              isSend = aIn_Right.type == IPC_MACH_PORT_SEND;

        if(index == 0U)
                return (IPC_MACH_KERN_INVALID_VALUE);
        if(pEntry->rights == 0U)
                return (isSend && IndexOfPort(aIn_pSpace, aIn_Right.pPort) != 0U
                                ? IPC_MACH_KERN_RIGHT_EXISTS
                                : IPC_MACH_KERN_SUCCESS);
        if(NameOf(index, pEntry->generation) != aName || pEntry->pPort != aIn_Right.pPort)
                return (IPC_MACH_KERN_NAME_EXISTS);
        return (isSend ? IPC_SPACE_MERGE : IPC_MACH_KERN_RIGHT_EXISTS);
}

int
Ipc_Space_Insert(Ipc_Space_tSpace* aInOut_pSpace, Ipc_Mach_tName aName, Ipc_Port_tRight aIn_Right)
{
        const uint32_t    index  = IndexOf(aName);
        Ipc_Space_tEntry* pEntry = &aInOut_pSpace->entries[ index ];
        const int         result = CanInsert(aInOut_pSpace, aName, aIn_Right);

        if(result == IPC_SPACE_MERGE)
                return (MergeSend(pEntry, aIn_Right));
        if(result != IPC_MACH_KERN_SUCCESS)
        {
                Ipc_Port_ReleaseRight(aIn_Right);
                return (result);
        }
        /* A free entry takes the name, with its generation. */
        pEntry->pPort      = aIn_Right.pPort;
        pEntry->rights     = aIn_Right.type == IPC_MACH_PORT_SEND ? IPC_SPACE_SEND
                                                                  : IPC_SPACE_SEND_ONCE;
        pEntry->urefs      = 1U;
        pEntry->generation = aName & 0xFFU;
        return (IPC_MACH_KERN_SUCCESS);
}

/**
 * @brief Destroys the receive right of an entry, as mach_port_mod_refs with -1 does: the port dies,
 * and a send right of the same name becomes a dead name.
 *
 * @param[in,out] aInOut_pEntry The entry.
 */
static void
DestroyReceive(Ipc_Space_tEntry* aInOut_pEntry)
{
        Ipc_Port_tPort* pPort = aInOut_pEntry->pPort;

        Ipc_Port_Kill(pPort);
        aInOut_pEntry->rights &= ~IPC_SPACE_RECEIVE;
        if((aInOut_pEntry->rights & IPC_SPACE_SEND) != 0U)
        {
                MakeDead(aInOut_pEntry);
                return;
        }
        FreeEntry(aInOut_pEntry);
        Ipc_Port_Release(pPort);
}

/**
 * @brief Changes the user references of a send right or a dead name.
 *
 * @param[in,out] aInOut_pEntry The entry.
 * @param[in]     aRight        IPC_SPACE_SEND or IPC_SPACE_DEAD.
 * @param[in]     aDelta        The change.
 * @return IPC_MACH_KERN_SUCCESS, IPC_MACH_KERN_INVALID_VALUE or IPC_MACH_KERN_UREFS_OVERFLOW.
 */
static int
ChangeUrefs(Ipc_Space_tEntry* aInOut_pEntry, uint32_t aRight, int32_t aDelta)
{
        const int64_t urefs = (int64_t) aInOut_pEntry->urefs + aDelta;

        if(urefs < 0)
                return (IPC_MACH_KERN_INVALID_VALUE);
        if(urefs > (int64_t) IPC_MACH_UREFS_MAX)
                return (IPC_MACH_KERN_UREFS_OVERFLOW);
        if(urefs > 0)
        {
                aInOut_pEntry->urefs = (uint32_t) urefs;
                return (IPC_MACH_KERN_SUCCESS);
        }
        if(aRight == IPC_SPACE_SEND)
                DropRight(aInOut_pEntry, IPC_SPACE_SEND);
        else
                FreeEntry(aInOut_pEntry);
        return (IPC_MACH_KERN_SUCCESS);
}

/**
 * @brief Changes a right that has no user references, a receive or a send-once right: 0 changes
 * nothing, -1 destroys it, any other change is refused.
 *
 * @param[in,out] aInOut_pEntry The entry.
 * @param[in]     aRight        IPC_SPACE_RECEIVE or IPC_SPACE_SEND_ONCE.
 * @param[in]     aDelta        The change.
 * @return IPC_MACH_KERN_SUCCESS, IPC_MACH_KERN_INVALID_RIGHT or IPC_MACH_KERN_INVALID_VALUE.
 */
static int
ChangeSingle(Ipc_Space_tEntry* aInOut_pEntry, uint32_t aRight, int32_t aDelta)
{
        if((aInOut_pEntry->rights & aRight) == 0U)
                return (IPC_MACH_KERN_INVALID_RIGHT);
        if(aDelta != 0 && aDelta != -1)
                return (IPC_MACH_KERN_INVALID_VALUE);
        if(aDelta == -1 && aRight == IPC_SPACE_RECEIVE)
                DestroyReceive(aInOut_pEntry);
        else if(aDelta == -1)
                DropRight(aInOut_pEntry, IPC_SPACE_SEND_ONCE);
        return (IPC_MACH_KERN_SUCCESS);
}

int
Ipc_Space_ModRefs(Ipc_Space_tSpace* aInOut_pSpace,
                  Ipc_Mach_tName    aName,
                  uint32_t          aRight,
                  int32_t           aDelta)
{
        Ipc_Space_tEntry* pEntry = Ipc_Space_Find(aInOut_pSpace, aName);

        if(pEntry == NULL)
                return (IPC_MACH_KERN_INVALID_NAME);
        switch(aRight)
        {
        case IPC_MACH_RIGHT_RECEIVE:
                return (ChangeSingle(pEntry, IPC_SPACE_RECEIVE, aDelta));
        case IPC_MACH_RIGHT_SEND_ONCE:
                return (ChangeSingle(pEntry, IPC_SPACE_SEND_ONCE, aDelta));
        case IPC_MACH_RIGHT_SEND:
                return ((pEntry->rights & IPC_SPACE_SEND) == 0U
                                ? IPC_MACH_KERN_INVALID_RIGHT
                                : ChangeUrefs(pEntry, IPC_SPACE_SEND, aDelta));
        case IPC_MACH_RIGHT_DEAD_NAME:
                return ((pEntry->rights & IPC_SPACE_DEAD) == 0U
                                ? IPC_MACH_KERN_INVALID_RIGHT
                                : ChangeUrefs(pEntry, IPC_SPACE_DEAD, aDelta));
        case IPC_MACH_RIGHT_PORT_SET:
                /* OpenTala has no port sets, so no name holds one. */
                return (IPC_MACH_KERN_INVALID_RIGHT);
        default:
                return (IPC_MACH_KERN_INVALID_VALUE);
        }
}

int
Ipc_Space_Deallocate(Ipc_Space_tSpace* aInOut_pSpace, Ipc_Mach_tName aName)
{
        if(aName == IPC_MACH_PORT_NULL || aName == IPC_MACH_PORT_DEAD)
                return (IPC_MACH_KERN_SUCCESS);
        const Ipc_Space_tEntry* pEntry = Ipc_Space_Find(aInOut_pSpace, aName);

        if(pEntry == NULL)
                return (IPC_MACH_KERN_INVALID_NAME);
        if((pEntry->rights & IPC_SPACE_SEND) != 0U)
                return (Ipc_Space_ModRefs(aInOut_pSpace, aName, IPC_MACH_RIGHT_SEND, -1));
        if((pEntry->rights & IPC_SPACE_SEND_ONCE) != 0U)
                return (Ipc_Space_ModRefs(aInOut_pSpace, aName, IPC_MACH_RIGHT_SEND_ONCE, -1));
        if((pEntry->rights & IPC_SPACE_DEAD) != 0U)
                return (Ipc_Space_ModRefs(aInOut_pSpace, aName, IPC_MACH_RIGHT_DEAD_NAME, -1));
        return (IPC_MACH_KERN_INVALID_RIGHT);
}

/**
 * @brief The kernel port of a space that a kind names.
 *
 * @param[in] aIn_pSpace The space.
 * @param[in] aKernel    Ipc_Port_kTask, Ipc_Port_kThread or Ipc_Port_kHost.
 * @return The port.
 */
static Ipc_Port_tPort*
KernelPortOf(const Ipc_Space_tSpace* aIn_pSpace, Ipc_Port_tKernel aKernel)
{
        switch(aKernel)
        {
        case Ipc_Port_kTask:
                return (aIn_pSpace->pTaskPort);
        case Ipc_Port_kThread:
                return (aIn_pSpace->pThreadPort);
        default:
                return (Ipc_Port_Host());
        }
}

Ipc_Mach_tName
Ipc_Space_KernelPort(Ipc_Space_tSpace* aInOut_pSpace, Ipc_Port_tKernel aKernel)
{
        Ipc_Port_tPort* pPort = KernelPortOf(aInOut_pSpace, aKernel);
        Ipc_Port_tRight right = { pPort, IPC_MACH_PORT_SEND };
        Ipc_Mach_tName  name  = IPC_MACH_PORT_NULL;

        pPort->sendRights++;
        pPort->references++;
        return (Ipc_Space_CopyOut(aInOut_pSpace, right, &name) == IPC_MACH_KERN_SUCCESS
                        ? name
                        : IPC_MACH_PORT_NULL);
}

bool
Ipc_Space_IsSelf(Ipc_Space_tSpace* aInOut_pSpace, Ipc_Mach_tName aName)
{
        const Ipc_Space_tEntry* pEntry = Ipc_Space_Find(aInOut_pSpace, aName);

        return (pEntry != NULL && (pEntry->rights & IPC_SPACE_SEND) != 0U
                && pEntry->pPort == aInOut_pSpace->pTaskPort);
}
