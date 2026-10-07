# AHI v7 ("AHI2"): modern audio for AROS, design document

- **Status:** Draft 3. Supersedes Draft 2 in full; Draft 2 superseded
  Draft 1. What changed and why is in Appendix E.
- **Audience:** implementers, driver authors, reviewers.
- **Conventions:**
  - MUST / SHOULD / MAY are normative.
  - **[VERIFY]** marks a claim about AROS internals that is still unchecked.
  - **[CHECKED]** marks one that has been read in the tree or measured, with
    where. Unless stated otherwise, "the tree" means the AROS x86-64 ABIv11
    source and SDK (AHI headers at `ahi.h` 6.1, 2017).
  - **[MEASURE]** marks a claim that needs a number nobody has taken yet.

---

## 0. How to read this document

This document commits to one design. Where an earlier draft said "we could do
A or B", this one says "we do A, here is why, here is what we give up".

Three sections are load-bearing; everything else depends on them:

- **§2, the gap analysis.** If a row cannot be defended, delete the row. If
  no rows survive, delete the project.
- **§3, the viability gate.** If AROS cannot wake a task from an interrupt
  fast enough, the first deliverable is kernel work, not audio work.
- **§13, the driver header.** If a competent Amiga driver author cannot
  implement it in a weekend, the interface is wrong.

Everything else is negotiable.

---

## 1. Problem statement

AHI v6 is a good 1990s design that solved the 1990s problem: retargetable
playback with software mixing, on a uniprocessor without a usable FPU.

The 2020s problem is different:
- float DSP chains;
- **negotiated and reported** latency;
- explicit channel layouts;
- full duplex with matched, reported offsets;
- hotplug;
- SMP.

AHI v6 meets some of these partway (it has 32-bit samples, a fixed 7.1
layout, full duplex and HiFi mixing; see §2). It cannot meet the rest without
changing its internal data path and its callback contract.

**The project:** replace the engine, keep the chassis, fix the contract.

---

## 2. Gap analysis

This section justifies the project. Every later decision traces back to a row
here. Rows are corrected against the tree: Draft 2 overstated G1 and G2, and
G9 was stated as fact without evidence.

| # | AHI v6 behaviour | Concrete failure | AHI2 disposition |
|---|---|---|---|
| G1 | Integer sample types: 8/16-bit, plus 32-bit (`AHIST_M32S`, `AHIST_S32S`) and "HiFi" 32-bit mixing (`AHIDB_HiFi`) since V6 [CHECKED `devices/ahi.h`]. **No float type. No headroom contract.** | A float DSP chain converts at every boundary. There is no defined clipping point, so headroom is wherever the mixer happens to saturate. | Replace: F32 non-interleaved is the only internal format; one clip point (§7.1). |
| G2 | Mono and stereo, plus **one fixed layout**, `AHIST_L7_1` (8 × 32-bit, interleaved) [CHECKED]. There is no channel map, nothing beyond 8 channels, and the order is implied by a constant. | No way to describe 5.1, quad or an arbitrary bus. Two drivers can disagree about which slot is LFE with no field to tell them apart. | Replace: an explicit channel map on every stream (§7.2). |
| G3 | Latency is neither negotiated nor reported. `AHIDB_MaxPlaySamples` is a capability hint. **The rate is not reliably reported either:** on real HDA hardware (Dell E6320, Sandy Bridge, ABIv11) a low-level client asking for 48000, 32000 or 22050 Hz reads 44100 back for every request [CHECKED: rate probe, 2026-10-03]. | An application cannot know its own output delay, or even its output rate. There is no A/V sync, no DAW and no delay compensation. | Replace: negotiation plus an itemised latency report (§9.1). |
| G4 | `ahi.device` unit path (multi-client): latency is unspecified. | Believed sluggish for interactive use [MEASURE]. | Replace with a mixer whose latency is a published number. |
| G5 | `AHIA_SoundFunc`/`AHIA_PlayerFunc` are documented as callable from interrupt or mixer context, with m68k Hook conventions. | Hostile to SMP (§11) and to C; hard to debug; "no blocking" cannot be enforced. | Change: all hooks, including legacy ones, run in task context (Decision 2), at a small compatibility cost (§15.3). |
| G6 | Full duplex exists (`AHIDB_FullDuplex`, `AHIST_INPUT`, `AHIA_RecordFunc`) [CHECKED], but the input/output offset is neither matched nor reported. | No dependable live monitoring; a recorded take lands at an unknown offset. | Replace: duplex is a stream direction with a shared clock and reported offsets. |
| G7 | Rate conversion happens where the application cannot see it. **Observed twice:** the Dell HDA reports 44100 for a 48000 request (G3); a QEMU AC97 guest ran AHI at 48000 while the host played 44100, so audio was 8.1 % slow with **no error anywhere in the stack** [CHECKED: 2026-10-03]. | Pitch and tempo errors that look like performance bugs. Unpredictable CPU. No guaranteed bit-exact path. | Move SRC to the core: drivers never resample (§13), and the bit-exact path is a CI test (§17.2). |
| G8 | No device-change, hotplug or rate-change notification. | USB and HDMI audio are second-class; unplugging is a hang, not an event [MEASURE: which drivers hang]. | Add: device change is a stream state transition (§8.2). |
| G9 | The driver API requires understanding the mixer. **The tree ships about 20 AHI drivers** (ac97, VIA-AC97, HDAudio, CMI8738, SB128, EMU10kx, Envy24, Envy24HT, Alsa, OSS, PulseAudio, WASAPI, Paula, Toccata, Void, Filesave …) [CHECKED `workbench/devs/AHI/Drivers`]. The problem is not that nobody writes drivers; it is **driver quality and maintenance**. Example: HDAudio's `DriverInit` faults on codec-less hardware [CHECKED, 2026-09-21]. | Drivers exist but are hard to verify: no conformance suite and no way to check latency honesty. | Replace with a nine-entry-point driver that does no mixing and no conversion (§13), plus a conformance tool (§18, `AHI2DrvTest`). |
| G10 | Position reporting is coarse and not tied to a system clock. | Cannot timestamp; cannot sync to anything. | Add: a presentation timestamp per buffer (§9.3). |
| G11 | No concurrency model beyond "don't do that". | Undefined on SMP, and AROS x86-64 **does** run SMP: the Dell above runs 4 CPUs under ABIv11 [CHECKED: `KrnGetCPUCount`, 2026-10-07]. | Add a small, enforceable model (§11). |

### 2.1 What AHI v6 got right, and we keep

Stated explicitly so nobody "improves" it away:

- **Retargetability.** The driver/core split is correct.
- **Device + library duality.** Low-level ownership for applications that
  need it; a device for those that don't.
- **Tag-based configuration** at the outer API: extensible without breaking
  the ABI. Internally we use versioned structs.
- **Single-owner discipline.** Informal in v6, formalised here.
- **`AHI_SampleFrameSize`, format enumeration, and the general shape of
  capability query.** Extend these; don't replace them.

### 2.2 What this project is not fixing

AHI v6 performance on 68k. The float path will be slower on a 68030. AHI2
therefore keeps an integer path for the **legacy API only**, on FPU-less
targets (§15.4). This is the one sanctioned exception to Principle 4 (one data
format), and it is bounded in §15.4.

---

## 3. Phase 0: the viability gate

Nothing past §4 is valid until Phase 0 completes. It takes about two weeks,
and it decides whether this is an audio project or a kernel project.

### 3.1 What must be measured

| # | Measurement | Method | Why it matters |
|---|---|---|---|
| M1 | Interrupt → task wakeup latency, worst case | The device period interrupt `Signal()`s a high-priority task, which reads `ReadEClock()` and records the delta. 10⁶ samples, idle and under load. | Sets the minimum safe period. The whole design rests on this number. |
| M2 | M1 under adversarial load | Concurrently: disk I/O; graphics blitting and **GUI repaint storms**; one spinning pri-0 task per CPU; heavy `AllocMem`/`FreeMem` churn; **`input.device` traffic** (see §3.4) | Xruns come from the worst case, not the average. |
| M3 | Period-to-period jitter of M1 | Variance of M1 | Determines the ring depth needed. |
| M4 | Worst-case `Forbid()`/`Disable()` hold time | Instrument exec, or sample with a high-priority timer | A 3 ms `Disable()` anywhere caps latency at 3 ms whatever audio does. |
| M5 | `ReadEClock()` cost, resolution and monotonicity across CPUs | Tight loop; cross-CPU comparison | Timestamps (§9.3) and attribution (§17.4) depend on it. |
| M6 | DMA-coherent allocation: API and alignment | Read the exec/HIDD memory APIs | Decides whether drivers can be written as specified. |

### 3.2 Decision rule

Let **W** = M2 at the 99.99th percentile, and report the maximum next to it.

| W | Verdict | Consequence |
|---|---|---|
| < 250 µs | **Green** | Period floor ≈ 64 frames at 48 kHz. Proceed as written. Round trip under 5 ms is achievable. |
| 250 µs – 1 ms | **Amber** | Period floor ≈ 128–256 frames; round trip 8–15 ms. Usable for games, desktop and tracking with monitoring caveats; not for live amp simulation. Say so; do not claim "pro audio". |
| > 1 ms | **Red** | Stop. The deliverable becomes exec work: a real-time scheduling class, bounded `Disable()` regions, interrupt latency. Audio resumes afterwards. |

### 3.3 Phase 0 output

A short report with the six numbers, the hardware, and the verdict. It becomes
§3.5, and every latency claim elsewhere cites it. Until then this document
contains no latency claims, only a latency architecture.

### 3.4 Preliminary data (not the Phase 0 report)

These are not M1–M6: they were taken through today's AHI (low-level API,
player hook signalling a render task), not through a bare interrupt, and not
under the M2 load. They are evidence the gate is plausible, and they shape M2.

- **Hardware and settings:** Dell E6320 (i5-2520M, Intel HDA), ABIv11 AROS,
  48 kHz mix, 256-frame buffer (5333 µs period).
