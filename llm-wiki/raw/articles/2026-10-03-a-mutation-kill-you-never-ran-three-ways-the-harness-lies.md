# A mutation kill you never ran: three ways the harness lies

- Source: ReIncarnation session, 2026-10-03 (opencode lane; gating `t151`/`t152`/`t154`/`t155` and proving each against the code it claims to test)
- Collected: 2026-10-03
- Published: 2026-10-03
- Prior: [The testability boundary](2026-10-02-testability-boundary-aros-only-code-and-mirrored-tests.md) (why a green test may mean nothing — this is the same disease in the *harness* rather than the test), [Bounding the damage-box build](2026-10-02-damage-box-bounded-build-1p9x-less-gui-work-no-xrun-change.md) (the change `t155` covers), [The audio-failure fix](2026-10-02-a-lost-audio-path-must-not-be-silent.md) (where the same two traps first bit, on a different change)
- Commits: `9816b62` (gates + the `t155` strengthening), `2ecffd0` (the `t158` mutation set)

## The shape of the problem

A mutation run is supposed to answer one question: *does this test actually see
the code it claims to test?* The failure mode is not a mutant surviving. It is
**a kill being reported for a mutant that was never compiled, or never reached** —
which reads as rigor and is the most expensive kind of wrong, because every
downstream conclusion inherits it.

Three separate instances in one session, each looking like a legitimate result.

## 1. A build that fails is not a kill

`ri_core_audio_failure_is_loud` returns `err == RI_AUDIO_ERR_ABSENT ? 0 : 1`.
The obvious mutants are `return 1;` and `return 0;`. Both "failed to kill":

```
  M4 always loud: killed (sha=)
  M5 always quiet: killed (sha=)
```

The `sha=` is the tell — there is no object to hash. `-Wunused-parameter` with
`-Werror` refuses the compile, so **no `.o` is written at all**. A script that
asks "did the test pass? no → killed" reads that as a kill when nothing ran. The
file did not change; the compiler did not agree to build it.

Redone with the parameter still referenced (`? 1 : 1`, `? 0 : 0`) so the mutant
compiles, and both die immediately:

```
  M4 always loud: killed (sha=5561ec058418048b) -- FAIL t158:51: err 2 (ahi.device absent -> documented offline render): loud=1, want 0
  M5 always quiet: killed (sha=6ca90be3416617eb) -- FAIL t158:51: err 1 (msgport): loud=0, want 1
```

**Rule: a mutation verdict without an object hash is not a verdict.** Report
*inconclusive* and change the mutant until it compiles.

## 2. A header mutant leaves every object byte-identical

`ri_rsection_box_why` is `static inline` in `gui/panelui.h`. Mutating it
recompiles the **test translation unit**, not any library object, so
`/tmp/ri/build/*.o` is identical for the mutant and for the clean tree:

```
  M5 upper bound OTHER->BAR vs t152: STALE BINARY (8410028cc56bf9d2) -> mutant never compiled, inconclusive
  M8 COUNT 4->3 vs t154:           STALE BINARY (8410028cc56bf9d2) -> mutant never compiled, inconclusive
```

M8 was a *second* false signal on the same day, and note what the identical
hashes were: the same value in both cases, because the test binary was stale
rather than the objects. Verified by deleting the test binary, rebuilding, and
comparing `sha256` of the **binary** — then both mutants died.

This one is nastier than trap 1 because the build genuinely succeeds. Every
check that looks at the object directory says "fine".

**Rule: hash the artefact the mutant actually lands in.** For a `.c` in a
library module that is `*.o`; for a `static inline` in a header it is the linked
test binary.

## 3. The wrong test cannot see the constant

M8 (`RI_RSEC_BOX_COUNT 4 → 3`) was first run against `t152`. `t152` never
mentions `RI_RSEC_BOX_COUNT`, so the header edit could not reach it — the report
was "STALE BINARY", which reads as a build problem and is really **wrong test
selection**. Against `t154`, which does reference it:

```
  M8 COUNT 4->3 vs t154: killed (bin 7fd82ba0027251af) -- FAIL t154:64: four reason codes (3)
```

The trap generalises past headers: any mutant whose effect lives in a symbol the
chosen test never calls will report the same way, and the obvious fix — rebuild
harder — will never help.

**Rule: before believing an inconclusive, confirm the test references the
mutated symbol at all.** `grep` the test for the constant or the function name.

## 4. And the one that is not a trap: a stale object after the run

After the `t155` mutation loop, `t155_damage_clip_build` failed **eight**
assertions after having passed minutes earlier, with no source change. Cause: the
mutation harness leaves the last mutant's `.o` in `/tmp/ri/build`, and
`ri_build_host.sh test` links `"$OUT"/*.o` **without rebuilding library
objects** — documented, and still hit.

