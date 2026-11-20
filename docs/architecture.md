<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# Architecture

## Layout

| Directory | What it holds |
| --- | --- |
| `amd64/` | What is particular to amd64: the boot code, the processor's tables, the trap stubs, the PIC, the timer, the serial port, its page tables and the SYSCALL entry |
| `arm64/` | What is particular to arm64: the boot code, the exception vectors, the GICv2 and the generic timer, the PL011, its page tables and the SVC entry |
| `kern/` | What does not depend on the machine: the kernel's main code, the device tree, its console and panic, the page frame allocator, the heap, processes and system calls |
| `ipc/` | Mach IPC: ports, the spaces that name them, messages and the Mach traps of IPC |
| `tools/` | Scripts the build uses, such as the QEMU boot test |

Files are named `<Part>.<Subject>`, as `Amd64.Boot.S` and `Kern.Main.c`.

`kern/Kern.Machine.h` is what `kern/` asks of an architecture: the console, traps, the breakpoint, the timer, user
pages and copies from user memory, the way into and out of user mode, the tests of its own, and the halt. Each
architecture implements it, most of it in `Amd64.Machine.c` or `Arm64.Machine.c`, whose entry reads its loader's
structures into `Kern_Boot_tInformation` (`kern/Kern.Boot.h`: the RAM, the reserved ranges, the command line) and
calls `Kern_Main`. Nothing in `kern/` includes a header of an architecture.

## Boot on amd64

1. `Amd64.Boot.S` carries a Multiboot header with the a.out-kludge flag, so a Multiboot loader copies the kernel to
   its load addresses without reading Mach-O. The kernel is a Mach-O executable whose `__TEXT` starts at 1 MiB
   (`-pagezero_size 0x100000`).
2. The loader enters in 32-bit protected mode. The boot code builds page tables that map the first GiB, turns on long
   mode and jumps to 64-bit code.
3. `Amd64_Main` (`Amd64.Machine.c`) reads the Multiboot memory map and command line, holding back the first MiB
   and the loader's structures, and calls `Kern_Main`, which sets up the rest, below.

The addresses the Multiboot header needs come from `segment$start$__TEXT`, because Mach-O has no relocation for an
absolute address in a 32-bit field.

## Boot on arm64

1. `tools/arm64-image.sh` puts the 64-byte header of an arm64 Linux Image before the Mach-O kernel, which is linked
   with `__TEXT` at `0x40200000` (`-pagezero_size 0x40200000`, classic rebase information instead of chained
   fixups, so every pointer in the file is its own address). The header's `text_offset` is 2 MiB less the header,
   so a loader that boots Linux images (QEMU's `-kernel`, U-Boot's `booti`) puts the Mach-O file at 2 MiB into RAM,
   and its first word branches to `Arm64_Boot_Entry`. The loader gives the device tree in X0 and the command line in
   its `/chosen/bootargs`.
2. `Arm64.Boot.S` drops from EL2 to EL1 when it starts at EL2, masks interrupts, traps floating point, clears the
   zero-filled sections (the loader copies only the file) and calls `Arm64_Main`.
3. `Arm64_Main` turns the MMU on (`Arm64.Paging.c`), reads the device tree (`kern/Kern.DeviceTree.c`: the RAM of
   `/memory`, the reservation block and the tree itself as reserved, `/chosen/bootargs`; every offset and length is
   checked against the blob), and calls `Kern_Main`.

| File | What it does |
| --- | --- |
| `Arm64.Vectors.S` | The 16 vectors of EL1, 128 bytes each: each saves the registers, EL0's stack pointer, the return address and state (`struct Arm64_Trap_Frame`) and calls `Arm64_Trap_Dispatch`, then returns with ERET. |
| `Arm64.Trap.c` | An IRQ goes to the GIC; a BRK in the kernel is counted and resumed after; an SVC from EL0 is a system call; any other exception from EL0 kills the process with XNU's signal for it; anything else in the kernel writes the registers, ESR and FAR and panics. |
| `Arm64.Gic.c` | The GICv2 of QEMU's virt machine (`gic-version=2`) and the EL1 physical timer at 100 Hz on interrupt 30. |
| `Arm64.Paging.c` | 4 KiB pages, 48-bit addresses, TTBR0 only. The first GiB, virt's devices, is Device-nGnRE and never executable; the second, its RAM, is the kernel's, one to one, which EL0 cannot reach. User pages from 4 GiB are never writable and executable at once, never executable by the kernel (PXN), and PAN is on where the processor has it; `Kern_Machine_CopyIn` checks each page and reads with LDTRB, EL0's permissions. |
| `Arm64.UserEntry.S`, `Arm64.User.c` | `Kern_Machine_Enter` and `Kern_Machine_Leave`, and Darwin's system call: `svc #0x80` (another immediate kills the process with SIGSYS), the number in X16 (positive BSD, negative Mach trap, `0x80000000` machine-dependent), the arguments in X0 to X5; a BSD error sets the carry flag with the errno value in X0. |
| `Arm64.Serial.c` | The PL011 of virt at `0x09000000`. |

