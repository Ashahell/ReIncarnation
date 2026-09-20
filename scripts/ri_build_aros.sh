#!/bin/bash
# AROS cross-build. Heute: proves the path works by building an empty stub library.
set -e
ROOT="$(dirname "$0")/.."
if [ ! -f ../Vulkan4Aros/scripts/aros_build_env.sh ]; then echo "FAIL: Vulkan4AROS tree (toolchain source) not found"; exit 1; fi
. ../Vulkan4Aros/scripts/aros_build_env.sh
OUT=/tmp/ri/aros
mkdir -p "$OUT"
x86_64-aros-gcc -c -mcmodel=large -mno-red-zone -ffixed-r12 -Wall "$ROOT/audio_io/aros_stub.c" -o "$OUT/aros_stub.o"
x86_64-aros-ld -r "$OUT/aros_stub.o" -o "$OUT/reincarnation_stub.library"
x86_64-aros-readelf -s "$OUT/reincarnation_stub.library" | awk '$7=="UND" && $1!="0:" { found=1 } END { exit !found }' && { echo "FAIL: unresolved symbols"; exit 1; } || true
echo "AROS STUB BUILD OK"