```
rm -f /tmp/ri/build/*.o && bash scripts/ri_build_host.sh all   # 89 objects
PASS damage_clip_build
```

**A sudden mass failure immediately after a mutation run is the leftover object
until proven otherwise** — not the source, and not the test.

## Equivalence is a result, not an excuse

One mutant survived and was genuinely, provably equivalent:

```
  M1 wake max `>` -> `>=`:  *** SURVIVED ***
```

`ri_livedrv_report_wake` only ever raises `wake_us_max` from 0, so at
`us == wake_us_max` the store rewrites the identical value and the strictness of
the comparison is unobservable. It was **replaced** with two real mutants (drop
the guard; store 0), both of which die — per the testability article's rule 4,
*never keep a mutant you cannot kill*.

The distinction that matters: **an equivalent mutant is a fact about the design
and is worth writing down; a stale build is a fact about the harness and is
worth nothing.** Both print `SURVIVED`. Only one of them is knowledge.

## The test that was genuinely blind

One survivor was not equivalent. `t155_damage_clip_build` survived removing
`dl->clip = 0u` from the empty-box branch of `ri_dlist_set_clip`, and the reason
is a shape this wiki had not recorded before:

```c
static void build(struct ri_dlist *dl, ..., int cx0, int cy0, int cx1, int cy1) {
    ri_dlist_init(dl, backing, CAP, sp, 8192u);   /* zeroes clip */
    ri_dlist_set_clip(dl, cx0, cy0, cx1, cy1);    /* called exactly once */
    ...
}
```

Every call site went through a helper that re-initialised the structure and set
the clip **once**, so the branch that clears an existing clip was **unreachable
through the test's own harness**. The test was not mirroring the code; it was
driving it through a setup that skipped the line.

Strengthened with the only arrangement that sees it — a real box, then a
degenerate box on the same dlist:

```c
ri_dlist_set_clip(&d3, 495, 195, 555, 245);   /* a real, tight box */
RI_ASSERT(d3.clip == 1u, "clip is on");
ri_dlist_set_clip(&d3, 555, 245, 495, 195);   /* x1 < x0: degenerate */
RI_ASSERT(d3.clip == 0u, "an empty box must clear the clip, not keep the stale one");
```

Mutant killed at `t155_damage_clip_build.c:180`. That sequence is also the one
that matters in production: a damage box that comes back empty must stop
clipping, or the next commands are tested against a stale rectangle.

And the finding that makes the whole exercise worth recording:
**`ri_dlist_set_clip` has no production caller.** `grep` across the tree finds it
only in `canvas.c`'s definition, the header, and tests. A green mutation set on a
function nothing calls is the testability article's failure mode arriving from a
completely different direction.

## Gating, and proving the gate

Four passing-but-ungated tests (`t151`, `t152`, `t154`, `t155`) now run in
`scripts/ri_audit.sh`; `t156` was deliberately left alone because it belongs to
another lane that was mid-edit on it. The suite runs **157** gated tests.

> **SUPERSEDED 2026-10-03 (count only).** Both facts below have since changed, and
> the changes are recorded rather than left to rot: `t156` **is** now gated — the
> lane that owned it landed (`a98691a`…`c7c2e1d`) and the test passes — and the
> suite runs **182** gated with **one** named exemption, after
> [26 tests were never gated](2026-10-03-26-tests-were-never-gated-and-two-had-already-gone-stale.md)
> closed a further 21 gaps, two of which (`t51`, `t107`) had already gone stale
> against their own headers. Phase 0d now fails the audit on any ungated test, so
> the 157 figure cannot go stale the same way twice.

A gate never observed failing is not known to work, so it was checked by
inverting a gated test's expectation to a wrong-but-compiling value:

```
FAIL: t155_damage_clip_build (display-list build bounded to the damage box)
REAL audit exit=1
GATE WORKS (nonzero on a broken gated test)
```

The first attempt at that control replaced an expectation with a literal, making
a variable unused, which `-Werror` rejected — a failure, but a *compile* error
rather than the behavioural failure intended. The control has to fail the way the
real bug would fail.

## Method findings

- **Inconclusive is a verdict.** "The mutant did not compile", "the binary did
  not change" and "the test cannot see this constant" all print like results.
  None of them are.
- **Hash the artefact the mutant lands in, and hash it before and after.** One
  sha per mutant, recorded in the output, is the cheapest possible guard.
- **Prefer a mutant that compiles to a mutant that is clever.** Two of the three
  traps existed only because the mutant was degenerate.
- **A survivor is a question, not a verdict.** Three of the five `t151`/`t155`
  survivors were harness artefacts, one was provable equivalence, and one was a
  real hole in the test. They are four different things and the harness gave all
  four the same output.