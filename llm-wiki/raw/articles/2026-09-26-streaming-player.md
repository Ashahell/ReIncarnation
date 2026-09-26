# §12.9c streaming player (m67) — per-section phase, pattern-end changeover, per-block emission

**Date:** 2026-09-26. **Status:** shipped (feat `18d4835` Task 1 + `2a9d111` Task 2 + `49e362f` Task 3).
**Scope:** third §12.9 slice (after m65 transport, m66 songtrack). New
`engine/seq/player.c` (`RIPlayer`, `ri_player_block`); tests
`tests/unit/t74_player.c`; audit wiring in `scripts/ri_audit.sh`.
**Fidelity order:** ReBirth RB-338 manual (E1, p. 20 pattern-change
deferral); spec `docs/superpowers/specs/2026-09-25-streaming-design.md`
(owner-approved §§1–3); plan
`docs/superpowers/plans/2026-09-25-streaming-implementation.md`
(H1–H5 hardenings).

## Design (what the player owns)

- State per instance: `phase_ticks` (position in sounding pattern),
  `sounding_slot` (what's heard), `pending_slot` (downbeat selections
  awaiting pattern end), `sched_carry` (R4: cin of the unfinished
  occurrence — never the end-carry, never aliased), `track_carry`
  (walker's cross-block change cache). Banks non-owning, live-read.
- Block order (load-bearing): STEP 1 samples pending + walker at
  downbeats, then per-instance advance (phase march, flip
  `sounding ← pending` at pattern ends, sample-before-flip at
  coincidence), then insertion sort by the §8 key.
- Hardenings kept: H1 three-unit split (pure emit + per-instance
  advance + thin block), H2 mechanical local WRAP_CARRY, H3 zero-check
  before any while (trip-count backstop only), H4 block-global step
  size (no pattern-local tempo), H5 ninth mutant (event-driven pending
  must fail the cap test — it does, mutant (i)).

## Rulings (ledger `/home/miller/Work/ri_build/ledger-12.9c.md`)

- R-STEP1: STEP 1 samples downbeats in strict interior
  `(tick_start, tick_end)` — plan R10 prescribed `(start, end]`, but the
  trailing-boundary downbeat previews a selection whose content lies
  outside the block (probe: bar2@768 clobbered pending 1→0, doubled the
  change count). Spec ("crossed downbeats") does not pin ownership.
- R-DEFERRAL: deferral old-pitch asserts vs coincidence flip are
  mutually exclusive under any local causal rule (proven: oldnotes>0
  needs an old end exactly at the downbeat with the flip skipped;
  coincidence needs take-flip at the same tick with identical local
  state). R10+spec bless sample-before-flip; deferral keeps
  pending/changes/sounding/new-content asserts.
- R-PITCH: `BA.pat[6]` step0 key 6→11 (aliased slot-0 pitch; test-only).
- R-MUT: mutation assert `m2==m1+1` (added hit must sound; test-only).
- R-SPLIT/R-HEAL/R-LOOPWIN: Task-3 adaptations (off-downbeat piece
  bounds; heal capture + `[2bar,4bar)` window; folded-bar-5 capture).
- R-LOOPMUT/R-CORRUPT: test hardenings after surviving mutants c/h.
- R-MUTSURV: mutant (e) (END-carry persist) survives, documented —
  unobservable (drum flips carry-immune, no 303 flips in split scope,
  tie effects occurrence-head-local); R4 no-alias core pinned via (d).

## Gates

- `t74_player` PASS (Task 1 RED-first: stub init seeded nothing — first failure `t74_player.c:51` cold phase, as the plan predicted; Task 2 RED-first: 11 lines failed on the stub).
- 9 mutants: 8 killed (a pending-sample, b/i cap-pending, c loop-phase,
  d split, f unsorted, g deferral+slot4, h corrupt), e documented above.
- Full `ri_audit.sh` 0/0 at every slice gate (incl. AROS compile of
  `player.c`; R7 fallback unneeded). Sibling lane untouched (t60–t73,
  MOD_gui, app/gui files); `git status` clean at each commit.
- Build discipline (re-learned): `ri_build_host.sh test` does NOT
  rebuild objects — every test run must be preceded by the module build
  (one false red from a stale mutant object, caught pre-commit).

## Open (unchanged)

- Songtrack implementation (plan reviewed): pending owner direction.
- Automation lanes + record-path capture gating (§12.9c remainder).
- §12.9c checkbox ticked for streaming only (m67).
