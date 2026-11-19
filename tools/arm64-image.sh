#!/bin/sh
# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MIT
#
# Usage: arm64-image.sh <kernel> <image>
#
# Writes <image>: the 64-byte header of an arm64 Linux Image (Documentation/arch/arm64/booting.rst)
# followed by the Mach-O <kernel> as it is. A loader that boots such images puts the image at a
# 2 MiB boundary of RAM plus text_offset; text_offset is 2 MiB less the header, so the Mach-O file
# lands at 2 MiB, where it is linked (0x40200000 on QEMU's virt machine). The header's first word
# branches to Arm64_Boot_Entry. The Mach-O file is not changed: the header is outside it.

set -eu

kernel=$1
image=$2
otool=${OTOOL:-llvm-otool}
nm=${NM:-llvm-nm}
header=64
base=$((0x40200000))
start=$((base - header))

entry=$("$nm" "$kernel" | awk '$3 == "Arm64_Boot_Entry" { print "0x" $1 }')
if [ -z "$entry" ]; then
        echo "arm64-image: $kernel has no Arm64_Boot_Entry"
        exit 1
fi
# The end of the highest segment: what the loader must leave free, the zero-filled part included.
segments=$("$otool" -l "$kernel" | awk '
        $1 == "segname" { segment = $2 }
        $1 == "vmaddr" { vmaddr = $2 }
        $1 == "vmsize" && segment != "__PAGEZERO" { print vmaddr, $2 }')
end=0
while read -r vmaddr vmsize; do
        if [ $((vmaddr + vmsize)) -gt "$end" ]; then
                end=$((vmaddr + vmsize))
        fi
done <<EOF
$segments
EOF

# le <bytes> <value>: <value> as <bytes> little-endian bytes.
le() {
        count=$1
        value=$2
        while [ "$count" -gt 0 ]; do
                printf '%b' "\\0$(printf '%03o' $((value & 255)))"
                value=$((value >> 8))
                count=$((count - 1))
        done
}

offset=$(((entry - start) / 4))
if [ "$offset" -le 0 ] || [ "$offset" -ge $((1 << 25)) ]; then
        echo "arm64-image: Arm64_Boot_Entry at $entry is out of a branch's reach"
        exit 1
fi
{
        le 4 $((0x14000000 | offset))      # code0: b Arm64_Boot_Entry
        le 4 0                             # code1
        le 8 $((0x200000 - header))        # text_offset
        le 8 $((end - start))              # image_size, the zero-filled part included
        le 8 0                             # flags: little-endian, any page size, 2 MiB base
        le 8 0                             # res2
        le 8 0                             # res3
        le 8 0                             # res4
        le 4 $((0x644D5241))               # magic: "ARM\x64"
        le 4 0                             # res5
        cat "$kernel"
} >"$image.tmp"
mv "$image.tmp" "$image"
