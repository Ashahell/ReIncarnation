#!/bin/bash
# usage: ri_build_host.sh [kernels|clock|sched|dsp303|dsp808|dsp909|fx|pcf|mixer|audio|gui|all|test NAME|golden NAME|clean]
set -e
ROOT="$(dirname "$0")/.."
OUT=/tmp/ri/build
CFLAGS="-std=c99 -O2 -Wall -Wextra -Werror -pedantic -ffp-contract=off -fno-unsafe-math-optimizations -ftrapv -I$ROOT"
mkdir -p "$OUT"
MOD_kernels="engine/dsp/kernels.c"
MOD_engine="engine/engine.c"
MOD_clock="engine/seq/clock.c"
MOD_sched="engine/seq/sched.c engine/seq/riseq.c engine/seq/songsteps.c engine/seq/snapbuild.c"
MOD_dsp303="engine/dsp/rb303.c engine/dsp/params.c"
MOD_dsp808="engine/dsp/rb808.c"
MOD_dsp909="engine/dsp/rb909.c project/rbnm.c"
MOD_fx="engine/fx/fx.c"
MOD_mixer="engine/mixer/mixer.c engine/framework/ridevice.c"
MOD_audio="audio_io/audio.c audio_io/backend_null.c"
MOD_gui="gui/knob_logic.c gui/panels.c gui/catalog.c gui/knob_art.c"
MOD_formats="project/sha256.c project/rbng.c project/arexx.c project/arexx_dispatch.c project/undo.c midi_io/midi.c"
# Later tasks APPEND paths to MOD_dsp808, MOD_fx, ... and add matching case lines.
compile_list() { for f in $1; do test -f "$ROOT/$f" || { echo "MISSING $f"; exit 1; }; gcc $CFLAGS -c "$ROOT/$f" -o "$OUT/$(basename $f .c).o"; done; }
# PCF ledger gate (Task 10, gate G10): the -D flag is issued ONLY when the
# ledger-verified data file exists. Without it pcf.c hits its #error, so a
# missing table fails the pcf target with the compiler error (negative
# gate). `all` skips pcf when the table is absent (fails that target only,
# never all); `pcf` is strict.
build_pcf() {
  if [ -f "$ROOT/reference/pcf-table.bin" ]; then
    gcc $CFLAGS -DPCF_TABLE_VERIFIED=1 -c "$ROOT/engine/fx/pcf.c" -o "$OUT/pcf.o";
  elif [ "$1" = strict ]; then
    echo "pcf: table missing, proving the gate (expect #error)";
    gcc $CFLAGS -c "$ROOT/engine/fx/pcf.c" -o "$OUT/pcf.o";
  else
    echo "pcf: SKIP (table unverified)";
  fi }
case "${1:-all}" in
  kernels|engine|clock|sched|dsp303|dsp808|dsp909|fx|mixer|audio|gui|formats) compile_list "$(eval echo \$MOD_$1)" ;;
  pcf) build_pcf strict ;;
  all) for t in kernels engine clock sched formats dsp303 dsp808 dsp909 fx mixer audio gui; do "$0" $t; done; build_pcf skip ;;
  test) test -n "$2" || { echo "usage: $0 test NAME"; exit 1; }
    gcc $CFLAGS -o "$OUT/$2" "$ROOT/tests/unit/$2.c" "$ROOT/tests/property/$2.c" "$OUT"/*.o -lm 2>/dev/null || \
    gcc $CFLAGS -o "$OUT/$2" $(ls "$ROOT/tests/unit/$2.c" "$ROOT/tests/property/$2.c" 2>/dev/null) "$OUT"/*.o -lm
    "$OUT/$2" || exit 1 ;;  # propagate test failures (was masked by BUILD OK echo)
  clean) rm -rf /tmp/ri ;;
  *) echo "unknown target $1"; exit 1 ;;
esac
echo "BUILD $1 OK"
