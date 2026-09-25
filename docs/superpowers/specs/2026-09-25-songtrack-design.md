# Song track (second §12.9 sub-slice) — design

**Date:** 2026-09-25. **Status:** owner-approved (§§1–4 in chat).
**Scope:** dense pattern-selection track, change-only PATTERN_CHANGE
emission, record-gated capture, STRK codec. Explicitly OUT: automation
lanes, streaming emission, GUI song editor, tempo editing.
**Fidelity order:** ReBirth RB-338 2.0.1 Owner's Manual (E1);
unverified choices are ledger rows.

## 1. Data model (new `engine/seq/songtrack.h`)

- `RISongTrack {nbars 1..999, slot[999][4]}` — slot 0..31 per
  instance (0–3 = 303A/303B/808/909, matching event device ids, route
  sections, and bank instances). Caller-owned, no allocation (~4 KB).
- Unwritten tail = keep-previous: init fills slot 0 everywhere.
- `selected(bar, instance)` clamps bar into `[0, nbars)`; instance ≥ 4
  returns slot 0 (fail-closed).
- Capture writes only through `ri_track_capture` (the caller gates on
  `state == RECORD`; pattern mode plays selected patterns without
  writing). Capture clamps bar the same way.
- Song length in bars is owned by the track (`nbars`); transport
  seeks take `song_bars` from the track.

## 2. Emission

- At each bar line crossed in a render window, emit PATTERN_CHANGE
  per instance whose slot differs from the previous bar (device =
  instance, value = slot, sample = bar-start tick through
  `ri_map_tick`). Changes take effect immediately at the bar line
  (E1).
- Pure function over (track, banks, bar range, map) → events; pattern
  contents emit through the existing per-slot emitters; cap-guarded
  like existing emitters (later bars drop first, deterministic).
- Range start always emits the four current slots (establishes
  downstream state); later bars emit change-only. No other events
  when nothing changes.

## 3. Codec (`STRK`, v1.1)

- Body: u16 nbars + nbars×4 slot bytes, row-major (bar, then
  instance). Exact-length check; rejects: nbars 0 or > 999, any slot
  > 31. Minor-0 files never carry STRK (same rule as BANK).
- v1.0 files (no STRK): single-bar track, slot 0 everywhere —
  playback identical to today by construction.

## 4. Testing

- New `t59_songtrack`: selection (incl. tail-keep and clamp),
  capture gating (RECORD writes, pattern mode does not),
  change-only emission with exact bar-line samples, STRK round-trip +
  each reject, v1.0 default, 999-bar boundary.
- Existing suites green unmodified. Full `ri_audit.sh` 0/0 (t59 wired
  beside t58).