## Traps

| File | What it does |
| --- | --- |
| `Amd64.Cpu.c` | OpenTala's GDT with a task state segment, and the IDT. The double fault, the NMI and the machine check use stacks 1 to 3 of the interrupt stack table; every gate is an interrupt gate of privilege 0, except the breakpoint's. |
| `Amd64.Traps.S` | One stub of 16 bytes per vector, so the IDT finds the stub of vector v at 16 v without a table of addresses (Mach-O would need a relocation for each). A stub pushes 0 where the processor pushes no error code, then its vector; the common part saves the registers and calls `Amd64_Trap_Dispatch`. |
| `Amd64.Trap.c` | The dispatch: a breakpoint is counted and resumed, an IRQ goes to its handler and is acknowledged, any other exception writes the registers and panics, with CR2 for a page fault. |
| `Amd64.Pic.c` | The two 8259 PICs, moved to vectors 32 to 47 (their own vectors are those of exceptions), every IRQ masked until a handler asks for it, spurious IRQs 7 and 15 not acknowledged. |
| `Amd64.Timer.c` | The 8254 timer, channel 0, at 100 Hz on IRQ 0. |

## Memory

`Kern.PageFrames.c` keeps one bit per 4 KiB frame of the first GiB, the part the boot code maps. It starts with
every frame in use; `Kern.Main.c` frees what the Multiboot memory map calls available, then holds back the first MiB,
the kernel image (`segment$start$__TEXT` to `segment$end$__DATA`) and the loader's structures. Allocation goes on from
where the last one stopped; freeing a frame that is not in use panics, since it would give one frame to two owners.

`Kern.Memory.c` is the kernel's heap: blocks of 32 to 2048 bytes in powers of two, and blocks of a whole page less
its header, every block aligned to 16 bytes and filled with zeros. A page holds blocks of one size and starts with a
header that names it, so a block finds its page by its address. When the last block of a page is freed, the frame
goes back to the page frame allocator, so the heap keeps nothing it does not use, and `Kern.Main.c` checks after each
process that no block is left. A freed block carries a mark, so freeing it twice panics.

## Mach IPC

| File | What it does |
| --- | --- |
| `Ipc.Mach.h` | Mach's values with XNU's numbers: `kern_return_t`, rights, dispositions, `msgh_bits`, `mach_msg` options and returns, the header and the trailer. |
| `Ipc.Port.c` | Ports: a queue of at most five messages and one receive right. A port is kept while a name, a right in a message or its receive right refers to it; when the receive right is destroyed it dies, and its queue is destroyed. The host's port lives as long as the kernel; a task's and a thread's die with the task. |
| `Ipc.Space.c` | A task's space: 127 names, `(index << 8) \| generation`, each naming one port and the rights to it: a receive right and send rights under one name, counted by user references, a send-once right under a name of its own, or a dead name, what a send right becomes when its port dies. The generation changes when a name is freed, so an old name names nothing. Destroying a space destroys its receive rights, so their ports die, and gives back every other right. |
| `Ipc.Message.c` | `mach_msg`: a header and a body of at most 2048 bytes, copied in whole; the destination and reply rights are taken from the sender's space by their dispositions (`COPY_SEND`, `MOVE_SEND`, `MAKE_SEND`, `MAKE_SEND_ONCE`, `MOVE_SEND_ONCE`); on receive the reply right goes into the receiver's space and the header gives its name, and an 8-byte trailer follows the body. A message too large for the buffer is destroyed, or kept with `MACH_RCV_LARGE`, its size written where the buffer's header has `msgh_size`. |
| `Ipc.Traps.c` | The traps, with XNU's numbers: `_kernelrpc_mach_port_allocate_trap` (16), `_deallocate_trap` (18), `_mod_refs_trap` (19) and `_insert_right_trap` (21), which libSystem calls instead of MIG messages; `mach_reply_port` (26), `thread_self_trap` (27), `task_self_trap` (28), `host_self_trap` (29) and `mach_msg_trap` (31). The target task of a port call must be the caller's own; any other is `MACH_SEND_INVALID_DEST`, as XNU answers. |

