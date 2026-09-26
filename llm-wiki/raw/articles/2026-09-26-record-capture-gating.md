# §12.9c record-path capture gating (m68) — the songtrack plan's deferred R10 test

**Date:** 2026-09-26. **Status:** shipped (feat `b7e4896`).
**Scope:** songtrack plan R10 deferral + spec §5 open item ("Pattern-mode
capture gating — record-path slice owns it; test moves there"). One
function, one test scope, no build/audit wiring changes (songtrack.c
already in MOD_sched; t59 + static-grep + AROS TU already wired).

## Design (spec-forced)

- `int ri_record_capture(struct RISongTrack *t, uint8_t rec_state,
  uint64_t cursor_ticks, uint32_t ppq, uint32_t instance, uint8_t slot)`
  (`engine/seq/songtrack.h/.c`). The model stays gate-blind.
- Refuse-first: `rec_state != RI_TR_RECORD` → rc 2, track untouched
  (off-record writes nothing, not even quantized). On RECORD:
  `bar = ri_bar_quantize_next(cursor, ppq)` (t58 law reused: downbeat
  keeps its bar, mid-measure moves forward, clamps to 998, never 999),
  then delegate to `ri_track_capture` (NULL / slot > 31 / rc-2 laws
  inherited, no re-derivation).
- Ruling R-GATE-SHAPE: state travels as a BYTE (caller passes
  `tr->state`) because t59's layer guards ban the `RITransport` text in
  both songtrack headers. Cost if wrong: guard failure (proved by the
  audit) or a struct-typed param nobody can declare.

## Gates

- TDD RED-first: 4 lines failed on the refuse-all stub (first:
  `t59_songtrack.c:611 rec downbeat rc`); GREEN after the body.
- Scope pins: STOPPED/PLAYING refused at any cursor (track stays
  empty); RECORD downbeat/mid-measure/998-edge land 1/2/998;
  NULL + slot-32 refused with prior content kept.
- Mutants killed: gate-inverted (off-record asserts), no-quantize
  (`rec mid stored next`).
- Full `ri_audit.sh` 0/0 (log `/tmp/ri/audit-recgate.log`); existing
  goldens unmoved; sibling lane files untouched.

## Open (unchanged)

- Automation lanes (§12.9c remainder: control-event stamp seam per
  spec §3, init-song/loop knob clearing).
- Todo ticked for gating only (m68).
