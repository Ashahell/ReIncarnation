#!/bin/bash
# build.sh — cross-build BLITPROBE for v11 (Dell F2 baseline).
# usage: build.sh
# Output: /tmp/ri/blitprobe/BLITPROBE.v11
set -e
ROOT="/home/miller/Work/projects/ReIncarnation"
VK="/home/miller/Work/projects/Vulkan4Aros"
OUT=/tmp/ri/blitprobe
mkdir -p "$OUT"
TC="$VK/src/abi/v11/toolchain-core-x86_64"
SDK="$VK/src/abi/v11/sdk/Developer"
[ -x "$TC/x86_64-aros-gcc" ] || { echo "FAIL: v11 toolchain absent"; exit 1; }
[ -f "$SDK/lib/startup.o" ] || { echo "FAIL: v11 SDK absent"; exit 1; }
CF="-std=gnu99 -O2 -mcmodel=large -mno-red-zone -mno-ms-bitfields -fno-strict-aliasing -ffixed-r12 -fno-builtin -fno-stack-protector -Wa,-W -I$ROOT -I$ROOT/lane/blitprobe -I$SDK/include -I$SDK/include/aros/stdc"
"$TC/x86_64-aros-gcc" $CF -c "$ROOT/lane/blitprobe/blitprobe.c" -o "$OUT/blitprobe_v11.o"
"$TC/x86_64-aros-gcc" -mcmodel=large -mno-red-zone -ffixed-r12 -nostartfiles -no-pie \
  -o "$OUT/BLITPROBE.v11" "$OUT/blitprobe_v11.o" \
  "$SDK/lib/startup.o" -L"$SDK/lib" -lamiga -lintuition -lgraphics -lutility -ldos -lexec -lautoinit
if "$TC/x86_64-aros-readelf" -s "$OUT/BLITPROBE.v11" | awk '$7=="UND" && $8!="" { found=1 } END { exit !found }'; then
    echo "FAIL: BLITPROBE.v11 has unresolved symbols"; exit 1
fi
echo "BLITPROBE.v11 OK"
