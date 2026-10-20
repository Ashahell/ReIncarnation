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
