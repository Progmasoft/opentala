# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MIT
#
# Builds OpenTala with BSD Make (bmake on Linux). OpenKBuild takes this file's place once it exists.
#
#   make                the kernel for amd64, build/amd64/opentala, a Mach-O executable
#   make ARCH=arm64     the kernel for arm64, build/arm64/opentala, and build/arm64/opentala.img, the
#                       same with the Image header that QEMU and other arm64 loaders boot
#   make test           boots it in QEMU and checks what it writes to the serial port
#   make run            boots it in QEMU, the serial port on this terminal
#   make format         checks the layout of every C file with clang-format

ARCH?=		amd64
# The kernel's compiler; CC stays the host's, which sys.mk of BSD Make sets to cc.
KCC?=		clang
LD64?=		ld64.lld
CLANG_FORMAT?=	clang-format

OBJ=		build/${ARCH}
KERNEL=		${OBJ}/opentala

# The Darwin targets give Mach-O objects and the Darwin calling convention. The kernel uses no
# floating point and no library.
COMMON_CFLAGS=	-std=gnu99 -ffreestanding -fno-builtin -nostdlib -nostdlibinc -fno-stack-protector \
		-fno-asynchronous-unwind-tables -fno-unwind-tables -O2 -g -Wall -Wextra -Werror -I.

KERN_SRCS=	kern/Kern.DeviceTree.c kern/Kern.Main.c kern/Kern.PageFrames.c kern/Kern.Print.c \
		kern/Kern.Process.c kern/Kern.SystemCalls.c
KERN_HDRS=	kern/Kern.Boot.h kern/Kern.DeviceTree.h kern/Kern.Machine.h kern/Kern.PageFrames.h \
		kern/Kern.Print.h kern/Kern.Process.h kern/Kern.SystemCalls.h

# The boot code comes first: on amd64 its __text holds the Multiboot header.
AMD64_SRCS=	amd64/Amd64.Boot.S amd64/Amd64.Traps.S amd64/Amd64.UserEntry.S \
		amd64/Amd64.UserProgram.S amd64/Amd64.Cpu.c amd64/Amd64.Machine.c amd64/Amd64.Paging.c \
		amd64/Amd64.Pic.c amd64/Amd64.Serial.c amd64/Amd64.Timer.c amd64/Amd64.Trap.c \
		amd64/Amd64.User.c
AMD64_HDRS=	amd64/Amd64.Cpu.h amd64/Amd64.Multiboot.h amd64/Amd64.Pic.h amd64/Amd64.Port.h \
		amd64/Amd64.Serial.h amd64/Amd64.Timer.h amd64/Amd64.Trap.h amd64/Amd64.User.h
ARM64_SRCS=	arm64/Arm64.Boot.S arm64/Arm64.Vectors.S arm64/Arm64.UserEntry.S \
		arm64/Arm64.UserProgram.S arm64/Arm64.Gic.c arm64/Arm64.Machine.c arm64/Arm64.Paging.c \
		arm64/Arm64.Serial.c arm64/Arm64.Trap.c arm64/Arm64.User.c
ARM64_HDRS=	arm64/Arm64.Gic.h arm64/Arm64.Mmio.h arm64/Arm64.Paging.h arm64/Arm64.Serial.h \
		arm64/Arm64.Trap.h arm64/Arm64.User.h

.if ${ARCH} == "amd64"
QEMU?=		qemu-system-x86_64
ARCH_CFLAGS=	-target x86_64-apple-macos11 -mno-red-zone -mno-mmx -mno-sse -mno-sse2
# __PAGEZERO is 1 MiB, so __TEXT, and the Mach-O header with it, starts at 1 MiB, where a Multiboot
# loader can place it.
KLDFLAGS=	-arch x86_64 -platform_version macos 11.0 11.0 -pagezero_size 0x100000 -no_pie \
		-e Amd64_Boot_Entry64
SRCS=		${AMD64_SRCS} ${KERN_SRCS}
HDRS=		${AMD64_HDRS} ${KERN_HDRS}
BOOTABLE=	${KERNEL}
.elif ${ARCH} == "arm64"
QEMU?=		qemu-system-aarch64
# Only the general registers, and no unaligned access: the code before the MMU is on sees all memory
# as device memory.
ARCH_CFLAGS=	-target arm64-apple-macos11 -mgeneral-regs-only -mstrict-align
# __TEXT starts at 0x40200000, 2 MiB into the RAM of QEMU's virt machine. Classic rebase information
# instead of chained fixups keeps every pointer in the file the address it points to.
KLDFLAGS=	-arch arm64 -platform_version macos 11.0 11.0 -pagezero_size 0x40200000 -no_fixup_chains \
		-e Arm64_Boot_Entry
SRCS=		${ARM64_SRCS} ${KERN_SRCS}
HDRS=		${ARM64_HDRS} ${KERN_HDRS}
BOOTABLE=	${KERNEL}.img
.else
.error ARCH is amd64 or arm64, not ${ARCH}
.endif

KCFLAGS=	${ARCH_CFLAGS} ${COMMON_CFLAGS}
OBJS=

all: ${BOOTABLE}

.for src in ${SRCS}
OBJS+=		${OBJ}/${src:R}.o
${OBJ}/${src:R}.o: ${src} ${HDRS}
	@mkdir -p ${.TARGET:H}
	${KCC} ${KCFLAGS} -c ${src} -o ${.TARGET}
.endfor

${KERNEL}: ${OBJS}
	${LD64} ${KLDFLAGS} -o ${.TARGET} ${OBJS}

${KERNEL}.img: ${KERNEL} tools/arm64-image.sh
	sh tools/arm64-image.sh ${KERNEL} ${.TARGET}

test: ${BOOTABLE}
	QEMU=${QEMU} sh tools/boot-test.sh ${ARCH} ${KERNEL}

run: ${BOOTABLE}
.if ${ARCH} == "amd64"
	${QEMU} -cpu max -kernel ${KERNEL} -serial stdio -display none -no-reboot
.else
	${QEMU} -machine virt,gic-version=2 -cpu max -nic none -m 128 -kernel ${KERNEL}.img -serial stdio \
		-display none -no-reboot
.endif

format:
	${CLANG_FORMAT} --style=file --dry-run --Werror ${AMD64_SRCS:M*.c} ${ARM64_SRCS:M*.c} \
		${KERN_SRCS} ${AMD64_HDRS} ${ARM64_HDRS} ${KERN_HDRS}

clean:
	rm -rf build

.PHONY: all test run format clean
