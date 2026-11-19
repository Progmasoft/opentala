/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MIT
 */

/*
 * The test program the kernel runs in EL0 on arm64. The kernel copies the bytes from
 * Machine_UserProgram_Start to Machine_UserProgram_End into a process, so the code reaches its data
 * only relative to the PC. It makes Darwin's system calls the way libSystem's stubs do: the number
 * in X16, the arguments in X0 to X2, `svc #0x80`, and the carry flag for an error.
 *
 * X0 picks the test: 0 writes a line and checks the errors of write, then exits with 0; 1 makes a
 * BSD system call OpenTala does not have; 2 makes a Mach trap it does not have (task_self_trap,
 * -28); 3 reads the kernel's memory. A check that fails exits with its own status, 101 and up.
 */

#define SYS_EXIT  1
#define SYS_WRITE 4
#define EBADF     9
#define EFAULT    14
/** The greeting's length; checked against its labels at the end. */
#define GREETING_LENGTH 27

        .text
        .globl Machine_UserProgram_Start
        .globl Machine_UserProgram_End
        .p2align 2
Machine_UserProgram_Start:
        cmp x0, #1
        b.eq UnknownBsd
        cmp x0, #2
        b.eq UnknownMach
        cmp x0, #3
        b.eq ReadKernel

        /* write(1, greeting, length) writes it all. */
        mov x16, #SYS_WRITE
        mov x0, #1
        adr x1, Greeting
        mov x2, #GREETING_LENGTH
        svc #0x80
        mov x19, #101
        b.cs Exit
        cmp x0, #GREETING_LENGTH
        b.ne Exit

        /* write(3, ...) is EBADF: the process has no descriptor 3. */
        mov x16, #SYS_WRITE
        mov x0, #3
        adr x1, Greeting
        mov x2, #1
        svc #0x80
        mov x19, #102
        b.cc Exit
        cmp x0, #EBADF
        b.ne Exit

        /* write(1, kernel address, 4) is EFAULT: nothing is copied the process may not read. */
        mov x16, #SYS_WRITE
        mov x0, #1
        movz x1, #0x4020, lsl #16
        mov x2, #4
        svc #0x80
        mov x19, #103
        b.cc Exit
        cmp x0, #EFAULT
        b.ne Exit

        mov x19, #0
Exit:
        mov x0, x19
        mov x16, #SYS_EXIT
        svc #0x80
        brk #1

UnknownBsd:
        mov x16, #999
        svc #0x80
        mov x19, #104
        b Exit

UnknownMach:
        mov x16, #-28
        svc #0x80
        mov x19, #105
        b Exit

ReadKernel:
        movz x9, #0x4020, lsl #16
        ldr x9, [x9]
        mov x19, #106
        b Exit

Greeting:
        .ascii "user: hello from user mode\n"
GreetingEnd:
.if (GreetingEnd - Greeting) != GREETING_LENGTH
.error "GREETING_LENGTH is not the greeting's length"
.endif
        .p2align 2
Machine_UserProgram_End:
