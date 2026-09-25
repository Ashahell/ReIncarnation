# Transport state machine (first §12.9 sub-slice) — design

**Date:** 2026-09-25. **Status:** owner-approved (§§1–5 in chat).
**Scope:** transport states/transitions, bar position mapping, loop
model, record/display state. Explicitly OUT: song track content,
pattern-change emission, automation capture, GUI widgets (sibling
lane), tempo editing.
**Fidelity order:** ReBirth RB-338 2.0.1 Owner's Manual (E1, pp.
145–146); unverified choices are ledger rows.

## 1. States + transitions (E1 pp. 145–146)

States: STOPPED / PLAYING / RECORD (+ loop on/off, record-armed).
Stored in `RISeq` beside its clock/loop/snapshot logic (one position
owner). All transitions are pure functions returning the cursor.

- Play from STOPPED: continue at the held cursor (E1: Play = continue).
- Stop clicks: 1st → STOPPED, cursor held; 2nd → cursor to loop start
  (song start when loop is off); 3rd → song start. Any Play resets the
  click count.
- Rew/FF: ∓10 bars per press, clamped at song bounds (0, song bars);
  held-key repeat lives GUI-side; the step is the fixed constant 10.
- Record button: from STOPPED → RECORD at the held cursor (plays and
  records); from PLAYING → punch in (`is_recording` on, cursor
  unchanged); from RECORD → punch out (stays PLAYING). Stop from
  RECORD → STOPPED (click count consumed as §1).

## 2. Position mapping (tempo-map aware)

Position lives in samples (existing clock) + ticks. Bar = ppq*4 ticks
(4/4 fixed, E1). New queries: `ri_seq_bar_at_tick` (tick → 0-based bar
via map segments), `ri_seq_tick_of_bar` (bar → start tick),
`ri_seq_bar_display` (bar + beat 1–4 + sixteenth 1–4 for the panel).
Segment-crossing bars resolve in the tick domain; the single rounding
happens at sample conversion (clock convention). Loop bounds store as
bars and resolve through the same queries.

## 3. Loop model

`{on, start_bar, len_bars}`, songs ≤ 999 bars (E1). The resolved sample
region feeds the existing `RiSeqLoopPos` unchanged. Loop edits
quantize to bar starts.

## 4. Record + display

Record = armed flag + punch state; pattern-change capture arrives with
the song-track slice — this slice stores the flags and exposes
`is_recording`. Display = `{bar, beat, sixteenth}` struct (1-based
beat/sixteenth for the panel).

## 5. Testing

New `t58_transport`: full transition table (incl. the 3-stop sequence
and click-count reset), ±10-bar steps with song-bound clamps,
bar/tick round-trips across a 2-segment tempo map, loop wrap
positions, record flag matrices. Existing clock/seq suites green
unmodified. Full `ri_audit.sh` 0/0.
