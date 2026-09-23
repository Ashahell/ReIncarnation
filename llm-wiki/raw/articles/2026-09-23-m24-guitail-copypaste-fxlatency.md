# 2026-09-23 — M2.4 GUI-tail host cores: TC-2.10.3 copypaste + TC-2.11.1 FX latency (no production change)

> Source: session evidence (test output ×3, audit 0/0 ×1 final + 1 pre-change baseline self-invalidated, see Proof), compiled by agent
> Collected: 2026-09-23
> Published: 2026-09-23

## Disposition
New. M2.4's GUI tail (2.9-full/2.10/2.11) is mostly on-device work,
but two TCs have host-testable cores, and both were unpinned:
TC-2.10.3 (copy/paste preserves accent/slide) and TC-2.11.1 (FX
knob audible within 1 buffer). Two pins (`t27_copypaste`,
`t27_fxlatency`), wired into the sequencer + Phase 10 audit
phases. Both green first run (property pins) — no production code
written. Explicitly out of host scope (reasons on record, not
waved): TC-2.10.2 (no engine switch API exists — pattern switching
is app-side unimplemented; the staged-swap arbiter it would ride
is pinned by t21_seqswap), TC-2.9.4/2.9.5 (artwork missing /
MCC-path AROS-only), TC-2.10.1 (human tester), TC-2.11.2
(OPEN-04, same as TC-2.5.1).

## What
- `t27_copypaste` (TC-2.10.3): 16 steps exercising every flag bit
  (REST/SLIDE/ACCENT/FLAM, pairs, plain) round-trip write → read
  → `ri_song_to_steps` with note AND flags bit-identical (step
  bits equal walker RI_STEP_* bits, so preservation here is
  preservation into playback). Ledger: `formats/rbng.md` gains a
  TC-2.10.3 section.
- `t27_fxlatency` (TC-2.11.1): DIST_DRIVE 0 → 127 mid-stream is
  audible in the very next 64-sample buffer — RMS move 0.348
  (bound 0.01, t1's engaged-differs scale), 63/64 samples differ
  (bound 32). The latency proof is positional (buffer N+1 already
  differs — a smoothed/deferred param would equal the drive-0
  continuation). Ledger: `pcf/engine.md` FX trio gains a latency
  row.
- Audit: `t27_copypaste` after the `t21_songsteps` line (converter
  domain), `t27_fxlatency` in Phase 10 after the t25 block,
  TC-tagged FAIL echoes.

## Proof
- TDD: pins written first, both green on frozen code (property
  pins, t22 precedent). No production cycle in this slice.
- Full `ri_audit.sh` 0/0 final over the committed tree (0 FAIL
  lines). A second audit run launched pre-change as a baseline
  SELF-INVALIDATED mid-flight (rc=2, syntax error at line 417):
  bash parses as it executes, and the run reached the edited
  region after the Phase 10/sequencer insertions shifted it. That
  failure proves nothing about any tree — lesson recorded (never
  edit `ri_audit.sh` under a live run; same class as the CWD-pin
  lesson). The pre-edit phases had passed; the final green run
  stands on its own over the complete tree.

## Files
- tests: `tests/unit/t27_copypaste.c` `tests/unit/t27_fxlatency.c`
  (new; no `engine/` file touched)
- env: `scripts/ri_audit.sh` (songsteps + Phase 10 lines)
- ledgers: `docs/evidence/formats/rbng.md` (TC-2.10.3 section),
  `docs/evidence/pcf/engine.md` (latency row)
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

No external websites consulted: contracts are WBS TC-2.10.3/2.11.1
+ the `RiFXSetParam`-stores-immediately code fact + RBNG/RIStep
flag-equality contract — all local; nothing was open that needed
the web. spirv-val vacuous (no SPIR-V in this repo — noted honestly
per standing).
