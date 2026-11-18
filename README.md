<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# OpenTala

OpenTala is the kernel of TalaOS: an independent kernel with a hybrid architecture, a Mach layer beside a Unix kernel.
It is not derived from Linux or XNU.

- **Unix and POSIX compatibility** comes first.
- **ABI and formats:** the native ABI is Darwin's (C++ uses the Itanium ABI), and objects and executables are Mach-O.
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

The kernel boots on amd64 in QEMU: it enters through Multiboot in 32-bit mode, maps the first GiB, switches to
64-bit mode, writes three lines to the first serial port and halts.

```text
OpenTala: amd64, 64-bit mode
OpenTala: loaded by Multiboot
OpenTala: halting
```

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
