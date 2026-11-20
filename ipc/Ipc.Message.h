/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "ipc/Ipc.Mach.h"
#include "ipc/Ipc.Port.h"
#include "ipc/Ipc.Space.h"

/*
 * Messages: what mach_msg sends and receives. A message is a header and a body of bytes, at most
 * IPC_MACH_MESSAGE_LARGEST; it carries the right to its destination and, when it has one, a reply
 * right. OpenTala takes no descriptors: a complex message is refused with MACH_SEND_INVALID_TYPE.
 *
 * A task has one thread, so nothing can wake a send that would wait for room in a full queue, or a
 * receive that would wait for a message: such a send ends with MACH_SEND_TIMED_OUT and such a receive
 * with MACH_RCV_TIMED_OUT, as with a timeout of 0.
 */

typedef struct Ipc_Message_Message
{
        struct Ipc_Message_Message* pNext;
        Ipc_Port_tRight             destination;
        Ipc_Port_tRight             reply;
        /** The header and the body, without the trailer. */
        uint32_t size;
        /** The header and the body as sent, and room for the trailer. */
        uint8_t bytes[];
} Ipc_Message_tMessage;

/**
 * @brief Sends a message from a task's memory, as mach_msg with MACH_SEND_MSG does.
 *
 * @param[in,out] aInOut_pSpace The sender's space.
 * @param[in]     aMessage      The user address of the message.
 * @param[in]     aSize         Its size, header and body.
 * @return MACH_MSG_SUCCESS or a MACH_SEND_* value.
 */
int
Ipc_Message_Send(Ipc_Space_tSpace* aInOut_pSpace, uint64_t aMessage, uint32_t aSize);

/**
 * @brief Receives the first message of a port into a task's memory, as mach_msg with MACH_RCV_MSG
 * does.
 *
 * @param[in,out] aInOut_pSpace The receiver's space.
 * @param[in]     aBuffer       The user address of the buffer.
 * @param[in]     aSize         Its size.
 * @param[in]     aName         The name of the receive right.
 * @param[in]     aOptions      The options; MACH_RCV_LARGE keeps a message too large for the buffer.
 * @return MACH_MSG_SUCCESS or a MACH_RCV_* value.
 */
int
Ipc_Message_Receive(Ipc_Space_tSpace* aInOut_pSpace,
                    uint64_t          aBuffer,
                    uint32_t          aSize,
                    Ipc_Mach_tName    aName,
                    uint32_t          aOptions);

/**
 * @brief Destroys a message that will not be received: its rights are given back, and its memory.
 *
 * @param[in] aIn_pMessage The message.
 */
void
Ipc_Message_Destroy(Ipc_Message_tMessage* aIn_pMessage);