- **Wakeup latency:** with the render task at **priority 21**, the
  hook-to-task wakeup latency over full songs (~55 000 periods per run, GUI
  active) has a **maximum of 38–68 µs** across runs [CHECKED: application
  logs, 2026-10-05 – 2026-10-07]. If M2 lands near this, the verdict is
  Green.
- **Priority is load-bearing.** At render priority 10, `input.device`
  (priority 20) pre-empted the audio task during GUI interaction and caused
  audible dropouts. Raising the render task to 21 removed them [CHECKED,
  2026-10-01]. **M2 must include input traffic, and §6.1's default engine
  priority must sit above `input.device`.**
- **`ReadEClock()` on PC AROS is a port read of the 8254 PIT channel 0**
  (`arch/all-pc/timer/ticks.c`), measured at **~2.2 µs per call** [CHECKED].
  - It is one global counter, so it is coherent across CPUs by construction
    (answering part of M5 and Q6).
  - It is slow: a pair of reads around a stage costs ~4 µs on this machine.
    This prices the always-on attribution of §17.4; see there.
  - There is no cheaper public monotonic source (no exposed TSC).
- **Screen writes go to an uncached VESA framebuffer** at ~220 MB/s, against
  ~4.3 GB/s RAM to RAM [CHECKED, 2026-10-07]. This is not an audio problem,
  but it makes "graphics blitting" in M2 much heavier on this class of
  machine than its pixel count suggests. A full-window repaint costs ~10 ms
  of CPU in the GUI task.

---

## 4. Design principles

In priority order; when two conflict, the lower-ranked principle gives way.

1. **Decisions over options.** A design that preserves optionality has not
   designed anything.
2. **Enforceable over exhortative.** A rule that a debug build or a test
   cannot check is not a rule.
3. **Compatibility is structural, not a layer.** Old code runs the same
   engine (§5, Decision 1).
4. **One data format, one clock, one mixer.** Each "and also" in a data path
   is a bug farm. The one sanctioned exception is §15.4.
5. **Explicit single ownership.** One task owns a stream; everyone else sends
   messages.
6. **Drivers are dumb.** A driver that can mix is a driver nobody can verify.
7. **Honest numbers.** Report real latency, including the embarrassing parts.
8. **Simplicity found by deletion.** Build it, run it, remove what turned out
   unnecessary, then document what is left.

---

## 5. The five decisions

### Decision 1: AHI v7, not a second stack

**Decided:** one `ahi.library` and one `ahi.device`, version 7. New functions
are additive LVOs. There is no `ahi2.library`.

**Why:** two stacks means two systems to maintain for a handful of part-time
developers, and a shim that translates (and is therefore subtly wrong) rather
than an identity. `lib_Version` exists for exactly this.

```c
/* v6 app, unchanged binary: */
AHIBase = OpenLibrary("ahi.library", 4);   /* gets v7, v6 entry points */

/* v7 app: */
AHIBase = OpenLibrary("ahi.library", 7);   /* fails on old systems */
```

Both reach the same engine. The v6 entry points are adapters over v7
primitives (§15).

**What we give up:** v6 internals cannot be broken freely, because v6
applications ride the same code. That is the point.

**Shipping order is not architecture:** see §20. During Phase 1 the old v6
library keeps serving v6 applications until the v7 engine is proven. The v6
adapters land in Phase 1b, in one release.

**Naming:** "AHI2" in prose; the artefact is `ahi.library` v7. New symbols use
the `AHI2_` prefix.

### Decision 2: every audio callback runs in task context

**Decided:** all audio callbacks (the new process hook and legacy
`SoundFunc`/`PlayerFunc`) run in the per-device engine task (§6.1). No audio
callback runs at interrupt level.

**Interrupt handler contract** (normative; nothing else, ever):

1. Acknowledge the hardware.
2. Store the DMA position into driver-private memory (a plain store; **not**
   an AHI call).
3. `Signal()` the engine task.

**Correction to Draft 2:** Draft 2 inserted a software interrupt between the
interrupt and the `Signal()`. On exec, `Signal()` is legal from interrupt code,
so the extra `Cause()` hop adds a scheduling stage and buys nothing. A driver
whose interrupt is shared and must stay minimal MAY use `Cause()`. The core
accepts either, and M1 is measured on the direct path.

**Why:** interrupt-level callbacks cannot coexist with SMP, cannot be
debugged, make "no blocking" impossible to enforce, and impose m68k Hook
register conventions on C. The cost is one wakeup per period, which is
exactly M1.

**What we give up:** the latency floor becomes a property of the AROS
scheduler, not of our code. §3 exists to measure it. We trade a hidden risk for
a measured one.

### Decision 3: the engine owns the hardware, always

**Decided:** there is one path. The engine task owns every hardware device.
"Low latency" is a stream configuration (a short period and a bypassed mix),
not a separate architecture.

**Why:** two paths means two sets of bugs, two latency models, two
compatibility stories, and an unanswered "both want the device" problem.

**Cost:** one mix stage. When a device has exactly one client at the device
rate with identity routing and unity gain, the engine MUST detect this and
degrade to a copy, or a pointer swap where the ring allows it. This fast path
is a CI test (§17.2).

**Escape hatch:** `AHI2F_HARD_EXCLUSIVE` on `AHI2_Configure`.
- It succeeds only if the device has no other active clients.
- While it is held, other opens fail with `AHI2ERR_DEVICEBUSY`.
- The client's hook writes the driver ring directly; the mix stage is gone,
  but the hook still runs in the engine task (Decision 2).
- It exists for the user with a pro interface who insists. Documentation
  MUST NOT present it as the normal path.

### Decision 4: one clock per device; SRC lives in the core; no aggregation yet

**Decided:**

- Each device's period interrupt is the sole timebase for its streams. There
  is no software clock.
- A client at the device rate with no format mismatch gets **bit-exact
  passthrough** (CI-tested).
- A client at another rate gets synchronous SRC at a fixed rational ratio, in
  the core, with the resampler's group delay reported in `LatSRC`.
- **Drivers never resample.** G7 shows what happens otherwise: a stack that
  resamples silently makes every rate and latency number wrong, and the
  symptom presents as a performance bug.
- **Multi-device aggregation** needs asynchronous SRC with drift tracking. It
  is roughly an order of magnitude harder than synchronous SRC, and it is out
  of scope until Phase 3 at the earliest.

**Drift reporting:** every stream exposes `ClockRateNum`/`ClockRateDen`, the
measured rate, which is never exactly nominal.

### Decision 5: one driver class, nine entry points, no intelligence

**Decided:** there are no driver tiers. One interface (§13).
- The driver describes the hardware, provides a DMA ring, reports position,
  and signals once per period.
- It does not mix, resample, convert or apply volume, except where the
  hardware does so for free, which it advertises.

**Why:** G9. The fix for unverifiable drivers is an interface small enough
that the header is the documentation, plus a conformance tool that tells the
author the driver is correct (§18).

**Bar:** a developer who has written a classic Amiga device driver and knows
their hardware MUST be able to implement it in a weekend. If a reviewer
disagrees, the interface shrinks.

---

## 6. Architecture

```text
  +----------------------------------------------------------------+
  |                         Applications                           |
  |    v6 API (unchanged binaries)          v7 API (AHI2_*)        |
  +----------------+-------------------------------+---------------+
                   |  adapter, not translation     |
                   v                               v
  +----------------------------------------------------------------+
  |                        ahi.library v7                          |
  |  negotiation | stream lifecycle | control ports | params        |
  +-------------------------------+--------------------------------+
                                  v
  +----------------------------------------------------------------+
  |                  Engine (one task per device)                  |
  |  latch control + params -> call hooks -> SRC -> channel map ->  |
  |  mix -> convert/clip once -> driver ring                       |
  |  fast path: 1 client, device rate, identity, unity -> copy      |
  |  exclusive: client hook writes the driver ring                  |
  +-------------------------------+--------------------------------+
                                  |  AHI2Driver (9 entry points)
                                  v
  +----------------------------------------------------------------+
  |  hdaudio.audio  ac97.audio  usb.audio  null.audio  hosted ...   |
  +-------------------------------+--------------------------------+
                                  v
                            hardware / DMA
```

`ahi.device` v7 remains the message-port front end for applications that
prefer IORequest semantics. It is a client of the engine like any other.

### 6.1 Task topology

- **One engine task per open device.** Its priority is configurable, and the
  **default MUST be above `input.device`** (§3.4). Optional CPU affinity
  (§11.4).
- **Client hooks run inside that task,** serially, in registration order. A
  hook that overruns harms every client on the device, hence the attribution
  in §17.4.
- **No audio code at interrupt level** beyond Decision 2's three steps.
- **A client that needs a task of its own** (e.g. a plugin host doing heavy
  work) uses `AHI2F_HARD_EXCLUSIVE`.

---

## 7. Formats, channels and buses

### 7.1 Sample format

```c
/* Format identifiers (an enumeration, NOT bit values). */
#define AHI2FMT_F32     0   /* the internal format: non-interleaved, native endian */
#define AHI2FMT_S16     1   /* driver boundary only */
#define AHI2FMT_S24_3   2   /* packed 3-byte, driver boundary only */
#define AHI2FMT_S32     3   /* driver boundary only */
#define AHI2FMT_U8      4   /* legacy capture hardware */

/* Format sets (capability fields) use this: */
#define AHI2FMTB(f)     (1UL << (f))
```

Draft 2 declared the format as `AHI2F_F32 = 0` and then used "bitmask of
`AHI2F_*`" in the capability structs. A format with value 0 cannot be a bit.
Identifiers and sets are now separate, and the `AHI2F_` prefix is left to
stream flags only.

- **Range:** nominal [−1.0, +1.0]. Values outside it are legal and MUST be
  preserved through the core.
- **Clipping happens exactly once,** at the driver boundary, when converting
  to an integer format. A client that clips internally throws away headroom.
- **Non-interleaved,** because every DSP kernel and plugin format wants it.
  Interleaving happens at the driver boundary and nowhere else.
- **Denormals:** the core MUST NOT change the floating-point environment
  observed by client hooks. Whether the engine sets flush-to-zero for its own
  mixing is an implementation choice, but it MUST NOT leak into the
  bit-exact path (§17.2).

### 7.2 Channel map

