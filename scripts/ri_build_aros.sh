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
# ABIv1 RULE (2026-09-21, probe_ahi guest page-fault): the v1 guest runs a
# build-pc lineage system (library base in rdx). The v11 sdk trees emit the
# stale r12 convention (verified: 15 mov %rax,%r12 in hello_ri) and fault
# on first LVO call. ALWAYS build guest binaries against the v1 tree below;
# verify with: objdump -d $OUT | grep -c 'mov *%rax,%r12'  (must be 0).
V1SDK="../Vulkan4Aros/src/abi/v1/core-pc-x86_64/bin/pc-x86_64/AROS/Developer/include"
if [ -d "$V1SDK" ]; then
    # Absolute: derived paths (shim symlinks, -L dirs) must survive CWD changes.
    SDK="$(cd "$V1SDK" && pwd)"
else
    echo "WARN: v1 build-pc SDK absent, falling back to env SDK (expect r12 faults on guest)"
    SDK="$AROS_SDK_INCLUDE"
fi
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
# Build-pc CRT rename shim (2026-09-21): the v1 SDK ships libstdc/libstdcio/
# libposixc where the cross-gcc LIB_SPEC expects libcrt/libstdlib/libcrtcrtprog.
# Symlink-shim the old names + link stdio/POSIX explicitly, else
# `cannot find -lstdlib -lcrt` at link time.
SHIM=/tmp/ri/libshim_v1
mkdir -p "$SHIM"
for _n in libcrt libstdlib libcrtprog; do ln -sf "$SDK/../lib/libstdc.a" "$SHIM/$_n.a"; done
STARTUP=()
[ -f "$SDK/../lib/startup.o" ] && STARTUP=("$SDK/../lib/startup.o")
x86_64-aros-gcc $CFLAGS_AROS -nostartfiles -no-pie -Wa,-W -o "$OUT/probe_ahi" "$OUT/probe_ahi.o" "${STARTUP[@]}" -L "$SHIM" -L "$SDK/../lib" -lstdcio -lposixc -ldos -lexec
test -f "$OUT/probe_ahi" || { echo "FAIL: probe_ahi not linked"; exit 1; }
x86_64-aros-readelf -h "$OUT/probe_ahi" | grep -q "Advanced Micro Devices X86-64" || { echo "FAIL: probe_ahi not X86-64 ELF"; exit 1; }
echo "AROS PROBE BUILD OK"
# Optional target (2026-09-25, §12.10 G4): `ri_build_aros.sh sections` also links
# RISECT, the RSection canvas proof (303 / 808) for the ABIv1 lane (riqemu1).
# Evidence: docs/evidence/gui/sect303-canvas-proof.md. Gates: 0 unresolved, 0 r12 moves.
if [ "${1:-}" = sections ]; then
  O3="$OUT/sections"; mkdir -p "$O3"
  CF3="$CFLAGS_AROS -Werror -fno-stack-protector -I$ROOT"
  OBJS3=""
  for f in app/sectproof.c gui/widgets/rsection.mcc.c gui/ctlreg.c gui/panelgeo.c gui/sect303.c gui/sect808.c gui/sect909.c gui/sectui.c gui/knob_logic.c engine/dsp/kernels.c engine/seq/pattern.c; do
    x86_64-aros-gcc $CF3 -c "$ROOT/$f" -o "$O3/$(basename "$f" .c).o"
    OBJS3="$OBJS3 $O3/$(basename "$f" .c).o"
  done
  x86_64-aros-gcc -mcmodel=large -mno-red-zone -ffixed-r12 -nostartfiles -no-pie -o "$OUT/RISECT" $OBJS3 "${STARTUP[@]}" \
    -L "$SHIM" -L "$SDK/../lib" -lmui -lamiga -lstdcio -lposixc -lintuition -lgraphics -lutility -ldos -lexec -lautoinit
  test "$(x86_64-aros-readelf -s "$OUT/RISECT" | awk '$7=="UND" && $8!=""' | wc -l)" = 0 || { echo "FAIL: RISECT unresolved"; exit 1; }
  test "$(objdump -d "$OUT/RISECT" | grep -c 'mov    %rax,%r12')" = 0 || { echo "FAIL: RISECT r12 base moves (v1)"; exit 1; }
  echo "AROS RISECT BUILD OK ($(stat -c%s "$OUT/RISECT") bytes)"
fi
