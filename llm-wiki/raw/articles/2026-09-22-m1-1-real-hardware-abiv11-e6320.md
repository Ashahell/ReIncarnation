# 2026-09-22 — M1.1 measured on REAL hardware (Dell Latitude E6320, ABIv11): v11-toolchain build fixes the probe; first full hardware run

> Source: session evidence (spike-agent exec output for `probe_ahi_v11` on the Dell E6320 laptop, verbatim `RI_PROBE` block; host build transcript; ABI header diff), compiled by agent
> Collected: 2026-09-22
> Published: 2026-09-22

## Disposition
New. The M1.1 codec-less guest measurements stay as the VOID baseline;
this article records the first REAL-hardware M1.1 run and the toolchain
grounding that made it work. No repo change beyond this record (probe is
repo `audio_io/probe_ahi.c`, already committed; the client board laptop
build is a scratch artifact under `/tmp/ri/`).

## TL;DR
The ABIv11 laptop (Dell Latitude E6320, boot-stick AROS) could not run ANY
v1-SDK-built probe — every one died in startup init with a privilege
violation in `exec_83_OpenResource` (Kickstart ELF Segment 1 text). The
fix was NOT another probe patch: it was building with the ABIv11 (default
`AROS_ABI=v11`) toolchain + SDK instead of the v1 SDK. The v11-built
`probe_ahi_v11` then ran M1.1 fully on real hardware — first hardware
measurement of the campaign. All V1-header differences turned out to be
naming renames (ThisTask→Private1 etc.), layout identical. Root cause of
the privilege violation: v1-SDK startup.o calls `OpenResource` on
`task.resource`, which the ABIv11 laptop kernel rejects (privilege
violation); v11 SDK's own startup object avoids that path.

## The hardware lane
- Dell Latitude E6320 laptop, ABIv11 boot stick (SFS img at
  `/home/miller/Work/stickwork/sfs_p5.img`, MBR `mbr.bin`).
- IP 192.168.1.60, agent pair name `e6320`, host 192.168.1.81, spike
  server `serve --port 9292 --spool /tmp/spike_spool_laptop --pairs
  /tmp/spike_spool_laptop/pairs.json` (explicit `--pairs` required).
- Agent redial after every laptop reboot: `SYS:ATCPBIN agent 192.168.1.81
  9292 e6320`. NordVPN on the host wipes nft rules — after each event:
  `sudo nft add rule inet nordvpn input ip saddr 192.168.1.60 accept`
  (and output), plus `nordvpn whitelist add subnet 192.168.1.0/24`;
  ufw allows 9292.

## The blocker and the toolchain switch
Every v1-SDK-built probe (including an empty-`main` `min3_probe`) died the
same way on the laptop:

```
privilege violation error. Module: Kickstart ELF Segment 1 text, Function Exec_83_OpenResource
```

That is startup init, before any probe code — so no probe patch could help.
V1 vs v11 header audit (`diff` of `struct ExecBase` in `exec/execbase.h`,
both 222 lines): only PRIVATE-field renames
(`ThisTask→Private1`, `Quantum→Private2`, `Elapsed→Private3`,
`IDNestCnt→Private4`, `TDNestCnt→Private5`) — layout identical. The real
difference is the SDK's startup object: v1 SDK startup.o references
`task.resource` (`strings` count = 1); v11 SDK startup.o does not
(count = 0). ABIv11's kernel rejects that `OpenResource` as a privilege
violation.

Switch to the v11 (default) env and build with the **v11 SDK + gcc-16.1.0
x86_64-aros-gcc** (`AROS_ABI` unset → `FAM=gnu`, `CC=
.../v11/toolchain-core-x86_64/x86_64-aros-gcc`, header includes from
`.../v11/sdk/Developer/include` + the source-tree AHI header
`.../v11/AROS/workbench/devs/AHI/Include/C/devices/ahi.h` — the v11 SDK
include tree does NOT ship ahi.h). Flags:
`-mcmodel=large -mno-red-zone -mno-ms-bitfields -fno-strict-aliasing
-ffixed-r12 -nostartfiles -no-pie`, link `startup.o` from
`$V11SDK/lib/startup.o` (v11 SDK) + `-ldos -lexec`. Result:
`probe_ahi_v11` 24064 B, UND=1, task.resource string count = 0, 225
`mov.*%r12` (the r12 convention is LIVE in this build).

## First real-hardware M1.1 run (verbatim, agent e6320 session 285)

