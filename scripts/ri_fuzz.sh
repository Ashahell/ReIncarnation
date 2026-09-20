#!/bin/bash
# ri_fuzz.sh — 500-mutation fuzz over tools/inspect (Task 13, gate G13).
# usage: ri_fuzz.sh [N]   (default 500)
#
# Seeds: the 10-song RBNG corpus + the RBNM clean pack. Each iteration
# deterministically mutates (seeded PRNG in the embedded python: byte
# flips, truncations, length-field inflation) and runs
# tools/inspect --rbng/--rbnm under `timeout 5`.
#
# Pass rule (spec §17 failure 2): corrupt input MUST take the clean
# requester path — exit 0 (still valid), 1 (invalid with chunk ID +
# byte-offset reason), or 2 (usage/IO). A signal death (rc > 128),
# a timeout (rc 124), or a hang fails the whole run. 500/500
# no-crash is gate G13.
set -e
ROOT="$(dirname "$0")/.."
N="${1:-500}"
OUT=/tmp/ri/build
FUZZ=/tmp/ri/run/fuzz13
mkdir -p "$FUZZ"
bash "$ROOT/scripts/ri_build_host.sh" all >/dev/null
CFLAGS="-std=c99 -O2 -Wall -Wextra -Werror -pedantic -ffp-contract=off -fno-unsafe-math-optimizations -ftrapv -I$ROOT"
gcc $CFLAGS -o "$OUT/inspect" "$ROOT/tools/inspect.c" "$OUT"/*.o
gcc $CFLAGS -o "$OUT/mksong" "$ROOT/tools/mksong.c" "$OUT"/*.o
SEEDS=""
for f in "$ROOT"/tests/golden/songs/corpus/s*.rbng; do
  SEEDS="$SEEDS rbng:$f"
done
SEEDS="$SEEDS rbnm:$ROOT/reference/packs/classic-01/pack.rbnm"
python3 - "$N" "$FUZZ" $SEEDS <<'PYEOF'
import random, sys, os
n = int(sys.argv[1])
outdir = sys.argv[2]
seeds = []
for tok in sys.argv[3:]:
    kind, path = tok.split(":", 1)
    seeds.append((kind, path))
random.seed(0x139D)
for i in range(n):
    kind, path = seeds[i % len(seeds)]
    data = bytearray(open(path, "rb").read())
    mode = i % 4
    if mode == 0:
        # random byte flips (1..8 positions)
        for _ in range(1 + (i % 8)):
            data[random.randrange(len(data))] = random.randrange(256)
    elif mode == 1:
        # truncation at a deterministic point
        cut = 4 + (i * 7919) % max(5, len(data) - 4)
        data = data[:cut]
    elif mode == 2:
        # inflate a length field (bytes 4..7 = FORM total, or a chunk size)
        at = 4 if (i % 2 == 0) else (12 + (i * 104729) % max(13, len(data) - 16))
        if at + 4 <= len(data):
            v = int.from_bytes(data[at:at + 4], "big")
            data[at:at + 4] = ((v + 0x1000 + i) & 0xFFFFFFFF).to_bytes(4, "big")
    else:
        # append garbage (pad/length confusion at EOF)
        data += bytes([(i + j) & 0xFF for j in range(1 + i % 64)])
    ext = ".rbng" if kind == "rbng" else ".rbnm"
    open(os.path.join(outdir, "f%04d%s" % (i, ext)), "wb").write(bytes(data))
print("mutated %d" % n)
PYEOF
pass=0
valid=0
invalid=0
for f in "$FUZZ"/f*.rbng "$FUZZ"/f*.rbnm; do
  case "$f" in
    *.rbng) flag=--rbng ;;
    *.rbnm) flag=--rbnm ;;
  esac
  set +e
  timeout 5 "$OUT/inspect" "$flag" "$f" >/dev/null 2>&1
  rc=$?
  set -e
  if [ "$rc" -eq 124 ]; then echo "FAIL: timeout on $f"; exit 1; fi
  if [ "$rc" -gt 128 ]; then echo "FAIL: signal $rc on $f"; exit 1; fi
  if [ "$rc" -gt 2 ]; then echo "FAIL: rc $rc on $f"; exit 1; fi
  pass=$((pass + 1))
  if [ "$rc" -eq 0 ]; then valid=$((valid + 1)); fi
  if [ "$rc" -eq 1 ]; then invalid=$((invalid + 1)); fi
done
echo "FUZZ 500: $pass no-crash (valid=$valid invalid=$invalid ioerr=$((pass - valid - invalid)))"
test "$pass" -eq "$N" || { echo "FAIL: $pass/$N files checked"; exit 1; }
echo "FUZZ $pass/$N PASS (no crash, no timeout)"
