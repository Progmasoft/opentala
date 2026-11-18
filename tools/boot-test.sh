#!/bin/sh
# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MIT
#
# Usage: boot-test.sh <kernel>
#
# Checks that the Mach-O layout of <kernel> can be loaded flat, then boots it in QEMU three
# times, with 128 MiB, and checks what it writes to the serial port:
#
#   - without a command line: traps, the memory map, the page frame allocator and the timer
#     work, and the kernel halts;
#   - with opentala.test=page-fault: a write to an unmapped address ends in a panic that names
#     the page fault and its address;
#   - with opentala.test=double-fault: a push to an unmapped stack ends in a panic that names the
#     double fault, which only its own stack (the IST) can report.
#
# Each expected line is an extended regular expression that must match one line of the output,
# in order.

set -eu

kernel=$1
qemu=${QEMU:-qemu-system-x86_64}
otool=${OTOOL:-llvm-otool}
base=$((0x100000))

# A Multiboot loader copies the file to 1 MiB as it is, so each segment with file contents must
# sit at its file offset from 1 MiB.
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

# boot <name> <command line> <last line> <expected lines>
boot() {
        name=$1
        line=$2
        last=$3
        expected=$4
        : >"$serial"
        "$qemu" -kernel "$kernel" -append "$line" -m 128 -serial "file:$serial" -display none \
                -no-reboot -monitor none 2>"$log" &
        pid=$!
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
                echo "boot-test: $name: no line matched: $missing"
                echo "boot-test: $name: QEMU said:"
                cat "$log"
                echo "boot-test: $name: the serial port showed:"
                printf '%s\n' "$actual"
                exit 1
        fi
        echo "boot-test: $name: passed"
}

start='^OpenTala: amd64, 64-bit mode$
^OpenTala: loaded by Multiboot$
^traps: breakpoint handled and resumed$
^memory: 12[0-9] MiB usable, [0-9]+ page frames free below 1 GiB$
^memory: allocator checked$
^timer: 10 ticks at 100 Hz$'

boot "halt" "" '^OpenTala: halting' "$start
^OpenTala: halting$"

boot "page fault" "opentala.test=page-fault" '^panic: ' "$start
^test: writing to 0x0000000040000000$
^  rip [0-9a-f]{16} cs 0008 rflags [0-9a-f]{8} ss 0010$
^panic: page fault at 0x[0-9a-f]{16}, address 0x0000000040000000, error 2$"

boot "double fault" "opentala.test=double-fault" '^panic: ' "$start
^test: pushing to a stack at 0x0000000040000000$
^panic: double fault \\(vector 8\\) at 0x[0-9a-f]{16}, error 0$"

echo "boot-test: passed"
