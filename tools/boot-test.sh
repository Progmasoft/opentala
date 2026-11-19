#!/bin/sh
# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MIT
#
# Usage: boot-test.sh <amd64|arm64> <kernel>
#
# Checks that the Mach-O layout of <kernel> can be loaded flat, then boots it in QEMU, with 128 MiB,
# once for each test, and checks what it writes to the serial port. amd64 boots <kernel> through
# Multiboot; arm64 boots <kernel>.img, the kernel behind its Image header, on the virt machine with
# a GICv2. QEMU runs with -cpu max, so that every protection the kernel turns on where the processor
# has it (no-execute, SMEP and SMAP on amd64, PAN on arm64) is there.
#
# Every boot runs a process in user mode, then:
#
#   - without a command line, the process writes a line through Darwin's write system call and
#     exits, and the kernel halts;
#   - with opentala.test=unknown-system-call or mach-trap, the process is killed by SIGSYS;
#   - with opentala.test=user-fault, its read of kernel memory kills it with SIGSEGV;
#   - with opentala.test=page-fault, the kernel writes to an unmapped address and panics, naming
#     the fault and the address;
#   - on amd64, with opentala.test=double-fault, a push to an unmapped stack ends in a panic that
#     names the double fault, which only its own stack (the IST) can report.
#
# Each expected line is an extended regular expression that must match one line of the output,
# in order.

set -eu

arch=$1
kernel=$2
otool=${OTOOL:-llvm-otool}
case $arch in
amd64)
        qemu=${QEMU:-qemu-system-x86_64}
        base=$((0x100000))
        ;;
arm64)
        qemu=${QEMU:-qemu-system-aarch64}
        base=$((0x40200000))
        ;;
*)
        echo "boot-test: the architecture is amd64 or arm64, not $arch"
        exit 2
        ;;
esac

