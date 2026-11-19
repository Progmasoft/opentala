<!-- SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com> -->
<!-- SPDX-License-Identifier: MIT -->

# Development

OpenTala needs BSD Make (`bmake`), Clang, `ld64.lld`, `llvm-otool` and `llvm-nm` from LLVM, and
`qemu-system-x86_64` or `qemu-system-aarch64`.

```sh
bmake                   # build/amd64/opentala
bmake test              # boots it six times in QEMU and checks the serial output (tools/boot-test.sh)
bmake ARCH=arm64        # build/arm64/opentala and build/arm64/opentala.img, with the Image header
bmake ARCH=arm64 test   # boots it five times on QEMU's virt machine with a GICv2
bmake run               # boots it in QEMU, the serial port on this terminal (ARCH=arm64 too)
bmake format            # checks the C files of both architectures with clang-format 23
```

The kernel's compiler is `KCC`, not `CC`, because the `sys.mk` of BSD Make sets `CC=cc`; output goes to `build/`, as
BSD Make changes into an existing `obj/`. On arm64 the kernel is built with `-mgeneral-regs-only -mstrict-align`: no
floating point, and no unaligned access, since the code before the MMU is on sees all memory as device memory.

CI builds and boots the kernel for amd64 and arm64 in QEMU, checks the format, and checks the workflows with zizmor
and actionlint.
