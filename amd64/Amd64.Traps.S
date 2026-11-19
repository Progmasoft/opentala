/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

/*
 * The entry points of the exceptions and interrupts on amd64.
 *
 * Each vector below AMD64_CPU_VECTORS has a stub of 16 bytes, one after the other from
 * Amd64_Traps_Stubs, so that Amd64_Cpu_Init() finds the stub of vector v at v * 16 without a
 * table of addresses. A stub pushes 0 where the processor pushes no error code, so that every
 * frame has the same shape, pushes its vector and jumps to the common part. That part saves the
 * general registers, calls Amd64_Trap_Dispatch() with the frame (struct Amd64_Trap_Frame),
 * restores them and returns with IRETQ.
 *
 * The stack is aligned for the call: the processor aligns RSP to 16 bytes before it pushes SS,
 * RSP, RFLAGS, CS and RIP (40 bytes); the error code and the vector make 56, the fifteen registers
 * 176, a multiple of 16.
 */

/*
 * One stub; error is 1 for the exceptions for which the processor pushes an error code: 8, 10 to
 * 14, 17, 21, 29 and 30.
 */
.macro STUB vector, error
        .p2align 4
.if \error == 0
        pushq $0
.endif
        pushq $\vector
        jmp Amd64_Traps_Common
.endm

        .text
        .globl Amd64_Traps_Stubs
        .p2align 4
Amd64_Traps_Stubs:
        STUB 0, 0
        STUB 1, 0
        STUB 2, 0
        STUB 3, 0
        STUB 4, 0
        STUB 5, 0
        STUB 6, 0
        STUB 7, 0
        STUB 8, 1
        STUB 9, 0
        STUB 10, 1
        STUB 11, 1
        STUB 12, 1
        STUB 13, 1
        STUB 14, 1
        STUB 15, 0
        STUB 16, 0
        STUB 17, 1
        STUB 18, 0
        STUB 19, 0
        STUB 20, 0
        STUB 21, 1
        STUB 22, 0
        STUB 23, 0
        STUB 24, 0
        STUB 25, 0
        STUB 26, 0
        STUB 27, 0
        STUB 28, 0
        STUB 29, 1
        STUB 30, 1
        STUB 31, 0
        STUB 32, 0
        STUB 33, 0
        STUB 34, 0
        STUB 35, 0
        STUB 36, 0
        STUB 37, 0
        STUB 38, 0
        STUB 39, 0
        STUB 40, 0
        STUB 41, 0
        STUB 42, 0
        STUB 43, 0
        STUB 44, 0
        STUB 45, 0
        STUB 46, 0
        STUB 47, 0

        .p2align 4
Amd64_Traps_Common:
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
        /* The kernel's C code uses no SSE; the direction flag must be clear for it. */
        cld
        movq %rsp, %rdi
        callq _Amd64_Trap_Dispatch
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
        /* The vector and the error code. */
        addq $16, %rsp
        iretq
