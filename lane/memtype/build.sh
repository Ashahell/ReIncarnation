#!/bin/bash
# build.sh — cross-build MEMTYPE for v11 (Dell) and v1 (QEMU guest).
# usage: build.sh [v11|v1|all]   (default: all)
# Outputs: /tmp/ri/memtype/MEMTYPE.v11, /tmp/ri/memtype/MEMTYPE.v1
set -e
ROOT="/home/miller/Work/projects/ReIncarnation"
VK="/home/miller/Work/projects/Vulkan4Aros"
OUT=/tmp/ri/memtype
mkdir -p "$OUT"
WHAT="${1:-all}"

build_v11() {
    TC="$VK/src/abi/v11/toolchain-core-x86_64"
    SDK="$VK/src/abi/v11/sdk/Developer"
    GEN="$VK/src/abi/v11/core-pc-x86_64/bin/pc-x86_64/gen/buildsdks/private/include"
    [ -x "$TC/x86_64-aros-gcc" ] || { echo "FAIL: v11 toolchain absent"; exit 1; }
    [ -f "$SDK/lib/startup.o" ] || { echo "FAIL: v11 SDK absent"; exit 1; }
    CF="-std=gnu99 -O2 -mcmodel=large -mno-red-zone -mno-ms-bitfields -fno-strict-aliasing -ffixed-r12 -fno-builtin -fno-stack-protector -Wa,-W -I$ROOT -I$ROOT/lane/memtype -I$SDK/include -I$SDK/include/aros/stdc -I$GEN"
    "$TC/x86_64-aros-gcc" $CF -c "$ROOT/lane/memtype/memtype.c" -o "$OUT/memtype_v11.o"
    "$TC/x86_64-aros-gcc" $CF -c "$ROOT/lane/memtype/memtype_decode.c" -o "$OUT/memtype_decode_v11.o"
    "$TC/x86_64-aros-gcc" -mcmodel=large -mno-red-zone -ffixed-r12 -nostartfiles -no-pie \
      -o "$OUT/MEMTYPE.v11" "$OUT/memtype_v11.o" "$OUT/memtype_decode_v11.o" \
      "$SDK/lib/startup.o" -L"$SDK/lib" -lamiga -ldos -lexec -lautoinit
    if "$TC/x86_64-aros-readelf" -s "$OUT/MEMTYPE.v11" | awk '$7=="UND" && $8!="" { found=1 } END { exit !found }'; then
        echo "FAIL: MEMTYPE.v11 has unresolved symbols"; exit 1
    fi
    echo "MEMTYPE.v11 OK"
    # CPUCOUNT (F3 gate: how many CPUs AROS runs — one SuperState window
    # covers one CPU; count>1 stops the trial, no user-mode IPI exists).
    "$TC/x86_64-aros-gcc" $CF -c "$ROOT/lane/memtype/cpucount.c" -o "$OUT/cpucount_v11.o"
    "$TC/x86_64-aros-gcc" -mcmodel=large -mno-red-zone -ffixed-r12 -nostartfiles -no-pie \
      -o "$OUT/CPUCOUNT.v11" "$OUT/cpucount_v11.o" \
      "$SDK/lib/startup.o" -L"$SDK/lib" -lamiga -ldos -lexec -lautoinit
    if "$TC/x86_64-aros-readelf" -s "$OUT/CPUCOUNT.v11" | awk '$7=="UND" && $8!="" { found=1 } END { exit !found }'; then
        echo "FAIL: CPUCOUNT.v11 has unresolved symbols"; exit 1
    fi
    echo "CPUCOUNT.v11 OK"
}

build_v1() {
    V1SDK="$VK/src/abi/v1/core-pc-x86_64/bin/pc-x86_64/AROS/Developer"
    [ -d "$V1SDK/include" ] || { echo "FAIL: v1 SDK absent"; exit 1; }
    # The cross-gcc lives in the v11 toolchain dir (same compiler family;
    # the v1 SDK headers select the rdx convention). Be explicit: the env
    # default for AROS_TOOLCHAIN has pointed at several trees over time.
    V1TC="$VK/src/abi/v11/toolchain-core-x86_64"
    export PATH="$V1TC:${AROS_TOOLCHAIN:-}:$PATH"
    command -v x86_64-aros-gcc >/dev/null || { echo "FAIL: no x86_64-aros-gcc in PATH"; exit 1; }
    CF="-std=gnu99 -O2 -mcmodel=large -mno-red-zone -mno-ms-bitfields -fno-strict-aliasing -ffixed-r12 -fno-builtin -I$ROOT -I$ROOT/lane/memtype -I$V1SDK/include -I$V1SDK/include/aros/stdc"
    x86_64-aros-gcc $CF -c "$ROOT/lane/memtype/memtype.c" -o "$OUT/memtype_v1.o"
    x86_64-aros-gcc $CF -c "$ROOT/lane/memtype/memtype_decode.c" -o "$OUT/memtype_decode_v1.o"
    # Build-pc CRT rename shim (see scripts/ri_build_aros.sh): the v1 SDK
    # ships libstdc where the cross-gcc LIB_SPEC expects libcrt/libstdlib.
    SHIM=/tmp/ri/libshim_memtype_v1
    mkdir -p "$SHIM"
    for _n in libcrt libstdlib libcrtprog; do ln -sf "$V1SDK/lib/libstdc.a" "$SHIM/$_n.a"; done
    x86_64-aros-gcc $CF -nostartfiles -no-pie -Wa,-W \
      -o "$OUT/MEMTYPE.v1" "$OUT/memtype_v1.o" "$OUT/memtype_decode_v1.o" \
      "$V1SDK/lib/startup.o" -L"$SHIM" -L"$V1SDK/lib" -lamiga -lstdcio -lposixc -ldos -lexec -lautoinit
    if x86_64-aros-readelf -s "$OUT/MEMTYPE.v1" | awk '$7=="UND" && $8!="" { found=1 } END { exit !found }'; then
        echo "FAIL: MEMTYPE.v1 has unresolved symbols"; exit 1
    fi
    if [ "$(x86_64-aros-objdump -d "$OUT/MEMTYPE.v1" | grep -c 'mov *%rax,%r12')" != "0" ]; then
        echo "FAIL: MEMTYPE.v1 has r12 moves (wrong SDK convention for v1 guest)"; exit 1
    fi
    echo "MEMTYPE.v1 OK"
}

case "$WHAT" in
  v11) build_v11 ;;
  v1) build_v1 ;;
  all) build_v11; build_v1 ;;
  *) echo "usage: $0 [v11|v1|all]"; exit 2 ;;
esac
