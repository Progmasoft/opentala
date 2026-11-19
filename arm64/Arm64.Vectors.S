/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

/*
 * The exception vectors of EL1 on arm64.
 *
 * VBAR_EL1 points at a table of 16 entries of 128 bytes, 2 KiB aligned: four sources (EL1 with
 * SP_EL0, EL1 with SP_EL1, EL0 in AArch64, EL0 in AArch32) times four kinds (synchronous, IRQ, FIQ,
 * SError). Each entry makes room for the frame, saves X0 and X1, puts its number in X0 and branches
 * to the common part, which saves the rest (struct Arm64_Trap_Frame), calls Arm64_Trap_Dispatch()
 * and returns with ERET, restoring the return address and state the dispatch may have changed.
 * An exception from EL0 arrives on SP_EL1, the kernel stack Kern_Machine_Enter() left for it.
 */

#define FRAME_SIZE 288

.macro VECTOR number
        .p2align 7
        sub sp, sp, #FRAME_SIZE
        stp x0, x1, [sp, #0]
        mov x0, #\number
        b Arm64_Vectors_Common
.endm

        .text
        .globl Arm64_Vectors
        .p2align 11
Arm64_Vectors:
        VECTOR 0
        VECTOR 1
        VECTOR 2
        VECTOR 3
        VECTOR 4
        VECTOR 5
        VECTOR 6
        VECTOR 7
        VECTOR 8
        VECTOR 9
        VECTOR 10
        VECTOR 11
        VECTOR 12
        VECTOR 13
        VECTOR 14
        VECTOR 15

        .p2align 4
Arm64_Vectors_Common:
        stp x2, x3, [sp, #16]
        stp x4, x5, [sp, #32]
        stp x6, x7, [sp, #48]
        stp x8, x9, [sp, #64]
        stp x10, x11, [sp, #80]
        stp x12, x13, [sp, #96]
        stp x14, x15, [sp, #112]
        stp x16, x17, [sp, #128]
        stp x18, x19, [sp, #144]
        stp x20, x21, [sp, #160]
        stp x22, x23, [sp, #176]
        stp x24, x25, [sp, #192]
        stp x26, x27, [sp, #208]
        stp x28, x29, [sp, #224]
        mrs x1, sp_el0
        stp x30, x1, [sp, #240]
        mrs x1, elr_el1
        mrs x2, spsr_el1
        stp x1, x2, [sp, #256]
        str x0, [sp, #272]
        mov x0, sp
        bl _Arm64_Trap_Dispatch
        ldp x1, x2, [sp, #256]
        msr elr_el1, x1
        msr spsr_el1, x2
        ldp x30, x1, [sp, #240]
        msr sp_el0, x1
        ldp x0, x1, [sp, #0]
        ldp x2, x3, [sp, #16]
        ldp x4, x5, [sp, #32]
        ldp x6, x7, [sp, #48]
        ldp x8, x9, [sp, #64]
        ldp x10, x11, [sp, #80]
        ldp x12, x13, [sp, #96]
        ldp x14, x15, [sp, #112]
        ldp x16, x17, [sp, #128]
        ldp x18, x19, [sp, #144]
        ldp x20, x21, [sp, #160]
        ldp x22, x23, [sp, #176]
        ldp x24, x25, [sp, #192]
        ldp x26, x27, [sp, #208]
        ldp x28, x29, [sp, #224]
        add sp, sp, #FRAME_SIZE
        eret