```
RI_PROBE compiler=x86_64-aros-gcc-16.1.0 built=Sep 22 2026 08:00:47
RI_PROBE engine_block_frames=64 rate=48000
RI_PROBE ahi.session: OPEN unit=255 base=0x0000000100eed720 version=6
RI_PROBE best_mode: id=0x003E0001
RI_PROBE alloc_audio: OK freq=44100 bits=16 stereo=1 hifi=1 maxch=128
RI_PROBE lowlevel frames=4096 playerfreq_hz=11 rc=0
RI_PROBE lowlevel frames=2048 playerfreq_hz=23 rc=0
RI_PROBE lowlevel frames=1024 playerfreq_hz=46 rc=0
RI_PROBE lowlevel frames=512 playerfreq_hz=93 rc=0
RI_PROBE lowlevel frames=256 playerfreq_hz=187 rc=0
RI_PROBE lowlevel frames=128 playerfreq_hz=375 rc=0
RI_PROBE lowlevel frames=64 playerfreq_hz=750 rc=0
RI_PROBE low_min_frames=64
RI_PROBE verify frames=64 window_s=5 observed=55 expected=3750 shortfall=3695
RI_PROBE device: unit 0 PRESENT
RI_PROBE device frames=4096 err1=0 err2=-2
RI_PROBE device frames=2048 err1=0 err2=-2
RI_PROBE device frames=1024 err1=0 err2=-2
RI_PROBE device frames=512 err1=0 err2=-2
RI_PROBE device frames=256 err1=0 err2=-2
RI_PROBE device frames=128 err1=0 err2=-2
RI_PROBE device frames=64 err1=0 err2=-2
RI_PROBE dev_min_frames=0
RI_PROBE SUMMARY low_min_frames=64 dev_min_frames=0 verify_obs=55 verify_exp=3750 dev_open_rc=0
```
`[submit] RESULT: PASS (agent e6320, session 285)`; agent alive after.

## Readings (real hardware, reference context)
- P1: `best_mode id=0x003E0001` (real mode ID, not VOID's `0x1F0002`);
  AHI version 6; lib base `0x0000000100eed720` — **above 4 GB**, which
  independently confirms why 32-bit pointers in probes fault on this box.
- P2 low-level ladder 7/7 rc=0 → `low_min_frames=64`.
- P3 (playback timing, honest real measure): `observed=55` over a 5 s
  window vs `expected=3750` — the AHI driver runs its Player at ~11 Hz
  instead of the requested ~750 Hz (frames=64 → `playerfreq_hz=750`).
  `shortfall=3695`. This is the FIRST real timing value for the gate
  (the 50M ticks were VOID unclocked).
- P4 unit 0 PRESENT, all rungs `err1=0 err2=-2` (r1 natural, r2
  abort-clean by bounded poll), `dev_min_frames=0` by design.
- SUMMARY complete, rc=0, agent alive, zero fault lines.

## Standing / open
- **Status 2026-09-22 (11 Hz verdict, CLOSED): the driver runs Player at
  a fixed ~11 Hz; requested PlayerFreq is accepted but not honored.**
  Ladder-truncated variants (ladder line only, `-O2` recipe proven to
  within 8 B of the recorded `.o`): frames=128 → observed=55/expected=
  1875; frames=256 → observed=55/expected=935; repro frames=64 →
  observed=55/expected=3750 (recipe validation). Observed is exactly 55
  in all three 5 s windows while the request spans 750/375/187 Hz, so
  the rate does NOT track — driver-owned fixed timing (deterministic,
  boot-stable). `AHIA_PlayerFreq` rungs all return rc=0 (accepted) while
  callback delivery stays 11 Hz; AllocAudio still negotiates 44100/16-bit.
  Consequence for the spec's latency/PlayerFunc-rate gate: write it
  against ~11 Hz reality, or make the engine independent of Player rate.
  Variants are scratch (`probe_ahi_{128,256}.c`, ladder-line-only diffs);
  repo `audio_io/probe_ahi.c` untouched.
- **Status 2026-09-22 (decision): the Dell E6320 is now the reference box
  for ITERATION; ultimate target is ABIv1.** This run's numbers are
  iteration-reference, not acceptance — see
  `2026-09-22-dell-reference-box-iteration-abiv1-target.md` (dual-build
  discipline, lane-scoped thresholds, characterize-on-Dell/confirm-on-ABIv1).
- **Status 2026-09-22 (reproduction): post-reboot rerun (session 750)
  reproduced every number** — ladder 7/7 rc=0, verify 55/3750, unit 0
  PRESENT err1=0/err2=-2; base `0x0000000101376c80` (placement varies per
  boot, always >4 GB). Reference numbers are boot-stable.
- Real hardware replaced VOID as the meaningful M1.1 lane. The
  ~11 Hz-vs-750 Hz shortfall is a real driver-timing discrepancy for the
  spec's latency/PlayerFunc-rate gate — worth a follow-up measurement at
  `frames=128/256` (does the observed rate track `playerfreq_hz` at all?)
  before deciding whether it is driver-owned timing vs engine_block
  request semantics. **DONE same day — driver-owned (see 11 Hz verdict
  above); engine_block request semantics exonerated.**
- The v1-SDK-builds-cannot-run-on-ABIv11 rule is upstream-general (any
  v1 SDK startup.o) — cross-posted to the Vulkan4Aros wiki (where the
  ABIs and laptop tooling live): `2026-09-22-laptop-abiv11-real-hardware-ahi-probe.md`.
- `docs/evidence/formats/m1-1-report.md` retains the codec-less appendix
  as VOID baseline; a real-hardware section should supersede/itemize next
  (not done this turn). **DONE 2026-09-22 — Appendix B + filled fields +
  hypothesis verdicts + Dell-lane procedure (G5 still OPEN: no commit,
  spec rows untouched).**

## Files
- `probe_ahi_v11` (24064 B), built from repo `audio_io/probe_ahi.c` with
  the v11 env; scratch under `/tmp/ri/`.
- Empty-`main` `min3_probe` (22992 B) proved the fault is startup-init,
  not probe code.
- Stash/toolchain: v11 SDK + source-tree AHI header path in the build
  recipe above.