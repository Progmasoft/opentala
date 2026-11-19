/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

/*
 * The first instructions of OpenTala on arm64.
 *
 * A loader that boots arm64 Linux images (QEMU's -kernel, U-Boot's booti) jumps to the Image header
 * that tools/arm64-image.sh puts before the kernel, whose first word branches here: the MMU and the
 * data cache off, the device tree's physical address in X0, at EL2 or EL1. The code drops to EL1
 * from EL2, clears the zero-filled sections (the loader copies only the file), takes the boot stack
 * and calls Arm64_Main().
 */

#define CURRENT_EL_EL2 0x8
/** HCR_EL2.RW: EL1 runs in AArch64. */
#define HCR_EL2_RW (1 << 31)
/** EL1h with every interrupt masked. */
#define SPSR_EL1H_MASKED 0x3C5
/** CNTHCTL_EL2: EL1 may read the physical counter and use the physical timer. */
#define CNTHCTL_EL1_ACCESS 0x3

#define BOOT_STACK_SIZE 16384

        .text
        .globl Arm64_Boot_Entry
        .p2align 2
Arm64_Boot_Entry:
        mov x19, x0
        mrs x9, CurrentEL
        cmp x9, #CURRENT_EL_EL2
        b.ne 1f
        mov x9, #HCR_EL2_RW
        msr hcr_el2, x9
        mov x9, #CNTHCTL_EL1_ACCESS
        msr cnthctl_el2, x9
        msr cntvoff_el2, xzr
        mov x9, #SPSR_EL1H_MASKED
        msr spsr_el2, x9
        adr x9, 1f
        msr elr_el2, x9
        eret
1:
        msr daifset, #0xF
        /* SCTLR_EL1's bits that are 1 on reset in every version, the MMU and the caches off. */
        mov x9, #0x0800
        movk x9, #0x30D0, lsl #16
        msr sctlr_el1, x9
        /* Floating point and SIMD trap at EL1 and EL0: the kernel uses neither. */
        msr cpacr_el1, xzr
        isb

        adrp x9, section$start$__DATA$__bss@PAGE
        add x9, x9, section$start$__DATA$__bss@PAGEOFF
        adrp x10, segment$end$__DATA@PAGE
        add x10, x10, segment$end$__DATA@PAGEOFF
2:
        cmp x9, x10
        b.hs 3f
        strb wzr, [x9], #1
        b 2b
3:
        adrp x9, gsBootStack@PAGE
        add x9, x9, gsBootStack@PAGEOFF
        add x9, x9, #BOOT_STACK_SIZE
        mov sp, x9
        mov x0, x19
        bl _Arm64_Main
4:
        wfi
        b 4b

        .zerofill __DATA, __bss, gsBootStack, BOOT_STACK_SIZE, 4
