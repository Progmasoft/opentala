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
/** The length of the line the Mach IPC test writes. */
#define CHECKED_LENGTH 23

/* Mach's traps, negative in X16, and its values, with XNU's numbers. */
#define MACH_PORT_ALLOCATE     (-16)
#define MACH_PORT_DEALLOCATE   (-18)
#define MACH_PORT_MOD_REFS     (-19)
#define MACH_PORT_INSERT_RIGHT (-21)
#define MACH_REPLY_PORT        (-26)
#define MACH_TASK_SELF         (-28)
#define MACH_MSG               (-31)
#define RIGHT_RECEIVE          1
#define MOVE_SEND_ONCE         18
#define COPY_SEND              19
#define MAKE_SEND              20
#define MAKE_SEND_ONCE         21
#define SEND_MSG               1
#define RCV_MSG                2
#define KERN_INVALID_NAME      15
/* The stack holds a message to send at 0, a buffer to receive into at 64, and a name at 256. */
#define SENT     0
#define RECEIVED 64
#define NAME     256

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
        cmp x0, #4
        b.eq MachIpc

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
        mov x16, #-100
        svc #0x80
        mov x19, #105
        b Exit

ReadKernel:
        movz x9, #0x4020, lsl #16
        ldr x9, [x9]
        mov x19, #106
        b Exit

/*
 * Mach IPC. X20 holds the name of the task's port, X21 a port the program receives on, X22 its reply
 * port, X23 the send-once right a request brings, and X24 the status of the check under way.
 */
