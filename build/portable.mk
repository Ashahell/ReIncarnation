# build/portable.mk — portable build (portability plan T10).
# Builds the portable core + host PAL + tests + main_headless (T8).
# AROS lane keeps the 5-script rule; this file lives under build/, not scripts/.
# Usage: make -f build/portable.mk [all|test|headless|clean]
# Owner decision §8.2 (CMake vs Makefile) pending — this Makefile keeps zero deps.

ROOT := $(dir $(lastword $(MAKEFILE_LIST)))..
OUT ?= /tmp/ri/portable
CC ?= gcc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror -pedantic -ffp-contract=off -fno-unsafe-math-optimizations -ftrapv -I$(ROOT)

CORE_TU := \
  app/core/live_driver.c app/core/canvas_events.c \
  gui/draw/canvas.c gui/draw/art_shared.c gui/draw/art_303.c \
  gui/draw/art_808.c gui/draw/art_909.c gui/draw/art_levi.c gui/draw/art_mix.c \
  gui/draw/art_fx.c gui/draw/art_pat.c gui/draw/art_tr.c \
  gui/draw/art_section.c platform/host/raster.c \
  engine/dsp/kernels.c engine/engine.c engine/live.c \
  engine/seq/clock.c engine/seq/sched.c engine/seq/riseq.c \
  engine/seq/songsteps.c engine/seq/snapbuild.c engine/seq/pattern.c \
  engine/seq/pattern_emit.c engine/seq/transport.c engine/seq/songtrack.c \
  engine/seq/player.c engine/seq/autolane.c engine/seq/ctlplane.c \
  engine/dsp/rb303.c engine/dsp/params.c engine/dsp/rb808.c \
  engine/dsp/rb909.c engine/dsp/levi.c project/rbnm.c engine/fx/fx.c engine/fx/route.c \
  engine/fx/pcf.c engine/mixer/mixer.c engine/framework/ridevice.c \
  audio_io/audio.c audio_io/backend_null.c platform/host/audio_null.c \
  project/sha256.c project/rbng.c project/arexx.c project/arexx_dispatch.c \
  project/undo.c midi_io/midi.c \
  gui/knob_logic.c gui/panels.c gui/catalog.c gui/knob_art.c gui/ctlreg.c \
  gui/panelctl.c gui/panelgeo.c gui/sect303.c gui/sect808.c gui/sect909.c \
  gui/sectlevi.c gui/sectmix.c gui/sectfx.c gui/sectpat.c gui/secttr.c gui/sectui.c \
  gui/keymap.c gui/panelui.c gui/livestate.c gui/midimap.c gui/skin.c

CORE_OBJS := $(patsubst %.c,$(OUT)/%.o,$(CORE_TU))

$(OUT)/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DPCF_TABLE_VERIFIED=1 -c $< -o $@

all: $(CORE_OBJS)
	@echo "PORTABLE BUILD OK $(OUT)"

test: all
	$(CC) $(CFLAGS) -pthread -o $(OUT)/t84_pal_thread $(ROOT)/tests/unit/t84_pal_thread.c $(CORE_OBJS) -lm -lpng
	$(OUT)/t84_pal_thread

# T8 headless frontend: renders the demo fixture to WAV via the driver.
headless: all
	$(CC) $(CFLAGS) -pthread -o $(OUT)/headless $(ROOT)/platform/host/main_headless.c $(CORE_OBJS) -lm -lpng
	$(OUT)/headless

clean:
	rm -rf $(OUT)

.PHONY: all test headless clean