```c
#define AHI2CH_NONE  0
#define AHI2CH_FL    1   /* front left   */
#define AHI2CH_FR    2
#define AHI2CH_FC    3   /* front centre */
#define AHI2CH_LFE   4
#define AHI2CH_SL    5   /* side left    */
#define AHI2CH_SR    6
#define AHI2CH_RL    7   /* rear left    */
#define AHI2CH_RR    8
#define AHI2CH_MONO  9
#define AHI2CH_AUX   16  /* AHI2CH_AUX .. AHI2CH_AUX+47: bus channels */

#define AHI2_MAXCHAN 32
```

- **Explicit map:** every stream carries one. There is no implicit order.
- **The v6 `AHIST_L7_1` layout** is mapped to an explicit map once, in the
  compatibility layer. The fixed order becomes data, not convention.

**Downmix policy,** when a client asks for channels the device lacks:
- with `AHI2F_ALLOW_DOWNMIX`, the core downmixes with the published matrix
  (Appendix C) and sets `AHI2FA_DOWNMIXED` in `Actual.Flags`;
- otherwise `AHI2_Configure` fails with `AHI2ERR_NOCHANNELS`;
- **silent downmixing is forbidden.**

### 7.3 Buses

- A stream has **one input bus and one output bus**, each up to
  `AHI2_MAXCHAN` channels.
- A client that needs a sidechain opens a second stream and synchronises by
  timestamp.
- **What this trades:** a plugin host does some bookkeeping, and the core
  avoids a routing graph.

---

## 8. Stream lifecycle

```text
                       AHI2_AllocStream
                              |
                              v
                        +-----------+
                        | ALLOCATED |-- AHI2_FreeStream --> (gone)
                        +-----+-----+
                              | AHI2_Configure
                              v
                        +------------+ <-- AHI2_Configure (re-negotiate)
              +-------> | CONFIGURED |
              |         +-----+------+
              |               | AHI2_Prepare (alloc buffers, bind driver)
              |               v
              |         +-----------+ <---------------- AHI2_Prepare
              |         | PREPARED  |                        |
              |         +-----+-----+                        |
              |               | AHI2_Start                   |
              |               v                              |
              |         +-----------+  AHI2_Pause  +--------+|
              |         |  RUNNING  |------------> | PAUSED ||
              |         +--+-----+--+ <----------- +---+----+|
              |            |     |     AHI2_Start      |     |
              |            |     | AHI2_Stop(drain)    |     |
              |            |     v                     |     |
              |            |  +----------+             |     |
              |            |  | DRAINING |             |     |
              |            |  +----+-----+             |     |
              |  AHI2_Stop |       | tail flushed      |     |
              |            v       v                   |     |
              |         +-----------+ <-- AHI2_Stop ---+     |
              |         |  STOPPED  |------------------------+
              |         +-----+-----+
              |               | AHI2_FreeStream
              |               v
              |            (gone)
              |
              |   +--------+
              +---| FAILED | <-- device lost, driver error, xrun storm (from ANY state)
   AHI2_Reset     +--------+
```

### 8.1 Call legality

**A call in an unlisted state MUST return `AHI2ERR_BADSTATE`.** It is not
undefined behaviour.

| Function | Legal states | Caller | RT-safe |
|---|---|---|---|
| `AHI2_AllocStream` | — | any | no |
| `AHI2_Configure` | ALLOCATED, CONFIGURED, STOPPED, FAILED | owner | no |
| `AHI2_Prepare` | CONFIGURED, STOPPED | owner | no |
| `AHI2_Start` | PREPARED, PAUSED | owner | no |
| `AHI2_Pause` | RUNNING | owner | no |
| `AHI2_Stop` | RUNNING, PAUSED, DRAINING | owner | no |
| `AHI2_Reset` | FAILED | owner | no |
| `AHI2_FreeStream` | ALLOCATED, CONFIGURED, PREPARED, STOPPED, FAILED | owner | no |
| `AHI2_GetActual` | CONFIGURED and later | any | no |
| `AHI2_GetInfo` | any | any | no |
| `AHI2_OpenControl` / `AHI2_CloseControl` | any except FAILED | any | no |
| `AHI2_PostControl` | any | the control port's task | yes |
| `AHI2_BeginParams` / `AHI2_CommitParams` | any | the stream's single param writer (§9.6) | yes |
| `AHI2_GetPosition` | RUNNING, DRAINING, PAUSED | any | yes |
| `AHI2_Params` | inside the hook only | engine | yes |

**Changes from Draft 2:**
- `AHI2_FreeStream` is **no longer legal in PAUSED.** A paused stream still
  holds a prepared driver; stop it first. One rule, no implicit stop.
- `PostControl` now goes through a per-producer control port (§9.6); see
  that section for why.

### 8.2 Device loss

When a driver reports device loss (USB unplug, HDMI sink change), the engine:

1. moves every stream on that device to FAILED;
2. calls each hook once more with `AHI2PF_DEVICELOST` and `Frames == 0`. The
   hook MUST return promptly and MUST NOT touch buffer pointers;
3. signals each owner's notification port.

The owner calls `AHI2_Reset`, which returns the stream to CONFIGURED, and
re-runs `AHI2_Configure`. The new device may differ: re-read `AHI2Actual` and
assume nothing survived.

**This is G8:** a state transition, not a callback flag, because device loss
invalidates every assumption the client made.

---

## 9. API

### 9.1 Negotiation

Buffer size is a negotiation, not an assignment.

```c
struct AHI2Spec
{
    ULONG   Size;              /* sizeof(struct AHI2Spec): ABI version tag  */

    ULONG   Direction;         /* AHI2D_PLAY | AHI2D_CAPTURE (OR = duplex)  */

    ULONG   SampleRate;        /* desired; 0 = follow device                */
    ULONG   RateMin, RateMax;  /* acceptable range if resampling permitted  */

    UWORD   ChansOut, ChansIn;
    UBYTE   MapOut[AHI2_MAXCHAN];
    UBYTE   MapIn [AHI2_MAXCHAN];

    ULONG   PeriodWanted;      /* frames per hook call; 0 = device default  */
    ULONG   PeriodMin;         /* hard floor the client can tolerate        */
    ULONG   PeriodMax;         /* hard ceiling                              */
    ULONG   Periods;           /* ring depth in periods, >= 2               */

    ULONG   Flags;             /* AHI2F_*                                   */
    BYTE    TaskPri;           /* engine task priority hint                 */
    LONG    Cpu;               /* -1 = any, else requested CPU (advisory)   */

    AHI2ProcFunc Process;      /* NULL = push mode (§9.5)                   */
    APTR    UserData;
};

/* Spec.Flags */
#define AHI2F_HARD_EXCLUSIVE  (1UL<<0)  /* Decision 3                        */
#define AHI2F_VARPERIOD       (1UL<<1)  /* I accept variable Frames          */
#define AHI2F_NO_SRC          (1UL<<2)  /* fail rather than resample         */
#define AHI2F_ALLOW_DOWNMIX   (1UL<<3)  /* §7.2                              */
#define AHI2F_NO_HW_VOLUME    (1UL<<4)  /* keep hardware gain at unity       */

struct AHI2Actual
{
    ULONG   Size;

    ULONG   SampleRate;        /* the rate the DEVICE runs at, not an echo  */
    UWORD   ChansOut, ChansIn;
    ULONG   Period;            /* 0 if VARPERIOD granted: use PeriodMax     */
    ULONG   PeriodMax;         /* worst-case Frames the hook may receive    */
    ULONG   Periods;
    ULONG   Flags;             /* AHI2FA_*: which requests were honoured    */

    /* Latency, itemised, in frames at SampleRate. Never one number. */
    ULONG   LatOutApp;         /* client ring                               */
    ULONG   LatOutMix;         /* engine mix stage; 0 if exclusive/fast     */
    ULONG   LatOutDrv;         /* driver DMA ring                           */
    ULONG   LatOutHw;          /* FIFO + codec group delay, driver-reported */
    ULONG   LatInHw, LatInDrv, LatInApp;
    ULONG   LatSRC;            /* resampler group delay, rounded to the
                                  nearest frame; 0 if no SRC (§17.3)        */

    /* Measured true rate. Never exactly SampleRate. */
    ULONG   ClockRateNum;
    ULONG   ClockRateDen;
};

LONG AHI2_Configure(struct AHI2Stream *s,
                    const struct AHI2Spec *want,
                    struct AHI2Actual *got);
```

- **`Actual.SampleRate` is the device's real rate.** G3/G7: today a client can
  ask for 48000 and run at 44100 without being told. Under AHI2, if the device
  runs at 44100 and the client wants 48000, the reply is either "44100, SRC
  off" or "48000, SRC on, `LatSRC` = n". Never "48000" alone.
- **Latency is itemised** because a single number hides which part the client
  can shrink:
  - if `LatOutDrv` dominates, ask for fewer periods;
  - if `LatOutHw` dominates, change hardware;
  - if `LatSRC` is non-zero, the requested rate is not native.
- **Negotiation is a loop.** `AHI2_Configure` may be called repeatedly in
  CONFIGURED: ask, read what you got, accept or relax.

### 9.2 Capability query

```c
struct AHI2DevInfo
{
    ULONG   Size;
    char    Name[64];
    char    Driver[32];
    ULONG   Formats;              /* AHI2FMTB() set of native formats       */
    ULONG   Rates[16];            /* 0-terminated, or all 0 if continuous   */
    ULONG   RateMin, RateMax;     /* nonzero if continuously variable       */
    UWORD   MaxChansOut, MaxChansIn;
    ULONG   PeriodMin, PeriodMax, PeriodGran;
    ULONG   HwLatencyOut, HwLatencyIn;
    ULONG   Flags;                /* AHI2DF_DUPLEX, AHI2DF_HW_VOLUME, ...   */
    ULONG   ActiveClients;
    BOOL    Exclusive;            /* someone holds HARD_EXCLUSIVE           */
};

LONG AHI2_EnumDevices(ULONG index, struct AHI2DevInfo *out);
```

Query before you negotiate, and negotiate anyway: the answer can change in
between (hotplug).

### 9.3 The process hook

