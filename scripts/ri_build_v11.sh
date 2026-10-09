#!/bin/bash
# ri_build_v11.sh — Dell (ABIv11) RIAPP build.
#
# IN THE REPO AS OF 2026-10-05, at the owner's direction. It lived at
# ~/Work/vms/ri-p9/build_v11.sh, which is exactly the failure mode the repo
# rule names -- "git holds everything load-bearing, /tmp only regenerables" --
# and the specific harm was measured, not theorised: **a build script outside
# the repo is how stale-binary mistakes happen**, and this session logged three
# of them (a probe linked against a pre-change .o reading as "the optimisation
# does nothing", a stale art_shared.o reading as "it does not reproduce", and a
# stale audit object printing a stale ctlreg_index failure). The first two were
# only caught because the delta was implausible. Nothing outside the repo can be
# diffed against HEAD, which is the property that would have caught all three.
#
# The v11 path is separate from scripts/ri_build_aros.sh on purpose: that one
# builds against the **v1** SDK (library base in rdx, zero `mov %rax,%r12`),
# which is right for the ABIv1 QEMU lane and WRONG for the Dell -- an ABIv11
# binary takes a "Software Failure!" requester at startup before the first log
# line. Both now build the approved MIXED configuration (engine/ -O2, app+GUI
# -O0) so they cannot disagree about what was measured.
#
# WHY THIS EXISTS. scripts/ri_build_aros.sh builds against the **v1** build-pc
# SDK (library base in rdx, zero `mov %rax,%r12`). That binary is right for the
# ABIv1 QEMU lane and *wrong for the Dell*, which is ABIv11: it takes a
# "Software Failure!" requester at startup before the first log line
# (llm-wiki/raw/articles/2026-10-01-dell-deploy-abiv11-usb-stick-layout.md).
# The v11 toolchain/SDK pair emits the r12 convention the Dell's kernel wants.
#
# usage: ri_build_v11.sh [source-tree] [out-binary]
#   source-tree  default: the ReIncarnation checkout (cwd)
#   out-binary   default: /tmp/opencode/RIAPP.v11
set -e
ROOT="${1:-/home/miller/Work/projects/ReIncarnation}"
OUTBIN="${2:-/tmp/opencode/RIAPP.v11}"
VK=/home/miller/Work/projects/Vulkan4Aros
TC="$VK/src/abi/v11/toolchain-core-x86_64"
SDK="$VK/src/abi/v11/sdk/Developer"
[ -d "$TC" ] || { echo "FAIL: v11 toolchain absent ($TC)"; exit 1; }
[ -d "$SDK/include" ] || { echo "FAIL: v11 SDK absent ($SDK)"; exit 1; }
PATH="$TC:$PATH"
export PATH
OBJ=/tmp/ri/arosv11/riapp
mkdir -p "$OBJ"
HASH="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo '?')"
# Flags as recorded in the wiki article (no -Werror: the v11 SDK headers warn
# on our clean build; the v1 script's -Werror is a v1-lane gate).
CF="-std=gnu99 ${RI_V11_OPT:--O2} -mcmodel=large -mno-red-zone -mno-ms-bitfields -fno-strict-aliasing -ffixed-r12 -fno-builtin -fno-stack-protector -DPCF_TABLE_VERIFIED=1 -Wa,-W -I$ROOT -I$SDK/include -I$SDK/include/aros/posixc -I$SDK/include/aros/stdc -DRIAPP_BUILD_HASH=\"$HASH\""
# The source list is ri_build_aros.sh's, verbatim.
SRCS="app/riapp.c app/core/live_driver.c app/core/canvas_events.c app/core/riapp_core.c project/rbnm.c project/rbng.c project/playlist.c gui/draw/canvas.c gui/draw/font_legend.c gui/draw/art_shared.c gui/draw/art_303.c gui/draw/art_808.c gui/draw/art_909.c gui/draw/art_levi.c gui/draw/art_mix.c gui/draw/art_fx.c gui/draw/art_pat.c gui/draw/art_tr.c gui/draw/art_section.c platform/aros/fs_aros.c platform/aros/log_aros.c platform/aros/image_dt.c platform/aros/fpu_aros.c platform/aros/pack_909.c audio_io/audio_ahi_live.c engine/engine.c engine/live.c engine/seq/clock.c engine/seq/sched.c engine/seq/riseq.c engine/seq/songsteps.c engine/seq/snapbuild.c engine/seq/pattern.c engine/seq/pattern_emit.c engine/seq/transport.c engine/seq/songtrack.c engine/seq/player.c engine/seq/autolane.c engine/seq/ctlplane.c engine/dsp/kernels.c engine/dsp/rb303.c engine/dsp/params.c engine/dsp/rb808.c engine/dsp/rb909.c engine/dsp/levi.c engine/dsp/levi_arp.c engine/dsp/levi_matrix.c engine/dsp/levi_fx.c engine/fx/fx.c engine/fx/route.c engine/fx/reverb.c engine/fx/pcf.c engine/mixer/mixer.c engine/framework/ridevice.c project/sha256.c gui/panelctl.c gui/ctlreg.c gui/panelgeo.c gui/zoomfit.c gui/skinsect.c gui/sect303.c gui/sect808.c gui/sect909.c gui/sectlevi.c gui/sectmix.c gui/sectfx.c gui/sectpat.c gui/secttr.c gui/sectui.c gui/keymap.c gui/panelui.c gui/livestate.c gui/knob_logic.c gui/knob_art.c gui/panels.c gui/visdev.c gui/tabpages.c gui/catalog.c gui/skin.c gui/skin_aros.c gui/widgets/rsection.mcc.c gui/midimap.c gui/miditrans.c midi_io/midi_bridge.c midi_io/midi_follow.c platform/aros/midi_camd.c"
# MIXED BUILD, DEFAULT (owner-approved 2026-10-04).
#
# The approved configuration is engine/ at -O2 with app + GUI at -O0: the DSP
# work is ~3x cheaper optimised, and app/GUI stay debuggable.
#
# This was a footgun with real cost. The uniform default here was -O2, but the
# standing habit was to pass RI_V11_OPT=-O0 "because RIAPP ships -O0", and four
# consecutive verification builds went out that way. The owner rejected two of
# them on audio alone while EVERY counter was green (xruns=0, overloads=0) --
# a droppy artefact produced a clean telemetry report, because the DSP is not
# overrun so the audio path never complains. So the approved shape is now the
# default, and it is expressed as TWO explicit flag sets rather than as string
# substitution on one: an earlier version of this block substituted the opt into
# a shared string and, with RI_V11_OPT unset, silently produced a uniform -O2
# build while cheerfully reporting it -- the same silent-wrong-default failure
# in the mechanism written to prevent exactly that.
#
#   RI_V11_MIX_OPT   engine/ optimisation level   (default -O2)
#   RI_V11_OPT       app + GUI optimisation level (default -O0)
#   RI_V11_MIXED=0   uniform build, RI_V11_OPT everywhere (default -O2)
MIXED=1
[ "${RI_V11_MIXED:-1}" = 0 ] && MIXED=0
RI_V11_MIX_OPT="${RI_V11_MIX_OPT:--O2}"
# In mixed mode the app/GUI level is -O0 unless explicitly asked otherwise. NOT
# ${RI_V11_OPT:--O2}: inheriting that default is what made the first mixed build
# come out entirely optimised.
APP_OPT="${RI_V11_APP_OPT:--O0}"
UNIFORM_OPT="${RI_V11_OPT:--O2}"
CF_DSP="$CF"
CF_APP="$CF"
if [ "$MIXED" = 1 ]; then
  CF_DSP="${CF/$UNIFORM_OPT/$RI_V11_MIX_OPT}"
  CF_APP="${CF/$UNIFORM_OPT/$APP_OPT}"
  case "$CF_DSP" in *"$RI_V11_MIX_OPT"*) ;; *) CF_DSP="$CF_DSP $RI_V11_MIX_OPT" ;; esac
  case "$CF_APP" in *"$APP_OPT"*) ;; *) CF_APP="$CF_APP $APP_OPT" ;; esac
