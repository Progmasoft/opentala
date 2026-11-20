/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "ipc/Ipc.Mach.h"
#include "ipc/Ipc.Message.h"
#include "ipc/Ipc.Space.h"
#include "ipc/Ipc.Traps.h"
#include "kern/Kern.Machine.h"
#include "kern/Kern.Process.h"
#include "kern/Kern.SystemCalls.h"

/**
 * @brief The result of a Mach trap: its return code as the first value.
 *
 * @param[in] aCode The kern_return_t, mach_msg_return_t or name.
 * @return The result.
 */
static Kern_SystemCalls_tResult
Result(uint64_t aCode)
{
        Kern_SystemCalls_tResult result = { 0, { aCode, 0U } };

        return (result);
}

/**
 * @brief The space of the process running.
 *
 * @return The space.
 */
static Ipc_Space_tSpace*
Space(void)
{
        return (Kern_Process_Space());
}

/**
 * @brief A name from a trap's argument: the low 32 bits, as mach_port_name_t is.
 *
 * @param[in] aArgument The argument.
 * @return The name.
 */
static Ipc_Mach_tName
NameOf(uint64_t aArgument)
{
        return ((Ipc_Mach_tName) (aArgument & 0xFFFFFFFFU));
}

Kern_SystemCalls_tResult
Ipc_Traps_PortAllocate(const uint64_t* aIn_pArguments)
{
        Ipc_Mach_tName name = IPC_MACH_PORT_NULL;

        if(!Ipc_Space_IsSelf(Space(), NameOf(aIn_pArguments[ 0 ])))
                return (Result(IPC_MACH_SEND_INVALID_DEST));
        /* OpenTala has no port sets; a dead name or a send right cannot be allocated on its own. */
        if(aIn_pArguments[ 1 ] != IPC_MACH_RIGHT_RECEIVE)
                return (Result(IPC_MACH_KERN_INVALID_VALUE));
        const int result = Ipc_Space_AllocateReceive(Space(), &name);

        if(result != IPC_MACH_KERN_SUCCESS)
                return (Result((uint64_t) result));
        if(!Kern_Machine_CopyOut(aIn_pArguments[ 2 ], &name, sizeof(name)))
        {
                (void) Ipc_Space_ModRefs(Space(), name, IPC_MACH_RIGHT_RECEIVE, -1);
                return (Result(IPC_MACH_KERN_INVALID_ADDRESS));
        }
        return (Result(IPC_MACH_KERN_SUCCESS));
}

Kern_SystemCalls_tResult
Ipc_Traps_PortDeallocate(const uint64_t* aIn_pArguments)
{
        if(!Ipc_Space_IsSelf(Space(), NameOf(aIn_pArguments[ 0 ])))
                return (Result(IPC_MACH_SEND_INVALID_DEST));
        return (Result((uint64_t) Ipc_Space_Deallocate(Space(), NameOf(aIn_pArguments[ 1 ]))));
}

Kern_SystemCalls_tResult
Ipc_Traps_PortModRefs(const uint64_t* aIn_pArguments)
{
        if(!Ipc_Space_IsSelf(Space(), NameOf(aIn_pArguments[ 0 ])))
                return (Result(IPC_MACH_SEND_INVALID_DEST));
        return (Result((uint64_t) Ipc_Space_ModRefs(Space(),
                                                    NameOf(aIn_pArguments[ 1 ]),
                                                    (uint32_t) aIn_pArguments[ 2 ],
                                                    (int32_t) (uint32_t) aIn_pArguments[ 3 ])));
}

Kern_SystemCalls_tResult
Ipc_Traps_PortInsertRight(const uint64_t* aIn_pArguments)
{
        Ipc_Port_tRight right = { 0 };

        if(!Ipc_Space_IsSelf(Space(), NameOf(aIn_pArguments[ 0 ])))
                return (Result(IPC_MACH_SEND_INVALID_DEST));
        const int copied = Ipc_Space_CopyIn(Space(),
                                            NameOf(aIn_pArguments[ 2 ]),
                                            (uint32_t) aIn_pArguments[ 3 ],
                                            &right);

        if(copied != IPC_MACH_KERN_SUCCESS)
                return (Result((uint64_t) copied));
        return (Result((uint64_t) Ipc_Space_Insert(Space(), NameOf(aIn_pArguments[ 1 ]), right)));
}

Kern_SystemCalls_tResult
Ipc_Traps_ReplyPort(const uint64_t* aIn_pArguments)
{
        Ipc_Mach_tName name = IPC_MACH_PORT_NULL;

        (void) aIn_pArguments;
        if(Ipc_Space_AllocateReceive(Space(), &name) != IPC_MACH_KERN_SUCCESS)
                return (Result(IPC_MACH_PORT_NULL));
        return (Result(name));
}

Kern_SystemCalls_tResult
Ipc_Traps_ThreadSelf(const uint64_t* aIn_pArguments)
{
        (void) aIn_pArguments;
        return (Result(Ipc_Space_KernelPort(Space(), Ipc_Port_kThread)));
}

Kern_SystemCalls_tResult
Ipc_Traps_TaskSelf(const uint64_t* aIn_pArguments)
{
        (void) aIn_pArguments;
        return (Result(Ipc_Space_KernelPort(Space(), Ipc_Port_kTask)));
}

Kern_SystemCalls_tResult
Ipc_Traps_HostSelf(const uint64_t* aIn_pArguments)
{
        (void) aIn_pArguments;
        return (Result(Ipc_Space_KernelPort(Space(), Ipc_Port_kHost)));
}

Kern_SystemCalls_tResult
Ipc_Traps_Msg(const uint64_t* aIn_pArguments)
{
        const uint64_t message = aIn_pArguments[ 0 ];
        const uint32_t options = (uint32_t) aIn_pArguments[ 1 ];

        if((options & IPC_MACH_SEND_MSG) != 0U)
        {
                const int sent = Ipc_Message_Send(Space(), message, (uint32_t) aIn_pArguments[ 2 ]);

                if(sent != IPC_MACH_MSG_SUCCESS)
                        return (Result((uint64_t) sent));
        }
        if((options & IPC_MACH_RCV_MSG) != 0U)
                return (Result((uint64_t) Ipc_Message_Receive(Space(),
                                                              message,
                                                              (uint32_t) aIn_pArguments[ 3 ],
                                                              NameOf(aIn_pArguments[ 4 ]),
                                                              options)));
        return (Result(IPC_MACH_MSG_SUCCESS));
}