# The loader puts the file in memory as it is, so each segment with file contents must sit at its
# file offset from the base.
segments=$("$otool" -l "$kernel" | awk '
        $1 == "segname" { segment = $2 }
        $1 == "vmaddr" { vmaddr = $2 }
        $1 == "fileoff" { fileoff = $2 }
        $1 == "filesize" { print segment, vmaddr, fileoff, $2 }')
bad=0
while read -r segment vmaddr fileoff filesize; do
        if [ "$filesize" -gt 0 ] && [ "$segment" != "__LINKEDIT" ] \
                && [ $((vmaddr - fileoff)) -ne "$base" ]; then
                echo "boot-test: $segment is at $vmaddr, file offset $fileoff"
                bad=1
        fi
done <<EOF
$segments
EOF
[ "$bad" -eq 0 ]

serial=$(mktemp)
log=$(mktemp)
pid=
trap 'rm -f "$serial" "$log"; [ -z "$pid" ] || kill "$pid" 2>/dev/null || true' EXIT

# qemu <command line>: starts QEMU in the background with the serial port in $serial.
qemu() {
        if [ "$arch" = amd64 ]; then
                "$qemu" -cpu max -kernel "$kernel" -append "$1" -m 128 -serial "file:$serial" \
                        -display none -no-reboot -monitor none 2>"$log" &
        else
                # No network card: the kernel has no use for one, and virt's default needs a ROM file.
                "$qemu" -machine virt,gic-version=2 -cpu max -nic none -kernel "$kernel.img" \
                        -append "$1" -m 128 -serial "file:$serial" -display none -no-reboot \
                        -monitor none 2>"$log" &
        fi
        pid=$!
}

# boot <name> <command line> <last line> <expected lines>
boot() {
        name=$1
        line=$2
        last=$3
        expected=$4
        : >"$serial"
        qemu "$line"
        for _ in $(seq 1 60); do
                if grep -Eq "$last" "$serial" || ! kill -0 "$pid" 2>/dev/null; then
                        break
                fi
                sleep 0.5
        done
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
        pid=

        actual=$(tr -d '\r' <"$serial")
        remaining=$actual
        missing=
        while IFS= read -r pattern; do
                [ -n "$pattern" ] || continue
                # The first line from here on that matches; the lines before it are skipped.
                found=$(printf '%s\n' "$remaining" | grep -En -m 1 -- "$pattern" | cut -d: -f1)
                if [ -z "$found" ]; then
                        missing=$pattern
                        break
                fi
                remaining=$(printf '%s\n' "$remaining" | tail -n +"$((found + 1))")
        done <<EOF
$expected
EOF
        if [ -n "$missing" ]; then
                echo "boot-test: $arch: $name: no line matched: $missing"
                echo "boot-test: $arch: $name: QEMU said:"
                cat "$log"
                echo "boot-test: $arch: $name: the serial port showed:"
                printf '%s\n' "$actual"
                exit 1
        fi
        echo "boot-test: $arch: $name: passed"
}

if [ "$arch" = amd64 ]; then
        start='^OpenTala: amd64, 64-bit mode$
^OpenTala: loaded by Multiboot$'
        memory='^memory: 12[0-9] MiB usable, [0-9]+ page frames free$'
        protection='^paging: no-execute on, SMEP on, SMAP on$
^user: system calls through SYSCALL, back through IRETQ$'
        user_fault='^process 1: page fault at 0x00000001000000[0-9a-f]{2}, address 0x0000000000100000, error 5$'
        page_fault='^  rip [0-9a-f]{16} cs 0008 rflags [0-9a-f]{8} ss 0010$
^panic: page fault at 0x[0-9a-f]{16}, address 0x0000000040000000, error 2$'
        unmapped=0x0000000040000000
else
        start='^OpenTala: arm64, EL1$
^OpenTala: loaded by a device tree$'
        memory='^memory: 128 MiB usable, [0-9]+ page frames free$'
        protection='^paging: execute-never, PAN on$
^user: system calls through SVC #0x80, back through ERET$'
        user_fault='^process 1: data abort at 0x00000001000000[0-9a-f]{2}, address 0x0000000040200000, ESR 9200000d$'
        page_fault='^  elr [0-9a-f]{16} spsr [0-9a-f]{8} esr [0-9a-f]{8} far 0000000080000000$
^panic: data abort at 0x[0-9a-f]{16}, address 0x0000000080000000, ESR 96000045$'
        unmapped=0x0000000080000000
fi

start="$start
^traps: breakpoint handled and resumed$
$memory
^memory: allocator checked$
^timer: 10 ticks at 100 Hz$
$protection"
halt='^process 1: every page given back$
^OpenTala: halting$'

boot "halt" "" '^OpenTala: halting' "$start
^user: hello from user mode$
^process 1: exited with status 0$
$halt"

boot "unknown system call" "opentala.test=unknown-system-call" '^OpenTala: halting' "$start
^process 1: system call 0x3e7 of class 2 is not implemented$
^process 1: killed by SIGSYS$
$halt"

boot "Mach trap" "opentala.test=mach-trap" '^OpenTala: halting' "$start
^process 1: system call 0x1c of class 1 is not implemented$
^process 1: killed by SIGSYS$
$halt"

boot "user fault" "opentala.test=user-fault" '^OpenTala: halting' "$start
$user_fault
^process 1: killed by SIGSEGV$
$halt"

boot "page fault" "opentala.test=page-fault" '^panic: ' "$start
^process 1: every page given back$
^test: writing to $unmapped$
$page_fault"

if [ "$arch" = amd64 ]; then
        boot "double fault" "opentala.test=double-fault" '^panic: ' "$start
^process 1: every page given back$
^test: pushing to a stack at 0x0000000040000000$
^panic: double fault \\(vector 8\\) at 0x[0-9a-f]{16}, error 0$"
fi

echo "boot-test: $arch: passed"