```c
struct AHI2Process
{
    struct AHI2Stream *Stream;

    float   **In;        /* [ChansIn][Frames];  NULL if playback-only     */
    float   **Out;       /* [ChansOut][Frames]; NULL if capture-only      */
    ULONG     Frames;

    UQUAD     PlayFrame; /* frames of Out emitted since Start             */
    UQUAD     CapFrame;  /* frames of In captured since Start             */
    UQUAD     EClock;    /* presentation time of Out[][0] / In[][0]       */

    ULONG     Xruns;     /* cumulative; compare against your last read    */
    ULONG     Flags;
    APTR      UserData;
};

/* Flags */
#define AHI2PF_FIRST       (1UL<<0)  /* first call after Start            */
#define AHI2PF_DRAINING    (1UL<<1)  /* flushing tail; input is silent    */
#define AHI2PF_DISCONT     (1UL<<2)  /* timeline jumped: reset filters    */
#define AHI2PF_DEVICELOST  (1UL<<3)  /* Frames == 0, buffers invalid      */

/* Return: a code, not void. */
#define AHI2PR_CONTINUE  0
#define AHI2PR_SILENCE   1   /* I wrote nothing; the mixer may skip me   */
#define AHI2PR_FINISHED  2   /* stop me after this buffer                */

typedef ULONG (*AHI2ProcFunc)(struct AHI2Process *);
```

- **One struct pointer:** extensible without an ABI break (fields are only
  ever appended).
- **`AHI2PR_SILENCE`:** in a mixer with many clients most are silent, and
  only the client knows it.

### 9.4 Normative guarantees to the hook

**Buffers**
- `In[c]` and `Out[c]` are 16-byte aligned.
- Input and output buffers never alias.
- Buffer pointer values may change on every call; do not cache them. The
  `In`/`Out` arrays themselves are stable for the life of the stream.
- `Out[c]` contains garbage on entry. The hook MUST write all `Frames`
  samples of every output channel, or return `AHI2PR_SILENCE` having written
  nothing.
- The core clips exactly once, at the driver boundary. The hook MUST NOT clip.

**Frames**
- `Frames == Actual.Period` on every call unless `AHI2F_VARPERIOD` was
  granted.
- `Frames` MAY be 0; handle it.
- `Frames <= Actual.PeriodMax` always.

**Time**
- `EClock` is the presentation time of the first output frame, compensated
  for `LatOutMix + LatOutDrv + LatOutHw`: the time the sample reaches the DAC,
  not the time the hook was called.
- For duplex streams, `EClock` refers to the output frame. The matching input
  frame is `LatInHw + LatInDrv` earlier.
- `PlayFrame` and `CapFrame` are monotonic and reset only by Start.
- **Resolution note:** on PC AROS, `ReadEClock()` ticks at the 8254 PIT rate
  (§3.4), so `EClock` resolution is below one frame at 48 kHz. Good enough
  for A/V sync; not a sample clock. Use `PlayFrame` for sample positions.

**After a discontinuity**
- `AHI2PF_DISCONT` means the timeline jumped. Reset filter state, tails and
  phase accumulators. `PlayFrame` may have jumped forward.

**Real-time constraints** (enforced, §17.4). The hook MUST NOT:
- block, `Wait()` or `ObtainSemaphore()`;
- allocate or free memory;
- call DOS, Intuition or graphics;
- call any AHI function not marked RT-safe in §8.1;
- take unbounded time. Its budget is `Period / SampleRate` seconds, shared
  with every other client on the device.

### 9.5 Push mode (ring buffers)

For clients that do not want a callback, `Spec.Process == NULL`:

```c
ULONG AHI2_WriteFrames(struct AHI2Stream *, const float **chans, ULONG frames);
ULONG AHI2_ReadFrames (struct AHI2Stream *, float **chans, ULONG frames);
ULONG AHI2_WriteSpace (struct AHI2Stream *);   /* frames writable now */
ULONG AHI2_ReadAvail  (struct AHI2Stream *);
```

- **Internally:** a hook the core installs, servicing an SPSC ring (§12) whose
  producer is the stream owner.
- **The trade:** strictly higher latency and strictly simpler. Most
  applications should use it, and the documentation MUST say so.

### 9.6 Control: two mechanisms, clearly separated

#### Queue: for events (order matters, every message matters)

**Defect fixed from Draft 2:** Draft 2 made `AHI2_PostControl(stream, …)`
callable "from ANY task", but implemented it on the SPSC ring of §12, which
allows **one** producer. Two tasks posting to the same stream would corrupt
the ring. On 68000 an MPSC queue would need CAS, which the 68000 lacks (only
`TAS`). So the producer becomes explicit:

```c
struct AHI2Ctrl
{
    UWORD Op;      /* AHI2C_* */
    UWORD Chan;
    ULONG Arg0;
    ULONG Arg1;
};  /* 12 bytes, fixed. No pointers unless the sender owns the lifetime for
       at least as long as the stream. */

/* Task context, not RT-safe: allocates one SPSC ring owned by the CALLING
   task. Each producer task opens its own port. */
struct AHI2CtrlPort *AHI2_OpenControl(struct AHI2Stream *, ULONG depth);
void                 AHI2_CloseControl(struct AHI2CtrlPort *);

/* Only the task that opened the port may post to it. Wait-free.
   Returns FALSE if full; the caller MUST handle that. RT-safe, so a hook
   may post to ANOTHER stream through a port it opened beforehand. */
BOOL AHI2_PostControl(struct AHI2CtrlPort *, const struct AHI2Ctrl *);
```

- **Draining:** the engine drains every port of a stream at the start of each
  period (C7).
- **Ordering:** per port, ordering is send order (C6).
- **Debug check:** a debug build stores the opener in the port and traps a
  post from any other task.

#### Parameter block: for continuous values (only the latest matters)

**Defect fixed from Draft 2:** Draft 2 had a two-sided double buffer written
by "any task". That tears in two ways:
- two writers race on the inactive side;
- a writer that commits twice within one period writes the side the engine
  is still reading.

Draft 2's own test (§17.7, "never observes a torn value") would have failed.

```c
struct AHI2Params
{
    float Gain[AHI2_MAXCHAN];
    float Pan;
    /* append only; the core never interprets these beyond Gain */
};

/* Writer: exactly ONE task per stream, the owner, or the task named by
   AHI2_SetParamWriter() while the stream is not RUNNING. */
struct AHI2Params *AHI2_BeginParams(struct AHI2Stream *);  /* private copy   */
void               AHI2_CommitParams(struct AHI2Stream *); /* publish       */

/* Engine, once per period (C7): copy the published block into the hook's
   frozen view. Inside the hook: */
const struct AHI2Params *AHI2_Params(struct AHI2Stream *);
```

**The mechanism** is a sequence lock with a single writer, plus a copy taken
at period start:

1. The writer bumps `seq` to odd, copies its private block into the
   published block, then bumps `seq` to even, using a release store.
2. The engine reads `seq`. If it is odd, it **keeps last period's copy**,
   with no retry loop, so it is bounded. Otherwise it copies the block and
   re-reads `seq`. If `seq` changed, it discards the copy and keeps the old
   one.
3. A published value is therefore seen **at most one period late, and never
   torn**.

The copy is `sizeof(struct AHI2Params)` bytes per stream per period, which is
trivially cheap.

**The rule, for the documentation:** if losing an intermediate value is
harmful, use the queue; if only the final value matters, use the parameter
block.

### 9.7 Notification

```c
LONG AHI2_SetNotifyPort(struct AHI2Stream *, struct MsgPort *, ULONG mask);

#define AHI2N_XRUN        (1UL<<0)
#define AHI2N_DEVICELOST  (1UL<<1)
#define AHI2N_RATECHANGE  (1UL<<2)
#define AHI2N_STATECHANGE (1UL<<3)
#define AHI2N_FINISHED    (1UL<<4)
```

- Messages are sent from the engine task, never from an interrupt.
- **Messages are preallocated** per stream, one per notification kind. The
  engine does not allocate on the hot path (§9.4 applies to it too).
- A notification already pending is not sent again; its count field
  increments.

---

## 10. Error codes

```c
#define AHI2ERR_OK            0
#define AHI2ERR_NOMEM        -1
#define AHI2ERR_BADSTATE     -2   /* §8.1: the common one                 */
#define AHI2ERR_NOTOWNER     -3
#define AHI2ERR_DEVICEBUSY   -4   /* HARD_EXCLUSIVE held by someone else  */
#define AHI2ERR_NORATE       -5   /* rate unavailable and NO_SRC set      */
#define AHI2ERR_NOCHANNELS   -6   /* channels unavailable, no downmix     */
#define AHI2ERR_NOPERIOD     -7   /* [PeriodMin, PeriodMax] unsatisfiable */
#define AHI2ERR_DEVICELOST   -8
#define AHI2ERR_BADSPEC      -9   /* malformed Spec, bad Size, etc.       */
#define AHI2ERR_UNSUPPORTED -10
```

- **Return values:** every `LONG`-returning function returns one of these.
  The `BOOL` and pointer-returning RT functions (`PostControl`,
  `BeginParams`) signal failure by `FALSE`/`NULL`.
- **`AHI2ERR_BADSTATE` is returned, not asserted,** because a client racing a
  hotplug event is not a bug in the client.

---

## 11. Concurrency and memory model

### 11.1 Ownership

- **C1.** Every stream has exactly one owner task, fixed at
  `AHI2_AllocStream`.
- **C2.** Only the owner may call functions marked "owner" in §8.1.
- **C3.** Non-owners interact only through their own control port
  (`AHI2_OpenControl`/`PostControl`), the parameter block if they are the
  designated writer, and read-only queries.
- **C4.** Ownership never changes. There is no transfer API; free and
  reallocate.

### 11.2 Memory ordering

- **C5.** Publication uses a release store, and consumption an acquire load.
  This is implemented once, inside the control port, the parameter sequence
  lock and the ring of §12. Clients never write memory-ordering code.
- **C6.** Messages on one control port are applied in send order. Messages
  on different ports have no defined relative order.

### 11.3 Snapshot consistency

