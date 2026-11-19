# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MIT
#
# Builds OpenTala for amd64 with BSD Make (bmake on Linux). OpenKBuild takes this file's place once
# it exists.
#
#   make          the kernel, build/opentala, a Mach-O executable
#   make test     boots it in QEMU and checks what it writes to the serial port
#   make run      boots it in QEMU, the serial port on this terminal
#   make format   checks the layout of the C files with clang-format

# The kernel's compiler; CC stays the host's, which sys.mk of BSD Make sets to cc.
KCC?=		clang
LD64?=		ld64.lld
QEMU?=		qemu-system-x86_64
CLANG_FORMAT?=	clang-format

OBJ=		build
KERNEL=		${OBJ}/opentala

# The Darwin target gives Mach-O objects and the Darwin calling convention. The kernel uses no
# floating point, no red zone and no library.
TARGET=		-target x86_64-apple-macos11
KCFLAGS=	${TARGET} -std=gnu99 -ffreestanding -fno-builtin -nostdlib -nostdlibinc \
		-mno-red-zone -mno-mmx -mno-sse -mno-sse2 -fno-stack-protector \
		-fno-asynchronous-unwind-tables -fno-unwind-tables -O2 -g \
		-Wall -Wextra -Werror -I.
# __PAGEZERO is 1 MiB, so __TEXT, and the Mach-O header with it, starts at 1 MiB, where a
# Multiboot loader can place it.
KLDFLAGS=	-arch x86_64 -platform_version macos 11.0 11.0 -pagezero_size 0x100000 -no_pie \
		-e Amd64_Boot_Entry64

# The boot code comes first: its __text holds the Multiboot header.
SRCS=		amd64/Amd64.Boot.S amd64/Amd64.Traps.S amd64/Amd64.UserEntry.S \
		amd64/Amd64.UserProgram.S amd64/Amd64.Cpu.c amd64/Amd64.Paging.c amd64/Amd64.Pic.c \
		amd64/Amd64.Serial.c amd64/Amd64.Timer.c amd64/Amd64.Trap.c amd64/Amd64.User.c \
		kern/Kern.Main.c kern/Kern.PageFrames.c kern/Kern.Print.c kern/Kern.Process.c \
		kern/Kern.SystemCalls.c
HDRS=		amd64/Amd64.Cpu.h amd64/Amd64.Multiboot.h amd64/Amd64.Paging.h amd64/Amd64.Pic.h \
		amd64/Amd64.Port.h amd64/Amd64.Serial.h amd64/Amd64.Timer.h amd64/Amd64.Trap.h \
		amd64/Amd64.User.h kern/Kern.Main.h kern/Kern.PageFrames.h kern/Kern.Print.h \
		kern/Kern.Process.h kern/Kern.SystemCalls.h
OBJS=

all: ${KERNEL}

.for src in ${SRCS}
OBJS+=		${OBJ}/${src:R}.o
${OBJ}/${src:R}.o: ${src} ${HDRS}
	@mkdir -p ${.TARGET:H}
	${KCC} ${KCFLAGS} -c ${src} -o ${.TARGET}
.endfor

${KERNEL}: ${OBJS}
	${LD64} ${KLDFLAGS} -o ${.TARGET} ${OBJS}

test: ${KERNEL}
	QEMU=${QEMU} sh tools/boot-test.sh ${KERNEL}

run: ${KERNEL}
	${QEMU} -cpu max -kernel ${KERNEL} -serial stdio -display none -no-reboot

format:
	${CLANG_FORMAT} --style=file --dry-run --Werror ${SRCS:M*.c} ${HDRS}

clean:
	rm -rf ${OBJ}

.PHONY: all test run format clean
