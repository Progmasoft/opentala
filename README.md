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
- **Security:** security modules are built into the kernel (OTSEM), and loading unsigned kernel modules goes through
  OTMOG. The kernel gives seccomp-BPF, Landlock, Kernel Lockdown, IMA/EVM, an eBPF verifier, Yama, `pledge(2)`,
  `unveil(2)`, `pinsyscalls(2)`, Capsicum, Veriexec, securelevels, security event auditing and RCTL resource limits;
  immutable and execute-only memory, `MAP_CONCEAL`, W^X and NX, ASLR and KASLR, KARL relinking at each boot,
  RETGUARD and stack protectors; the traditional user, group and file permissions, privilege separation, a packet
  filter, and the separation of user and kernel memory.
- **Signing:** Secure Boot is optional and uses keys that Progmasoft manages. Anyone may sign a kernel module.

It is written in GNU99 under the
[Progmasoft Core C Coding Guidelines](https://github.com/Progmasoft/progmasoft-cpp) and built with
[OpenKBuild](https://github.com/Progmasoft/openkbuild). [OpenTalaLoader](https://github.com/Progmasoft/opentalaloader)
boots it.

No code is written yet.

## License

[MIT](LICENSE.txt). Progmasoft's own code is also covered by the [Progmasoft Patent Grant 1.1](PATENTS.md); code
taken from elsewhere keeps its own license and notices. Participation follows the [Code of Conduct](CODE_OF_CONDUCT.md).
