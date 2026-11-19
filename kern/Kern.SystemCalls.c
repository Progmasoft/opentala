/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdint.h>

#include "kern/Kern.Machine.h"
#include "kern/Kern.Print.h"
#include "kern/Kern.Process.h"
#include "kern/Kern.SystemCalls.h"

/** The BSD table's length: up to the highest number OpenTala has. */
#define KERN_SYSTEM_CALLS_BSD_COUNT 5
/** The length of XNU's Mach trap table. */
#define KERN_SYSTEM_CALLS_MACH_COUNT 128
/** How much of a write is copied in at a time. */
#define KERN_SYSTEM_CALLS_CHUNK 256
/** SSIZE_MAX: a write of more cannot report its count. */
#define KERN_SYSTEM_CALLS_SSIZE_MAX 0x7FFFFFFFFFFFFFFFULL

/** A system call: it reads its arguments and fills the result. */
typedef Kern_SystemCalls_tResult (*Kern_SystemCalls_tFn)(const uint64_t* aIn_pArguments);

static Kern_SystemCalls_tResult
Result(int aError, uint64_t aValue)
{
        Kern_SystemCalls_tResult result = { aError, { aValue, 0 } };

        return (result);
}

/** exit(int rval): ends the process with the low 8 bits of rval as its status. */
static Kern_SystemCalls_tResult
Exit(const uint64_t* aIn_pArguments)
{
        Kern_Process_Exit((int) (aIn_pArguments[ 0 ] & 0xFF));
}

/**
 * write(int fd, user_addr_t cbuf, user_size_t nbyte): descriptors 1 and 2 are the console, which
 * is all a process has yet.
 */
static Kern_SystemCalls_tResult
Write(const uint64_t* aIn_pArguments)
{
        const int64_t  descriptor = (int64_t) (int32_t) aIn_pArguments[ 0 ];
        const uint64_t buffer     = aIn_pArguments[ 1 ];
        const uint64_t length     = aIn_pArguments[ 2 ];
        char           chunk[ KERN_SYSTEM_CALLS_CHUNK ];

        if(descriptor != 1 && descriptor != 2)
                return (Result(KERN_ERRNO_BADF, 0));
        if(length > KERN_SYSTEM_CALLS_SSIZE_MAX)
                return (Result(KERN_ERRNO_INVAL, 0));
        for(uint64_t done = 0; done < length;)
        {
                const uint64_t size = length - done < KERN_SYSTEM_CALLS_CHUNK
                        ? length - done
                        : KERN_SYSTEM_CALLS_CHUNK;

                /* Nothing is written from a buffer the process cannot read itself. */
                if(!Kern_Machine_CopyIn(chunk, buffer + done, (size_t) size))
                        return (done == 0 ? Result(KERN_ERRNO_FAULT, 0) : Result(0, done));
                Kern_Machine_ConsoleWrite(chunk, (size_t) size);
                done += size;
        }
        return (Result(0, length));
}

static const Kern_SystemCalls_tFn gskBsd[ KERN_SYSTEM_CALLS_BSD_COUNT ] = {
        [KERN_SYSTEM_CALLS_EXIT]  = Exit,
        [KERN_SYSTEM_CALLS_WRITE] = Write,
};

/** No Mach trap is implemented yet; the table has XNU's length so the numbers stay XNU's. */
static const Kern_SystemCalls_tFn gskMach[ KERN_SYSTEM_CALLS_MACH_COUNT ];

/** The function of a class and number, or NULL. */
static Kern_SystemCalls_tFn
Find(Kern_SystemCalls_tClass aClass, uint32_t aNumber)
{
        if(aClass == Kern_SystemCalls_Class_Bsd && aNumber < KERN_SYSTEM_CALLS_BSD_COUNT)
                return (gskBsd[ aNumber ]);
        if(aClass == Kern_SystemCalls_Class_Mach && aNumber < KERN_SYSTEM_CALLS_MACH_COUNT)
                return (gskMach[ aNumber ]);
        return (NULL);
}

Kern_SystemCalls_tResult
Kern_SystemCalls_Call(Kern_SystemCalls_tClass aClass,
                      uint32_t                aNumber,
                      const uint64_t*         aIn_pArguments)
{
        const Kern_SystemCalls_tFn fCall = Find(aClass, aNumber);

        if(fCall == NULL)
        {
                Kern_Print("process %u: system call 0x%x of class %u is not implemented\n",
                           Kern_Process_Id(),
                           aNumber,
                           (unsigned) aClass);
                Kern_Process_Kill(KERN_SIGNAL_SYS);
        }
        return (fCall(aIn_pArguments));
}
