# §12.3 — One renderer: engine core wired into CLI, export, and live

**Date:** 2026-09-24 (host lane; devices untouched)
**Scope:** review §5.1 (one integrated render graph, skeleton) + §5.3 (stereo);
spec §5 + audit I1 tripwire retired.
**Disposition:** New (third improvement-review slice; TDD RED-first)

## What was tripled

Three structurally identical walkers: `render_song` + `render_rbngsong`
(block-aligned + event splits, mono pcm) and `au_render_frames` (event splits
only), each with its own `apply_event` copy (audio.c's ignored AUTOMATION;
render.c's 303A-only forwarded it — converged behaviorally on first-light
songs, which carry no AUTOMATION, hence t6's old IDENTICAL).

## TDD record

`t37_engine_single.c`: (a) device routing (A-only/B-only silence halves),
(b) centre-unity stereo (L == R sample-exact), (c) chunk-agnosticism,
(d) bit-exact legacy reproduction (independent in-test walker),
(e) AUTOMATION routing by ctl block (bends B, never A).
Link-RED first (no engine existed); a mid-course segfault was a TEST bug
(unsorted event list — contract requires sorted input, now documented);
revert-check (303A section forced off → 19201 FAILs → restore → PASS).

## Production changes

- `engine/engine.{h,c}` (new; `MOD_engine` in build script): walker + 303A/303B
  voices + f64 master with single final rounding (spec §15) + centre-unity pan
  + 64-frame mono fold. Sorted-input contract; unknown devices/blocks ignored.
- `tools/render.c`: both song loops rewired (event sourcing, totals, dumps,
  prints untouched); retired `apply_event` deleted (-Werror would flag it).
- `audio_io/audio.c`: `au_render_frames` = thin mono sink; `au_rewind` loads
  the engine (per-object staging buffer kept — pool holds several); sinks use
  sink-local totals; retired `auf_*` copies + the `RI_LIVE_FULL_GRAPH_*` marker.
- `scripts/ri_audit.sh` Phase 6: tripwire → wired-core gate (audio.c must call
  the engine; t6 IDENTICAL still required).

## Proof

- First-light golden + events byte-identical through the rewired CLI path.
- t6 file-vs-live IDENTICAL (both sinks now run the same core — stronger).
- Full `ri_audit.sh` 0/0.

## Honest notes

- Stereo buses exist; mono sinks fold (L+R)/2. Stereo FILE export and pan
  parameters arrive with the mixer/song slices (D-f stands, staged).
- 808/909/FX/mixer bits reserved in the section mask; their events are
  ignored by the core until their slices wire them (never misrouted).
- AROS app linkage folds `engine.c` in when live leaves the stub (noted for M1.x).
- Process: audit Phase 0a rejects the English word "preallocated" (contains
  `realloc`) — comment diction is load-bearing in this repo.
