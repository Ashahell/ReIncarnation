# The riqemu1 "Software Failure" is two faults, and neither is ReIncarnation: a wrong-ABI build (fixed) and misaligned `movaps` in this image's AHI (not fixable here) (2026-10-02)

- Source: ReIncarnation session, 2026-10-02 (opencode lane)
- Collected: 2026-10-02
- Published: 2026-10-02
- Prior: [riqemu1 up, audio verified, no RIAPP, and a record corrected](2026-10-02-riqemu1-up-audio-verified-no-riapp-and-a-record-corrected.md) (**partly corrected again here — its "illegal instruction" reading and its r12 explanation were both wrong**), [r12moves is an inlining counter](2026-10-02-r12moves-is-an-inlining-counter-not-an-abi-hazard.md)
- Raw: [riqemu1 AHI privilege-violation reports](../evidence/2026-10-02-riqemu1-ahi-privilege-violation-reports.md) — both AROS requester dumps transcribed verbatim, including the two `movaps` addresses the diagnosis rests on
- Commit: unpushed at collection. No ReIncarnation device numbers here.

## What the user reported, and what it actually was

The VM showed a `Software Failure!` requester. Two *independent* faults produce that
same title, and separating them was the whole job.

**Correction to the previous record, which got this wrong twice.** It read the
requester as `Type: Illegal instruction (?!)` and blamed the v11 SDK's r12 library-call
convention. Both wrong:

- The error is **`0x00000008 - Privilege violation error`**, not an illegal
  instruction. At this guest's capture ceiling (scale 2, 512×384) the two read alike.