- **C7.** At the start of each period, before the first hook, the engine
  drains all control ports and copies all parameter blocks, once. Every hook
  sees a frozen view for the whole period. Anything posted during a hook
  takes effect next period at the earliest.

### 11.4 Scheduling: a dependency on exec, not a guarantee from us

| What we want | What the tree provides | Action |
|---|---|---|
| Real-time scheduling class | Priority only [CHECKED: no RT class in exec] | Use the highest practical priority, above `input.device` (§3.4). |
| SMP | **Present:** ABIv11 x86-64 runs all CPUs (4 on an i5-2520M) [CHECKED `KrnGetCPUCount`] | The engine is SMP-safe by construction (§11.1–11.3). |
| CPU affinity | Exec has an internal `TASKTAG_AFFINITY`/`TASKAFFINITY_ANY`, used under `__AROSEXEC_SMP__` in `rom/exec/exec_init.c`, but **it is not in the public SDK headers** [CHECKED v11 SDK] | `Spec.Cpu` stays advisory, and `Actual.Flags` reports whether it was honoured. Ask upstream to export the tag. |
| Cross-CPU calls (IPI) | Kernel-internal only. `kernel.conf` reserves the IPI LVO with no public entry [CHECKED] | Not needed by AHI2. Noted because SMP-wide operations cannot be done from a library. |
| Priority inheritance | Believed absent [VERIFY] | Avoid the problem: no semaphore on the audio path, ever. |
| Bounded `Disable()`/`Forbid()` | Unknown; measure M4 | A bad M4 is a kernel bug report, and it becomes our latency floor. |

**Stated plainly:** if the scheduler misses a deadline, AHI2 will xrun. What
AHI2 can do is detect it, report it (`AHI2N_XRUN`), attribute it (§17.4), and
degrade predictably.

### 11.5 Interrupt rules

- **C8.** A driver's period interrupt does exactly Decision 2's three steps.
  It calls **no** AHI function: the position is a plain store into
  driver-private memory, read later by `Position()`.
- **C9.** No AHI2 API function may be called from interrupt context. There is
  no interrupt-safe subset.

Draft 2's C8 ("calls no AHI function other than the position update")
contradicted C9; this version does not.

---

## 12. Lock-free primitive

One primitive: an SPSC ring, used for every control port and every push-mode
ring.

### 12.1 Design notes

- **Free-running counters,** not masked indices: `used = head - tail` with
  unsigned wrap. This gives full capacity and an unambiguous full/empty test.
- **Three cache lines:** one immutable, one producer-private, one
  consumer-private.
- **The pad is a full line after each group,** so the groups land in different
  lines whatever the struct's alignment. An aligned allocation is still
  recommended, because it avoids straddling and wastes less, but it is not
  required for correctness.
- **No neutral queries.** The only fill-level query is consumer-side.
- **The uniprocessor fallback** is a compiler barrier. The ring never uses
  CAS, so it works on 68000.
- **Atomics:** the x86-64 GCC 16 toolchain ships `<stdatomic.h>` [CHECKED];
  m68k is still [VERIFY] (Q4).

### 12.2 `ahi2_ring.h`

```c
/* ahi2_ring.h: SPSC wait-free ring.
 *
 * Exactly one producer task and one consumer task.
 *
 * Index discipline: free-running uint32 counters, wrapping by unsigned
 * overflow.
 *     used  = head - tail         correct across wrap
 *     empty = (used == 0)
 *     full  = (used == cap)
 *     slot  = idx & mask
 * Capacity must be a power of two and <= 2^31.
 */
#ifndef AHI2_RING_H
#define AHI2_RING_H

#include <stdint.h>
#include <string.h>

/* ---- Atomic shim -------------------------------------------------- *
 * Define AHI2_NO_ATOMICS for uniprocessor targets (m68k) or toolchains
 * without C11 atomics. On a UP system a compiler barrier suffices: the
 * only reordering that can hurt is the compiler's.
 */
#if defined(AHI2_NO_ATOMICS)
  typedef volatile uint32_t ahi2_u32a;
  #define AHI2_BARRIER()     __asm__ __volatile__("" ::: "memory")
  #define AHI2_LD_RLX(p)     (*(p))
  static inline uint32_t ahi2__ld_acq(ahi2_u32a *p)
  { uint32_t v = *p; AHI2_BARRIER(); return v; }
  #define AHI2_LD_ACQ(p)     ahi2__ld_acq(p)
  #define AHI2_ST_REL(p,v)   do { AHI2_BARRIER(); *(p) = (v); } while (0)
#else
  #include <stdatomic.h>
  typedef _Atomic uint32_t ahi2_u32a;
  #define AHI2_LD_RLX(p)     atomic_load_explicit((p), memory_order_relaxed)
  #define AHI2_LD_ACQ(p)     atomic_load_explicit((p), memory_order_acquire)
  #define AHI2_ST_REL(p,v)   atomic_store_explicit((p), (v), memory_order_release)
#endif

#ifndef AHI2_CLINE
#define AHI2_CLINE 64
#endif
#define AHI2_PAD(n) char _pad##n[AHI2_CLINE]

typedef struct AHI2Ring
{
    /* immutable after init; read by both sides, never written */
    uint8_t  *buf;
    uint32_t  mask;      /* cap - 1 */
    uint32_t  esz;
    AHI2_PAD(0);

    /* producer-private */
    ahi2_u32a head;
    uint32_t  tail_cache;
    AHI2_PAD(1);

    /* consumer-private */
    ahi2_u32a tail;
    uint32_t  head_cache;
    AHI2_PAD(2);
} AHI2Ring;

/* Allocation on AROS: the tree has no AllocVecTags/AVT_Alignment and no
 * aligned AllocVec; the MorphOS-style aligned allocators are ".skip"
 * placeholders in exec.conf [CHECKED]. Over-allocate and align:
 *   raw  = AllocVec(sizeof(AHI2Ring) + AHI2_CLINE, MEMF_ANY | MEMF_CLEAR);
 *   ring = (AHI2Ring *)(((uintptr_t)raw + AHI2_CLINE-1) & ~(uintptr_t)(AHI2_CLINE-1));
 * Keep `raw` for FreeVec. `buf` must be >= cap*esz bytes, aligned for the
 * element type. */
static inline int ahi2_ring_init(AHI2Ring *r, void *buf,
                                 uint32_t cap, uint32_t esz)
{
    if (!r || !buf || esz == 0)               return 0;
    if (cap < 2 || cap > 0x80000000u)         return 0;
    if (cap & (cap - 1))                      return 0;   /* not pow2 */

    r->buf  = (uint8_t *)buf;
    r->mask = cap - 1;
    r->esz  = esz;
    r->tail_cache = 0;
    r->head_cache = 0;
    AHI2_ST_REL(&r->head, 0);
    AHI2_ST_REL(&r->tail, 0);
    return 1;
}

/* PRODUCER ONLY. Wait-free. Returns 0 if full. */
static inline int ahi2_ring_push(AHI2Ring *r, const void *item)
{
    const uint32_t h   = AHI2_LD_RLX(&r->head);
    const uint32_t cap = r->mask + 1;

    if ((uint32_t)(h - r->tail_cache) == cap) {
        r->tail_cache = AHI2_LD_ACQ(&r->tail);
        if ((uint32_t)(h - r->tail_cache) == cap)
            return 0;                       /* genuinely full */
    }
    memcpy(r->buf + (size_t)(h & r->mask) * r->esz, item, r->esz);
    AHI2_ST_REL(&r->head, h + 1);
    return 1;
}

/* CONSUMER ONLY. Wait-free. Returns 0 if empty. */
static inline int ahi2_ring_pop(AHI2Ring *r, void *out)
{
    const uint32_t t = AHI2_LD_RLX(&r->tail);

    if (t == r->head_cache) {
        r->head_cache = AHI2_LD_ACQ(&r->head);
        if (t == r->head_cache)
            return 0;                       /* genuinely empty */
    }
    memcpy(out, r->buf + (size_t)(t & r->mask) * r->esz, r->esz);
    AHI2_ST_REL(&r->tail, t + 1);
    return 1;
}

/* CONSUMER ONLY. Items available now; may grow behind your back, never
 * shrinks. There is deliberately no producer-side or neutral variant. */
static inline uint32_t ahi2_ring_avail(AHI2Ring *r)
{
    r->head_cache = AHI2_LD_ACQ(&r->head);
    return r->head_cache - AHI2_LD_RLX(&r->tail);
}

#endif /* AHI2_RING_H */
```

### 12.3 Topology

- **One ring per producer:** each control port (§9.6) is one ring, opened by
  its producer task. The engine drains all ports of all streams at period
  start (C7).
- **Why not MPSC:** SPSC is wait-free with no CAS, works on 68000, and has no
  contention. An MPSC queue would save the engine a loop over N rings and
  cost a CAS retry loop on the hot path. With N small, the loop is free and
  the CAS is not.

---

## 13. Driver interface

The bar from Decision 5: a competent Amiga driver author implements this in a
weekend. The header is the documentation.

### 13.1 Header

