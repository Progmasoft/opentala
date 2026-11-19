/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include "amd64/Amd64.Cpu.h"
#include "amd64/Amd64.User.h"
#include "kern/Kern.Machine.h"
#include "kern/Kern.Print.h"
#include "kern/Kern.Process.h"
#include "kern/Kern.SystemCalls.h"

#define AMD64_USER_EFER       0xC0000080U
#define AMD64_USER_STAR       0xC0000081U
#define AMD64_USER_LSTAR      0xC0000082U
#define AMD64_USER_FMASK      0xC0000084U
#define AMD64_USER_EFER_SCE   0x1ULL
#define AMD64_USER_STACK_SIZE 16384

/** The flags of RFLAGS. */
#define AMD64_USER_FLAG_CARRY     0x00001ULL
#define AMD64_USER_FLAG_TRAP      0x00100ULL
#define AMD64_USER_FLAG_INTERRUPT 0x00200ULL
#define AMD64_USER_FLAG_DIRECTION 0x00400ULL
#define AMD64_USER_FLAG_NESTED    0x04000ULL
#define AMD64_USER_FLAG_ALIGNMENT 0x40000ULL
/** The flags user mode may hold: the arithmetic ones, the trap, direction and alignment flags. */
#define AMD64_USER_FLAGS_OWN 0x40DD5ULL
/** Bit 1, always set. */
#define AMD64_USER_FLAG_RESERVED 0x00002ULL

/** The class of a system call is the high byte of RAX's low 32 bits. */
#define AMD64_USER_CLASS_SHIFT 24
#define AMD64_USER_NUMBER_MASK 0xFFFFFFULL

/** The kernel stack of user mode: SYSCALL, interrupts and exceptions from a process run on it. */
static uint8_t gsKernelStack[ AMD64_USER_STACK_SIZE ] __attribute__((aligned(16)));

/* The entry reads these by their assembly names. */
uint64_t    gAmd64_User_KernelStackTop __asm__("Amd64_User_KernelStackTop");
uint64_t    gAmd64_User_SavedStack __asm__("Amd64_User_SavedStack");
extern char gAmd64_User_SystemCallEntry[] __asm__("Amd64_User_SystemCallEntry");

static void
WriteMsr(uint32_t aMsr, uint64_t aValue)
{
        __asm__ volatile("wrmsr"
                         :
                         : "c"(aMsr), "a"((uint32_t) aValue), "d"((uint32_t) (aValue >> 32)));
}

static uint64_t
ReadMsr(uint32_t aMsr)
{
        uint32_t low  = 0;
        uint32_t high = 0;

        __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(aMsr));
        return (((uint64_t) high << 32) | low);
}

void
Kern_Machine_InitUser(void)
{
        gAmd64_User_KernelStackTop = (uint64_t) (uintptr_t) (gsKernelStack + AMD64_USER_STACK_SIZE);
        Amd64_Cpu_SetKernelStack(gAmd64_User_KernelStackTop);
        WriteMsr(AMD64_USER_EFER, ReadMsr(AMD64_USER_EFER) | AMD64_USER_EFER_SCE);
        /*
         * SYSCALL loads CS from bits 32-47 and SS from them plus 8; SYSRET would take its selectors
         * from bits 48-63, plus 8 for SS and plus 16 for 64-bit CS, which is why the user's data
         * descriptor comes before its code descriptor in the GDT.
         */
        WriteMsr(AMD64_USER_STAR,
                 ((uint64_t) AMD64_CPU_SELECTOR_DATA << 48)
                         | ((uint64_t) AMD64_CPU_SELECTOR_CODE << 32));
        WriteMsr(AMD64_USER_LSTAR, (uint64_t) (uintptr_t) gAmd64_User_SystemCallEntry);
        WriteMsr(AMD64_USER_FMASK,
                 AMD64_USER_FLAG_TRAP | AMD64_USER_FLAG_INTERRUPT | AMD64_USER_FLAG_DIRECTION
                         | AMD64_USER_FLAG_NESTED | AMD64_USER_FLAG_ALIGNMENT);
        Kern_Print("user: system calls through SYSCALL, back through IRETQ\n");
}

/** Whether an address is one user mode may return to. */
static int
IsUserAddress(uint64_t aAddress)
{
        return (aAddress >= KERN_MACHINE_USER_START && aAddress < KERN_MACHINE_USER_END);
}

void
Amd64_User_SystemCall(Amd64_Trap_Frame* aInOut_pFrame)
{
        const uint64_t number                                   = aInOut_pFrame->rax;
        const uint64_t arguments[ KERN_SYSTEM_CALLS_ARGUMENTS ] = {
                aInOut_pFrame->rdi, aInOut_pFrame->rsi, aInOut_pFrame->rdx,
                aInOut_pFrame->r10, aInOut_pFrame->r8,  aInOut_pFrame->r9,
        };
        const Kern_SystemCalls_tClass kind
                = (Kern_SystemCalls_tClass) ((number >> AMD64_USER_CLASS_SHIFT) & 0xFF);
        const Kern_SystemCalls_tResult result = Kern_SystemCalls_Call(
                kind,
                (uint32_t) (number & AMD64_USER_NUMBER_MASK),
                arguments);
        uint64_t flags = (aInOut_pFrame->rflags & AMD64_USER_FLAGS_OWN) | AMD64_USER_FLAG_RESERVED
                | AMD64_USER_FLAG_INTERRUPT;

        if(kind == Kern_SystemCalls_Class_Bsd)
        {
                /* Darwin's BSD convention: the carry flag and the errno value on an error. */
                flags &= ~AMD64_USER_FLAG_CARRY;
                if(result.error != 0)
                        flags |= AMD64_USER_FLAG_CARRY;
                aInOut_pFrame->rax = result.error != 0 ? (uint64_t) result.error
                                                       : result.values[ 0 ];
                aInOut_pFrame->rdx = result.error != 0 ? 0 : result.values[ 1 ];
        } else
                aInOut_pFrame->rax = result.values[ 0 ];
        aInOut_pFrame->rflags = flags;
        if(!IsUserAddress(aInOut_pFrame->rip))
        {
                Kern_Print("process %u: returns to %p, outside user memory\n",
                           Kern_Process_Id(),
                           (void*) aInOut_pFrame->rip);
                Kern_Process_Kill(KERN_SIGNAL_SEGV);
        }
}
