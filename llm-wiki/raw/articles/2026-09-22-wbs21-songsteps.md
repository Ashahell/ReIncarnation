# 2026-09-22 — WBS 2.1 song→steps converter (builder step 1, TDD)

> Source: session evidence (test output, mutant run, audit), compiled by agent
> Collected: 2026-09-22
> Published: 2026-09-22

## Disposition
New. TDD work unit (RED watched, mutant-caught, GREEN verified, audit
0/0). First half of the snapshot builder (song → immutable event
lists); walker feed + file-level golden path are next.

## What (minimal)
- `engine/seq/songsteps.h` / `songsteps.c`:
  `ri_song_to_steps(song, out, cap)` — bounded copy of
  RBSongStep.{note,flags} to RIStep (layouts identical by contract,
  rbng.h + t1_formats §16). NULL/0-cap → 0; truncates at min(nsteps,
  cap, RI_RBNG_MAX_STEPS). REST passes through (walker owns gate
  semantics); automation/mods are not steps (later work).
- `tests/unit/t21_songsteps.c`: hand-built 5-step song (plain/slide/
  accent/rest/flam+accent) → count + per-step asserts; cap truncation,
  NULL song/out, cap 0, empty song. Hermetic (no files; struct built
  in code).
- Build/audit: `MOD_sched` gains `songsteps.c`; Phase 6b runs the
  third seq test.

## Proof
- RED watched: missing `songsteps.h` (feature-missing).
- Load-bearing proven by mutant (FLAM bit masked → `FAIL step4 52/2`,
  exit 1) with revert verified.
- GREEN: `PASS t21_songsteps`; full `ri_audit.sh` 0/0.
- Note: `sed` for edits failed twice this session (pattern drift +
  a mangled newline); editor tool used throughout after.

## Files (uncommitted)
- `engine/seq/songsteps.h`, `engine/seq/songsteps.c`,
  `tests/unit/t21_songsteps.c`, `scripts/ri_build_host.sh`,
  `scripts/ri_audit.sh`.