Each process has a space, made before it starts and destroyed when it ends. `Kern_Machine_CopyOut` writes to user
memory as `Kern_Machine_CopyIn` reads it: only to pages user mode may write, between STAC and CLAC on amd64 and with
STTRB, EL0's permissions, on arm64.

What OpenTala does not do: it takes no descriptors, so a complex message is refused with `MACH_SEND_INVALID_TYPE`,
and a voucher with `MACH_SEND_INVALID_VOUCHER`; it has no port sets and moves no receive right in a message; its
kernel ports take no message (`MACH_SEND_INVALID_DEST`). A task has one thread, so nothing can wake a send that
would wait for room in a full queue or a receive that would wait for a message: they end with `MACH_SEND_TIMED_OUT`
and `MACH_RCV_TIMED_OUT`, as with a timeout of 0. `mach_msg_trap` reads its first six arguments; the seventh, the
notify port, is not read.

## User mode and system calls

| File | What it does |
| --- | --- |
| `Amd64.Cpu.c` | The GDT also has the user's data and code descriptors, privilege 3, at `0x18` and `0x20`, in the order SYSCALL and SYSRET need; the TSS's RSP0 is the stack interrupts and exceptions from user mode run on. |
| `Amd64.Paging.c` | User pages from 4 GiB (where Darwin starts a program, after its `__PAGEZERO`) to the end of the lower half less one page, 4 KiB each, their tables from the page frame allocator. The kernel's GiB is never open to user mode. A user page is never both writable and executable; no-execute, SMEP and SMAP are turned on where the processor has them. `Kern_Machine_CopyIn` checks that every byte is in a present user page before it copies, between STAC and CLAC. |
| `Amd64.UserEntry.S` | The SYSCALL entry: it moves to the kernel stack, builds the frame of the trap stubs and returns with IRETQ, not SYSRET, which faults in kernel mode on the user's stack when the return address is not canonical (CVE-2012-0217 on Intel). `Kern_Machine_Enter` goes into user mode with every register but the argument zeroed; `Kern_Machine_Leave` comes back to it when the process ends. |
| `Amd64.User.c` | SYSCALL's MSRs (EFER.SCE, STAR, LSTAR, FMASK, which turns interrupts, the trap flag, the direction flag and AC off on entry), and the system call: Darwin's class in bits 24 to 31 of RAX, the arguments in RDI, RSI, RDX, R10, R8, R9; a BSD error sets the carry flag with the errno value in RAX. The flags given back are only those user mode may hold. |
| `Amd64.Trap.c` | An exception in user mode kills the process with the signal XNU gives it (SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP), and the kernel goes on. |
| `Kern.SystemCalls.c` | The BSD table, with XNU's numbers (`exit` 1, `write` 4), and the Mach trap table, with XNU's 128 entries and the traps of IPC. A class or number without a function kills the process with SIGSYS, as XNU does. |
| `Kern.Process.c` | One process at a time: its program in a read-only, executable page at 4 GiB, a stack of four writable, non-executable pages below `0x7FF7C0000000`, and every page and table given back when it ends. |
| `Amd64.UserProgram.S`, `Arm64.UserProgram.S` | The test program: it writes a line, checks that `write` gives EBADF and EFAULT with the carry flag, and exits; the command line can make it call what does not exist, read the kernel's memory, or check Mach IPC through its traps. |

## Console

`Kern.Print.c` has `Kern_Print`, a small printf (`%s %c %d %u %x %p`, `l` and `ll`, width and `0`) on the serial port,
and `Kern_Panic`, which writes `panic:`, a space and its message, and stops the processor.
