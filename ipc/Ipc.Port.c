/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdint.h>

#include "ipc/Ipc.Mach.h"
#include "ipc/Ipc.Message.h"
#include "ipc/Ipc.Port.h"
#include "kern/Kern.Memory.h"
#include "kern/Kern.Print.h"

/* The host's port: it refers to itself, so it is never freed. */
static Ipc_Port_tPort gsHostPort = { 1, 0, 0, NULL, NULL, 0, NULL, Ipc_Port_kHost, false };
static uint64_t       gsAlive;

Ipc_Port_tPort*
Ipc_Port_Create(Ipc_Port_tKernel aKernel)
{
        Ipc_Port_tPort* pPort = Kern_Memory_Allocate(sizeof(*pPort));

        if(pPort == NULL)
                return (NULL);
        pPort->kernel = aKernel;
        gsAlive++;
        return (pPort);
}

Ipc_Port_tPort*
Ipc_Port_Host(void)
{
        return (&gsHostPort);
}

void
Ipc_Port_Release(Ipc_Port_tPort* aInOut_pPort)
{
        if(aInOut_pPort->references == 0)
                Kern_Panic("a port with no reference was released");
        aInOut_pPort->references--;
        if(aInOut_pPort->references != 0 || aInOut_pPort == &gsHostPort)
                return;
        /* A live port is referred to by its receive right, so the last reference is a dead port's. */
        if(!aInOut_pPort->isDead || aInOut_pPort->queued != 0)
                Kern_Panic("a live port lost its last reference");
        Kern_Memory_Free(aInOut_pPort);
        gsAlive--;
}

void
Ipc_Port_Kill(Ipc_Port_tPort* aInOut_pPort)
{
        if(aInOut_pPort->isDead)
                Kern_Panic("a dead port was killed again");
        aInOut_pPort->isDead    = true;
        aInOut_pPort->pReceiver = NULL;
        /* A message may hold the port's last references but the receive right's, so the port stays. */
        for(Ipc_Message_tMessage* pMessage = aInOut_pPort->pFirst; pMessage != NULL;)
        {
                Ipc_Message_tMessage* pNext = pMessage->pNext;

                Ipc_Message_Destroy(pMessage);
                pMessage = pNext;
        }
        aInOut_pPort->pFirst = NULL;
        aInOut_pPort->pLast  = NULL;
        aInOut_pPort->queued = 0;
        Ipc_Port_Release(aInOut_pPort);
}

void
Ipc_Port_ReleaseRight(Ipc_Port_tRight aIn_Right)
{
        if(aIn_Right.pPort == NULL)
                return;
        if(aIn_Right.type == IPC_MACH_PORT_SEND_ONCE)
                aIn_Right.pPort->sendOnceRights--;
        else
                aIn_Right.pPort->sendRights--;
        Ipc_Port_Release(aIn_Right.pPort);
}

uint64_t
Ipc_Port_Alive(void)
{
        return (gsAlive);
}
