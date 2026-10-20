#!/bin/bash
# usage: ri_build_host.sh [kernels|clock|sched|dsp303|dsp808|dsp909|fx|pcf|mixer|audio|gui|all|test NAME|golden NAME|clean]
set -e
ROOT="$(dirname "$0")/.."
OUT=/tmp/ri/build
CFLAGS="-std=c99 -O2 -Wall -Wextra -Werror -pedantic -ffp-contract=off -fno-unsafe-math-optimizations -ftrapv -I$ROOT"
mkdir -p "$OUT"
MOD_kernels="engine/dsp/kernels.c"
MOD_engine="engine/engine.c engine/live.c"
MOD_clock="engine/seq/clock.c"
MOD_sched="engine/seq/sched.c engine/seq/riseq.c engine/seq/songsteps.c engine/seq/snapbuild.c engine/seq/pattern.c engine/seq/pattern_emit.c engine/seq/transport.c engine/seq/songtrack.c engine/seq/player.c engine/seq/autolane.c engine/seq/ctlplane.c"
MOD_dsp303="engine/dsp/rb303.c engine/dsp/params.c"
MOD_dsp808="engine/dsp/rb808.c"
MOD_dsp909="engine/dsp/rb909.c project/rbnm.c"
MOD_dsplevi="engine/dsp/levi.c engine/dsp/levi_arp.c engine/dsp/levi_matrix.c engine/dsp/levi_fx.c"
MOD_fx="engine/fx/fx.c engine/fx/route.c engine/fx/reverb.c"
MOD_mixer="engine/mixer/mixer.c engine/framework/ridevice.c"
MOD_audio="audio_io/audio.c audio_io/backend_null.c platform/host/audio_null.c platform/host/fpu_host.c"
MOD_gui="gui/knob_logic.c gui/panels.c gui/visdev.c gui/tabpages.c gui/catalog.c gui/knob_art.c gui/ctlreg.c gui/panelctl.c gui/panelgeo.c gui/zoomfit.c gui/skinsect.c gui/sect303.c gui/sect808.c gui/sect909.c gui/sectlevi.c gui/sectmix.c gui/sectfx.c gui/sectpat.c gui/secttr.c gui/sectui.c gui/keymap.c gui/panelui.c gui/livestate.c gui/midimap.c gui/miditrans.c gui/skin.c project/sha256.c platform/host/key_tables.c platform/host/fs_host.c platform/host/log_host.c platform/host/image_host.c"
MOD_formats="project/sha256.c project/rbng.c project/songscript.c project/playlist.c project/arexx.c project/arexx_dispatch.c project/undo.c midi_io/midi.c midi_io/midi_follow.c midi_io/midi_bridge.c midi_io/midi_chan.c midi_io/midi_levi.c midi_io/midi_clockout.c midi_io/midi_out.c midi_io/midi_mmc.c midi_io/midi_interval.c project/smf_export.c midi_io/midi_devout.c gui/ctlreg.c project/stem_wav.c project/stem_render.c project/smf_bridge.c project/loop_render.c project/song_sections.c midi_io/note_in.c midi_io/note_in_app.c platform/host/midi_host.c"
MOD_core="app/core/live_driver.c app/core/canvas_events.c app/core/riapp_core.c"
MOD_draw="gui/draw/canvas.c gui/draw/font_legend.c gui/draw/art_shared.c gui/draw/art_303.c gui/draw/art_808.c gui/draw/art_909.c gui/draw/art_levi.c gui/draw/art_mix.c gui/draw/art_fx.c gui/draw/art_pat.c gui/draw/art_tr.c gui/draw/art_section.c platform/host/raster.c"
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
# Rebuild any module whose object is older than the NEWEST header in the tree.
# A header-only edit (a constant, a struct, an inline) otherwise leaves the
# objects stale, and a test that links them answers for the old code. That is
# not a hypothetical: on 2026-10-20 the drum-channel mutant DM appeared to
# SURVIVE t201 for exactly this reason, and died the moment the module was
# recompiled. A green that was never rebuilt proves nothing.
#
# The check is deliberately coarse -- ANY header newer than an object
# invalidates it -- because it is cheap and it cannot be wrong in the unsafe
# direction. Over-rebuilding costs a second; missing a rebuild costs a
# conclusion.
rebuild_stale_headers() {
  [ -d "$OUT" ] || return 0
  local newest
  newest=$(ls -t $(find "$ROOT" -name '*.h' -not -path '*/.git/*' 2>/dev/null) 2>/dev/null | head -1)
  [ -n "$newest" ] || return 0
  local mod srcs o stale_mod
  for mod in kernels engine clock sched formats dsp303 dsp808 dsp909 dsplevi fx mixer audio gui core draw; do
    eval "srcs=\"\$MOD_$mod\""
    stale_mod=""
    for src in $srcs; do
      o="$OUT/$(basename "$src" .c).o"
      [ -f "$o" ] || continue
      if [ -n "$(find "$o" -newer "$newest" -print -quit 2>/dev/null)" ]; then
        :                       # the object is newer than the newest header: fine
      else
        stale_mod="$mod"; break
      fi
    done
    if [ -n "$stale_mod" ]; then
      echo "-- $stale_mod: a header is newer than its objects -- rebuilding"
      compile_list "$srcs"
    fi
  done
}
case "${1:-all}" in
  kernels|engine|clock|sched|dsp303|dsp808|dsp909|dsplevi|fx|mixer|audio|gui|formats|core|draw) compile_list "$(eval echo \$MOD_$1)" ;;
  pcf) build_pcf strict ;;
  all) for t in kernels engine clock sched formats dsp303 dsp808 dsp909 dsplevi fx mixer audio gui core draw; do "$0" $t; done; build_pcf skip ;;
  test) test -n "$2" || { echo "usage: $0 test NAME"; exit 1; }
    # A HEADER change must invalidate the objects that use it. This target
    # links the objects in $OUT, and nothing else rebuilt them, so a session
    # that edited a header and then ran a test linked STALE objects: the test
    # answered for the old code, and a mutant looked like it had survived.
    # Measured 2026-10-20: the drum-channel mutant DM "passed" t201 until
    # the module was recompiled, and then failed as it must. The audit builds
    # `all` first and was never at risk, but a hand-run test was.
    rebuild_stale_headers
    # -lpng: the T7 host image backend (platform/host/image_host.c); present
    # on dev/CI Linux alongside the mkskin toolchain dep. All other tests
    # link it harmlessly (no png symbols pulled unless referenced).
    gcc $CFLAGS -pthread -o "$OUT/$2" "$ROOT/tests/unit/$2.c" "$ROOT/tests/property/$2.c" "$OUT"/*.o -lm -lpng 2>/dev/null || \
    gcc $CFLAGS -pthread -o "$OUT/$2" $(ls "$ROOT/tests/unit/$2.c" "$ROOT/tests/property/$2.c" 2>/dev/null) "$OUT"/*.o -lm -lpng
    "$OUT/$2" || exit 1 ;;  # propagate test failures (was masked by BUILD OK echo)
  clean) rm -rf /tmp/ri ;;
  *) echo "unknown target $1"; exit 1 ;;
esac
echo "BUILD $1 OK"
