# R12 N0 — builds, both ABIs (2026-10-19)

## Pins (verified against the remote before use)

| PR | branch | pin | `git rev-parse` |
|---|---|---|---|
| #1678 | `JJDSNT/AROS:review/applemidi-sessions` | `31d1ffdde5d4af3aa4139ac071d407afb6961645` | matches |
| #1679 | `JJDSNT/AROS:review/netmidid-service` | `db9e3073c3a0645a50dabf388092cece9902e344` | matches |
| ADT `master` | #1585/#1586/#1587/#1676 | `bf7075aab84` (2026-10-19) | — |

Both draft branches live on the **JJDSNT fork**, not on
`aros-development-team/AROS`; `git ls-remote` against the latter returns
"couldn't find remote ref". `review/netmidid-service` is 5 commits ahead of
upstream master and **already contains** the merged #1585, #1586, #1587 and
#1676 (`merge-base --is-ancestor b1edfb9866 review/netmidid-service` → yes).
So it is used as a single self-contained base, not cherry-picked.

## ABIv1 (riqemu1) — ALL FOUR BUILD

Base `db9e3073c3` in worktree `aros-scratch/r12netmidi/src`, fresh build dir
`aros-scratch/r12netmidi/build`. The lane's own tree (`abi/v1/AROS`, 60
uncommitted nvk-lane patches) was **not touched**.

| artifact | bytes | sha256 | $VER |
|---|---|---|---|
| `Libs/camd.library` | 53528 | `baca61a17193d06647657ab671ef086712c7066ca66b0aabe58d0e7c8afd152a` | `camd.library 41.1 (19.10.2026)` |
| `Devs/Midi/applemidi` | 20768 | `9363a85d09159f9ee5aafc2c15de425adc63ed8296313b01851cf51aec7fb133` | `AppleMIDI CAMD driver 1.0 (12.10.2026)` |
| `System/Network/Services/netmidid` | 127120 | `c92ebb77d11e576c75cc2548de0195ac013115cb4f9990fa76287f4d91cd448c` | — |
| `Developer/lib/librtpmidi.a` | 72004 | `a4645602ae1e2c77ac324b8bbd48fa7736ce695b497cddef429317dd09854fe4` | — |

`camd.library` carries #1585 (aligned `MidiDeviceData` scan) and #1676
(expunge returns success) as a side effect of the base, plus the M0 fixes
`28ec43a517` / `cb8c4c5f39` (already in the base's ancestry).

## ABIv11 (Dell) — 3 of 4 BUILD, `netmidid` BLOCKED

Base `fbc242c104` (deadwood2/AROS) in worktree
`aros-scratch/r12netmidi-v11/src`, fresh build dir alongside, per the M0 route.
ADT sources copied in: `workbench/devs/camd/{applemidi,networkmidi2}`,
`workbench/network/netmidid`. `workbench/devs/mmakefile.src` gained one
`workbench-devs-camd` line (v11 had no `devs/camd` at all).

camd also needed #1585 + #1676, applied from upstream diffs. `1585.diff` needs
`-C1` on v11 (the file has shorter trailing context). M0's two carried v11
patches: the `va_list` one applies; `camd-loaddriver-skip-arena-hunks.v11.patch`
collides with #1585 and was applied hunk-by-hunk at both sites instead.

| artifact | bytes | sha256 | $VER |
|---|---|---|---|
| `Libs/camd.library` | 53904 | `f2026d949c923d1ada72154a4c0383c674b7ae904c34108cb9fe4960f15bb8be` | `camd.library 41.1 (19.10.2026)` |
| `Devs/Midi/applemidi` | 11880 | `9b88da4991e6cee366dc19001f232adca1cc24af03fd3be056e77df2fc9b00ac` | `AppleMIDI CAMD driver 1.0 (12.10.2026)` |
| `Development/lib/librtpmidi.a` | 76420 | `1de1094a0ac9d7377d23642aaacc3f1ffe918be39be195d0fa9369d6e6832e35` | — |
| `netmidid` | — | — | **DOES NOT BUILD** |

The driver and both link libraries compile **unchanged** against the ABIv11
SDK. Nothing upstream was patched to make them fit.

## The ABIv11 failure — exact cause

`netmidid` fails on `workbench/network/netmidid/network.c:16`:

```
fatal error: proto/entropy.h: No such file or directory
```

`network.c` opens `entropy.resource` to get randomness for SSRC/token
generation (`netmidi_network_random`). **The ABIv11 tree has no
`entropy.resource` at all** — it was added upstream in `41b0698b24`
(2026-07-01, "New crypto RNG (backed by entropy.resource)") and the Dell's tree
(`fbc242c104`, 2026-09-04) does **not** contain it:
`find` over the whole v11 tree returns zero `entropy` files, and
`merge-base --is-ancestor 41b0698b24 fbc242c104` → no.

`proto/entropy.h` is a *generated* SDK header, produced by building
`kernel-entropy`. In ABIv1 that target exists and building it fixed the
error. **In ABIv11 the target does not exist**, so the header can never be
generated — the build cannot be satisfied from inside the Dell tree.

### Why staging entropy is not a fix (measured, then reverted)

Staging upstream `rom/entropy` + `arch/all-pc/entropy` +
`compiler/include/resources/entropy.h` into the v11 tree and building
`kernel-entropy` **also fails**:

```
rom/entropy/entropy_software.c:68:43: error: 'struct ExecBase' has no member named 'TDNestCnt'
```

Cause: `compiler/include/exec/execbase.inc` in v11 *does* define `TDNestCnt`
(line 58), but the **installed SDK header** the guest build actually uses
(`AROS/Development/include/exec/execbase.h`) replaces the private fields with
placeholders — `ThisTask`→`Private1`, `Quantum`/`Elapsed`→`Private2/3`,
`IDNestCnt`→`Private4`, `TDNestCnt`→`Private5`. Upstream's `entropy_software.c`
reads those private fields by name for entropy
(`TaskReady`, `DispCount`, `IdleCount`, `Quantum`, `Elapsed`, `IDNestCnt`,
`TDNestCnt`, `SysFlags`, `AttnResched`).

So carrying `entropy.resource` onto the Dell is not a header copy: it needs the
v11 execbase ABI names reconciled, which is **kernel ABI work, not network
MIDI work**, and the plan forbids patching upstream to make it fit. The staging
was reverted; the tree holds only the intended changes.

### Note on the failure mode (no gate needed to read this)

`netmidid` treats randomness as optional at runtime: `random32()` falls back to
`srand(now)` and prints `no random source; identifiers are weak`. Only the SSRC
and tokens weaken, not the protocol. So a **runtime** fallback would be
functionally acceptable for a lane test; it is the *compile-time* include that
blocks. Recorded as an option for the owner, not taken unilaterally.

## Status vs the plan

- **N0**: ABIv1 complete. ABIv11 partial — 3 of 4; `netmidid` needs an owner
  decision (this is the plan's §3 "an ABIv11 build fails" gate).
- **N1**: not started. **riqemu1 is DOWN** — its spike server listens on 9295
  but no QEMU process is running. Also gated: `rtpmidid` is not installed on the
  host and the plan requires owner approval before installing anything there.
- **N2/N3**: not started. The **Dell lane is DOWN** — `spike-laptop.service`
  logged `session 23 (e6320) ended: [Errno 122] Disk quota exceeded`. Host disk
  is fine (204 G used of 1.9 T), so this is a quota on the Dell's own volume and
  needs the owner.
