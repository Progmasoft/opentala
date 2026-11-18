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

## Console

`Kern.Print.c` has `Kern_Print`, a small printf (`%s %c %d %u %x %p`, `l` and `ll`, width and `0`) on the serial port,
and `Kern_Panic`, which writes `panic: ` and its message and stops the processor.
