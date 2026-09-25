# Song track plan review (§12.9b): eleven findings, all applied, verified by assembling the plan's code

- Source: ReIncarnation commit `39d8de4` — `docs/superpowers/plans/2026-09-25-songtrack-implementation.md` (section "Review 2026-09-25") + companion spec edits in `docs/superpowers/specs/2026-09-25-songtrack-design.md`
- Collected: 2026-09-25
- Published: 2026-09-25

## Verdict

The model, edit and codec designs are sound and the tests were already sharp (the phase pin, the overflow pin, the minor-forcing fix). The defects were consistency failures: plan vs spec, plan vs its own laws, plan vs its own TDD rule. None changes owner-approved behaviour.

## Findings (R1–R11) and fixes

| # | Severity | Finding | Fix |
|---|----------|---------|-----|
| R1 | HIGH | Emitter carry mirrored the TRACK even when the cap dropped an event → the dropped change was never re-sent (wrong pattern until the next real change). | Carry mirrors what was EMITTED (`known` bit mask + `prev[4]`); a capped change is re-sent at the next measure — late, never lost. |
| R2 | HIGH | Range walker used a raw loop: `start 995 len 10` walked into bar 999 and stopped while transport (which clamps) keeps looping. | Walker clamps a local copy with `ri_loop_clamp(&l, RI_SONG_BARS)` first. |
| R3 | HIGH | Spec §6 replay property and emission sweep silently dropped (the run VIEW deferral is a different thing). | Replay over a change-dense 999-bar track + sweep (4 + 998 = 1002 events) added. |
| R4 | MEDIUM | Every task went scaffolding RED → full body: the required behavioral RED never happened. | Step 3a per task: stub bodies, record the first failing `RI_ASSERT`. |
| R5 | MEDIUM | Task 2 narrowed Task 1's layer guard; `rbng.h → songtrack.h` would drag sched/clock into the project layer. | Emitter split into `songtrack_emit.h`; guard never narrowed; second guard on the emit header. |
| R6 | MEDIUM | Bulk writers masked slots (`& 31`: 40 → pattern 8), contradicting the fail-closed law of capture and codec. | Validate-then-write, all-or-nothing, `int` rc (0 ok / 2 refused); copy/cut stay `void`. |
| R7 | MEDIUM | STRK reject tests passed on any error; the bad byte was the FIRST body byte, hiding partial stores. | Reject tests assert their own `err` text; bad byte is the LAST (bar 998, instance 3); `parse_strk` validates before storing. |
| R8 | MEDIUM | Task 1 edited `scripts/ri_build_aros.sh` line 56 — the §12.10 GUI proof-app TU list — which never calls songtrack and is not what the audit builds. | Dropped; songtrack.c added to the audit's AROS compile-only loop (no engine/seq TU was AROS-compiled by the audit before). |
| R9 | MEDIUM | "No mutable static state in songtrack.c" had no enforcement point. | Audit grep for non-const file-scope statics. |
| R10 | LOW | Spec still said init-loop "CLEARS the rest of the loop" (contradicts its own E1 p. 176 quote), showed the old `prev[4], force` emitter and "(track, banks, …)" inputs; spec §6's "pattern-mode path never calls capture" has no code in this slice. | Task 0 Step 4 amends the spec; capture-gating test deferred to the record-path slice. |
| R11 | LOW | Quantizer cases already pinned in t58 (`q exact`/`q mid`/`q late`); nits (a "4-bar paste" comment for a 2-bar clip, `%u` casts). | Cited; fixed. |

Checked and accepted as-is: local `seq = 0` per range call (same as `pattern_emit.c`; the sort key reaches `seq` only after sample/type/device/voice); a separate `ppq` argument beside `map->ppq` (same as `ri_sched_emit_*`); the 999-bar cap per walker call (the carry crosses calls).

## Verification method (reusable)

The revised plan's code blocks were assembled verbatim into a scratch `git worktree` at `879be9e` (Tasks 1–4 plus the rbng edits as written) and built with the repo `CFLAGS` (`-Werror -pedantic -ftrapv`), with a separate build output dir so the live tree's objects were untouched: `PASS songtrack`. Each of the eight mutants listed in the plan was applied and each FAILED (the clamp-before-add mutant (d) as a hard fault, rc 1, no assert line). The run caught one defect in the revision itself — the emit header's comment named the codec header and tripped its own grep guard — fixed before commit. Lesson: a plan that ships verbatim code is only as good as a real build of that code; assembling it in a throwaway worktree costs minutes.

## Coexistence

The plan and spec were uncommitted work of the opencode session; the review was applied on top and committed together (`39d8de4`, co-authored). Execution of the plan stays with that session.
