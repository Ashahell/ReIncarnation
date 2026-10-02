# The testability boundary: `gui/widgets/` and `app/` cannot be reached by a host test, and a mirrored test survives every mutant (2026-10-02)

- Source: ReIncarnation session, 2026-10-02 (opencode lane; the third occurrence of the same structural failure)
- Collected: 2026-10-02
- Published: 2026-10-02
- Prior: [2026-10-02-damage-box-bounded-build-1p9x-less-gui-work-no-xrun-change.md](2026-10-02-damage-box-bounded-build-1p9x-less-gui-work-no-xrun-change.md), [2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md](2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md) (instances two and three), [2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md](2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md)

## The rule

`gui/widgets/*.mcc.c`, `gui/widgets/rsection.h` and everything in `app/` are **AROS-only**. They `#error` on a host build because they need the Zune custom-class API or `exec/types.h`. So:

> **No host test can reach them. Therefore no host test can kill a mutant of them. A test that mirrors their logic instead survives every mutation of the real code — which is worse than no test, because it reports green.**

This is not a discipline lapse; it is a property of the tree, and it has now cost three separate investigations in one lane.

## The three occurrences

**1. `t153` and the volume table.** The candidate-volume list belongs in `platform/aros/fs_aros.c`, which is AROS-only. The first `t153` therefore declared its own copy of the list. **All eight mutants of the real file survived it.** The mutation set was theatre: 8/8 survived, not 0/8, and a reader would reasonably have concluded the code was well covered. The fix was `platform/pal/ri_pal_sticky.h` — one shared header included by both — after which 5/5 died behaviourally.

**2. The repaint-reason codes.** `RI_RSEC_BOX_*` first went in `gui/widgets/rsection.h`, so `t152` could not include it at all (`#error "rsection.h is AROS-only"`). The policy had to move to `gui/panelui.h` before it could be tested at all. Nothing was lost by moving it — it was never widget-specific, it was a policy that happened to be filed next to its only caller.

**3. The AROS wiring for the damage clip.** `mut_fixM5.txt` covers `gui/draw/canvas.c` and `platform/pal/ri_pal_draw.h`, which the host build compiles. Its caller — `gui/widgets/rsection.mcc.c` — is untestable, so the wiring is proved **on target** instead, by the scripted harness. The set says so in its own header rather than pretending to cover more.

## The tell, and what to do instead

The tell is always the same shape: a test file that re-declares a constant, a struct, or a function that already exists in production code. Ask of any such test — *which mutation of the real file does this kill?* If the answer is none, the test is decoration.

Prefer, in order:

1. **Put the policy in a host-compiled translation unit.** `gui/panelui.h` and `platform/pal/ri_pal_sticky.h` are both host-includable and both exist because a test forced them into being.
2. **Make the test read the production data**, not a copy. `t153` finally kills mutants because it includes the real header.
3. **If the code is irreducibly AROS-only, test it on target** and say so in the mutation file's header. An honest `mut_fixM5.txt` that covers half the change beats a `mut_fixM5.txt` that claims all of it and kills nothing.
4. **Never keep a mutant you cannot kill.** If it survives because the behaviour is equivalent, that is a fact about the design — replace the mutant and record why (see the `ri_dlist_init` clip reset in the bounded-build record).

## Why this matters more than it looks

The lane's own history is the argument. Over three days the same shape produced: a wake-latency metric that could not be cross-checked, a repaint-reason split that silently reported `0/0/0` for four runs, a volume table with no live test, and a mutation set that was 8/8 theatre. **Every one of those was found by on-target measurement or by a reviewer, not by the test suite** — because the test suite structurally could not see them.

A green suite in this tree means *the host-compiled parts are covered*. It does not mean the widget and app layers are covered, and it is not evidence for claims about them.

## Method findings

- **A mutation set that kills nothing because the test cannot see the code is worse than an absent one**, because it converts a known gap into a false assurance. `8/8 survived` should have been read as *my test is not connected*, not as *my code is well covered*.
- **When a test cannot include the thing it tests, that is a finding about placement**, not about the test. Two of the three fixes here were a file moving by fifty lines.
- **A mirrored constant is the commonest form.** Copying a `volatile` volume list into a test is easy to write and silently worthless.
- **A green suite's claim is bounded by what the host build compiles.** Saying so in the record prevents the next lane from citing it as coverage of the widget layer.

## Standing gaps

- The widget and app layers have no unit coverage at all, and by construction they cannot get any without either a host-buildable extraction or a host AROS harness. Neither exists.
- `mut_fixM4.txt` and `mut_fixM5.txt` are explicitly partial, by design rather than by omission. There is no automated check that a mutation set's scope matches its claims.
- No lint rule enforces the placement rule; it is currently a convention held in this one lane's head.

## See Also

- [Bounded damage-box build: 1.9× less GUI work, and no change to the xruns](2026-10-02-damage-box-bounded-build-1p9x-less-gui-work-no-xrun-change.md)
- [Where `RIAPP.LOG` lives, and why file size cannot tell you which lane you built](2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md)