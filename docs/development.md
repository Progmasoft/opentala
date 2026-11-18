<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# Development

OpenTala needs BSD Make (`bmake`), Clang, `ld64.lld` and `llvm-otool` from LLVM, and `qemu-system-x86_64`.

```sh
bmake           # build/opentala
bmake test      # boots it in QEMU and checks the serial output (tools/boot-test.sh)
bmake run       # boots it in QEMU, the serial port on this terminal
bmake format    # checks the C files with clang-format 23
```

The kernel's compiler is `KCC`, not `CC`, because the `sys.mk` of BSD Make sets `CC=cc`; output goes to `build/`, as
BSD Make changes into an existing `obj/`.

CI builds the kernel, boots it in QEMU, checks the format, and checks the workflows with zizmor and actionlint.