```c
/* ahi2_driver.h: the complete driver interface. */

struct AHI2DrvCaps
{
    ULONG Size;
    char  Name[64];

    ULONG Formats;          /* AHI2FMTB() set of formats accepted NATIVELY  */
    ULONG Rates[16];        /* 0-terminated list, or all zero if variable   */
    ULONG RateMin, RateMax; /* nonzero only if continuously variable        */

    UWORD MaxChansOut, MaxChansIn;

    ULONG PeriodMin;        /* frames; smallest interrupt interval          */
    ULONG PeriodMax;
    ULONG PeriodGran;       /* period must be a multiple of this            */

    /* BE HONEST. These numbers reach the user's latency display and every
     * A/V sync calculation. If you do not know, measure with a loopback
     * cable (AHI2Latency). Do not guess low. */
    ULONG HwLatencyOut;     /* FIFO + DAC group delay, in frames            */
    ULONG HwLatencyIn;

    ULONG Flags;
};

/* DrvCaps.Flags */
#define AHI2DF_DUPLEX         (1UL<<0)  /* in and out share one clock      */
#define AHI2DF_HW_VOLUME      (1UL<<1)  /* SetHwVolume is implemented      */
#define AHI2DF_POSITION_EXACT (1UL<<2)  /* Position() is sample-accurate;
                                           absent = period-granular        */
#define AHI2DF_REMOVABLE      (1UL<<3)  /* can vanish: USB, HDMI           */

struct AHI2DrvCfg
{
    ULONG Size;
    ULONG Direction;
    ULONG SampleRate;       /* one of Rates[] or within [RateMin, RateMax]  */
    ULONG Format;           /* one native AHI2FMT_* value                   */
    UWORD ChansOut, ChansIn;
    ULONG Period;           /* frames between interrupts                    */
    ULONG Periods;          /* ring depth                                   */
    struct Task *Engine;    /* the task the interrupt Signal()s             */
    ULONG        SigMask;   /* ... with this mask                           */
};

struct AHI2DrvRing
{
    APTR  OutBase;          /* DMA-coherent, driver-allocated, or NULL      */
    APTR  InBase;
    ULONG Frames;           /* total frames in ring = Period * Periods      */
    ULONG Format;           /* what the hardware actually consumes          */
    UWORD ChansOut, ChansIn;
    BOOL  Interleaved;      /* almost always TRUE at the hardware boundary  */
    ULONG OutStride;        /* bytes between frames (between channels if
                               non-interleaved)                             */
    ULONG InStride;
};

struct AHI2Driver
{
    ULONG Version;          /* AHI2_DRIVER_VERSION                          */

    /* ---- Task context. May block. May allocate. ---- */
    LONG  (*GetCaps)(APTR self, struct AHI2DrvCaps *out);
    LONG  (*Open)   (APTR self, const struct AHI2DrvCfg *cfg,
                                struct AHI2DrvRing *ring);
    void  (*Close)  (APTR self);
    LONG  (*Start)  (APTR self);
    void  (*Stop)   (APTR self);

    /* ---- Engine task, hot path. MUST NOT BLOCK. ----
     * Writes the monotonic hardware frame counter to *frames and returns
     * AHI2ERR_OK, or AHI2ERR_DEVICELOST once the device is gone. */
    LONG  (*Position)(APTR self, ULONG *frames);

    /* ---- Optional. NULL if unsupported. ---- */
    LONG  (*SetHwVolume)(APTR self, UWORD chan, float gain);
    LONG  (*Prefs)      (APTR self, struct TagItem *tags);
    void  (*Dispose)    (APTR self);
};
```

**Nine entry points, three of them optional.** Draft 2 said "four optional";
only `SetHwVolume`, `Prefs` and `Dispose` are.

**Draft 2 defects fixed here:**
- **`Position` returned `ULONG`** and was also required to return
  `AHI2ERR_DEVICELOST` (−8), which a frame counter cannot carry
  unambiguously. Status and value are now separate.
- **`Formats` was a "bitmask of `AHI2F_*`"** whose first value was 0. It is
  now `AHI2FMTB()`.
- **The interrupt target is a task and a signal mask** (Decision 2), not a
  software interrupt.

**The frame counter wraps** after 2³² frames (~24.8 hours at 48 kHz). The core
extends it to 64 bits; drivers only need to be monotonic modulo 2³².

### 13.2 Driver rules (normative)

1. **Do not mix.** Ever.
2. **Do not resample.** If the hardware cannot do a rate, leave it out of
   `Rates[]`, and the core resamples. G7 is what happens otherwise.
3. **Do not report a rate you are not running.** `Open` either sets the
   requested rate exactly or fails. The core verifies this by measuring
   `Position()` against `ReadEClock()` during `AHI2DrvTest`.
4. **Do not convert formats** unless the hardware does it for free. Advertise
   what you natively consume.
5. **Do not apply volume** unless `AHI2DF_HW_VOLUME` is set and the core
   asked.
6. **The interrupt handler does three things:**
   - acknowledge the hardware;
   - store the position;
   - `Signal()` the engine.

   No allocation, no logging, no other tasks.
7. **`Position()` is on the hot path.** If reading the hardware counter is
   slow, return the value the interrupt cached plus an interpolated estimate,
   and do not set `AHI2DF_POSITION_EXACT`.
8. **If `PeriodGran > 1`, say so.**
9. **`HwLatencyOut`/`In` must be measured, not guessed.** The test is §17.3.
10. **Removable devices:** set `AHI2DF_REMOVABLE`, and return
    `AHI2ERR_DEVICELOST` from `Position()` when the device disappears. The
    core handles the rest (§8.2).
11. **SMP:** state shared between the interrupt handler and
    Open/Close/Start/Stop must be protected with plain atomics or the §12
    ring. Never use a semaphore from the interrupt side.

### 13.3 Reference drivers (deliverables, not examples)

| Driver | Purpose | Phase |
|---|---|---|
| `null.audio` | Discards output, generates silence; timer-driven virtual clock. CI runs on it. | 1a, first |
| `loopback.audio` | Output feeds input with a configurable delay. Tests duplex, latency reporting, bit-exactness. | 1a, first |
| `hosted.audio` | Hosted AROS → host audio. **Port the existing `Alsa`/`PulseAudio` AHI drivers rather than start fresh.** On WSL2 hosts this needs the ALSA→Pulse bridge [CHECKED, 2026-07-16]. | 1a |
| `hdaudio.audio` | The driver that matters on real machines. **Port from the existing `HDAudio` AHI driver**, and fix its codec-less `DriverInit` fault on the way. | 1a |
| `ac97.audio` | Older hardware; proves the integer-format path. Port from `ac97`/`VIA-AC97`. | 2 |
| `usb.audio` | USB Audio Class 1/2 via Poseidon; proves hotplug. | 2 |

`null.audio` and `loopback.audio` exist before the engine does; they are how
the engine gets tested at all.

---

## 14. The engine

### 14.1 Per-period sequence

```text
period interrupt
  └─ driver: ack, store position, Signal(engine)
        └─ engine task wakes

  1. Position(); compute frames to produce
  2. detect xrun (position advanced more than expected) -> flag DISCONT
  3. drain every control port (§9.6, C7)
  4. copy every parameter block (seqlock, §9.6)
  5. for each client, in registration order:
       a. if SRC needed: run the resampler into the client's view
       b. call the hook
       c. if it returned SILENCE: skip the mix-add
       d. else: apply gain, apply the channel map, mix-add into the accumulator
       e. attribution sample (§17.4)
  6. convert accumulator -> driver format (clip exactly once, here)
  7. write into the driver ring at the write pointer
  8. Wait() for the next signal
```

### 14.2 Fast paths

Both are required, and both are CI-tested (§17.2):

- **Single-client passthrough:** one client, device rate, identity map, unity
  gain, matching format. Steps 5a, 5d and 6 collapse to a copy, and the result
  MUST be bit-exact. If the driver format is integer, step 6 still converts:
  bit-exactness is then defined against the reference conversion, not the
  float input.
- **Exclusive:** `AHI2F_HARD_EXCLUSIVE`. The hook is handed the driver ring
  region directly, and steps 5d and 6 vanish.

### 14.3 Xrun handling

An xrun is detected when the hardware position has passed the point the
engine intended to fill. On detection:

1. increment the per-stream and per-device xrun counters;
2. set `AHI2PF_DISCONT` for the next hook call;
3. post `AHI2N_XRUN`, with the attribution from §17.4;
4. resynchronise the write pointer to a safe distance ahead of the read
   pointer.

**Do not silently grow the buffer.** Hiding an xrun behind extra latency is how
audio systems become untrustworthy.

---

## 15. Compatibility with AHI v6

This is the hardest part of the project and the most likely to be
underestimated.

### 15.1 Mechanism

The v6 entry points are adapters onto v7 primitives inside the same library
(Decision 1). `AHI_AllocAudioA` builds an `AHI2Spec` from the v6 tags, calls
the internal `AHI2_Configure`, and installs an internal hook that runs the v6
mixing semantics.

### 15.2 Per-feature disposition

| v6 feature | Disposition |
|---|---|
| `AHI_AllocAudioA` / `AHI_FreeAudio` | Wrapped onto AllocStream + Configure + Prepare. |
| `AHI_ControlAudioA` | Wrapped tag by tag. Unmapped tags return `AHIE_UNKNOWN`. |
| `AHI_LoadSound` / `UnloadSound` / `PlayA` | Emulated in the compatibility layer. Sample playback and the v6 mixer live there, not in the core. |
| `AHIST_M32S`/`S32S`/`L7_1` | Converted at the adapter: `L7_1`'s fixed order becomes an explicit channel map. |
| `AHIA_SoundFunc` / `AHIA_PlayerFunc` / `AHIA_RecordFunc` | Semantics change (§15.3). |
| `AHIET_*` effects | Implemented in the compatibility layer; deprecated for new code. |
| `AHI_SampleFrameSize`, format constants | Unchanged. |
| `ahi.device` units, `CMD_WRITE`, `AHIET_*` requests | Unchanged interface, new engine. |
| `AHI_NextAudioID` / `AHI_GetAudioAttrsA` | Unchanged; attribute set extended. **`AHIDB_Frequencies`/`AHIDB_FrequencyArg` report the device's real rates** (G3). |
| Public `struct AHIAudioCtrl` fields | Offsets preserved; the v7 handle hangs off a private extension. Enumerate and lock them in a test (Q7). |

### 15.3 The one deliberate break

v6 documents `SoundFunc`/`PlayerFunc` as callable from interrupt or mixer
context; in v7 they run in task context (Decision 2).

- **Code that already runs fine from a task:** unaffected, which is believed
  to be most code [MEASURE: Q8].
- **Code that relies on interrupt-level exclusion:** may now race with its own
  main task.
- **Code using m68k Hook register conventions:** unaffected; the Hook
  mechanism is preserved.
- **Timing:** callbacks run slightly later and with slightly more jitter.

**Accepted.** The alternative forecloses SMP, enforceable real-time rules and
debuggability for the lifetime of the system.

**`AHI2F_COMPAT_IRQHOOK` is deferred, not designed.** Draft 2 proposed a
per-application option restoring interrupt dispatch. That is a second dispatch
path, which Principle 4 and Decision 3 exist to forbid.
- Build it only if Q8 finds real applications that break.
- If it is built, it is uniprocessor-only, and it makes the device refuse v7
  clients while it is active, so the two worlds never share an engine.