- r12 is **not** a stale convention. AROS's own
  [`arch/x86_64-all/ABI_SPECIFICATION`](https://github.com/deadw00d/AROS/blob/master/arch/x86_64-all/ABI_SPECIFICATION)
  (titled *"Specification of AROS x86_64 ABIv11 calling conventions"*) specifies **R12
  for base / SysV x86_64 ABI for arguments**, library-side base "R12", and states
  *"the only requirement is that R12 is used to pass base during call and that R12 is
  a callee-saved register"*. It also documents `-ffixed-r12` as the sanctioned
  mechanism: *"GCC is now hardcoded to have R12 as fixed register… R12 is not going to
  be allocated by GCC"*, and explicitly *"Not using R12 in caller side code is however
  NOT an ABI requirement."*

So the comment in `scripts/ri_build_aros.sh` — *"the v11 sdk trees emit the stale r12
convention… and fault on first LVO call"* — does not match upstream. **v11's r12
sequence is the documented AROS x86_64 ABI.** What is true is narrower and is what
`riqemu1` actually is: a **v1-lineage** guest. The repo's own `V1SDK=` default and its
`r12moves == 0` assertion are right; the *explanation* attached to them is not.

## Fault 1: wrong ABI for the guest — FIXED

The v11 RIAPP on riqemu1 produced a `Software Failure!` before any window existed.
The repo **already ships the correct recipe** and I had needlessly reinvented it:

```
bash scripts/ri_build_aros.sh riapp
AROS RIAPP BUILD OK (1099600 bytes)
objdump -d /tmp/ri/aros/RIAPP | grep -c 'mov    %rax,%r12'   ->  0
```

The script's own rule: *"ALWAYS build guest binaries against the v1 tree; verify with
`objdump -d $OUT | grep -c 'mov *%rax,%r12'` (must be 0)."* The v11 binary was 41.

With the v1 binary, **the panel opens and renders** — I have the capture: full synth
panel, five sections, transport, tabs. `RIAPP zoom: screen=1024x768`, `full_avg=712 us
n=5`. That is a real, verified fix, and it is what riqemu1 needed.

The difference from my own `build_ri.sh`: it links `-lstdcio` (v1's runtime) and the
shim at `-L /tmp/ri/libshim_v1`, not v11's `-lstdlib`/`-lcrt`. **Use the repo script;
do not re-derive a guest build.**

## Fault 2: AHI cannot open on this image — misaligned `movaps`, not fixable here

RIAPP reports `AHI unavailable - null backend active (offline render only) [err 7]`
(err 7 = open timeout) and a requester appears. Isolating it from ReIncarnation using
the repo's own probe, `audio_io/probe_ahi.c`, which contains **no ReIncarnation code**:

- With all six modes present, the requester names **`sb128.audio`**:

```
Task : RIAPP render
Error: 0x00000008 - Privilege violation error
Module sb128.audio  Function DriverInit
  mov $0x4bfab87c,%rdx ;  movaps (%rdx),%xmm0      <- 0x...7c not 16-byte aligned
Stack: ahi.device __DevOpen -> __AHI_LoadModeFile -> sb128.audio -> _LibInit_
```

- With the five faulting drivers quarantined (`DEVS:AHI/*.audio.noqemu`, leaving only
  `ac97.audio` and `void.audio`), the fault **moved into AHI itself**:

```
Task : RAM:probe_ahi
Module ahi.device  Function ReadConfig
  mov $0x4bf4d88c,%rax ;  movaps (%rax),%xmm0      <- 0x...8c not 16-byte aligned
Stack: ahi.device __DevOpen -> probe_ahi main
```

**Same mechanism both times: `movaps` on a misaligned address raises #GP.** The
compiler emitted a 16-byte-aligned SSE load against a constant that is not 16-byte
aligned:

| report | sequence | address | mod 16 |
|---|---|---|---|
| 1 | `mov $0x4bfab87c,%rdx` ; `movaps (%rdx),%xmm0` | `0x4bfab87c` | **12** |
| 2 | `mov $0x4bf4d88c,%rax` ; `movaps (%rax),%xmm0` | `0x4bf4d88c` | **12** |

Both are misaligned by 4 bytes, and both reports carry error `0x00000008`, which is
#GP on x86-64. Both dumps are in
[the raw evidence file](../evidence/2026-10-02-riqemu1-ahi-privilege-violation-reports.md),
transcribed from the operator-supplied reports. This is a defect in the
**binaries on this image**, not in ReIncarnation and not in AHI's logic: the fault is in
`ahi.device`'s own `ReadConfig`, on every open path, and `probe_ahi` reproduces it with
zero ReIncarnation code involved.

It is **image-specific, not AROS-wide**: the Dell's AHI works — every xruns
measurement in this lane depends on it. So this `riqemu1_dh0.img` carries a
miscompiled `ahi.device`/`sb128.audio`.

**Quarantine does not fix it; it only moves it.** Renaming all six mode files and five
drivers produced a *different* fault in a *different* module, and `probe_ahi` still
failed. **All eleven renames were reverted and the guest restored** to 7 drivers and 6
modes, verified. Nothing was left changed on the lane.

## Upstream check, as asked

- **Is there an ES1370 AHI driver?** No. The complete driver set in
  `workbench/devs/AHI/Drivers` is `Alsa, Aura, CMI8738, Device, EMU10kx, Envy24,
  Envy24HT, Filesave, HDAudio, NVHDMI, OSS, Paula, PulseAudio, RPiHDMI, RPiI2S, RPiPWM,
  SB128, SoundBlasterAWE, Toccata, VIA-AC97, Void, Wavetools, ac97, amiga-m68k`. The
  suggestion `-audio driver=sdl,model=es1370` is generic-correct — an emulated ES1370
  works on a current AROS — but **this AHI has no ES1370 driver**, so it would present a
  card with no mode and give silence. `ac97` is right for QEMU's `-device AC97`.
- **Is the r12 convention right?** Yes — it is AROS's own documented x86_64 ABI
  (quoted above). The repo's "stale" wording is the thing that is wrong.
- **Forum corroboration for the AC97 choice:** on arosworld, AROS's own developers
  state *"only the AC'97 audio emulation… is (fully) supported by AROS"*, *"VBox's
  HDAudio emulation has never worked with AROS"*, and *"the audio driver will be
  selected automatically in AROS. It doesn't matter what sound hardware the host machine
  uses."* That is 2019–2020 VirtualBox-era advice and it is not decisive for this
  image, but it agrees on AC97 and on HDAudio being the broken one.

## What this means for riqemu1

**Audio is unusable on this image.** Host-side the hardware path is fine — PulseAudio
shows `Sink Input #250`, `application.name = "riqemu1"`, `process.id` = the qemu pid,
`media.name = pa0`, s16le 2ch 44100 Hz — but the guest's AHI cannot open, so nothing
ever reaches it. **riqemu1 is therefore a GUI/render-cost lane, not an audio lane.**

**A `NOAUDIO` launch argument was written and then reverted.** `au_live_run()` at
`app/riapp.c:1832` is a single attempt point beside the existing `CAPTURE=` parsing, so
skipping AHI is a two-line change — but it did **not** clear the requester, and the log
was unreadable behind it, so it could not even be verified as honoured. Shipping an
unproven code path is worse than not shipping one: **reverted, tree left clean.**

The remaining fix is AROS-side and outside this repo: rebuild `ahi.device` (and
`sb128.audio`) for this image with alignment the compiler cannot assume away — e.g.
`-mno-sse` is too blunt, but ensuring the offending constants are actually 16-byte
aligned, or building with a toolchain whose default matches — or install an image whose
AHI is not miscompiled.

## Method findings

- **A requester's title is not its diagnosis.** `Software Failure!` covered a wrong-ABI
  load *and* a misaligned-SSE #GP *and*, for a while, I read one as the other. The
  guest's capture ceiling (scale 2; scale 1 refused "screen too large") made
  "Illegal instruction" and "Privilege violation" indistinguishable at 512×384. The
  full report arrived as **text**, and it is legible in a way the screenshot never was.
  **When a guest reports a crash, get the report, not a picture of it.**
- **A binary's own build script is the recipe.** I wrote `build_ri.sh` to
  parameterise `build_v11.sh` across ABIs and got the C runtime wrong twice. The repo
  already had `ri_build_aros.sh riapp`, correct, with the ABI assertion built in. The
  clue was one line of it (`V1SDK=`) that answered the question I had been
  archaeology-ing for.
- **Compare the crash module before and after an intervention.** Quarantining five
  drivers moved the fault from `sb128.audio` to `ahi.device`. That single observation
  is what proved the problem was not driver selection but the binaries themselves —
  a conclusion four earlier experiments had not reached.
- **A clock stalls in `rename` is a symptom.** `cmi8738.audio` and `HDAUDIO` renames
  took **9.9 s** where every other rename took 7–22 ms. The slow ones are the drivers
  that probe hardware on access, and they are the same family that later faults. A
  latency outlier in a bulk operation was pointing at the culprit all along.

## Still open, and now blocked on this

The **Levi sub-split** (`RI_ENGINE_ST_ARPA / LEVSEQ / LEVVOICE / LEVMIX` inside `SLEVI`,
each gated on the section test, `SLEVI` keeping its own timestamp because it wraps
four stages, `TOTAL` moved last) is built, tested and mutation-proven — `t156` plus
`mut_fixM6` 14/14 and `mut_fixM7` 25/25, every kill behavioural, audit 0/0, ASan clean.
**It still has no device number**, and this investigation is why: riqemu1 has no
usable AHI, and the Dell belongs to another session. Every remaining audio
measurement is blocked on one lane or the other.

## Standing gaps

- **AHI on `riqemu1_dh0.img` cannot open.** Until `ahi.device` is rebuilt or replaced,
  no audio measurement is possible on this VM. This blocks the Levi sub-split
  measurement on any lane, since the Dell belongs to another session.
- The repo's `ri_build_aros.sh` comment still calls the v11 r12 convention "stale",
  which contradicts AROS's own ABI specification. **Not changed**: it is a load-bearing
  explanatory comment next to a gate that is demonstrably right, and rewording it is an
  owner call, not a drive-by edit.
- `start_riqemu1.sh` documents the audio state as of 2026-09-30 (sb128 and hdaudio
  faulting, ac97 left alone). Today **six** modes are present and the fault is broader
  than those two. That file's comment is now out of date in a way that matters.

## See Also

- [riqemu1 up, audio verified, no RIAPP, and a record corrected](2026-10-02-riqemu1-up-audio-verified-no-riapp-and-a-record-corrected.md)
- [r12moves is an inlining counter, not an ABI hazard](2026-10-02-r12moves-is-an-inlining-counter-not-an-abi-hazard.md)
- [AROS `arch/x86_64-all/ABI_SPECIFICATION`](https://github.com/deadw00d/AROS/blob/master/arch/x86_64-all/ABI_SPECIFICATION) (R12 is the documented convention)
- [Two Dell-lane traps: bare-path launch and a hanging capture](2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md)