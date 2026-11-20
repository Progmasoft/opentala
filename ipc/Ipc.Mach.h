/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/*
 * The values of Mach's interface that a program sees, with XNU's numbers: <mach/kern_return.h>,
 * <mach/port.h> and <mach/message.h>. A program built for Darwin passes them as they are.
 */

/** A name in a task's space: (index << 8) | generation. */
typedef uint32_t Ipc_Mach_tName;

#define IPC_MACH_PORT_NULL 0x0U
#define IPC_MACH_PORT_DEAD 0xFFFFFFFFU

/* kern_return_t */
#define IPC_MACH_KERN_SUCCESS           0
#define IPC_MACH_KERN_INVALID_ADDRESS   1
#define IPC_MACH_KERN_RESOURCE_SHORTAGE 6
#define IPC_MACH_KERN_NAME_EXISTS       13
#define IPC_MACH_KERN_INVALID_NAME      15
#define IPC_MACH_KERN_INVALID_RIGHT     17
#define IPC_MACH_KERN_INVALID_VALUE     18
#define IPC_MACH_KERN_UREFS_OVERFLOW    19
#define IPC_MACH_KERN_RIGHT_EXISTS      21

/* mach_port_right_t */
#define IPC_MACH_RIGHT_SEND      0U
#define IPC_MACH_RIGHT_RECEIVE   1U
#define IPC_MACH_RIGHT_SEND_ONCE 2U
#define IPC_MACH_RIGHT_PORT_SET  3U
#define IPC_MACH_RIGHT_DEAD_NAME 4U

/* The dispositions of a right in a message, and the types a received right has. */
#define IPC_MACH_MOVE_RECEIVE   16U
#define IPC_MACH_MOVE_SEND      17U
#define IPC_MACH_MOVE_SEND_ONCE 18U
#define IPC_MACH_COPY_SEND      19U
#define IPC_MACH_MAKE_SEND      20U
#define IPC_MACH_MAKE_SEND_ONCE 21U
#define IPC_MACH_PORT_SEND      IPC_MACH_MOVE_SEND
#define IPC_MACH_PORT_SEND_ONCE IPC_MACH_MOVE_SEND_ONCE

/* msgh_bits */
#define IPC_MACH_BITS_REMOTE(dA_Bits)      ((dA_Bits) & 0x1FU)
#define IPC_MACH_BITS_LOCAL(dA_Bits)       (((dA_Bits) >> 8) & 0x1FU)
#define IPC_MACH_BITS_VOUCHER(dA_Bits)     (((dA_Bits) >> 16) & 0x1FU)
#define IPC_MACH_BITS(dA_Remote, dA_Local) ((uint32_t) (dA_Remote) | ((uint32_t) (dA_Local) << 8))
#define IPC_MACH_BITS_COMPLEX              0x80000000U

/* mach_msg_option_t */
#define IPC_MACH_SEND_MSG  0x1U
#define IPC_MACH_RCV_MSG   0x2U
#define IPC_MACH_RCV_LARGE 0x4U

/* mach_msg_return_t */
#define IPC_MACH_MSG_SUCCESS          0x0
#define IPC_MACH_SEND_INVALID_DATA    0x10000002
#define IPC_MACH_SEND_INVALID_DEST    0x10000003
#define IPC_MACH_SEND_TIMED_OUT       0x10000004
#define IPC_MACH_SEND_INVALID_VOUCHER 0x10000005
#define IPC_MACH_SEND_MSG_TOO_SMALL   0x10000008
#define IPC_MACH_SEND_INVALID_REPLY   0x10000009
#define IPC_MACH_SEND_TOO_LARGE       0x1000000E
#define IPC_MACH_SEND_INVALID_TYPE    0x1000000F
#define IPC_MACH_SEND_INVALID_HEADER  0x10000010
#define IPC_MACH_RCV_INVALID_NAME     0x10004002
#define IPC_MACH_RCV_TIMED_OUT        0x10004003
#define IPC_MACH_RCV_TOO_LARGE        0x10004004
#define IPC_MACH_RCV_INVALID_DATA     0x10004008
#define IPC_MACH_RCV_HEADER_ERROR     0x1000400B

/** mach_msg_header_t: 24 bytes. */
typedef struct Ipc_Mach_Header
{
        uint32_t bits;
        uint32_t size;
        uint32_t remotePort;
        uint32_t localPort;
        uint32_t voucherPort;
        int32_t  id;
} Ipc_Mach_tHeader;

/** mach_msg_trailer_t: the trailer every received message carries, 8 bytes. */
typedef struct Ipc_Mach_Trailer
{
        uint32_t type;
        uint32_t size;
} Ipc_Mach_tTrailer;

_Static_assert(sizeof(Ipc_Mach_tHeader) == 24U, "mach_msg_header_t is 24 bytes");
_Static_assert(sizeof(Ipc_Mach_tTrailer) == 8U, "mach_msg_trailer_t is 8 bytes");

/** The most a message may hold, header and body, without its trailer. */
#define IPC_MACH_MESSAGE_LARGEST 2048U
/** How many messages a port queues: MACH_PORT_QLIMIT_DEFAULT. */
#define IPC_MACH_QUEUE_LIMIT 5U
/** The most user references a name holds: MACH_PORT_UREFS_MAX less one, as XNU keeps it. */
#define IPC_MACH_UREFS_MAX 0xFFFEU
