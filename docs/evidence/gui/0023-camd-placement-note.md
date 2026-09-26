# C3 handoff: camd mysprintf patch placement (2026-09-26, opencode G8)

## What this is

`0023-camd-mysprintf-varargs.diff` (sibling file) puts the G7
`mysprintf` varargs fix into the Vulkan4AROS v1 patch series format
(`--- a/…` / `+++ b/…`, applies with `patch -p1` from the AROS tree
root — dry-run verified against the reconstructed pristine file).

## Why it is NOT in the series yet

- The series lives in `Vulkan4Aros/src/abi-patches/v1/aros/` on branch
  `t8-workgroup-size`, which is dirty and owned by another session.
  Rule: never commit to someone else's branch without asking the owner.
- Next free number at writing time is 0023 (0015–0022 present).
- The fix currently exists ONLY as an edit in the git-ignored v1 build
  tree (`src/abi/v1/AROS/…`, re-provisioned from scratch by script) plus
  the evidence diff `2026-09-26-camd-mysprintf-x86_64.diff`. If that tree
  is re-provisioned before the series patch lands, the fix is GONE from
  source (the deployed `DH0:Libs/camd.library` on riqemu1 survives, with
  `.orig` backup). Landing the series patch is time-sensitive.
- Fidelity: the sibling .diff was regenerated from the live tree state
  (pristine reconstructed by reverse-applying the evidence diff; tree
  matches the evidence diff line-for-line).

## Upstream PR (blocked)

The plan names `tools/aros-upstream/prs.py` (one change per PR, x86_64-pc
tested). That tool does not exist anywhere in this repo (searched). The
upstream PR is therefore recorded here and not attempted: needs the tool
(or its manual equivalent) plus an x86_64-pc test lane.

## debugdriver hang (open investigation, see C3 record)

With correct paths, camd init loads `DEVS:Midi/debugdriver` and the first
`OpenLibrary("camd.library")` blocks. Driver parked on riqemu1 at
`SYS:debugdriver.parked`. Static analysis of the load path is in progress;
the fix, if found, goes the same series route (never the other branch
directly).
