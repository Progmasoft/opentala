/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ipc/Ipc.Mach.h"
#include "ipc/Ipc.Message.h"
#include "ipc/Ipc.Port.h"
#include "ipc/Ipc.Space.h"
#include "kern/Kern.Machine.h"
#include "kern/Kern.Memory.h"

void
Ipc_Message_Destroy(Ipc_Message_tMessage* aIn_pMessage)
{
        Ipc_Port_ReleaseRight(aIn_pMessage->destination);
        Ipc_Port_ReleaseRight(aIn_pMessage->reply);
        Kern_Memory_Free(aIn_pMessage);
}

/**
 * @brief Takes the destination and the reply right of a header out of the sender's space.
 *
 * @param[in,out] aInOut_pSpace   The sender's space.
 * @param[in]     aIn_pHeader     The header as sent.
 * @param[out]    aOut_pMessage   The message, whose rights are set.
 * @return MACH_MSG_SUCCESS, MACH_SEND_INVALID_HEADER, MACH_SEND_INVALID_DEST or
 * MACH_SEND_INVALID_REPLY; on failure no right is held.
 */
static int
CopyInRights(Ipc_Space_tSpace*       aInOut_pSpace,
             const Ipc_Mach_tHeader* aIn_pHeader,
             Ipc_Message_tMessage*   aOut_pMessage)
{
        const int destination = Ipc_Space_CopyIn(aInOut_pSpace,
                                                 aIn_pHeader->remotePort,
                                                 IPC_MACH_BITS_REMOTE(aIn_pHeader->bits),
                                                 &aOut_pMessage->destination);

        if(destination == IPC_MACH_KERN_INVALID_VALUE)
                return (IPC_MACH_SEND_INVALID_HEADER);
        if(destination != IPC_MACH_KERN_SUCCESS)
                return (IPC_MACH_SEND_INVALID_DEST);
        if(aIn_pHeader->localPort == IPC_MACH_PORT_NULL)
                return (IPC_MACH_MSG_SUCCESS);
        const int reply = Ipc_Space_CopyIn(aInOut_pSpace,
                                           aIn_pHeader->localPort,
                                           IPC_MACH_BITS_LOCAL(aIn_pHeader->bits),
                                           &aOut_pMessage->reply);

        if(reply == IPC_MACH_KERN_SUCCESS)
                return (IPC_MACH_MSG_SUCCESS);
        Ipc_Port_ReleaseRight(aOut_pMessage->destination);
        aOut_pMessage->destination.pPort = NULL;
        return (reply == IPC_MACH_KERN_INVALID_VALUE ? IPC_MACH_SEND_INVALID_HEADER
                                                     : IPC_MACH_SEND_INVALID_REPLY);
}

/**
 * @brief Checks the size of a message to send.
 *
 * @param[in] aSize The size, header and body.
 * @return MACH_MSG_SUCCESS, MACH_SEND_MSG_TOO_SMALL or MACH_SEND_TOO_LARGE.
 */
static int
CheckSize(uint32_t aSize)
{
        if(aSize < sizeof(Ipc_Mach_tHeader) || aSize % 4U != 0U)
                return (IPC_MACH_SEND_MSG_TOO_SMALL);
        if(aSize > IPC_MACH_MESSAGE_LARGEST)
                return (IPC_MACH_SEND_TOO_LARGE);
        return (IPC_MACH_MSG_SUCCESS);
}

int
Ipc_Message_Send(Ipc_Space_tSpace* aInOut_pSpace, uint64_t aMessage, uint32_t aSize)
{
        const int size = CheckSize(aSize);

        if(size != IPC_MACH_MSG_SUCCESS)
                return (size);
        Ipc_Message_tMessage* pMessage = Kern_Memory_Allocate(sizeof(*pMessage) + aSize
                                                              + sizeof(Ipc_Mach_tTrailer));

        if(pMessage == NULL)
                return (IPC_MACH_SEND_TIMED_OUT);
        if(!Kern_Machine_CopyIn(pMessage->bytes, aMessage, aSize))
        {
                Kern_Memory_Free(pMessage);
                return (IPC_MACH_SEND_INVALID_DATA);
        }
        const Ipc_Mach_tHeader* pHeader = (const Ipc_Mach_tHeader*) pMessage->bytes;

        pMessage->size = aSize;
        if((pHeader->bits & IPC_MACH_BITS_COMPLEX) != 0U)
        {
                Kern_Memory_Free(pMessage);
                return (IPC_MACH_SEND_INVALID_TYPE);
        }
        if(IPC_MACH_BITS_VOUCHER(pHeader->bits) != 0U)
        {
                Kern_Memory_Free(pMessage);
                return (IPC_MACH_SEND_INVALID_VOUCHER);
        }
        const int rights = CopyInRights(aInOut_pSpace, pHeader, pMessage);

        if(rights != IPC_MACH_MSG_SUCCESS)
        {
                Kern_Memory_Free(pMessage);
                return (rights);
        }
        Ipc_Port_tPort* pPort = pMessage->destination.pPort;

        /* A dead port and a kernel port take no message; nobody waits on a full queue. */
        if(pPort->isDead || pPort->pReceiver == NULL || pPort->queued >= IPC_MACH_QUEUE_LIMIT)
        {
                const int result = pPort->queued >= IPC_MACH_QUEUE_LIMIT
                        ? IPC_MACH_SEND_TIMED_OUT
                        : IPC_MACH_SEND_INVALID_DEST;

                Ipc_Message_Destroy(pMessage);
                return (result);
        }
        if(pPort->pLast != NULL)
                pPort->pLast->pNext = pMessage;
        else
                pPort->pFirst = pMessage;
        pPort->pLast = pMessage;
        pPort->queued++;
        return (IPC_MACH_MSG_SUCCESS);
}

