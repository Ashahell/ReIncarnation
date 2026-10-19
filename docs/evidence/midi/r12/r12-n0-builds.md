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

## Lane outages — corrected (my error)

The first version of this file said the Dell lane was down on **"a quota on
the Dell's own volume"**, inferred from `[Errno 122] Disk quota exceeded` plus
`df` on `/home` showing 1.7 T free. **That was wrong.** Errno 122 is a *Linux*
errno, so it came from the **host**, not from AROS on the Dell. The e6320 spool
is `/tmp/spike_spool_laptop`, which lives on the host's `/tmp` tmpfs, and
`/tmp` had filled (4.9 G of 15 G, and the spike spool is inside it). The owner
cleared `/tmp`; the Dell redialled and is **live**: `session 28 (e6320)`
connected from `192.168.1.60`, and a `version` exec round-trips
`RESULT: PASS`.

The mistake was checking the wrong volume and then attributing a host-side
errno to the guest. Same shape as the three earlier "AROS does X" findings the
plan warns about in §0: **rule out the caller before blaming the far end.**

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

## The ABIv11 entropy fix (build-only shim, owner-directed)

Owner direction: shim it for the ABIv11 build; do not patch the protocol.

The upstream contract is preserved exactly. `netmidi_network_random()` already
returns -1 when the resource cannot be opened, and `random32()` in `main.c`
already handles that. So the guard is in `network.c`:

```c
#include <proto/exec.h>                 /* still needed: OpenLibrary/CloseLibrary */
#ifndef RI_NETMIDI_NO_ENTROPY
#include <proto/entropy.h>
#include <resources/entropy.h>
#endif
...
#ifdef RI_NETMIDI_NO_ENTROPY
int netmidi_network_random(void *buffer, size_t length)
{ (void)buffer; (void)length; return -1; }
#else
   ... upstream body, unchanged ...
#endif
```

The define is set in **our build config only**
(`r12netmidi-v11-build/config/make.cfg`), never in upstream source:

```
RI_NETMIDI_NO_ENTROPY = 1
CONFIG_CPPFLAGS += -DRI_NETMIDI_NO_ENTROPY
```

`USER_CPPFLAGS +=` does **not** work here: `netmidid/mmakefile.src` line 17 does
`USER_CPPFLAGS := -DNETMIDI_SERVICE`, which replaces the variable and silently
discards the append (measured: the first attempt compiled with the define
absent and still failed on `proto/entropy.h`). `CONFIG_CPPFLAGS` survives,
because `target.cfg:114` composes
`TARGET_CPPFLAGS = $(USER_INCLUDES) $(CONFIG_CPPFLAGS) $(USER_CPPFLAGS)`.

### Proof the shim is what actually got built

Not inferred from a passing exit code — disassembled:

```
0000000000004a90 <netmidi_network_random>:
    4a90:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
    4a95:	c3                   	ret
```

`GetEntropy` / `entropy.resource` references in the binary: **0**. Upstream's
own fallback string is still present: `netmidid: no random source; identifiers
are weak`.

**A false pass was caught and killed here.** The first "successful" v11 build
was NOT using the shim: a leftover generated `proto/entropy.h` from the reverted
entropy staging was still in the build tree, so `network.c` compiled the real
function and linked `OpenResource(ENTROPYNAME)`. The disassembly showed a real
function body, not the stub, which is what exposed it. The leftover header
(`bin/pc-x86_64/gen/include/proto/entropy.h`, `bin/pc-x86_64/AROS/Development/
include/proto/entropy.h`, `.../resources/entropy.h`, and `gen/rom/entropy`) was
deleted and the cached depfile `gen/workbench/network/netmidid/network.d` — which
still listed the removed header — had to be deleted too before the guard could
be exercised. Only then did the honest compile fail on `proto/entropy.h`, which
is what led to the `CONFIG_CPPFLAGS` fix above.

| artifact | bytes | sha256 |
|---|---|---|
| `System/Network/Services/netmidid` (ABIv11, shimmed) | 122448 | `c71fe8ff5a63396004951af74bb336b1da62687ba28d9216f3478c02f0cb1172` |

### Known limitation of the Dell build (recorded, not hidden)

SSRC and peer tokens are weak on the Dell. The seed is **not** constant:
`now_ticks()` is `gettimeofday()` in 10 kHz units (`main.c:91-96`), and
`random32()` does `srand((unsigned)(now ^ (now>>32)))` once per process. Two
starts one second apart seed differently, and there are 3.6 M distinct seeds
across an hour at 10 kHz granularity, so a restarted session cannot be mistaken
for the old one by a peer. For the current epoch the `now>>32` term is only
~4093, so the seed is effectively the low 32 bits of the tick count. Adequate
for a LAN test; not a security claim.

### ABIv11 build is now 4 of 4

`camd.library`, `DEVS:Midi/applemidi`, `librtpmidi.a` and `netmidid` all build
for ABIv11. The driver and the link libraries still compile **unchanged**; the
only conditional compile is the one entropy stub above.

## Scratch location (corrected)

Scratch moved off `/tmp` to `/home/miller/.worktrees/`:
`r12netmidi-v1{,-build}`, `r12netmidi-v11{,-build}`.

**Relocation hazard, recorded because it cost real time.** Rewriting the old
path across both build trees with a bulk `sed -i` also rewrote **ELF binaries**
in those trees. `mmake`, `collect-aros`, `collect-aros32` and ~20 other host
tools were corrupted (`missing section headers`, sizes changed), which surfaced
as `Segmentation fault (core dumped)` from `make`, then from `clang: error:
unable to execute command` at link. Every damaged file was restored from the
pristine lane trees (`abi/v1/core-pc-x86_64`, `abi/v11/core-pc-x86_64-m0`) by
identical relative path, and the lane trees themselves were checked and are
**clean**. Both scratch builds then reproduce byte-identical artifacts
(`camd.library` `baca61a1…` v1 / `f2026d94…` v11, `applemidi` `9363a85d…`,
`librtpmidi.a` `a4645602…`), which is the proof the move was lossless.
**Rule: never bulk-`sed` a build tree; filter to text files.**

## Status vs the plan

- **N0**: COMPLETE for both ABIs.
- **N1**: riqemu1 was DOWN (spike server on 9295 up, no QEMU). Started per
  owner instruction.
- **N2**: Dell is reachable — `session 28 (e6320)` connected from
  `192.168.1.60`, and a `version` exec round-trips PASS. Staging can begin;
  the `camd` install still needs the owner's reboot.
- **N3**: blocked behind N2.
