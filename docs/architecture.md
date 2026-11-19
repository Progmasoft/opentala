<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# Architecture

## Layout

| Directory | What it holds |
| --- | --- |
| `amd64/` | What is particular to amd64: the boot code, the processor's tables, the trap stubs, the PIC, the timer and the serial port |
| `kern/` | What does not depend on the machine: the kernel's main code, its console and panic, and the page frame allocator |
| `tools/` | Scripts the build uses, such as the QEMU boot test |

Files are named `<Part>.<Subject>`, as `Amd64.Boot.S` and `Kern.Main.c`.

## Boot on amd64

1. `Amd64.Boot.S` carries a Multiboot header with the a.out-kludge flag, so a Multiboot loader copies the kernel to
   its load addresses without reading Mach-O. The kernel is a Mach-O executable whose `__TEXT` starts at 1 MiB
   (`-pagezero_size 0x100000`).
2. The loader enters in 32-bit protected mode. The boot code builds page tables that map the first GiB, turns on long
   mode and jumps to 64-bit code.
3. `Kern.Main.c` takes over and sets up the rest, below.

The addresses the Multiboot header needs come from `segment$start$__TEXT`, because Mach-O has no relocation for an
absolute address in a 32-bit field.

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

## User mode and system calls

| File | What it does |
| --- | --- |
| `Amd64.Cpu.c` | The GDT also has the user's data and code descriptors, privilege 3, at `0x18` and `0x20`, in the order SYSCALL and SYSRET need; the TSS's RSP0 is the stack interrupts and exceptions from user mode run on. |
| `Amd64.Paging.c` | User pages from 4 GiB (where Darwin starts a program, after its `__PAGEZERO`) to the end of the lower half less one page, 4 KiB each, their tables from the page frame allocator. The kernel's GiB is never open to user mode. A user page is never both writable and executable; no-execute, SMEP and SMAP are turned on where the processor has them. `Amd64_Paging_CopyIn` checks that every byte is in a present user page before it copies, between STAC and CLAC. |
| `Amd64.UserEntry.S` | The SYSCALL entry: it moves to the kernel stack, builds the frame of the trap stubs and returns with IRETQ, not SYSRET, which faults in kernel mode on the user's stack when the return address is not canonical (CVE-2012-0217 on Intel). `Amd64_User_Enter` goes into user mode with every register but the argument zeroed; `Amd64_User_Leave` comes back to it when the process ends. |
| `Amd64.User.c` | SYSCALL's MSRs (EFER.SCE, STAR, LSTAR, FMASK, which turns interrupts, the trap flag, the direction flag and AC off on entry), and the system call: Darwin's class in bits 24 to 31 of RAX, the arguments in RDI, RSI, RDX, R10, R8, R9; a BSD error sets the carry flag with the errno value in RAX. The flags given back are only those user mode may hold. |
| `Amd64.Trap.c` | An exception in user mode kills the process with the signal XNU gives it (SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP), and the kernel goes on. |
| `Kern.SystemCalls.c` | The BSD table, with XNU's numbers (`exit` 1, `write` 4), and the Mach trap table, with XNU's 128 entries and none implemented yet. A class or number without a function kills the process with SIGSYS, as XNU does. |
| `Kern.Process.c` | One process at a time: its program in a read-only, executable page at 4 GiB, a stack of four writable, non-executable pages below `0x7FF7C0000000`, and every page and table given back when it ends. |
| `Amd64.UserProgram.S` | The test program: it writes a line, checks that `write` gives EBADF and EFAULT with the carry flag, and exits; the command line can make it call what does not exist or read the kernel's memory. |

## Console

`Kern.Print.c` has `Kern_Print`, a small printf (`%s %c %d %u %x %p`, `l` and `ll`, width and `0`) on the serial port,
and `Kern_Panic`, which writes `panic:`, a space and its message, and stops the processor.