### 15.4 Bit-exactness and the 68k integer path

- **Output changes:** running the v6 mixer on a float engine changes the
  output. A tracker that byte-compares AHI output will differ. Document this;
  it is not a bug.
- **The integer path (the single sanctioned exception to Principle 4):** on
  FPU-less targets the float path is unacceptably slow, so the compatibility
  layer keeps an integer mixer. It is selected **only** when:
  - the target lacks an FPU;
  - every client on the device is a v6 client;
  - no v7 client is present.

  It is bounded: no v7 client ever sees it, and it is tested by the same v6
  regression corpus (§15.5). Without it, v7 is a regression on classic
  hardware (§2.2).

### 15.5 Compatibility regression suite

A Phase 1b deliverable: a corpus of real AHI applications (trackers, players,
games, AHI-Handler, datatypes), run before and after every engine change, with
output captured and compared.

The corpus MUST include clients that hit the G3/G7 cases: a 48000 request on
hardware that runs 44100, and a duplex client.

---

## 16. Plugin host readiness

### 16.1 The honest position

**Hosting VST3 is not a goal of this project and appears in no phase.**
- There are no VST3 plugins for AROS. VST3 ships as compiled Windows, macOS
  or Linux binaries.
- Hosting them means one of:
  - recompiling plugins and porting the VST3 SDK's COM-style machinery
    (weeks before one plugin makes a sound, and few would build);
  - a PE loader and a Win32 subset, which is Wine-scale.
- On top of either, GUI embedding is its own multi-month project.

### 16.2 What is a goal

The primitives any host needs, and which DAWs, trackers and games need anyway:

| Primitive | Where |
|---|---|
| Float32, non-interleaved, no clipping in the core | §7.1 |
| Negotiated block size with hard min/max | §9.1 |
| Itemised, honest latency for delay compensation | §9.1 |
| Sample position + presentation timestamp | §9.3 |
| Real-time callback with documented constraints | §9.3, §9.4 |
| Multiple independent streams | §6 |
| Device and rate change notification | §8.2, §9.7 |
| Measured true clock rate for external sync | §9.1 |

### 16.3 If a host is ever built, target CLAP first

CLAP is a plain C ABI, MIT-licensed, with no COM and no mandatory GUI
interface. It is a fraction of the VST3 work.

### 16.4 Explicitly out of scope

- **Transport and tempo:** host state, not audio-system state.
- **MIDI:** belongs in CAMD or its successor. AHI2 carries audio and a clock;
  that is the seam. A host syncing MIDI to audio needs exactly §9.3's
  timestamp and §9.1's measured rate, and nothing more.
- **Plugin scanning, loading and GUI embedding.**

---

## 17. Testing and validation

A rule that cannot be tested is not a rule. Everything here runs in CI on
`null.audio` and `loopback.audio`, on hosted AROS, with no hardware.

### 17.1 State-machine conformance

Generated from §8.1: for every (function, state) pair, call it and assert
`AHI2ERR_BADSTATE` where illegal and success where legal. About 150 cases.

### 17.2 Bit-exact passthrough, the most valuable test

```text
Setup:  null.audio configured for AHI2FMT_F32 (a test-only native format),
        48000 Hz, 1 client, identity map, unity gain, AHI2F_NO_SRC,
        period 256.
Input:  1,000,000 frames of known PRNG data, including values outside
        [-1.0, +1.0], denormals, +/-0.0, and +/-Inf.
Assert: driver ring content == input, bit for bit, every sample.
Repeat: with 2, 4 and 8 clients where all but one return AHI2PR_SILENCE.
Repeat: with null.audio in AHI2FMT_S32 and S16, asserting equality against
        the reference float->int conversion with the single clip.
```

A failure means a hidden gain stage, unwanted dither, a needless SRC, a
denormal flush, or a rounding bug.

### 17.3 Latency verification

```text
Setup:  loopback.audio with a known injected delay D.
Method: emit an impulse, capture it, measure the round trip in frames.
Assert (no SRC):   measured == LatOutApp + LatOutMix + LatOutDrv + LatOutHw
                             + LatInHw + LatInDrv + LatInApp,   exactly.
Assert (with SRC): |measured - (above + LatSRC)| <= 1 frame.
```

- **Why the tolerance:** Draft 2 demanded zero tolerance with SRC, which is
  impossible, because a resampler's group delay is generally a fractional
  number of frames and `LatSRC` is an integer.
- **Without SRC** the sum is exact or it is a lie.
- Any real driver must pass the no-SRC case with a physical loopback cable
  before it ships.

### 17.4 Real-time safety enforcement

**Not documentation: a debug build that traps.**

```c
#ifdef AHI2_RTDEBUG
  /* Set per task around every hook call. Patched AllocMem, AllocVec,
   * FreeMem, FreeVec, Wait, ObtainSemaphore, Printf and the DOS entry
   * points check the flag and Alert() with the offending caller's PC. */
  void AHI2_RTEnter(void);
  void AHI2_RTLeave(void);
#endif
```

**Attribution, always on, priced honestly.** The engine times each client
hook and keeps a per-client mean and worst case.
- **Thresholds:**
  - worst case above 50 % of the period budget: logged and exposed via
    `AHI2_GetInfo`;
  - worst case above 100 %: the client is named as the xrun cause in
    `AHI2N_XRUN`. After a configurable number of consecutive overruns it is
    muted and moved to FAILED.
- **Cost:** on PC AROS a clock read is a PIT port read, about 2.2 µs (§3.4).
  - Bracketing every hook costs about 4.4 µs per client per period.
  - At a 64-frame period (1333 µs) with 16 clients, that is about 5 % of the
    budget, spent measuring.
- **So attribution is sampled by default:** time every hook on one period in
  N (N = 16), and every hook in any period that overran, which is the case
  that matters.
  - Always-on full timing is a debug setting.
  - If a cheaper monotonic counter becomes public (e.g. an exported TSC),
    switch to it.

This is still the highest-value code in the project: it names the
application responsible when audio glitches.

### 17.5 Xrun under adversarial load

Run the full M2 load (§3.1) for one hour at the minimum supported period, and
record the xrun count and attribution. This number goes in the release notes.

### 17.6 Compatibility regression

Covered in §15.5. Differences are expected (the float mixer) but must be
characterised: within N dB, no clicks, no dropouts, correct length. A new
difference after an engine change is a regression.

### 17.7 Concurrency

- **Control ports:** four tasks, each with its own control port, post at the
  maximum rate while the engine drains. Assert per-port ordering (C6), no
  loss except honest queue-full returns, and no corruption.
- **Misuse trap:** a post from a task that does not own the port traps in the
  debug build.
- **Parameter block:** the writer commits at the maximum rate. Assert that the
  hook never observes a torn block and lags at most one period.
- **ThreadSanitizer** on hosted AROS [VERIFY toolchain support].

### 17.8 Fuzzing

Call `AHI2_Configure` with randomised `AHI2Spec` values, including hostile
ones: wrong `Size`, `PeriodMin > PeriodMax`, duplicate channel maps, 2³¹
channels, NaN rates.

Assert a clean error, no crash, no leak, and that the stream is still usable
afterwards.

---

## 18. Tooling

Shipped, not optional.

| Tool | Does |
|---|---|
| `AHI2Latency` | Round-trip measurement with a loopback cable; validates a driver's `HwLatency*`. |
| `AHI2Monitor` | Live view: clients, periods, xrun counts, per-client hook time (mean/worst), attribution. |
| `AHI2Bench` | Sweeps period sizes to find the minimum that survives N minutes of M2 load; reports it as the user's practical floor. |
| `AHI2Prefs` | Device selection, default rate and period, per-application overrides. |
| `AHI2DrvTest` | The conformance suite a driver author runs before submitting: caps sanity, position monotonicity, **real rate against claimed rate** (§13.2 rule 3), latency honesty, hotplug, interrupt timing. |

`AHI2DrvTest` matters disproportionately. G9 is a verification problem, and
this is the verifier.

---

## 19. Effort and staffing

A design that does not confront "there are perhaps two to five people on Earth
who will work on this, part-time" is fiction.

| Component | Estimate |
|---|---|
| Engine: mixer, SRC, format conversion, channel mapping, fast paths | 8,000 – 12,000 LOC |
| Library: lifecycle, negotiation, control, notification, v7 API | 4,000 – 6,000 LOC |
| v6 compatibility layer (incl. integer path, sample playback, effects) | 5,000 – 9,000 LOC |
| `ahi.device` v7 front end | 1,500 – 2,500 LOC |
| Reference drivers (null, loopback, hosted) | 1,500 LOC |
| Real drivers, each (ported from the existing AHI drivers) | 1,000 – 3,000 LOC |
| Test harness and suites (§17) | 3,000 – 4,000 LOC |
| Tools (§18) | 3,000 – 5,000 LOC |
| Documentation to the claimed standard | 2 – 4 months of writing |

- **Total for Phase 1a + 1b:** 20,000–30,000 LOC.
- **Schedule:** at part-time volunteer rates, Phase 1 is 12–24 months as a
  single block.
- **The highest-risk component is the v6 compatibility layer:** bug-for-bug
  archaeology against a system defined partly by its implementation. It will
  take longer than estimated.
- **That is why §20 splits Phase 1:** a project that ships nothing for two
  years with a handful of part-time developers usually does not ship.

---

## 20. Roadmap

### Phase 0: viability gate (2 weeks)

Measure M1–M6 (§3). Publish them, and decide Green, Amber or Red. Everything
else depends on it.

### Phase 1a: the spine (the first thing that ships)

- `null.audio`, `loopback.audio` and `hosted.audio` first, before the engine.
- Engine: F32, non-interleaved, one task per device, single-client fast path.
- Stream state machine, `AHI2_Configure` negotiation, itemised latency.
- The process hook in task context (Decision 2).
- Control ports (§9.6, §12) and the parameter block.
- Push mode (§9.5).
- `hdaudio.audio`, ported.
- Tests §17.1–17.4; `AHI2Monitor`; `AHI2DrvTest`.
- **v6 applications keep using the existing AHI v6, untouched.** v7 is
  opt-in: `OpenLibrary("ahi.library", 7)` succeeds only on systems where the
  v7 library is installed.

