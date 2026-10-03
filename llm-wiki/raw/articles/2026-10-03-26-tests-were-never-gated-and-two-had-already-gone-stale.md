# 26 tests were never gated, and two of them had already gone stale

- Source: ReIncarnation session, 2026-10-03 (opencode lane; asking whether `scripts/ri_audit.sh` was complete, then gating what it was missing)
- Collected: 2026-10-03
- Published: 2026-10-03
- Prior: [A mutation kill you never ran](2026-10-03-a-mutation-kill-you-never-ran-three-ways-the-harness-lies.md) (four ungated tests found the same day by another lane, and the standing rule that a gate never observed failing is not known to work), [The testability boundary](2026-10-02-testability-boundary-aros-only-code-and-mirrored-tests.md) (why a green test may mean nothing)
- Commits: this session's `audit:` commit; gates for `t151`/`t152`/`t154`/`t155` were `9816b62`, which left `t156` deliberately ungated

## The shape of the problem

The question was narrow — *is the audit script complete and up to date?* — and the
answer had two halves that pull in opposite directions.

It was current: the script was the most recently touched file in the repo, had no
broken references, no placeholder markers, and passed `AUDIT 0/0` from a clean
checkout. It was also **missing 26 of the 183 tests it is supposed to be gating.**

An ungated test is not coverage. It passes in its author's terminal, then the code
under it changes, and nothing says so. The suite cannot distinguish "this behaviour
is protected" from "this test exists", because from the suite's point of view an
ungated test is indistinguishable from no test at all.

The expensive part is not the omission. It is that **an omission is
indistinguishable from a decision**, so nobody goes looking.

## They were never gated, not removed

The obvious first hypothesis was that `t33`–`t52` were a consolidation into
`t1_808`/`t1_909`/`t1_fx` that got abandoned — twenty contiguous numbers is exactly
what an unfinished merge looks like, and it would have justified deleting them.

That was wrong, and one command said so:

```
$ git log --oneline -S<tname> -- scripts/ri_audit.sh | wc -l
0
```

For all 26: **zero commits, ever.** Not one had been gated and then removed. `git
log -S` on a removed gate still shows the commit that removed it, so zero means the
name was never in the file. Nothing encodes an intent, because no intent was
recorded — not "superseded", not "pending", not anything.

Written between **2026-09-24 and 2026-10-02**: nine days, 26 tests, zero gates.

## Two had already rotted, in the same shape

Nineteen of the twenty `t33`–`t52` passed the moment they were run. The twentieth
failed, and the reason is the whole point of the exercise.

### `t51_route`: a literal where a macro belonged

`9788288` (2026-09-25) created `t51` and `engine/fx/route.h` with
`RI_ROUTE_MASTER 4`. Then `59a3e01` (2026-09-28) added Levi as the 5th section, so
section 4 became Levi and **master had to move 4 → 5**. `route.h` was updated.
`t51` was not touched again.

It fails here:

```c
RI_ASSERT(ri_route_assign(&r, RI_ROUTE_COMP, 5) == -2, "owner 5 taken");
```

Under the old contract (`owner > 4` rejected) that was correct. Under the new one,
owner 5 **is** master, and is legal for comp. The implementation is right; the test
encodes a retired contract.

The detail that makes the diagnosis certain rather than plausible: the same test
uses the `RI_ROUTE_MASTER` *macro* everywhere it assigns comp to master (lines
39–42, correct under both contracts) and a hardcoded *literal* `5` at line 50. The
author updated the macro uses and missed the literal. **A test that mixes a symbolic
constant with a literal of the same value will fail the moment the value moves, and
only at the literal.**

### `t107_levi_sect`: the same disease, still running

`t107` fails three assertions on the RIBBON encoder map — `dead slot inert`,
`dead slot blank`, `module clamp (RIBBON, P8d)`. Encoder slot 5 stopped being dead
when RIBBON took it. Identical shape to `t51`: the subject grew underneath a test
that nothing ran.

It is **still ungated**, and it is the *only* exemption the lint added below
carries. The other four Levi strays (`t108_levi_algo`, `t109_levi_morph`,
`t118_levi_seq_player`, `t122_levi_lfo`) passed as soon as they were run and are now
gated. So of the 26, **25 are closed and one is open and named** — narrowing the
known-red surface to a single test rather than leaving five under a blanket
exemption. Two instances of one failure mode, one fixed and one deliberately left
standing, is the honest state.

## The comments had rotted too, and one could mislead code

Nobody reads a header comment except a person deciding what a function does, which
makes a stale comment a quiet correctness hazard rather than a cosmetic one. Three
in `route.h`, one in `engine/engine.h`, all consequences of Levi taking section 4:

| Claim | Reality |
|---|---|
| `owner < -1 or > 4` rejected | `> RI_ROUTE_MASTER`, i.e. `> 5` |
| `/* -1 none, 0..3 section, 4 master */` | `0..4` sections, master `5` |
| `Sections > 3 (incl. master) read 0` | only `section >= 5` reads 0 |
| `engine.h`: `Sections 0..3 (303A/303B/808/909)` | `0..RI_ROUTE_NSECTIONS-1` |

