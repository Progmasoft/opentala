/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

/*
 * The test program the kernel runs in user mode. The kernel copies the bytes from
 * Machine_UserProgram_Start to Machine_UserProgram_End into a process, so the code reaches its data
 * only relative to RIP. It makes Darwin's system calls the way libSystem's stubs do: the class and
 * number in RAX, the arguments in RDI, RSI and RDX, `syscall`, and the carry flag for an error.
 *
 * RDI picks the test: 0 writes a line and checks the errors of write, then exits with 0; 1 makes a
 * BSD system call OpenTala does not have; 2 makes a Mach trap it does not have (task_self_trap,
 * 28); 3 reads the kernel's memory. A check that fails exits with its own status, 101 and up.
 */

#define BSD(number)  (0x2000000 + (number))
#define MACH(number) (0x1000000 + (number))
#define SYS_EXIT     BSD(1)
#define SYS_WRITE    BSD(4)
#define EBADF        9
#define EFAULT       14
/** An address of the kernel's: the start of its image. */
#define KERNEL_ADDRESS 0x100000

        .text
        .globl Machine_UserProgram_Start
        .globl Machine_UserProgram_End
        .p2align 4
Machine_UserProgram_Start:
        cmpq $1, %rdi
        je UnknownBsd
        cmpq $2, %rdi
        je UnknownMach
        cmpq $3, %rdi
        je ReadKernel

        /* write(1, greeting, length) writes it all. */
        movl $SYS_WRITE, %eax
        movl $1, %edi
        leaq Greeting(%rip), %rsi
        movl $(GreetingEnd - Greeting), %edx
        syscall
        movl $101, %edi
        jc Exit
        cmpq $(GreetingEnd - Greeting), %rax
        jne Exit

        /* write(3, ...) is EBADF: the process has no descriptor 3. */
        movl $SYS_WRITE, %eax
        movl $3, %edi
        leaq Greeting(%rip), %rsi
        movl $1, %edx
        syscall
        movl $102, %edi
        jnc Exit
        cmpq $EBADF, %rax
        jne Exit

        /* write(1, kernel address, 4) is EFAULT: nothing is copied the process may not read. */
        movl $SYS_WRITE, %eax
        movl $1, %edi
        movl $KERNEL_ADDRESS, %esi
        movl $4, %edx
        syscall
        movl $103, %edi
        jnc Exit
        cmpq $EFAULT, %rax
        jne Exit

        xorl %edi, %edi
Exit:
        movl $SYS_EXIT, %eax
        syscall
        ud2

UnknownBsd:
        movl $BSD(999), %eax
        syscall
        movl $104, %edi
        jmp Exit

UnknownMach:
        movl $MACH(28), %eax
        syscall
        movl $105, %edi
        jmp Exit

ReadKernel:
        movl $KERNEL_ADDRESS, %eax
        movq (%rax), %rax
        movl $106, %edi
        jmp Exit

Greeting:
        .ascii "user: hello from user mode\n"
GreetingEnd:
Machine_UserProgram_End:
