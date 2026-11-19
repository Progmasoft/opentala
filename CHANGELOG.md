<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# Changelog

Versions follow [Semantic Versioning](https://semver.org/).

## Unreleased

- Traps: OpenTala's own GDT, a task state segment whose interrupt stack table gives the double fault, the NMI and
  the machine check stacks of their own, and an IDT of the 32 exceptions and 16 legacy IRQs. A breakpoint is resumed
  after; any other exception writes the registers and panics, naming a page fault's address.
- A page frame allocator over the Multiboot memory map, the first MiB, the kernel and the loader's structures held
  back; a double free panics.
- The timer: the PICs moved to vectors 32 to 47, the PIT at 100 Hz on IRQ 0.
- `Kern_Print` and `Kern_Panic` on the serial port.
- `bmake test` boots three times: normally, with `opentala.test=page-fault` and with `opentala.test=double-fault`.
- The kernel boots on amd64 in QEMU through Multiboot, enters 64-bit mode, reports on the serial port
  and halts; `bmake test` checks it in CI.
- The repository of OpenTala: README, MIT license, Patent Grant and code of conduct.
