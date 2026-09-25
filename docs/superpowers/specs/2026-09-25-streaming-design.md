# Streaming player (third §12.9 sub-slice) — design

**Date:** 2026-09-25. **Status:** owner-approved (§§1–3 in chat).
**Scope:** per-section loop phase, pattern-end changeover, per-block
emission from track state on the live path. Explicitly OUT:
automation lanes, GUI, tempo editing, engine-voice changes (the engine
keeps consuming event lists).
**Fidelity order:** ReBirth RB-338 Owner's Manual (E1); unverified
choices are ledger rows.

## 1. State + advance (new `engine/seq/player.h`)

- `RIPlayer {phase_ticks[4], sounding_slot[4], pending_slot[4],
  track_carry}` — phase = position within the sounding pattern per
  instance; sounding = what's heard; pending = downbeat selections
  awaiting pattern end; carry = the track walker's `RITrackCarry`
  reused across blocks.
- Advance per block from the transport tick cursor: `phase +=
  block_ticks` per instance; pattern length from the instance's bank
  (`banks[instance].pat[sounding_slot].length × step_ticks`, banks
  held as non-owning pointers set at player init); on phase wrap, `sounding_slot ← pending` (falling back
  to the track selection at the new pattern start when no pending
  selection exists), `phase -= length`. All tick-domain, no samples,
  no allocation.

## 2. Emission per block

- (a) Pattern contents for sounding slots through the existing
  per-slot emitters (303/drum, cyclic carry across pattern wraps).
- (b) PATTERN_CHANGE at crossed downbeats through the track walker
  (recorded truth).
- (c) NOTHING for pending selections until their section's pattern
  end (sounding truth deferred per section — the p. 20 rule, live).
- Cap-guarded like existing emitters (later bars/instances drop
  first, deterministic).

## 3. Testing

- New `t60_player`: phase advance with unequal pattern lengths
  across instances, changeover exactly at pattern end (not at the
  downbeat), pending overwrite chains (two selections before one
  pattern end → latest wins), loop wrap + song end, block-split
  renders identical to whole-run emission.
- Existing suites green unmodified. Full `ri_audit.sh` 0/0.
