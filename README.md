<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# OpenTala

OpenTala is the kernel of TalaOS: an independent kernel with a hybrid architecture, a Mach layer beside a Unix kernel.
It is not derived from Linux or XNU.

- **Unix and POSIX compatibility** comes first.
- **Mach:** the Mach layer is OpenTala's own, written for it rather than taken from XNU's osfmk or another Mach:
  tasks and threads, ports and messages, Mach IPC, and port rights (receive, send and send-once) that a task holds in
  its port name space and passes to another in a message. A name whose port is gone becomes a dead name and is not
  reused, so a task never mistakes a new port for an old one; a port set lets a task wait on many receive rights in
  one call.
- **ABI and formats:** the native ABI is Darwin's (C++ uses the Itanium ABI), and objects and executables are Mach-O.
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

and halts. A fault it does not expect writes the registers and ends in a panic that names it.

```text
OpenTala: amd64, 64-bit mode
OpenTala: loaded by Multiboot
traps: breakpoint handled and resumed
memory: 127 MiB usable, 32447 page frames free below 1 GiB
memory: allocator checked
timer: 10 ticks at 100 Hz
OpenTala: halting
```

`opentala.test=page-fault` on the command line (`-append` in QEMU) writes to an unmapped address and
`opentala.test=double-fault` pushes to an unmapped stack; `bmake test` boots all three ways.

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
