<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# Architecture

## Layout

| Directory | What it holds |
| --- | --- |
| `amd64/` | What is particular to amd64: the boot code and the serial port |
| `kern/` | What does not depend on the machine: the kernel's main code |
| `tools/` | Scripts the build uses, such as the QEMU boot test |

Files are named `<Part>.<Subject>`, as `Amd64.Boot.S` and `Kern.Main.c`.

## Boot on amd64

1. `Amd64.Boot.S` carries a Multiboot header with the a.out-kludge flag, so a Multiboot loader copies the kernel to
   its load addresses without reading Mach-O. The kernel is a Mach-O executable whose `__TEXT` starts at 1 MiB
   (`-pagezero_size 0x100000`).
2. The loader enters in 32-bit protected mode. The boot code builds page tables that map the first GiB, turns on long
   mode and jumps to 64-bit code.
3. `Kern.Main.c` takes over: it writes to the first serial port through `Amd64.Serial.c` and halts.

The addresses the Multiboot header needs come from `segment$start$__TEXT`, because Mach-O has no relocation for an
absolute address in a 32-bit field.
