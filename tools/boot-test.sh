#!/bin/sh
# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MIT
#
# Usage: boot-test.sh <kernel>
#
# Checks that the Mach-O layout of <kernel> can be loaded flat, boots it in QEMU and waits up to
# 30 seconds for the lines it must write to the serial port.

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
"$qemu" -kernel "$kernel" -serial "file:$serial" -display none -no-reboot -monitor none \
        2>"$log" &
pid=$!

expected="OpenTala: amd64, 64-bit mode
OpenTala: loaded by Multiboot
OpenTala: halting"

for _ in $(seq 1 60); do
        if grep -q "OpenTala: halting" "$serial" || ! kill -0 "$pid" 2>/dev/null; then
                break
        fi
        sleep 0.5
done

actual=$(tr -d '\r' <"$serial")
if [ "$actual" != "$expected" ]; then
        echo "boot-test: QEMU said:"
        cat "$log"
        echo "boot-test: the serial port showed:"
        printf '%s\n' "$actual"
        exit 1
fi
printf '%s\n' "$actual"
echo "boot-test: passed"
