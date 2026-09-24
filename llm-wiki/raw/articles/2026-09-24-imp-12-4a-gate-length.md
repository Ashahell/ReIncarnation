# §12.4a — Gate-length rule: notes fall at half step (D-h, E0)

**Date:** 2026-09-24 (host lane; devices untouched)
**Scope:** review §4.1 gate timing + D-h adoption (E0 fraction now). First
vertical third of §12.4; MEG/VEG split, log-domain slide, osc/filter stay open.
**Disposition:** New (TDD RED-first; contract change, not a regression)

## TDD record

`t38_gate_fraction.c`: (a) two plain notes → ON@0, OFF@12t, ON@24t, OFF@36t
(RED: OFFs at 24/48t); (b) slide tie holds (ON, ON+slide, end OFF);
(c) rest+slide tie holds (ON, ON+slide, CONTINUE, OFF); (d) shuffled odd-step
OFF follows its shuffled start + half step; sorted invariant everywhere.

## Production change

`engine/seq/sched.c` (`ri_sched_emit_timed` only): after each non-slide,
non-legato NOTE_ON, emit NOTE_OFF at `start_tick + step_ticks/2` unless the
next step ties (slide flag, any rest/note shape) — then set `gate = 0`, which
suppresses the now-redundant boundary OFFs through the existing bookkeeping.
Cap-exhaustion fails open toward full length. E0 ledger:
`docs/evidence/sequencer/gate-length.md`.

## Deliberate pin updates (each root-caused to the moved OFF, D-h cited)

- `t1_303walk` (1 assertion), `t21_songfile` (1 sample), `t21_schedfeed`
  (2 OFF samples + seq permutation — hand-traced + scratch-dump verified;
  types/values/flags byte-identical), `t21_snapbuild` (2 OFF samples),
  `t21_looppcm` (size pin −3000 samples exactly; halves equality holds).
- `t1_sched` passes UNCHANGED (OFF count preserved by construction).
- 3 first-light goldens re-baselined (wav/events/441/aiff + sha + sox table):
  first PCM diff exactly at the first gate-fall (release vs sustain
  signature); totals shrink by exactly half a step at every rate
  (48k −2572, 44.1k −2363); tails exact 0, no truncation clicks.

## Gates

- Full `ri_audit.sh` 0/0 — all 14 phases (sched-check goldens unchanged:
  that fixture ties everything, so no fractional OFF exists in it).
- Process notes: a misread dump cost one confused round-trip (resolved by
  re-running clean — evidence before synthesis); one heredoc slipped into
  scratch-file creation against the repo rule (removed, Write tool after).
