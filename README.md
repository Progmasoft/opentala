<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# OpenTala

OpenTala is the kernel of TalaOS: an independent kernel with a hybrid architecture, a Mach layer beside a Unix kernel.
It is not derived from Linux or XNU.

- **Unix and POSIX compatibility** comes first. The POSIX it follows is
  [POSIX.1-2024](https://pubs.opengroup.org/onlinepubs/9799919799/) (IEEE Std 1003.1-2024, Issue 8).
- **Mach:** the Mach layer is OpenTala's own, written for it rather than taken from XNU's osfmk or another Mach:
  tasks and threads, ports and messages, Mach IPC, and port rights (receive, send and send-once) that a task holds in
  its port name space and passes to another in a message. A name whose port is gone becomes a dead name and is not
  reused, so a task never mistakes a new port for an old one; a port set lets a task wait on many receive rights in
  one call.
- **ABI and formats:** the native ABI is Darwin's (C++ uses the Itanium ABI), and objects and executables are Mach-O.
- **System calls:** the system call ABI is Darwin's too: the BSD system calls and Mach traps keep XNU's numbers,
  entered by `syscall` on amd64 (the class in the high byte of `rax`) and `svc #0x80` on arm64 (the number in `x16`),
  with the carry flag set and the errno value returned on an error. The numbers and conventions are taken from the
  documented interface; no code of XNU is.
  `long double` is the x87 80-bit extended format on amd64 and IEEE 754 binary64 on aarch64.
  The usual extensions are kept: `.dylib`, `.so`, `.a`, `.framework`, `.bundle`, `.kext`, `.systemextension`, `.xpc`,
  `.tbd` and `.plist`.
- **Drivers:** IOKit and DriverKit, written in GNU99 and shaped by Progmasoft's own tools; they are not the same as
  XNU's.
- **Security:** OTSEM, the OpenTala Security Modules, builds security modules into the kernel, and OTMOG, the
  OpenTala Module Gate, decides how unsigned kernel modules are loaded. The kernel gives seccomp-BPF, Landlock, Kernel Lockdown, IMA/EVM, an eBPF verifier, Yama, `pledge(2)`,
  `unveil(2)`, `pinsyscalls(2)`, Capsicum, Veriexec, securelevels, security event auditing and RCTL resource limits;
  immutable and execute-only memory, `MAP_CONCEAL`, W^X and NX, ASLR and KASLR, KARL relinking at each boot,
  RETGUARD and stack protectors; the traditional user, group and file permissions, privilege separation, a packet
  filter, and the separation of user and kernel memory.
- **Module signing:** anyone may sign a kernel module.

It is written in GNU99 under the
[Progmasoft Core C Coding Guidelines](https://github.com/Progmasoft/progmasoft-cpp) and is to be built with
[OpenKBuild](https://github.com/Progmasoft/openkbuild); until OpenKBuild exists, a BSD Makefile builds it.

## What it does

The kernel boots on amd64 in QEMU: it enters through Multiboot in 32-bit mode, maps the first GiB and switches to
64-bit mode. Then it

- loads its own GDT, a task state segment and an IDT of the 32 exceptions and 16 legacy IRQs; the double fault,
  the NMI and the machine check run on stacks of their own, so a fault on a broken stack is still reported;
- takes a breakpoint and goes on after it;
- reads the loader's memory map into a page frame allocator, with the first MiB, the kernel and the loader's
  structures held back, and checks it;
- starts the timer at 100 Hz through the PIC and the PIT, waits for ten ticks,
- turns on no-execute pages, SMEP and SMAP, and runs a process in user mode, which writes a line and exits through
  Darwin's system calls (`syscall`, XNU's numbers, the carry flag on an error) and gives every page back,

and halts. A fault it does not expect writes the registers and ends in a panic that names it.

```text
OpenTala: amd64, 64-bit mode
OpenTala: loaded by Multiboot
traps: breakpoint handled and resumed
memory: 127 MiB usable, 32442 page frames free
memory: allocator checked
timer: 10 ticks at 100 Hz
paging: no-execute on, SMEP on, SMAP on
user: system calls through SYSCALL, back through IRETQ
user: hello from user mode
process 1: exited with status 0
process 1: every page given back
OpenTala: halting
```

The line `user: hello from user mode` is written by a process in user mode through Darwin's `write` system call.
On the command line (`-append` in QEMU), `opentala.test=page-fault` writes to an unmapped address and
`opentala.test=double-fault` pushes to an unmapped stack, each ending in a panic; `opentala.test=unknown-system-call`
and `opentala.test=mach-trap` make the process call what OpenTala does not have, and `opentala.test=user-fault`
makes it read the kernel's memory, each killing only the process. `bmake test` boots all six ways.

On arm64 it boots on QEMU's virt machine (`bmake ARCH=arm64 run`) through the header of an arm64 Linux Image, so
QEMU's `-kernel` and other loaders of such images start it with the device tree, at EL2 or EL1. It drops to EL1, turns
the MMU on with the kernel's RAM closed to EL0, reads the RAM and the command line from the device tree, takes a
breakpoint (`brk`), starts the generic timer through the GICv2, turns PAN on where the processor has it, and runs the
same process in EL0 through `svc #0x80` with the number in X16:

```text
OpenTala: arm64, EL1
OpenTala: loaded by a device tree
traps: breakpoint handled and resumed
memory: 128 MiB usable, 32480 page frames free
memory: allocator checked
timer: 10 ticks at 100 Hz
paging: execute-never, PAN on
user: system calls through SVC #0x80, back through ERET
user: hello from user mode
process 1: exited with status 0
process 1: every page given back
OpenTala: halting
```

`bmake ARCH=arm64 test` boots it the same five ways as amd64, the double fault aside, which arm64 does not have.

The kernel is a Mach-O executable whose `__TEXT` segment starts at 1 MiB. Its Multiboot header gives the load
addresses itself, so a Multiboot loader copies the file without reading Mach-O.

## Building and running

It needs BSD Make (`bmake` on Linux), Clang and `ld64.lld` from LLVM, `llvm-otool`, and `qemu-system-x86_64`.

```sh
bmake           # build/opentala
bmake test      # boots it in QEMU and checks the serial output
bmake run       # boots it in QEMU, the serial port on this terminal
bmake format    # checks the C files with clang-format 23
```

## License

[MIT](LICENSE.txt), also covered by the [Progmasoft Patent Grant 1.1](PATENTS.md). Participation follows the
[Code of Conduct](CODE_OF_CONDUCT.md).
