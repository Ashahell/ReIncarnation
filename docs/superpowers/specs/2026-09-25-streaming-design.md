# Streaming player (third §12.9 sub-slice) — design

**Date:** 2026-09-25. **Status:** owner-approved (§§1–3 in chat);
**hardened after brutal review — needs owner re-approval before any
plan.**
**Scope:** per-section loop phase, pattern-end changeover, per-block
emission from track state on the live path. Explicitly OUT:
automation lanes, GUI, tempo editing, engine-voice changes (the engine
keeps consuming event lists).
**Fidelity order:** ReBirth RB-338 Owner's Manual (E1; this repo's
`/tmp/rb201-manual.pdf`). Page numbers are the manual's printed numbers.

## 0. E1 evidence (verbatim, located by grep in the PDF text)

- Pattern-end effect (p. 20, "Pattern Changes take Effect on the Next
  Downbeat"): *"When you select a new Pattern, the change doesn't take
  effect until the end of the Pattern. ... this is true for each
  individual Pattern, regardless of its length."*
- Independent pattern looping (pattern-vs-song-modes list):
  *"the currently selected Patterns always loop. In Song mode the Loop
  is used for repeating any section of the Song."* — pattern phase
  marches on regardless of song position; only pattern ends wrap it.

## 1. State + advance (new `engine/seq/player.h`)

- `RIPlayer {phase_ticks[4], sounding_slot[4], pending_slot[4],
  track_carry}` — phase = position within the sounding pattern per
  instance; sounding = what's heard; pending = downbeat selections
  awaiting pattern end; carry = the track walker's `RITrackCarry`
  reused across blocks. Banks held as non-owning pointers
  (`banks[instance].pat[sounding_slot]`, set at player init).
- Cold start: `phase = 0`, `sounding = pending =` track selection at
  the start bar, cold carry. No alternative sources — the fallback in
  the first draft ("or track selection") was dead code dressed as
  design; pending is always current because the player samples the
  track at every crossed downbeat.
- Advance per block from the transport tick cursor, per instance
  independently, in this exact order (order is load-bearing: a
  downbeat coinciding with a pattern-end boundary must not race):
  1. Sample pending from the track at every crossed downbeat (using
     the transport bar of that tick).
  2. Advance phase by the block's tick delta.
  3. While phase >= current live length: `sounding_slot ←
     pending_slot`, `phase -= length`.
  All tick-domain, no samples, no allocation.
- Banks held as non-owning pointers (`banks[instance]` set at player
  init). Lengths are read per block from the live banks, so edits
  take effect without restart. Optional pure refresh path for
  pointer swaps (hot-swap/reallocated banks):
  `ri_player_refresh_banks(p, banks)` updates only the four pointers
  — never phase, never sounding. Callers who mutate banks call it;
  everyone else never sees it. Zero cost on the normal path.
- Slot 0 is a first-class pattern number, absolutely (song-track
  law, restated here so no future reader invents a sentinel):
  cold-start and changeover treat it exactly like any other slot.
  There is no "silent" or "no-op" meaning inside the player.
- The player's ONLY source of PATTERN_CHANGE events is the track
  walker invocation per block; the player itself constructs none —
  it consumes the walker's selections into pending and applies the
  sounding deferral. Stated absolutely so the two truths can never
  be re-collapsed by a future edit.
- Pending source (stated once, absolutely, no event round-trip):
  step 1 above is the ONLY writer of `pending`. Pending never comes
  from the player's own PATTERN_CHANGE events.
- Pattern phase is INDEPENDENT of song position: transport
  sample-wraps, seeks, and loop wraps never reset pattern phase —
  only pattern ends (step 3) wrap it. The song loop constrains which
  bars the walker visits; it does not touch phase. (This is the point
  the first draft left ambiguous, and ambiguity here is a desync bug.)

## 2. Emission per block

- (a) Pattern contents for sounding slots through the existing
  per-slot emitters (303/drum, cyclic carry across pattern wraps).
- (b) PATTERN_CHANGE at crossed downbeats through the track walker
  (recorded truth).
- (c) NOTHING for pending selections until their section's pattern
  end (sounding truth deferred per section — the p. 20 rule, live).
- Cap-guarded like existing emitters (later bars/instances drop
  first, deterministic). Dropped content is recomputed from state on
  the next block — player emission is stateless across blocks except
  the track carry, which self-heals by its own design (m66). No
  R1-class staleness is possible here by construction.

## 3. Testing

- New `t60_player`: cold-start establishment, phase advance with
  unequal pattern lengths across instances, changeover exactly at
  pattern end (not at the downbeat), pending overwrite chains (two
  selections before one pattern end → latest wins), simultaneous
  length edits on two unequal-length instances landing inside one
  block (both changeovers at their own pattern ends, never at the
  song bar line), bank mutation mid-stream, phase independence
  (transport seek/wrap mid-pattern neither resets phase nor moves
  changeover), loop wrap + song end, block-split renders identical
  to whole-run emission.
- Existing suites green unmodified. Full `ri_audit.sh` 0/0.
