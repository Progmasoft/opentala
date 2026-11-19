<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# Changelog

Versions follow [Semantic Versioning](https://semver.org/).

## Unreleased

- arm64 on QEMU's virt machine: an Image header before the Mach-O kernel, so QEMU's `-kernel` and other arm64 Linux
  loaders boot it; EL2 to EL1, exception vectors, the GICv2 and the generic timer, the PL011, the MMU with the
  kernel's RAM closed to EL0, PXN and UXN, PAN, and Darwin's `svc #0x80` with X16, the carry flag and errno.
  `bmake ARCH=arm64 test` boots five times, as amd64 does.
- The device tree is read for the RAM, the reserved ranges and the command line, with every offset checked.
- `kern/Kern.Machine.h` is what the kernel asks of an architecture; nothing in `kern/` includes an
  architecture's header any more. The page frame allocator covers the first GiB of RAM wherever RAM starts.
- User mode on amd64: user descriptors in the GDT, the kernel stack in the TSS, user pages from 4 GiB with
  no-execute, SMEP and SMAP, and the kernel's memory closed to user mode.
- Darwin's system call ABI: `syscall` with the class in RAX's high byte, the BSD table with XNU's numbers (`exit`,
  `write`), the Mach trap table, the carry flag and errno on an error, SIGSYS for what is not there. The return is
  through IRETQ.
- An exception in a process kills the process with its signal instead of panicking the kernel.
- `bmake test` boots six times: a process writes and exits, a missing BSD call and a missing Mach trap end in
  SIGSYS, and a read of kernel memory ends in SIGSEGV.
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
