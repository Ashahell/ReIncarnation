#!/bin/bash
set -e
ROOT="$(dirname "$0")/.."
echo "== Phase 0a: render-path hygiene =="
if grep -rn "malloc\|calloc\|realloc\|free(\|Forbid\|Disable(" "$ROOT/engine/" 2>/dev/null; then echo "FAIL: banned construct in engine/"; exit 1; fi
echo "== Phase 0b: no platform transcendentals/FMA in engine/ =="
if grep -rn "tanhf\|sinf\|cosf\|expf\|powf\|fmodf\|mul_add\|ffast-math" "$ROOT/engine/" 2>/dev/null | grep -v "ri_tanh\|ri_exp\|ri_sin\|ri_pow2"; then echo "FAIL"; exit 1; fi
echo "== Phase 0c: evidence dirs =="
for d in 303 808 909 pcf sequencer gui formats; do test -d "$ROOT/docs/evidence/$d" || { echo "FAIL: missing $d"; exit 1; }; done
echo "AUDIT 0/0 PASS"