**The third is the one that matters.** `ri_route_section_mask` returns 0 only for
out-of-range indices, so **Levi at section 4 gets a live mask**. A caller who
believed the comment would skip masking for the Levi strip and get it wrong. The
other three would have misled a reader about bounds; that one would have misled a
writer about behaviour.

## The fix: a lint on the condition, not on the list

Twenty-one gates went in, placed by domain rather than appended in a block — the
one-renderer contract (`t37_engine_single`) into phase 6, whose subject *is* the
one-renderer contract; `t51_route`/`t52_engine_fx` into phase 10 with the other
insert and routing tests.

That fixes the 26. It does not fix the 27th, so phase 0d now fails the audit if any
`tests/unit/*.c` is unreachable from a gate, named or via a `for t in` loop:

```
== Phase 0d: no test ships ungated ==
-- every test reachable from a gate; exemptions: 5 Levi (encoder-map contract pending) --
```

Two design choices worth stating. Exemptions live in a **named list inside the
script**, so "not gated" is a recorded decision someone has to edit deliberately,
rather than an absence nobody notices — the 26 stayed invisible precisely because
nothing required accounting for them. And the lint resolves its own path from
`${BASH_SOURCE[0]}` rather than `$0`, which is wrong under `bash < ri_audit.sh` and
under a symlinked entry point; a self-inspecting gate that silently inspects
nothing is worse than none.

**157 gated → 182 gated, 1 exemption, 183 total.** The lint fails in seconds, before
any compilation, because it is a grep.

The size of that exemption list was itself a decision worth recording. The first
draft carried all five Levi tests, which would have shipped an audit reporting
`0/0` while a known-red test sat inside it — technically disclosed, and still a
number a reader would have to stop and interpret. Gating the four that pass and
narrowing the exemption to `t107` costs nothing and leaves exactly one thing to fix.

## Both new gates observed failing

Per the standing rule from the prior article — *a gate never observed failing is not
known to work* — and per its sharper form, **the control has to fail the way the
real bug would**.

The lint, with its (now single-entry) exemption list emptied:

```
== Phase 0d: no test ships ungated ==
FAIL: t107_levi_sect is not gated by this audit — add a gate, or an exemption with a reason
EXIT=1
```

A gate line, by injecting a failing assertion into `t37_engine_single` — **not** by
tripping `-Werror`, which is the control that fails for the wrong reason:

```
== Phase 6: W1 one-renderer proof (Task 6, gate G6) ==
 100 |     RI_ASSERT(rms(engL, H) > 1e-4f, "A silent in A-only run");
FAIL: t37_engine_single (shared core: per-device routing, centre-unity stereo, mono fold == legacy)
EXIT=1
```

It halts in the phase the gate was placed in, and the message carries the contract
rather than just a test name, so a future failure says *which* law broke. `t37` was
restored bit-identical to HEAD and re-run.

## Two corrections to this session's own work

**The consolidation hypothesis was wrong.** Stated above because the record is more
useful than the conclusion, and because "twenty contiguous numbers must be an
abandoned merge" is a confident-sounding inference that one cheap command refutes.

**The first proof that the 21 gates ran was invalid.** It grepped the audit log for
`PASS <name>`. Every gate line is `test X >/dev/null || { FAIL; exit 1; }`, so the
log contains no per-test output at all and the grep found nothing. The proof is
structural instead: the audit is fail-fast, so reaching `0/0` **is** the evidence
that all 21 passed. That is a proof by exhaustion rather than by record, which is
worth knowing about the artifact — **a green audit log is not a record of what ran.**

## What a green here does and does not mean

`AUDIT 0/0 PASS` on this host, with the mingw gate reporting `SKIP` (no
`x86_64-w64-mingw32-gcc` installed) and the external `sox` header checks degrading to
the in-tree `inspect --wav`. Three checks narrow rather than fail, so the claim
`0/0` supports on this machine is smaller than the string reads. Portability lane
T11 is unverified here.

Also unchanged and still true from the prior article: a green suite here covers the
host-compiled parts and is not evidence for claims about the widget and app layers.

## Method findings

- **An omission is indistinguishable from a decision.** That is the whole reason 26
  tests survived nine days. Any list that can be incomplete without failing is a
  list nobody checks.
- **`git log -S<name> -- <file>` distinguishes "never added" from "removed",** and
  the two demand opposite responses (wire it up vs. delete it). Guessing from the
  shape of the numbering guessed wrong.
- **Mixing a macro with a literal of the same value is a latent failure.** `t51` had
  both spellings of "master" in one file; only the literal broke. Write the
  derivation, not the value.
- **A stale header comment can be a correctness hazard, not a cosmetic one.** Only
  one of the four could have produced wrong code — `section_mask`'s "sections > 3
  read 0" — and it is the one a caller acts on.
- **Prefer a lint on the condition.** Naming the 21 exceptions would have fixed this
  session and left the next one identical.
- **Verify a proof can fail before trusting it.** Both controls here were confirmed
  to produce exit 1, and the first attempt at the second was checking the wrong
  thing (log contents) and would have "passed" a suite that ran nothing.