#!/bin/bash
# AROS cross-build. Proves the path works (stub library) and builds the
# M1.1 AHI measurement probe (Task 5, gate G5).
set -e
ROOT="$(dirname "$0")/.."
if [ ! -f ../Vulkan4Aros/scripts/aros_build_env.sh ]; then echo "FAIL: Vulkan4AROS tree (toolchain source) not found"; exit 1; fi
. ../Vulkan4Aros/scripts/aros_build_env.sh
# Task 1 ledger fact: toolchain binaries live DIRECTLY in $AROS_TOOLCHAIN
# (no bin/ subdir); callers export PATH themselves.
export PATH="$AROS_TOOLCHAIN:$PATH"
SDK="$AROS_SDK_INCLUDE"
OUT=/tmp/ri/aros
mkdir -p "$OUT"
x86_64-aros-gcc -c -mcmodel=large -mno-red-zone -ffixed-r12 -Wall "$ROOT/audio_io/aros_stub.c" -o "$OUT/aros_stub.o"
x86_64-aros-ld -r "$OUT/aros_stub.o" -o "$OUT/reincarnation_stub.library"
x86_64-aros-readelf -s "$OUT/reincarnation_stub.library" | awk '$7=="UND" && $1!="0:" { found=1 } END { exit !found }' && { echo "FAIL: unresolved symbols"; exit 1; } || true
echo "AROS STUB BUILD OK"
# M1.1 probe (AROS-only executable; never in the host build).
CFLAGS_AROS="-std=gnu99 -O2 -Wall -Wextra -Wno-pointer-sign -mcmodel=large -mno-red-zone -mno-ms-bitfields -fno-strict-aliasing -ffixed-r12 -fno-builtin -I$SDK -I$SDK/aros/posixc -I$SDK/aros/stdc"
x86_64-aros-gcc $CFLAGS_AROS -c "$ROOT/audio_io/probe_ahi.c" -o "$OUT/probe_ahi.o"
# Executable link mirrors build_cap_probes.sh: -nostartfiles + explicit
# startup.o (guarded) + stub libs; -no-pie is mandatory (gotcha: GCC 16
# defaults to PIE, AROS LoadSeg rejects R_X86_64_RELATIVE).
STARTUP=()
[ -f "$SDK/../lib/startup.o" ] && STARTUP=("$SDK/../lib/startup.o")
x86_64-aros-gcc $CFLAGS_AROS -nostartfiles -no-pie -Wa,-W -o "$OUT/probe_ahi" "$OUT/probe_ahi.o" "${STARTUP[@]}" -L "$SDK/../lib" -ldos -lexec
test -f "$OUT/probe_ahi" || { echo "FAIL: probe_ahi not linked"; exit 1; }
x86_64-aros-readelf -h "$OUT/probe_ahi" | grep -q "Advanced Micro Devices X86-64" || { echo "FAIL: probe_ahi not X86-64 ELF"; exit 1; }
echo "AROS PROBE BUILD OK"
