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
 * BSD system call OpenTala does not have; 2 makes a Mach trap it does not have (100); 3 reads the
 * kernel's memory; 4 checks Mach's ports and messages through the traps libSystem uses. A check
 * that fails exits with its own status, 101 and up.
 */

#define BSD(number)  (0x2000000 + (number))
#define MACH(number) (0x1000000 + (number))
#define SYS_EXIT     BSD(1)
#define SYS_WRITE    BSD(4)
#define EBADF        9
#define EFAULT       14
/** An address of the kernel's: the start of its image. */
#define KERNEL_ADDRESS 0x100000

/* Mach's traps and values, with XNU's numbers. */
#define MACH_PORT_ALLOCATE     MACH(16)
#define MACH_PORT_DEALLOCATE   MACH(18)
#define MACH_PORT_MOD_REFS     MACH(19)
#define MACH_PORT_INSERT_RIGHT MACH(21)
#define MACH_REPLY_PORT        MACH(26)
#define MACH_TASK_SELF         MACH(28)
#define MACH_MSG               MACH(31)
#define RIGHT_RECEIVE          1
#define MOVE_SEND_ONCE         18
#define COPY_SEND              19
#define MAKE_SEND              20
#define MAKE_SEND_ONCE         21
#define SEND_MSG               1
#define RCV_MSG                2
#define KERN_INVALID_NAME      15
#define SEND_INVALID_DEST      0x10000003
#define SEND_TOO_LARGE         0x1000000E
#define RCV_TIMED_OUT          0x10004003
/** "ping-ipc", the body of the first message. */
#define BODY 0x6370692D676E6970
/* The stack holds a message to send at 0, a buffer to receive into at 64, and a name at 256. */
#define SENT     0
#define RECEIVED 64
#define NAME     256

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
        cmpq $4, %rdi
        je MachIpc

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
        movl $MACH(100), %eax
        syscall
        movl $105, %edi
        jmp Exit

ReadKernel:
        movl $KERNEL_ADDRESS, %eax
        movq (%rax), %rax
        movl $106, %edi
        jmp Exit

/*
 * Mach IPC. R12 holds the name of the task's port, R13 a port the program receives on, R14 its reply
 * port and R15 the send-once right a request brings.
 */
