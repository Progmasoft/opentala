/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

/*
 * The SYSCALL entry, and the way into and out of user mode.
 *
 * SYSCALL leaves the user's RIP in RCX and RFLAGS in R11, loads the kernel's CS and SS from STAR
 * and clears the flags FMASK names, interrupts among them, but does not change RSP. With one
 * processor and interrupts off, the user's RSP can wait in Amd64_User_SavedStack while the entry
 * moves to the kernel stack. The frame it builds is the one of Amd64.Traps.S (struct
 * Amd64_Trap_Frame), so the return is the same IRETQ.
 */

#define SELECTOR_KERNEL_DATA 0x10
#define SELECTOR_USER_DATA   0x1B
#define SELECTOR_USER_CODE   0x23
#define SYSCALL_VECTOR       256
/** Interrupts on, and bit 1, which is always set. */
#define USER_RFLAGS 0x202

        .text
        .globl Amd64_User_SystemCallEntry
        .p2align 4
Amd64_User_SystemCallEntry:
        movq %rsp, Amd64_User_SavedStack(%rip)
        movq Amd64_User_KernelStackTop(%rip), %rsp
        pushq $SELECTOR_USER_DATA
        pushq Amd64_User_SavedStack(%rip)
        pushq %r11
        pushq $SELECTOR_USER_CODE
        pushq %rcx
        pushq $0
        pushq $SYSCALL_VECTOR
        pushq %rax
        pushq %rbx
        pushq %rcx
        pushq %rdx
        pushq %rsi
        pushq %rdi
        pushq %rbp
        pushq %r8
        pushq %r9
        pushq %r10
        pushq %r11
        pushq %r12
        pushq %r13
        pushq %r14
        pushq %r15
        cld
        movq %rsp, %rdi
        callq _Amd64_User_SystemCall
        popq %r15
        popq %r14
        popq %r13
        popq %r12
        popq %r11
        popq %r10
        popq %r9
        popq %r8
        popq %rbp
        popq %rdi
        popq %rsi
        popq %rdx
        popq %rcx
        popq %rbx
        popq %rax
        addq $16, %rsp
        iretq

/*
 * int64_t Kern_Machine_Enter(Kern_Machine_tContext* context, uint64_t entry, uint64_t stack,
 *                            uint64_t argument): the context holds RBX, RBP, R12 to R15 and RSP.
 */
        .globl _Kern_Machine_Enter
        .p2align 4
_Kern_Machine_Enter:
        movq %rbx, 0(%rdi)
        movq %rbp, 8(%rdi)
        movq %r12, 16(%rdi)
        movq %r13, 24(%rdi)
        movq %r14, 32(%rdi)
        movq %r15, 40(%rdi)
        movq %rsp, 48(%rdi)
        pushq $SELECTOR_USER_DATA
        pushq %rdx
        pushq $USER_RFLAGS
        pushq $SELECTOR_USER_CODE
        pushq %rsi
        movq %rcx, %rdi
        movw $SELECTOR_USER_DATA, %ax
        movw %ax, %ds
        movw %ax, %es
        xorl %eax, %eax
        xorl %ebx, %ebx
        xorl %ecx, %ecx
        xorl %edx, %edx
        xorl %esi, %esi
        xorl %ebp, %ebp
        xorl %r8d, %r8d
        xorl %r9d, %r9d
        xorl %r10d, %r10d
        xorl %r11d, %r11d
        xorl %r12d, %r12d
        xorl %r13d, %r13d
        xorl %r14d, %r14d
        xorl %r15d, %r15d
        iretq

/* void Kern_Machine_Leave(const Kern_Machine_tContext* context, int64_t value) */
        .globl _Kern_Machine_Leave
        .p2align 4
_Kern_Machine_Leave:
        movw $SELECTOR_KERNEL_DATA, %ax
        movw %ax, %ds
        movw %ax, %es
        movq 0(%rdi), %rbx
        movq 8(%rdi), %rbp
        movq 16(%rdi), %r12
        movq 24(%rdi), %r13
        movq 32(%rdi), %r14
        movq 40(%rdi), %r15
        movq 48(%rdi), %rsp
        movq %rsi, %rax
        retq
