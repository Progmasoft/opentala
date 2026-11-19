/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

/*
 * The first instructions of OpenTala on amd64.
 *
 * A Multiboot loader, such as the one QEMU runs for -kernel, enters here in 32-bit protected mode
 * with paging off. The header below uses the address fields of Multiboot (flag 16), so the loader
 * needs no ELF: it copies the Mach-O file flat to the address of its __TEXT segment, 1 MiB, and
 * clears the zero-filled sections after it.
 *
 * The code maps the first GiB one to one with 2 MiB pages, turns on long mode and calls
 * Amd64_Main() in 64-bit mode.
 */

#define MULTIBOOT_MAGIC     0x1BADB002
#define MULTIBOOT_ADDRESSES 0x00010000

#define CR0_PE   0x00000001
#define CR0_PG   0x80000000
#define CR4_PAE  0x00000020
#define EFER     0xC0000080
#define EFER_LME 0x00000100

#define PAGE_PRESENT  0x001
#define PAGE_WRITABLE 0x002
#define PAGE_LARGE    0x080

#define SELECTOR_CODE 0x08
#define SELECTOR_DATA 0x10

#define BOOT_STACK_SIZE 16384

        .text

/*
 * The Multiboot header. It must lie in the first 8 KiB of the file; the link puts this object
 * first, so it follows the Mach-O load commands.
 */
        .p2align 2
Amd64_Boot_MultibootHeader:
        .long MULTIBOOT_MAGIC
        .long MULTIBOOT_ADDRESSES
        .long -(MULTIBOOT_MAGIC + MULTIBOOT_ADDRESSES)
        /* header_addr, load_addr, load_end_addr, bss_end_addr, entry_addr */
        .long Amd64_Boot_MultibootHeader
        .long segment$start$__TEXT
        .long section$start$__DATA$__bss
        .long segment$end$__DATA
        .long Amd64_Boot_Entry32

        .code32
Amd64_Boot_Entry32:
        cli
        movl $gsBootStack + BOOT_STACK_SIZE, %esp
        /* The loader's magic value and information pointer, for Kern_Main(). */
        movl %eax, gsBootMagic
        movl %ebx, gsBootInformation

        /* PML4[ 0 ] -> PDPT, PDPT[ 0 ] -> PD. */
        movl $gsBootPdpt + (PAGE_PRESENT | PAGE_WRITABLE), %eax
        movl %eax, gsBootPml4
        movl $gsBootPd + (PAGE_PRESENT | PAGE_WRITABLE), %eax
        movl %eax, gsBootPdpt

        /*
         * PD[ i ] maps 2 MiB at i * 2 MiB, for the first GiB. Mach-O has no relocation for an
         * absolute address with an index, so the table's address goes through EDI.
         */
        movl $gsBootPd, %edi
        xorl %ecx, %ecx
1:
        movl %ecx, %eax
        shll $21, %eax
        orl $(PAGE_PRESENT | PAGE_WRITABLE | PAGE_LARGE), %eax
        movl %eax, (%edi, %ecx, 8)
        incl %ecx
        cmpl $512, %ecx
        jb 1b

        movl $gsBootPml4, %eax
        movl %eax, %cr3
        movl %cr4, %eax
        orl $CR4_PAE, %eax
        movl %eax, %cr4
        movl $EFER, %ecx
        rdmsr
        orl $EFER_LME, %eax
        wrmsr
        movl %cr0, %eax
        orl $(CR0_PG | CR0_PE), %eax
        movl %eax, %cr0

        lgdt Amd64_Boot_GdtDescriptor
        ljmp $SELECTOR_CODE, $Amd64_Boot_Entry64

        .code64
        .globl Amd64_Boot_Entry64
Amd64_Boot_Entry64:
        movw $SELECTOR_DATA, %ax
        movw %ax, %ds
        movw %ax, %es
        movw %ax, %ss
        xorw %ax, %ax
        movw %ax, %fs
        movw %ax, %gs
        leaq gsBootStack + BOOT_STACK_SIZE(%rip), %rsp

        movl gsBootMagic(%rip), %edi
        movl gsBootInformation(%rip), %esi
        callq _Amd64_Main
2:
        cli
        hlt
        jmp 2b

/* Null, 64-bit code and data descriptors, with the accessed bit already set. */
        .p2align 3
Amd64_Boot_Gdt:
        .quad 0
        .quad 0x00AF9B000000FFFF
        .quad 0x00CF93000000FFFF
Amd64_Boot_GdtDescriptor:
        .word Amd64_Boot_GdtDescriptor - Amd64_Boot_Gdt - 1
        .long Amd64_Boot_Gdt

        .zerofill __DATA, __bss, gsBootPml4, 4096, 12
        .zerofill __DATA, __bss, gsBootPdpt, 4096, 12
        .zerofill __DATA, __bss, gsBootPd, 4096, 12
        .zerofill __DATA, __bss, gsBootStack, BOOT_STACK_SIZE, 4
        .zerofill __DATA, __bss, gsBootMagic, 4, 2
        .zerofill __DATA, __bss, gsBootInformation, 4, 2