MachIpc:
        subq $512, %rsp

        /* task_self_trap() names the task's port. */
        movl $MACH_TASK_SELF, %eax
        syscall
        movl $110, %edi
        testl %eax, %eax
        jz Exit
        movl %eax, %r12d

        /* mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &name). */
        movl $MACH_PORT_ALLOCATE, %eax
        movl %r12d, %edi
        movl $RIGHT_RECEIVE, %esi
        leaq NAME(%rsp), %rdx
        syscall
        movl $111, %edi
        testl %eax, %eax
        jnz Exit
        movl NAME(%rsp), %r13d
        testl %r13d, %r13d
        jz Exit

        /* mach_port_insert_right(task, name, name, MACH_MSG_TYPE_MAKE_SEND). */
        movl $MACH_PORT_INSERT_RIGHT, %eax
        movl %r12d, %edi
        movl %r13d, %esi
        movl %r13d, %edx
        movl $MAKE_SEND, %r10d
        syscall
        movl $112, %edi
        testl %eax, %eax
        jnz Exit

        /* Sends "ping-ipc" with id 0x4F54 to the port, copying the send right. */
        movl $COPY_SEND, SENT(%rsp)
        movl $32, SENT+4(%rsp)
        movl %r13d, SENT+8(%rsp)
        movq $0, SENT+12(%rsp)
        movl $0x4F54, SENT+20(%rsp)
        movabsq $BODY, %rax
        movq %rax, SENT+24(%rsp)
        movl $113, %ebx
        movl $SEND_MSG, %esi
        movl $32, %edx
        call Message
        testl %eax, %eax
        jnz Fail

        /* Receives it: the size, the bits, the port it came to, the id, the body and the trailer. */
        movl $114, %ebx
        movl %r13d, %r8d
        call Receive
        testl %eax, %eax
        jnz Fail
        movl $115, %edi
        cmpl $32, RECEIVED+4(%rsp)
        jne Exit
        movl $116, %edi
        cmpl $0x1100, RECEIVED(%rsp)
        jne Exit
        movl $117, %edi
        cmpl %r13d, RECEIVED+12(%rsp)
        jne Exit
        movl $118, %edi
        cmpl $0x4F54, RECEIVED+20(%rsp)
        jne Exit
        movl $119, %edi
        movabsq $BODY, %rax
        cmpq %rax, RECEIVED+24(%rsp)
        jne Exit
        movl $120, %edi
        cmpl $8, RECEIVED+36(%rsp)
        jne Exit

        /* The queue is empty, and nothing else could fill it: MACH_RCV_TIMED_OUT. */
        movl $121, %ebx
        movl %r13d, %r8d
        call Receive
        cmpl $RCV_TIMED_OUT, %eax
        jne Fail

        /* mach_reply_port() gives a port for the answer. */
        movl $MACH_REPLY_PORT, %eax
        syscall
        movl $122, %edi
        testl %eax, %eax
        jz Exit
        movl %eax, %r14d

        /* A request with a send-once right to the reply port. */
        movl $((MAKE_SEND_ONCE << 8) | COPY_SEND), SENT(%rsp)
        movl $24, SENT+4(%rsp)
        movl %r13d, SENT+8(%rsp)
        movl %r14d, SENT+12(%rsp)
        movl $0, SENT+16(%rsp)
        movl $1, SENT+20(%rsp)
        movl $123, %ebx
        movl $SEND_MSG, %esi
        movl $24, %edx
        call Message
        testl %eax, %eax
        jnz Fail
        movl $124, %ebx
        movl %r13d, %r8d
        call Receive
        testl %eax, %eax
        jnz Fail
        movl $125, %edi
        cmpl $0x1112, RECEIVED(%rsp)
        jne Exit
        movl $126, %edi
        movl RECEIVED+8(%rsp), %r15d
        testl %r15d, %r15d
        jz Exit
        cmpl %r14d, %r15d
        je Exit

        /* The answer goes through the send-once right, which it uses up. */
        movl $MOVE_SEND_ONCE, SENT(%rsp)
        movl %r15d, SENT+8(%rsp)
        movl $0, SENT+12(%rsp)
        movl $2, SENT+20(%rsp)
        movl $127, %ebx
        movl $SEND_MSG, %esi
        movl $24, %edx
        call Message
        testl %eax, %eax
        jnz Fail
        movl $128, %ebx
        movl %r14d, %r8d
        call Receive
        testl %eax, %eax
        jnz Fail
        movl $129, %edi
        cmpl $2, RECEIVED+20(%rsp)
        jne Exit
        movl $130, %edi
        cmpl $0x1200, RECEIVED(%rsp)
        jne Exit
        movl $131, %ebx
        movl $SEND_MSG, %esi
        movl $24, %edx
        call Message
        cmpl $SEND_INVALID_DEST, %eax
        jne Fail

        /* A message over the largest is refused before anything is read. */
        movl $132, %ebx
        movl $SEND_MSG, %esi
        movl $4096, %edx
        call Message
        cmpl $SEND_TOO_LARGE, %eax
        jne Fail

        /* Destroying the receive right kills the port: its send right becomes a dead name. */
        movl $MACH_PORT_MOD_REFS, %eax
        movl %r12d, %edi
        movl %r13d, %esi
        movl $RIGHT_RECEIVE, %edx
        movq $-1, %r10
        syscall
        movl $133, %edi
        testl %eax, %eax
        jnz Exit
        movl $COPY_SEND, SENT(%rsp)
        movl %r13d, SENT+8(%rsp)
        movl $134, %ebx
        movl $SEND_MSG, %esi
        movl $24, %edx
        call Message
        cmpl $SEND_INVALID_DEST, %eax
        jne Fail
        movl $MACH_PORT_DEALLOCATE, %eax
        movl %r12d, %edi
        movl %r13d, %esi
        syscall
        movl $135, %edi
        testl %eax, %eax
        jnz Exit
        movl $MACH_PORT_DEALLOCATE, %eax
        movl %r12d, %edi
        movl %r13d, %esi
        syscall
        movl $136, %edi
        cmpl $KERN_INVALID_NAME, %eax
        jne Exit

        /* A port call for a task that is not the caller's own. */
        movl $MACH_PORT_ALLOCATE, %eax
        movl %r13d, %edi
        movl $RIGHT_RECEIVE, %esi
        leaq NAME(%rsp), %rdx
        syscall
        movl $137, %edi
        cmpl $SEND_INVALID_DEST, %eax
        jne Exit

        /* A message left on the reply port: the kernel destroys it with the task. */
        movl $MAKE_SEND, SENT(%rsp)
        movl %r14d, SENT+8(%rsp)
        movl $138, %ebx
        movl $SEND_MSG, %esi
        movl $24, %edx
        call Message
        testl %eax, %eax
        jnz Fail

        movl $SYS_WRITE, %eax
        movl $1, %edi
        leaq Checked(%rip), %rsi
        movl $(CheckedEnd - Checked), %edx
        syscall
        xorl %edi, %edi
        jmp Exit

Fail:
        movl %ebx, %edi
        jmp Exit

/* mach_msg(&sent, ESI, EDX, 0, 0, 0): the message at SENT, as the caller's frame has it. */
Message:
        movl $MACH_MSG, %eax
        leaq 8+SENT(%rsp), %rdi
        xorl %r10d, %r10d
        xorl %r8d, %r8d
        xorl %r9d, %r9d
        syscall
        ret

/* mach_msg(&received, MACH_RCV_MSG, 0, 64, R8D, 0): into RECEIVED, as the caller's frame has it. */
Receive:
        movl $MACH_MSG, %eax
        leaq 8+RECEIVED(%rsp), %rdi
        movl $RCV_MSG, %esi
        xorl %edx, %edx
        movl $64, %r10d
        xorl %r9d, %r9d
        syscall
        ret

Checked:
        .ascii "user: Mach IPC checked\n"
CheckedEnd:

Greeting:
        .ascii "user: hello from user mode\n"
GreetingEnd:
Machine_UserProgram_End:
