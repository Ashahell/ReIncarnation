# The stale-object trap: a mutant that was never compiled looks like a mutant that survived

**Date:** 2026-10-20
**Scope:** `scripts/ri_build_host.sh` (the `test` target), the R12 O1b drum-channel
fix, and the two mutant forms that exposed it.
**Related:** `docs/evidence/midi/ledger.md` (the R12 entry, "R12 O1: the
drum-channel defect"), `docs/evidence/midi/r12/r12-o1-notes-and-mmc-on-the-dell.md`,
`midi_io/midi_devout.h`, `midi_io/midi_devout.c`, `tests/unit/t201_devout_conventions.c`.

## The claim that started it

The R12 O1b record said, of the drum-channel fix:

> The tests now assert literal `0x99` (note-on) and `0x89` (release) and
> carry a round trip: every drum message devout emits is fed through
> `ri_notein_note()` and must resolve to the 808 bass drum. **The raw-nibble
> version is a mutant and dies.**

That last sentence is a **claim about a mutant**, and a claim about a mutant
is worth exactly as much as the measurement behind it. So it was measured.

## What the measurement found: the first attempt PASSED

Putting the raw 1-based value back
(`RI_DEVOUT_GM_PERCUSSION_NIBBLE` = `RI_DEVOUT_GM_PERCUSSION`), rebuilding
the `formats` module, and running `t201_devout_conventions` gave **six
assertion failures and rc=1** — the mutant dies, and the round trip is what
kills it, not the literal byte alone: the emitted drum becomes `0x9A`, its
nibble is 10, the note input binds the 808 at nibble 9, so the note is
**unclaimed and refused**.

But the **first** attempt at that measurement **passed**. The reason was not
a weak test; it was a build defect:

    ri_build_host.sh test NAME

links the object files already in `/tmp/ri/build` and **rebuilds nothing**.
So a **header-only** edit — a constant, a struct, an inline — left the
objects stale, and the test answered for the OLD code. A mutant that was
never compiled is indistinguishable from a mutant that survived, and the
conclusion it produces ("this test does not kill the mutant") is worse than
no conclusion, because it is confident.

`ri_audit.sh` was never at risk: it builds `all` first, so its objects are
current. A hand-run test was, and so was any session that checked a mutant
by hand.

## The same trap in its other form: a .c-only edit

The first fix watched **headers** only. A `.c`-only edit is the same trap
with the other hat on: the object for that one source is older than the
source, and nothing else noticed. The `test` target still linked the stale
object.

So the rule now has two halves, and both are proved by a mutant that dies
on `ri_build_host.sh test` **alone** — no manual rebuild, no `all`:

| mutant form | what changed | the rebuild the test target now does | result |
|---|---|---|---|
| **header** | `RI_DEVOUT_GM_PERCUSSION_NIBBLE` = the raw 1-based value in `midi_io/midi_devout.h` | "a header is newer than its object" (the coarse rule) | **rc=1**, 11 assertion failures |
| **.c only** | `channel = RI_DEVOUT_GM_PERCUSSION;` in `midi_io/midi_devout.c`, the header untouched | "`midi_io/midi_devout.c` is newer than its object" | **rc=1**, 6 assertion failures |

The fixed build passes: `t201` (and `t200`, `t202`, `t211`, `t212`, `t213`,
`t214`), and `ri_audit.sh` reports **AUDIT 0/0 PASS**.

## The rule, and why the header half is coarse on purpose

`ri_build_host.sh test NAME` now rebuilds any module where

- **any source** of the module is newer than its own object, **or**
- **any header in the tree** is newer than that object.

The header half is deliberately **coarse** — ANY newer header invalidates
an object — because it is cheap (a full rebuild is **7.6 s**) and it cannot
be wrong in the unsafe direction. Over-rebuilding costs a second; missing a
rebuild costs a conclusion. Fine-grained dependency tracking (per-object
`.d` files) would be the precise answer, and it is not needed to make the
mutant discipline sound.

## The general rule this is

**A green that was never rebuilt proves nothing.** That is the whole lesson,
and it is the same shape as the project's other "the thing is not what it
looks like" findings: the drum fix's own claim about its mutant was sound,
and the instrument that was supposed to prove it was the thing that lied.
The instrument was not wrong about the mutant; it was wrong about which
code it was looking at.

## And the trap had a third form: the CHECK was itself CWD-dependent

The source check in `rebuild_stale()` was written as

    if [ -n "$(find "$src" -newer "$o" ...)" ]

but `$src` comes from the module lists and is **repo-relative**
(`engine/dsp/kernels.c`), while `compile_list` compiles `"$ROOT/$f"`. So
`find` resolved the source against the **current directory**: from the repo
root it happened to work, and from anywhere else it matched **nothing**,
rule (a) silently never fired, the object stayed stale, and the trap came
back through the other door.

Measured, with the `.c`-only mutant in place:

| invocation | before the fix | after the fix |
|---|---|---|
| `scripts/ri_build_host.sh test t201…` (repo root) | rc=1 — the rebuild fired | rc=1 |
| `(cd /tmp && ~/Work/projects/ReIncarnation/scripts/ri_build_host.sh test t201…)` | **rc=0 — the mutant PASSED, no rebuild** | **rc=1**, 16 assertion failures, and the log says *"`midi_io/midi_devout.c` is newer than its object -- rebuilding"* |

The fix is `find "$ROOT/$src"`, and `ROOT` is now made **absolute**
(`ROOT="$(cd "$(dirname "$0")/.." && pwd)"`) so that every path in the script
is built from one place. `cd … && pwd` also pins the semantics for an
invocation as `./ri_build_host.sh`, where a bare `"$(dirname "$0")/.."`
names the **parent** of the repo root.

**The same class was still live in two scripts this work did not touch, and
is now fixed (owner call, 2026-10-20):**

- `scripts/ri_build_aros.sh`: `ROOT="$(dirname "$0")/.."` and, worse, the
  SIBLING tree was named relative to the current directory —
  `../Vulkan4Aros/scripts/aros_build_env.sh` and
  `../Vulkan4Aros/src/abi/v1/…`. Measured: run from `/tmp`, it failed
  immediately with *"FAIL: Vulkan4AROS tree (toolchain source) not found"*;
- `scripts/ri_audit.sh`: `ROOT="$(dirname "$0")/.."`, so the audit that calls
  the script above only worked from the repo root — which is why an audit run
  in a clean worktree failed at *"AROS build (stub+probe)"* for a reason that
  had nothing to do with the code. (It already had a `cd "$ROOT"`; the CWD
  was pinned but **ROOT itself was relative**, so the pin landed in the
  wrong place when the script was invoked from elsewhere.)

Both now use

```sh
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VK="${VK_ROOT:-$ROOT/../Vulkan4Aros}"
```

and every `../Vulkan4Aros` is now `"$VK"`, with a guard that **names $VK**
when it is missing and says that `VK_ROOT` overrides it. `VK_ROOT` exists
because a git worktree has **no sibling tree at all**: `~/.worktrees/X` has
no `../Vulkan4Aros` next to it, so a worktree audit could not find the
toolchain that was sitting right there in the main checkout.

`ri_build_aros.sh` also pins itself with `cd "$ROOT"`. That is safe
because **neither script takes a path argument**: `ri_audit.sh` takes none
at all, and `ri_build_aros.sh`'s only argument is a TARGET NAME
(`sections` | `riapp`), never a path.

Proved, all three:

| proof | result |
|---|---|
| from the repo root | `AUDIT 0/0 PASS` (unchanged) |
| `(cd /tmp && ~/Work/projects/ReIncarnation/scripts/ri_audit.sh)` | `AUDIT 0/0 PASS`; `ri_build_aros.sh` from `/tmp` prints `AROS STUB BUILD OK` / `AROS PROBE BUILD OK` |
| a worktree under `~/.worktrees` with `VK_ROOT=~/Work/projects/Vulkan4Aros` | the AROS build step passes — it could not even find the toolchain before — and the **whole audit** passes there too |

## And the THIRD instance: a shared output directory between two checkouts

Running proof 3 in a worktree **broke the main tree's next audit**, and the
reason is the same class one level down: `build/portable.mk` derives `ROOT`
from its own location (so the main tree and a worktree are different source
trees) but wrote both into **one** output directory, `/tmp/ri/portable`. The
worktree's dependency files then name ITS sources, and when the worktree was
removed, the main tree's next build died with

    make: *** No rule to make target
    '/home/miller/.worktrees/ri-audit-vk-test/build/../app/core/live_driver.c',
    needed by '/tmp/ri/portable/app/core/live_driver.o'.  Stop.

— a failure in the **other** tree, caused by a build that ran in a directory
that no longer exists. `OUT` is now per checkout, derived from the absolute
`ROOT` and still overridable (`OUT=… make …`).

The step that proves it: the audit passes in the main tree, the audit passes
in the worktree, the worktree is **removed**, and the main tree's audit
**still passes**. That last step is the one that failed before.

## The FOURTH instance: per-LANE state, and the audit that is the gate

The per-checkout OUT separates two CHECKOUTS. It does not separate two
LANES, because both lanes work in **one** checkout. What was still shared:

- `OUT=/tmp/ri/build`, hard-coded in `scripts/ri_build_host.sh` **and**
  `scripts/ri_audit.sh` — two copies that had to agree;
- the run scaffolds (`/tmp/ri/run/…`), the probe artefacts
  (`/tmp/ri/aros/…`) and the portable build;
- `ri_build_host.sh clean` = `rm -rf /tmp/ri`, which wiped **every** lane's
  and every checkout's builds mid-run.

The n36a lane's first audit failed **linking** objects this lane was
rebuilding, and two concurrent runs can produce a **false GREEN** from mixed
objects.

**One root now**: `RI_TMP="${RI_TMP:-/tmp/ri}"`, defined the same way in
every script, exported so the children inherit it, and every artefact
derived from it — `build/`, `run/`, `aros/`, `portable-*`. `clean` removes
only `$RI_TMP`.

Getting there needed **four** measurements, each of which the first fix
missed:

| # | what the first attempt missed | how it showed up |
|---|---|---|
| 1 | the C sources still hard-coded `/tmp/ri` (113 sites in 21 files) | the audit read `$RI_TMP/run/t6/…` while the test wrote `/tmp/ri/run/t6/…` → *"FAIL: file-vs-live differ"* |
| 2 | the macro must expand to a **string literal**, not a bare path | `-DRI_TMP_ROOT=$RI_TMP` → *"expected expression before '/' token"*; the quotes have to survive into gcc: `-DRI_TMP_ROOT=\"$RI_TMP\"` |
| 3 | one test wrote its artefacts with **relative** paths, so they landed in the CWD — the repo root, which both lanes share | t217 read the other lane's `t217f_out32.wav` → *"FAIL: t217_wavfloat"* |
| 4 | the pcf negative gate **moved a repo file** aside and back | two lanes raced for `reference/pcf-table.bin` → *"mv: cannot stat"*; the gate now points the build at a path that is not there (`PCF_TABLE=…`), and nothing in the checkout is touched |
| 5 | the **sweep** found four more scripts with their own hard-coded `/tmp/ri` | `ri_soak.sh` benched against `/tmp/ri/build` — the OTHER lane's objects — so one lane's soak failed while the other rebuilt that directory; `ri_fuzz.sh`, `ri_build_v11.sh` and `lane_proof.sh` had the same shape. All four now derive from `RI_TMP` too. |

**The sweep is the part that matters.** The owner named two scripts; the
audit actually *calls* seven (`ri_build_host.sh`, `ri_build_aros.sh`,
`ri_soak.sh`, `ri_fuzz.sh`, `ri_build_v11.sh`, plus the two it greps), and
every one of them had to agree on the root or the separation is a fiction:
a lane's soak would bench against the other lane's objects and a lane's
fuzz would write into the other lane's run directory.

Proof, both:

| proof | result |
|---|---|
| two audits started at the same time with different `RI_TMP` | **both `AUDIT 0/0 PASS`** |
| `clean` under one `RI_TMP` | lane A: 968 files → 0; **lane B: 968 files, 216 M, untouched** |

The default (no `RI_TMP`) is unchanged and still passes.
