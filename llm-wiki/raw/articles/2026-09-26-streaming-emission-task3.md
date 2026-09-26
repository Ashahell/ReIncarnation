# §12.9c streaming emission Task 3 — merge, cap/heal, split-identity, loop/end, R4 carry

**Date:** 2026-09-26. **Status:** shipped (tests-only commit `07cc95b`;
production complete in Task 2). **Supersedes** the m67 record's mutant
tally: **9/9 killed** (no survivor).

## Scope (plan Task 3, tests-only unless a test reds)

`t74_player.c` emission scope: merge + §8 sort + window filter;
determinism (byte-identical rerun); cap truncate + pending-current +
deterministic recount + walker-carry heal across blocks; split-identity
whole-vs-pieces (modulo seq); loop fold [4,6) with phase preservation;
song-end past bar 999 (walker silent, content marches). No production
change — the one exception below is test-only too.

## R4 START-carry scope (the only new test)

Mutant (e) (persist END-carry) survived the planned split scope.
Root cause, proven by probe: that scope never resumes mid-occurrence
with a differing carry — drum flips ignore carries by construction and
the 303s never flip there. Dedicated scope: local track with a dev1
flip 0→1 at bar 1, BB.pat[1] tied new occurrence (slide step 15, gate
held), piece boundary mid-occurrence at tick 400. Whole-vs-pieces
(modulo seq). First version was incoherent (upfront pending flips at
the first old end + shared-track dev2 pollution) and failed on correct
code 39-vs-51; rewritten to flip-inside-one-piece with local track.
Mutant (e) now fails `carry split count 24 vs whole 23` + `carry split
diverges in 6 events`. Ruling R-CARRY-SCOPE — cost if wrong: none
beyond the test (production untouched; R4 stands).

## Mutant ledger (all 9, each reverted after kill)

(a) stub → `pending sampled at downbeat`; (b) event-sourced pending →
`pending current despite cap`; (c) phase-losing fold → `loop fold
preserves phase`; (d) aliased carry → `split count/diverges`;
(e) END-carry → `carry split count/diverges` (new scope);
(f) no sort → `unsorted`; (g) stale flip → deferral + slot-4 lines;
(h) no zero-check → `corrupt length silent`, no hang (trip-count
backstop holds); (i) walker-only pending → `pending current despite
cap` (H5 ninth mutant: event-driven pending fails the cap test).

## Gates

`t74_player` PASS; full `ri_audit.sh` 0/0 (`/tmp/ri/audit-129c.log`);
audit wiring (t74 line, player.c static-state grep, AROS TU) already
present — verified, not re-added. Worktree clean at commit; sibling
§12.10 files untouched.