Ship this. It is useful to new applications on its own, and it proves the
engine on real hardware before the riskiest work begins.

### Phase 1b: one library (Decision 1 completed)

- The v6 compatibility layer on the v7 engine, including the integer path.
- The regression corpus (§15.5, §17.6).
- `ahi.device` v7.
- Only when the corpus passes does v7 replace v6 as the installed
  `ahi.library`. From then on there is one stack.

### Phase 2: usable

- Full duplex with matched, verified latency.
- Multichannel, channel maps, the downmix matrix.
- Quality SRC.
- `ac97.audio` and `usb.audio`.
- Hotplug and device change (§8.2).
- `AHI2Latency`, `AHI2Bench`, `AHI2Prefs`.
- Tests §17.5, §17.7, §17.8.

### Phase 3: competitive

- SMP hardening and affinity, once exec exports the primitives (§11.4).
- Multiple streams per application.
- A prefs and routing UI.
- Power management.
- Device aggregation and async SRC (Decision 4), only if Phase 2 is stable.

### Deleted permanently

Each may be reconsidered the day Phase 2 ships, and not one day sooner.

- Object-based / Atmos audio.
- Network audio.
- Bluetooth LE Audio.
- 384 kHz.
- Hardware-accelerated effect hooks.
- VST3 hosting as a goal.
- Transport and tempo in the audio layer.
- MIDI in the audio layer.
- Multi-bus streams.
- A dual Direct/Shared architecture.
- Interrupt-level hook dispatch, except as the deferred §15.3 option.

---

## 21. Open questions

Tracked, not hidden. Each needs an owner and a resolution date.

| # | Question | Status | Blocks |
|---|---|---|---|
| Q1 | What is W (§3.2) on real hardware? | Open. Preliminary hook-to-task max 38–68 µs at pri 21 on one Dell (§3.4); not M2. | Everything |
| Q2 | Usable CPU affinity API? | **Answered:** internal `TASKTAG_AFFINITY` exists under `__AROSEXEC_SMP__`, not exported in the public SDK. Ask upstream to export it. | §11.4, `Spec.Cpu` |
| Q3 | Worst-case `Disable()`/`Forbid()` hold (M4)? | Open | Latency floor |
| Q4 | `<stdatomic.h>` on the AROS m68k toolchain? | x86-64 GCC 16: **yes** [CHECKED]. m68k: open. | §12 shim |
| Q5 | DMA-coherent allocation API and alignment for drivers? | Open [VERIFY] | §13 |
| Q6 | `ReadEClock()` coherent and monotonic across CPUs? | **Answered for PC:** global PIT counter, coherent by construction, ~2.2 µs per read (§3.4). Other ports open. | §9.3, §17.4 |
| Q7 | Which `struct AHIAudioCtrl` fields do applications touch? | Open | §15.2 |
| Q8 | How many real applications depend on interrupt-level `SoundFunc`? | Open: decides whether `COMPAT_IRQHOOK` is ever built | §15.3 |
| Q9 | `AllocVecTags`/`AVT_Alignment` in AROS? | **Answered: no.** Aligned allocators are `.skip` placeholders in `exec.conf`; over-allocate and align (§12.2). | §12.2 |
| Q10 | SRC implementation: port or write? Licence? | Open | Phase 2 |
| Q11 | Why does real HDA hardware read back 44100 for every requested rate, and is the HDAudio driver resampling or misreporting? | **New.** Open. | G3/G7; `hdaudio.audio` port |
| Q12 | Where exactly is the 48000 → 44100 conversion in the hosted/QEMU AC97 path? | **New.** Open. | `hosted.audio`, G7 |

---

## Appendix A: Glossary

- **Stream:** an `AHI2Stream`; one independent audio path (play, capture or
  duplex).
- **Owner:** the task that called `AHI2_AllocStream`. Fixed for life.
- **Control port:** one producer task's SPSC ring into one stream (§9.6).
- **Period:** frames between driver interrupts; the hook's block size.
- **Engine:** the per-device task that drains control, calls hooks, mixes and
  feeds the driver.
- **Fast path:** the single-client identity case that collapses to a copy
  (§14.2).
- **Xrun:** the hardware consumed or produced past the point the engine
  serviced.
- **W:** the §3.2 worst-case interrupt-to-task wakeup latency; the number the
  project rests on.

## Appendix B: Compatibility matrix

| Caller | `OpenLibrary` version | Gets (after Phase 1b) | Gets (Phase 1a) |
|---|---|---|---|
| v4 app | 4 | v6 entry points over the v7 engine | the existing v6 library |
| v6 app | 6 | the same | the existing v6 library |
| v7 app | 7 | `AHI2_*` plus all v6 entry points | `AHI2_*` (v7 installed alongside) |
| v7 app on an old system | 7 | `OpenLibrary` fails: correct and intended | the same |

## Appendix C: Downmix matrix

Fixed. It MUST NOT change without a version bump: a changed matrix changes
every user's mix.

```text
5.1 -> stereo:
  L = FL + 0.7071*FC + 0.7071*SL
  R = FR + 0.7071*FC + 0.7071*SR
  LFE discarded (summing LFE into stereo is almost always wrong)

7.1 -> 5.1:     SL += RL,  SR += RR      (equal gain)
stereo -> mono: M = 0.7071*(L + R)
mono -> stereo: L = R = M
```

- **Applied only** with `AHI2F_ALLOW_DOWNMIX`, and always reported in
  `Actual.Flags`.
- **Headroom note:** the 5.1 → stereo sum can exceed 1.0. That is legal in
  the float core (§7.1), and it clips once at the driver boundary like
  everything else.

## Appendix D: Changes from Draft 1 (summary)

- One `ahi.library` v7, not a second stack.
- One path plus `HARD_EXCLUSIVE`.
- Task-context callbacks.
- Negotiation, not setters.
- Eight itemised latency fields.
- A struct-pointer hook with a return code.
- A state machine instead of untestable invariants.
- A corrected SPSC ring.
- VST3 dropped as a goal; CLAP noted.
- Transport and MIDI removed.
- One driver tier.
- The Priority-4 feature list deleted.
- SRC separated from aggregation.
- Executable tests.
- An effort estimate.
- The unmeasured latency claim withdrawn.
- A gap analysis added.

## Appendix E: Changes from Draft 2, and why

| Draft 2 | Draft 3 | Reason |
|---|---|---|
| G1: "sample formats are 8/16-bit" | 8/16/32-bit and HiFi exist; the gap is float plus a headroom contract | `devices/ahi.h` has `AHIST_M32S`, `AHIST_S32S`, `AHIDB_HiFi` |
| G2: "mono/stereo; multichannel ad hoc" | One fixed 7.1 layout exists; the gap is an explicit map | `AHIST_L7_1` |
| G9: "almost nobody writes AHI drivers" (stated as root cause) | About 20 drivers exist; the gap is verification and maintenance | `workbench/devs/AHI/Drivers` listing; the HDAudio `DriverInit` fault |
| G3/G7 asserted without evidence | Two observed silent rate conversions cited | Dell HDA reads 44100 for any request; the QEMU AC97 path played 8.1 % slow |
| No preliminary data | §3.4: wakeup max 38–68 µs at pri 21; priority vs `input.device`; `ReadEClock` = PIT, 2.2 µs; uncached framebuffer | Measured on real hardware; shapes M2 |
| Interrupt → `Cause()` → softint → `Signal()` | Interrupt → `Signal()`; `Cause()` allowed but not required | `Signal()` is legal from interrupts; the extra hop is pure latency |
| C8 "calls no AHI function other than the position update" vs C9 "no AHI function from interrupts" | Position is a plain store; no AHI call | Contradiction |
| `AHI2_PostControl(stream)` from "ANY task" on an SPSC ring | Per-producer control ports (`AHI2_OpenControl`) | Multiple producers on one SPSC ring corrupt it; no CAS on 68000 |
| Double-buffered params, any writer | Single writer, seqlock, copied at period start | Draft 2's design tears; its own §17.7 test would fail |
| "Nine entry points. Four optional." | Three optional | Count |
| `ULONG Position()` returning `AHI2ERR_DEVICELOST` | `LONG Position(self, ULONG *frames)` | A frame counter cannot carry −8 |
| `AHI2F_F32 = 0` used as a bit in `Formats` | `AHI2FMT_*` identifiers plus `AHI2FMTB()` sets | Value 0 is not a bit |
| `PeriodIRQ` (software interrupt) in `DrvCfg` | Engine task + signal mask | Follows the Decision 2 correction |
| §17.3 zero tolerance including SRC | Exact without SRC; ±1 frame with SRC | Fractional group delay |
| §17.2 F32 driver assumed | `null.audio` gets a test-only F32 mode; integer modes tested against a reference conversion | Drivers' native formats are integer |
| Always-on attribution, cost unstated | Sampled 1-in-16 plus every overrun period; full timing in debug | About 4.4 µs per client per period on PC AROS |
| `AHI2_FreeStream` legal in PAUSED | Not legal; stop first | A paused stream holds a prepared driver |
| `COMPAT_IRQHOOK` designed in | Deferred until Q8 shows need; if built, excludes v7 clients | A second dispatch path contradicts Principle 4 and Decision 3 |
| Phase 1: 12–24 months, v6 compat inside | 1a (v7 spine, v6 untouched) then 1b (one library) | Ship something before the riskiest work; same end state |
| `[VERIFY]` on SMP, affinity, `AllocVecTags`, `stdatomic`, EClock | Answered where the tree answers (Q2, Q4 x86-64, Q6 PC, Q9); Q11/Q12 added | Read the tree |
| Engine priority "default high" | Default above `input.device` | Observed dropouts at pri 10 |
| Notification allocation unspecified | Preallocated, coalesced | The engine must not allocate on the hot path |
| Paste artefacts ("Run Code", "Copy code", `sql`/`scss`/`vbnet` fences), tab-separated tables | Real Markdown | It did not render |