fi
OBJS=""
NO2=""
for f in $SRCS; do
  o="$OBJ/$(basename "$f" .c).o"
  case "$f" in
    engine/*) CFU="$CF_DSP" ;;
    *)CFU="$CF_APP" ;;
  esac
  case "$CFU" in *"$RI_V11_MIX_OPT"*) NO2="$NO2 $(basename "$f" .c)" ;; esac
  x86_64-aros-gcc $CFU -c "$ROOT/$f" -o "$o"
  OBJS="$OBJS $o"
done
x86_64-aros-gcc -mcmodel=large -mno-red-zone -ffixed-r12 -nostartfiles -no-pie -o "$OUTBIN" $OBJS \
  "$SDK/lib/startup.o" -L "$SDK/lib" -lamiga -lmui -lintuition -lgraphics -lutility -ldos -lexec -lautoinit -lcybergraphics -lcamd
UND="$(x86_64-aros-readelf -s "$OUTBIN" | awk '$7=="UND" && $8!=""' | wc -l)"
[ "$UND" = 0 ] || { echo "FAIL: RIAPP(v11) unresolved: $UND"; exit 1; }
echo "AROS RIAPP v11 BUILD OK ($OUTBIN, $(stat -c%s "$OUTBIN") bytes, build=$HASH, r12moves=$(objdump -d "$OUTBIN" | grep -c 'mov    %rax,%r12'))"
if [ "$MIXED" = 1 ]; then
  echo "  mixed: engine/ at $RI_V11_MIX_OPT, rest at ${RI_V11_OPT:--O0} ($(echo $NO2 | wc -w) TUs optimised)"
fi
