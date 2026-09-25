# §12.9b song track (m66)

**Date:** 2026-09-25 (host lane; devices untouched)
**Scope:** dense 999×4 selection track, downbeat capture, change-only
emission, measure edits, STRK codec. OUT: automation, streaming
changeover, GUI editor.
**Disposition:** New (reviewed plan, native execution, TDD t59)

## What landed

- `engine/seq/songtrack.{h,c}` + `songtrack_emit.h`: model, capture,
  measure/walker emission with caller-owned carry, init/clipboard
  edits, run-free (view deferred per review).
- `project/rbng.*`: STRK chunk (exact 3996, slot ≤ 31, minor-0 rule),
  `track` on `RISong`, minor forced to 1 with banks-or-track.
- t59: model, emission, edits, codec, replay + sweep properties.

## E1 timing split (the load-bearing distinction)

Authority immediate (p. 72, song overrules the hand); sounding at
pattern end (p. 20, streaming slice). This emitter marks downbeat
selections only. Capture quantizes forward to the next bar (p. 76);
the model writes exactly the bar it is told.

## Loop-phase wrap (why the 4-bar test was blind)

`bar = start` collapse emits 6 events on the 8-bar probe and passes
the old 4-bar count; phase-preserving modulo emits 10 with bar-4/5
alternation in the tail. Count alone cannot see it — the tail pins it.

## Minor-forcing fix

A track-without-banks file written minor 0 is rejected by its own
reader; the writer raises minor iff banks OR a non-empty track.

## Self-healing carry

The carry mirrors EMITTED events (known bits set only on send); a
cap-dropped change stays pending and re-sends next measure — late,
never lost. Mirroring the track instead loses it silently.

## Refusal instead of masking

Every slot entry point refuses >31 all-or-nothing (capture, bulk
fills, pastes, codec validate-before-store). Masking would turn
corrupt 40 into a real, wrong pattern 8.

## Deferred (with rows)

Run view → GUI song-editor slice. Pattern-mode capture gating test →
record-path slice (no record path exists here to test).

## Gates

- t59 RED (missing header, then behavioral per task) → GREEN.
- Full `ri_audit.sh` 0/0 (baseline 0/0 before).

None from the reviewed plan. One plan-text casualty: the Task 2
`ev[n-1]` tail asserts needed `n`-guards — a RED must fail, never
bus-fault (found via stub run, fixed in the test).

## Mutation proof (8)

- (a) `!=3996` → `>`: t59 badlen accepted.
- (b) `bar = ls`: phase count 6.
- (c) `>=999` → `>`: bar-999 read.
- (d) bare `start+len` clamp: hard fault (SIGSEGV, caught by exit code).
- (e) carry mirrors track: cap state 15.
- (f) raw loop: raw loop count 6.
- (g) merged validate/store: slot32 partially stored.
- (h) `track_row_ok` → 1: init-song refusal.
(Failure lines recorded from the runs; (d) has no assert line by nature.)