MachIpc:
        sub sp, sp, #512

        /* task_self_trap() names the task's port. */
        mov x16, #MACH_TASK_SELF
        svc #0x80
        mov x19, #110
        cbz w0, Exit
        mov w20, w0

        /* mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &name). */
        mov x16, #MACH_PORT_ALLOCATE
        mov w0, w20
        mov x1, #RIGHT_RECEIVE
        add x2, sp, #NAME
        svc #0x80
        mov x19, #111
        cbnz w0, Exit
        ldr w21, [sp, #NAME]
        cbz w21, Exit

        /* mach_port_insert_right(task, name, name, MACH_MSG_TYPE_MAKE_SEND). */
        mov x16, #MACH_PORT_INSERT_RIGHT
        mov w0, w20
        mov w1, w21
        mov w2, w21
        mov x3, #MAKE_SEND
        svc #0x80
        mov x19, #112
        cbnz w0, Exit

        /* Sends "ping-ipc" with id 0x4F54 to the port, copying the send right. */
        mov w9, #COPY_SEND
        str w9, [sp, #SENT]
        mov w9, #32
        str w9, [sp, #SENT + 4]
        str w21, [sp, #SENT + 8]
        str xzr, [sp, #SENT + 12]
        mov w9, #0x4F54
        str w9, [sp, #SENT + 20]
        bl Body
        str x9, [sp, #SENT + 24]
        mov x24, #113
        mov x1, #SEND_MSG
        mov x2, #32
        bl Message
        cbnz w0, Fail

        /* Receives it: the size, the bits, the port it came to, the id, the body and the trailer. */
        mov x24, #114
        mov w4, w21
        bl Receive
        cbnz w0, Fail
        mov x19, #115
        ldr w9, [sp, #RECEIVED + 4]
        cmp w9, #32
        b.ne Exit
        mov x19, #116
        ldr w9, [sp, #RECEIVED]
        mov w10, #0x1100
        cmp w9, w10
        b.ne Exit
        mov x19, #117
        ldr w9, [sp, #RECEIVED + 12]
        cmp w9, w21
        b.ne Exit
        mov x19, #118
        ldr w9, [sp, #RECEIVED + 20]
        mov w10, #0x4F54
        cmp w9, w10
        b.ne Exit
        mov x19, #119
        bl Body
        ldr x10, [sp, #RECEIVED + 24]
        cmp x9, x10
        b.ne Exit
        mov x19, #120
        ldr w9, [sp, #RECEIVED + 36]
        cmp w9, #8
        b.ne Exit

        /* The queue is empty, and nothing else could fill it: MACH_RCV_TIMED_OUT. */
        mov x24, #121
        mov w4, w21
        bl Receive
        mov w9, #0x4003
        movk w9, #0x1000, lsl #16
        cmp w0, w9
        b.ne Fail

        /* mach_reply_port() gives a port for the answer. */
        mov x16, #MACH_REPLY_PORT
        svc #0x80
        mov x19, #122
        cbz w0, Exit
        mov w22, w0

        /* A request with a send-once right to the reply port. */
        mov w9, #((MAKE_SEND_ONCE << 8) | COPY_SEND)
        str w9, [sp, #SENT]
        mov w9, #24
        str w9, [sp, #SENT + 4]
        str w21, [sp, #SENT + 8]
        str w22, [sp, #SENT + 12]
        str wzr, [sp, #SENT + 16]
        mov w9, #1
        str w9, [sp, #SENT + 20]
        mov x24, #123
        mov x1, #SEND_MSG
        mov x2, #24
        bl Message
        cbnz w0, Fail
        mov x24, #124
        mov w4, w21
        bl Receive
        cbnz w0, Fail
        mov x19, #125
        ldr w9, [sp, #RECEIVED]
        mov w10, #0x1112
        cmp w9, w10
        b.ne Exit
        mov x19, #126
        ldr w23, [sp, #RECEIVED + 8]
        cbz w23, Exit
        cmp w23, w22
        b.eq Exit

        /* The answer goes through the send-once right, which it uses up. */
        mov w9, #MOVE_SEND_ONCE
        str w9, [sp, #SENT]
        str w23, [sp, #SENT + 8]
        str wzr, [sp, #SENT + 12]
        mov w9, #2
        str w9, [sp, #SENT + 20]
        mov x24, #127
        mov x1, #SEND_MSG
        mov x2, #24
        bl Message
        cbnz w0, Fail
        mov x24, #128
        mov w4, w22
        bl Receive
        cbnz w0, Fail
        mov x19, #129
        ldr w9, [sp, #RECEIVED + 20]
        cmp w9, #2
        b.ne Exit
        mov x19, #130
        ldr w9, [sp, #RECEIVED]
        mov w10, #0x1200
        cmp w9, w10
        b.ne Exit
        mov x24, #131
        mov x1, #SEND_MSG
        mov x2, #24
        bl Message
        bl InvalidDest
        cmp w0, w9
        b.ne Fail

        /* A message over the largest is refused before anything is read. */
        mov x24, #132
        mov x1, #SEND_MSG
        mov x2, #4096
        bl Message
        mov w9, #0x000E
        movk w9, #0x1000, lsl #16
        cmp w0, w9
        b.ne Fail

        /* Destroying the receive right kills the port: its send right becomes a dead name. */
        mov x16, #MACH_PORT_MOD_REFS
        mov w0, w20
        mov w1, w21
        mov x2, #RIGHT_RECEIVE
        mov x3, #-1
        svc #0x80
        mov x19, #133
        cbnz w0, Exit
        mov w9, #COPY_SEND
        str w9, [sp, #SENT]
        str w21, [sp, #SENT + 8]
        mov x24, #134
        mov x1, #SEND_MSG
        mov x2, #24
        bl Message
        bl InvalidDest
        cmp w0, w9
        b.ne Fail
        mov x16, #MACH_PORT_DEALLOCATE
        mov w0, w20
        mov w1, w21
        svc #0x80
        mov x19, #135
        cbnz w0, Exit
        mov x16, #MACH_PORT_DEALLOCATE
        mov w0, w20
        mov w1, w21
        svc #0x80
        mov x19, #136
        cmp w0, #KERN_INVALID_NAME
        b.ne Exit

        /* A port call for a task that is not the caller's own. */
        mov x16, #MACH_PORT_ALLOCATE
        mov w0, w21
        mov x1, #RIGHT_RECEIVE
        add x2, sp, #NAME
        svc #0x80
        mov x19, #137
        bl InvalidDest
        cmp w0, w9
        b.ne Exit

        /* A message left on the reply port: the kernel destroys it with the task. */
        mov w9, #MAKE_SEND
        str w9, [sp, #SENT]
        str w22, [sp, #SENT + 8]
        mov x24, #138
        mov x1, #SEND_MSG
        mov x2, #24
        bl Message
        cbnz w0, Fail

        mov x16, #SYS_WRITE
        mov x0, #1
        adr x1, Checked
        mov x2, #CHECKED_LENGTH
        svc #0x80
        mov x19, #0
        b Exit

Fail:
        mov x19, x24
        b Exit

/* mach_msg(&sent, X1, X2, 0, 0, 0): the message at SENT. */
Message:
        mov x16, #MACH_MSG
        add x0, sp, #SENT
        mov x3, #0
        mov x4, #0
        mov x5, #0
        svc #0x80
        ret

/* mach_msg(&received, MACH_RCV_MSG, 0, 64, W4, 0): into RECEIVED. */
Receive:
        mov x16, #MACH_MSG
        add x0, sp, #RECEIVED
        mov x1, #RCV_MSG
        mov x2, #0
        mov x3, #64
        mov x5, #0
        svc #0x80
        ret

/* X9 = "ping-ipc", the body of the first message. */
Body:
        movz x9, #0x6970
        movk x9, #0x676E, lsl #16
        movk x9, #0x692D, lsl #32
        movk x9, #0x6370, lsl #48
        ret

/* W9 = MACH_SEND_INVALID_DEST. */
InvalidDest:
        mov w9, #0x0003
        movk w9, #0x1000, lsl #16
        ret

Checked:
        .ascii "user: Mach IPC checked\n"
CheckedEnd:
.if (CheckedEnd - Checked) != CHECKED_LENGTH
.error "CHECKED_LENGTH is not the line's length"
.endif

Greeting:
        .ascii "user: hello from user mode\n"
GreetingEnd:
.if (GreetingEnd - Greeting) != GREETING_LENGTH
.error "GREETING_LENGTH is not the greeting's length"
.endif
        .p2align 2
Machine_UserProgram_End:
