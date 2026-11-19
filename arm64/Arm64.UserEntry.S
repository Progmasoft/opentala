/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

/*
 * The way into and out of EL0 on arm64.
 *
 * Kern_Machine_Enter() keeps the kernel's callee-saved registers and stack pointer in the context,
 * sets EL0's stack, return address and state (EL0t, interrupts open), moves SP_EL1 to the kernel
 * stack of user mode, on which exceptions from EL0 then arrive, zeroes every register but the
 * argument, and goes down with ERET. Kern_Machine_Leave() takes the context back up.
 */

/*
 * int64_t Kern_Machine_Enter(Kern_Machine_tContext* context, uint64_t entry, uint64_t stack,
 *                            uint64_t argument): the context holds X19 to X30 and SP.
 */
        .text
        .globl _Kern_Machine_Enter
        .p2align 2
_Kern_Machine_Enter:
        stp x19, x20, [x0, #0]
        stp x21, x22, [x0, #16]
        stp x23, x24, [x0, #32]
        stp x25, x26, [x0, #48]
        stp x27, x28, [x0, #64]
        stp x29, x30, [x0, #80]
        mov x9, sp
        str x9, [x0, #96]
        msr sp_el0, x2
        msr elr_el1, x1
        msr spsr_el1, xzr
        adrp x9, Arm64_User_KernelStackTop@PAGE
        ldr x9, [x9, Arm64_User_KernelStackTop@PAGEOFF]
        mov sp, x9
        mov x0, x3
        mov x1, xzr
        mov x2, xzr
        mov x3, xzr
        mov x4, xzr
        mov x5, xzr
        mov x6, xzr
        mov x7, xzr
        mov x8, xzr
        mov x9, xzr
        mov x10, xzr
        mov x11, xzr
        mov x12, xzr
        mov x13, xzr
        mov x14, xzr
        mov x15, xzr
        mov x16, xzr
        mov x17, xzr
        mov x18, xzr
        mov x19, xzr
        mov x20, xzr
        mov x21, xzr
        mov x22, xzr
        mov x23, xzr
        mov x24, xzr
        mov x25, xzr
        mov x26, xzr
        mov x27, xzr
        mov x28, xzr
        mov x29, xzr
        mov x30, xzr
        eret

/* void Kern_Machine_Leave(const Kern_Machine_tContext* context, int64_t value) */
        .globl _Kern_Machine_Leave
        .p2align 2
_Kern_Machine_Leave:
        ldp x19, x20, [x0, #0]
        ldp x21, x22, [x0, #16]
        ldp x23, x24, [x0, #32]
        ldp x25, x26, [x0, #48]
        ldp x27, x28, [x0, #64]
        ldp x29, x30, [x0, #80]
        ldr x9, [x0, #96]
        mov sp, x9
        mov x0, x1
        ret
