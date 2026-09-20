#!/bin/bash
# usage: ri_build_host.sh [kernels|clock|sched|dsp303|dsp808|dsp909|fx|mixer|all|test NAME|golden NAME|clean]
set -e
ROOT="$(dirname "$0")/.."
OUT=/tmp/ri/build
CFLAGS="-std=c99 -O2 -Wall -Wextra -Werror -pedantic -ffp-contract=off -fno-unsafe-math-optimizations -I$ROOT"
mkdir -p "$OUT"
MOD_kernels="engine/dsp/kernels.c"
MOD_clock="engine/seq/clock.c"
MOD_sched="engine/seq/sched.c"
MOD_dsp303="engine/dsp/rb303.c engine/dsp/params.c"
# Later tasks APPEND paths to MOD_dsp808, MOD_fx, ... and add matching case lines.
compile_list() { for f in $1; do test -f "$ROOT/$f" || { echo "MISSING $f"; exit 1; }; gcc $CFLAGS -c "$ROOT/$f" -o "$OUT/$(basename $f .c).o"; done; }
case "${1:-all}" in
  kernels|clock|sched|dsp303) compile_list "$(eval echo \$MOD_$1)" ;;
  all) for t in kernels clock sched dsp303; do "$0" $t; done ;;
  test) test -n "$2" || { echo "usage: $0 test NAME"; exit 1; }
    gcc $CFLAGS -o "$OUT/$2" "$ROOT/tests/unit/$2.c" "$ROOT/tests/property/$2.c" "$OUT"/*.o 2>/dev/null || \
    gcc $CFLAGS -o "$OUT/$2" $(ls "$ROOT/tests/unit/$2.c" "$ROOT/tests/property/$2.c" 2>/dev/null) "$OUT"/*.o
    "$OUT/$2" ;;
  clean) rm -rf /tmp/ri ;;
  *) echo "unknown target $1"; exit 1 ;;
esac
echo "BUILD $1 OK"