/**
 * @brief Takes the first message of a port off its queue.
 *
 * @param[in,out] aInOut_pPort The port.
 * @return The message.
 */
static Ipc_Message_tMessage*
Dequeue(Ipc_Port_tPort* aInOut_pPort)
{
        Ipc_Message_tMessage* pMessage = aInOut_pPort->pFirst;

        aInOut_pPort->pFirst = pMessage->pNext;
        if(aInOut_pPort->pFirst == NULL)
                aInOut_pPort->pLast = NULL;
        aInOut_pPort->queued--;
        pMessage->pNext = NULL;
        return (pMessage);
}

/**
 * @brief Refuses a message too large for the buffer: MACH_RCV_LARGE keeps it and writes its size
 * where the buffer's header has msgh_size; without it, the message is destroyed.
 *
 * @param[in,out] aInOut_pPort The port.
 * @param[in]     aBuffer      The user address of the buffer.
 * @param[in]     aOptions     The options.
 * @return MACH_RCV_TOO_LARGE.
 */
static int
RefuseLarge(Ipc_Port_tPort* aInOut_pPort, uint64_t aBuffer, uint32_t aOptions)
{
        const uint32_t needed = aInOut_pPort->pFirst->size + (uint32_t) sizeof(Ipc_Mach_tTrailer);

        if((aOptions & IPC_MACH_RCV_LARGE) != 0U)
                (void) Kern_Machine_CopyOut(aBuffer + offsetof(Ipc_Mach_tHeader, size),
                                            &needed,
                                            sizeof(needed));
        else
                Ipc_Message_Destroy(Dequeue(aInOut_pPort));
        return (IPC_MACH_RCV_TOO_LARGE);
}

int
Ipc_Message_Receive(Ipc_Space_tSpace* aInOut_pSpace,
                    uint64_t          aBuffer,
                    uint32_t          aSize,
                    Ipc_Mach_tName    aName,
                    uint32_t          aOptions)
{
        const Ipc_Space_tEntry* pEntry = Ipc_Space_Find(aInOut_pSpace, aName);

        if(pEntry == NULL || (pEntry->rights & IPC_SPACE_RECEIVE) == 0U)
                return (IPC_MACH_RCV_INVALID_NAME);
        Ipc_Port_tPort* pPort = pEntry->pPort;

        if(pPort->queued == 0U)
                return (IPC_MACH_RCV_TIMED_OUT);
        if(aSize < pPort->pFirst->size + sizeof(Ipc_Mach_tTrailer))
                return (RefuseLarge(pPort, aBuffer, aOptions));
        Ipc_Message_tMessage* pMessage = Dequeue(pPort);
        Ipc_Mach_tHeader*     pHeader  = (Ipc_Mach_tHeader*) pMessage->bytes;
        Ipc_Mach_tName        reply    = IPC_MACH_PORT_NULL;
        const uint32_t        type     = pMessage->reply.type;

        /* The reply right goes into the receiver's space; the message no longer holds it. */
        if(pMessage->reply.pPort != NULL)
        {
                const int copied = Ipc_Space_CopyOut(aInOut_pSpace, pMessage->reply, &reply);

                pMessage->reply.pPort = NULL;
                if(copied != IPC_MACH_KERN_SUCCESS)
                {
                        Ipc_Message_Destroy(pMessage);
                        return (IPC_MACH_RCV_HEADER_ERROR);
                }
        }
        const Ipc_Mach_tTrailer trailer = { 0U, (uint32_t) sizeof(Ipc_Mach_tTrailer) };

        pHeader->bits        = IPC_MACH_BITS(reply == IPC_MACH_PORT_NULL ? 0U : type,
                                             pMessage->destination.type);
        pHeader->size        = pMessage->size;
        pHeader->remotePort  = reply;
        pHeader->localPort   = aName;
        pHeader->voucherPort = IPC_MACH_PORT_NULL;
        for(uint32_t index = 0U; index < sizeof(trailer); index++)
                pMessage->bytes[ pMessage->size + index ] = ((const uint8_t*) &trailer)[ index ];
        const bool isWritten = Kern_Machine_CopyOut(aBuffer,
                                                    pMessage->bytes,
                                                    pMessage->size + sizeof(trailer));

        Ipc_Message_Destroy(pMessage);
        return (isWritten ? IPC_MACH_MSG_SUCCESS : IPC_MACH_RCV_INVALID_DATA);
}
