# ReIncarnation — Codebase Review: Improvement Opportunities

**Date:** 2026-09-24
**Scope:** whole tree at `cfb3d4e` (engine/, tools/, audio_io/, midi_io/, gui/, app/, project/, spec + evidence docs)
**Goal:** list every place where the code can move closer to (a) **ReBirth RB-338 2.0** as a product, and (b) the **original Roland TB-303 / TR-808 / TR-909** it emulates. Correctness, performance, architecture and process findings found along the way are included too.
**Status of this document:** review findings. Nothing here is normative until it is adopted into the spec (`docs/superpowers/specs/2026-09-20-reincarnation-spec.md`). Evidence classes follow spec §2.1 (E0–E5).

---

## 0. Method and sources

1. Read every engine translation unit in full: `engine/dsp/*`, `engine/fx/*`, `engine/mixer/*`, `engine/seq/sched.*`, and the render paths in `tools/render.c` and `audio_io/audio.c`. Skimmed `gui/`, `app/`, `project/` and `midi_io/`.
2. **Ran** host experiments for the suspect numerical paths (scratch harness, not committed). Their results are quoted verbatim below.
3. Compared the code feature by feature against the **Propellerhead ReBirth RB-338 2.0.1 Owner's Manual**, an E1 source that the repo does not cite yet. Page numbers below refer to that manual.
4. Compared the device models against published circuit analyses of the original hardware. See §13 for the list.

**One headline finding about sources:** the ReBirth 2.0.1 manual, Appendix D (pp. 205–220), draws **all 54 PCF patterns (0–53)** as clean vector bar charts. Each chart shows the step grid, loop start and end markers, and the trigger velocity as bar height. The data can be extracted mechanically, and it would close **OPEN-04** at E1 (see §4.4).

---

## 1. Executive summary: the ten highest-value items

| # | Item | Class | Where |
|---|------|-------|-------|
| 1 | `ri_exp` returns garbage (negative, then huge) for x < ≈ −25. After 2–3 s without a retrigger, the 808 RS, CL, CH and OH voices **ramp to full-scale noise** | **Critical bug** | `engine/dsp/kernels.c:26`, `engine/dsp/rb808.c` |
| 2 | The 303 panel "volume" knob (0x0305) drives the engine's **waveform** switch. No 303B control ID (0x031x) reaches any voice | **Critical bug** | `gui/panels.c:13-27`, `engine/dsp/rb303.h:18-24`, `engine/dsp/params.c:70` |
| 3 | There is no integrated engine. The song and live paths render **one 303 only**. 808, 909, FX and mixer exist only as isolated `--808/--909/--fx/--mix` test modes | Architecture gap | `tools/render.c:450,1037`, `audio_io/audio.c:391-402` |
| 4 | The PCF is modelled as a stepped-cutoff SVF with **no envelope**. ReBirth's PCF is a 2-pole LP/BP filter driven by a **pattern-retriggered Attack/Decay envelope** with velocity (manual p. 63, pp. 159–160). The pattern data is extractable from the manual | Fidelity + data | `engine/fx/pcf.c` |
| 5 | The delay ignores its "beats" parameter (hard-coded 0.75 beat on every render). The buffer is capped at 1 s, while ReBirth needs up to 32 eighth-note triplets (5.3 s at 120 BPM) | Bug + fidelity | `engine/fx/fx.c:332`, `:190` |
| 6 | The 808 metal oscillators use the wrong frequencies (830–5310 Hz, and 207–1327 Hz for CY). The 808 uses six fixed Schmitt oscillators at **205.3, 304.4, 369.6, 522.7, 540, 800 Hz** shared by CY, OH and CH. CB uses the 540 and 800 pair | Fidelity | `engine/dsp/rb808.c:12`, `rb808.h:59-66` |
| 7 | The 303 uses **one envelope for amp and filter**. The real 303 (and ReBirth) has a MEG (filter, Decay knob) and a fixed long VEG (amp), an accent that forces minimum MEG decay, and a cumulative accent-sweep capacitor | Fidelity | `engine/dsp/rb303.c:178-196` |
| 8 | The device and section model does not match ReBirth. 808: 11 slots / 16 sounds with switches, **maracas missing**, 6 kit-wide knobs instead of per-instrument controls. 909: 6 of 11 instruments, 4 kit-wide knobs. No per-instrument Level | Fidelity | `gui/panels.c`, `rb808.*`, `rb909.*` |
| 9 | The signal path is mono end to end. ReBirth has per-section **pan**, a stereo delay return with its own pan, a stereo master compressor and stereo export | Fidelity | `engine/mixer/mixer.h:17-19` |
| 10 | The PCF clock is a local float accumulator. It loses precision after ≈ 2048 sixteenths and **stops advancing** after ≈ 4096 (≈ 7 min at 140 BPM). It is also free-running instead of locked to the transport | Bug | `engine/fx/pcf.c:196-198` |

---

## 2. Verified defects (fix first)

### 2.1 `ri_exp` is unbounded-wrong outside [−8, 8] and callers exceed it (CRITICAL)

`engine/dsp/kernels.c:26-58` clamps `k` to ±32 but then evaluates a 9th-order Taylor series on an unreduced remainder `r`. For x < ≈ −25, `|r|` is no longer small and the polynomial diverges. Measured on the host:

```
exp(-20)=2.06e-09   exp(-23)=1.03e-10   exp(-30)=-3.15e-08
exp(-50)=-0.0048    exp(-100)=-60.1     exp(-200)=-108525
exp(-1000)=-5.2e+11 exp(-3000)=-1.18e+16
```

`rb808_voice_render` calls `ri_exp(-v->t / tau)` every sample, forever, because voices never deactivate (§2.3). With τ = 35 ms (CH) the argument passes −25 after ≈ 0.9 s. Measured peak per 100 ms window after a **single** trigger at t = 0 (window index : peak):

```
rs: 0:0.172 ... 19:0.057 20:0.110 22:0.361 24:1.000 28:1.394 29..39:1.400
cl: 0:0.182 ... 26:0.068 30:0.421 33:1.000 36:1.335 39:1.400
ch: 0:0.143 ... 22:0.070 25:0.361 27:0.930 30:1.298 33..39:1.400
oh: 0:0.182 ... 26:0.066 30:0.380 33:0.997 36:1.283 39:1.398
```

So any 808 pattern that leaves RS, CL, CH or OH silent for about 2–3 s turns into a full-scale buzz. That covers most breaks, song endings and the tail after Stop. The 1.4 ceiling is the soft-clip constant in `rb808_render_mix`. The SD noise term (`ri_exp(-t/0.09)`), the clap tail and the OH bleed have the same exposure. No test catches this because the goldens are shorter than the failure onset.

`ri_sin` has the same kind of cliff: its cycle clamp is ±64, so `ri_sin(500) = −7.8e15`. Current callers keep the argument small, but nothing enforces that.

**Fix:**
- Make the kernels total. For `ri_exp`, return 0 for x ≤ −87 (the float underflow point) and do a proper range reduction with |k| up to 126, so `r ∈ [−ln2/2, ln2/2]` always holds. For `ri_sin`, reduce with enough cycles for any finite input, or assert the domain in debug builds.
- Add a kernel property test over the full float range, not just the documented domain, in `tests/property/`.
- Stop evaluating `exp(-t/τ)` from absolute time. Use a per-sample multiplicative envelope (`env *= a`), as `rb303.c` already does. It is cheaper and cannot overflow (§6).
- Add a **long-silence regression test**: trigger each voice once, render 30 s, and assert monotone decay below −120 dBFS.

### 2.2 The panel and engine control-ID maps disagree (CRITICAL for the GUI and automation)

| ID | `gui/panels.c` + `ReIncarnation.guide` | `engine/dsp/rb303.h` |
|----|--------------------------------------|----------------------|
| 0x0305 | 303A **volume** | `RI_CTL_303A_WAVE` |
| 0x0306 | (none) | `RI_CTL_303A_VOLUME` |
| 0x0310–0x0315 | 303B controls | **not handled anywhere**: `rb303_set_param` only switches on 0x030x |

A user who turns 303A "volume" below 64 selects the sawtooth, and above 64 the square. 303B automation and panel moves go nowhere, and `render.c:442` only forwards `(ctl & 0xff00) == 0x0300`, which drops them.

**Fix:** keep one control table (ID, section, name, range, default, curve) as the only source, and generate `panels.c`, the `rb*_set_param` dispatch, the ARexx names, the manual rows and the `ri_audit.sh` greps from it. Dispatch per section on `(id & 0xFFF0)` and pass a section index, so 303A and 303B share one implementation. Add `Tune` and `Waveform` to the panel (ReBirth has both; §4.1).

### 2.3 808 voices never deactivate

`rb808_trigger` sets `active = 1`, and nothing ever clears it (`rb808.c:190`). After a voice fires once it is rendered every sample forever. That means 15 voices × `ri_exp` × one or more `ri_sin`/`ri_tanh` calls per sample, and `v->t` grows without bound. At t ≈ 350 s, `t += 1/48000` in float starts losing increments. Combined with §2.1, this is also what exposes the overflow.

**Fix:** deactivate when the envelope falls below about −100 dBFS and the filter state is below the denormal floor. Keep time as an integer sample count (`uint32_t n`) plus a recursive envelope.

### 2.4 The 808 mix soft-clip is discontinuous

`rb808.c:352`: `if (|m| > 1) m = tanh(m*0.5)*1.4`. At |m| = 1⁺ the output drops from 1.0 to tanh(0.5)·1.4 = **0.647**, a −3.8 dB step, which is audible as crackle on dense accented hits. ReBirth has no clipping inside sections either. The manual says "Clipping cannot occur within an individual section, even when its volume is set to maximum. Any distortion that appears can always be removed by lowering the Master level" (p. 23). So the section sum should stay linear (float headroom), with clipping only at the master or DAC stage. The 909 path (`rb909.c:287-289`) has the same issue in a milder form (`tanh(acc)` above 1).

**Fix:** remove the per-section clippers and keep float headroom through the graph. Clip only at the final integer conversion (`tools/render.c:63` already hard-clips there, which matches ReBirth's documented behaviour).

### 2.5 The delay ignores its time parameter and assumes 140 BPM / 48 kHz

- `RiFXRender` calls `ri_fxdelay_sync(&x->delay, bpm, 0.75f, sr)` on **every render** (`fx.c:332`), so `RI_FXID_DELAY_BEATS` is overwritten at once.
- `RiFXSetParam` syncs at a hard-coded `140.0f, 48000.0f` (`fx.c:273`).
- `RiFXCreate` initialises the compressor with `48000.0f` (`fx.c:225`), so attack and release are wrong at `--rate 44100`.
- The wrapper delay line is 48001 samples (1 s). ReBirth allows up to 32 eighth-note triplets (manual p. 70), which is 10.67 beats: 5.3 s at 120 BPM and 32 s at the 20 BPM minimum.
- Changing tempo jumps the read tap by whole samples with no interpolation, which clicks on every tempo change.

**Fix:** store the delay length in musical units (steps + straight/triplet flag) and resolve it against the live tempo and sample rate. Size the line for the worst case: 32 triplet-eighths at 20 BPM is 32 s × sr, about 1.5 M floats at 48 kHz. It should be caller-owned and allocated at load time, never on the render path. Crossfade or slew the tap on length changes.

### 2.6 The FX handle pool leaks

`RiFXCreate` hands out from `RI_FX_INST[8]` and `RI_FX_DBUF[2]`, and nothing ever releases them (`fx.c:186-218`). After eight creations (or two delays) every later create fails, for example on repeated song loads in a live session. **Fix:** add `RiFXDestroy`, or better, give the song engine one fixed instance per ReBirth unit (1 Delay, 1 Dist, 1 PCF, 1 Comp; §4.4), which is how the product is actually shaped.

### 2.7 PCF clock precision and ownership

`pcf.c:196-198` adds `bpm*4/(60*sr)` (≈ 1.9e-4 at 140 BPM) to a float `beat_pos` that never wraps. The float spacing at 2048 is 2.44e-4, so the clock runs about 26 % fast there, and above 4096 the increment rounds to zero and the clock **stalls**. That happens about 7 minutes into a 140 BPM session. **Fix:** derive the PCF phase from the master sample clock and the tempo map (`ri_map_tick_floor`) each block. This makes it transport-locked, as ReBirth requires: patterns restart with the sequence (manual p. 63). Keep an integer tick counter and wrap at the pattern length.

### 2.8 The live renderer is first-light scope only

`audio_io/audio.c:391-402` says so itself: `au_render_frames` renders one 303 voice. Spec §5 ("one renderer for live and offline") holds only for first-light songs. The full graph (`render.c --mix/--fx/--808/--909`) is a set of disconnected fixtures. See §5.1 for the target graph.

### 2.9 Smaller defects

| Where | Defect | Fix |
|-------|--------|-----|
| `rb909.c:212` `decay_env` | `t = pos / sr` mixes a 48 kHz-reference frame domain (scaled by `pitch_mult`) with the output rate. The CR/RD decay is off by the tune factor and by 48000/44100 at 44.1 kHz | Track elapsed output samples separately from the playhead |
| `rb909.c:260-272` | Scans all layers for `maxf` every sample | Cache the longest layer length at `rb909_set_layers` |
| `rb303.c:103-107` | Negative `midi` notes are impossible, but notes above 127 after octave-up are not clamped (octave flag is emitted but never applied) | Implement Up/Down per step (§4.1) and clamp |
| `sched.c` | `RI_EVFLAG_OCTAVE` is defined and never emitted. ReBirth steps have independent **Up** and **Down** flags (both on = neither, manual p. 154) | Model Up and Down bits in `RIStep` / RBNG |
| `midi.c:24-30` | `midi_note_step = note % 16` and accent = velocity ≥ 64 are arbitrary. ReBirth documents specific MIDI note maps for pattern select and remote control (manual pp. 20, 127, 224) | Take the map from the manual (E1) |
| `mixer.c:ri_mix_render` | `ri_mix_audible()` loops over 4 buses, and `ri_fader_gain()` runs per bus **per sample** | Compute targets once per block |
| `fx.c:ri_fxdist_render` | Normalising by `tanh(full-scale)` makes the output level depend on drive. `tanh` without oversampling aliases hard at drive 8 | 2×–4× oversample (or ADAA) and a fixed output law |
| `rb303.c:198-208` | Swaps `cutoff_hz` and `reso_k` in place to pass modulated values into `rb303_filter_step`, which also recomputes `g` with two `ri_sin` calls per sample | Pass the modulated values as arguments and update coefficients at control rate (§6) |
| `app/stepproof.c` | The playhead is clocked by a separate `timer.device` MICROHZ timer, measured at +347 µs/fire (+0.4 % drift, m43 record) | Derive the GUI playhead from the audio sample clock (the AHI render task posts a position), never from an independent timer |

---

## 3. Product fidelity: ReBirth RB-338 2.0.1 feature matrix

Source: ReBirth RB-338 2.0.1 Owner's Manual (E1). "Current" is the state of this tree.

### 3.1 Sections, patterns, transport

| Feature | ReBirth 2.0.1 (manual) | Current | Gap / action |
|---------|------------------------|---------|--------------|
| Sections | 303 ×2, 808, 909 (plus mixer, FX, transport) | Same list in tables. Engine integrates the 303 only | §5.1 |
| Patterns per section | **32**, 4 banks (A–D) × 8 (p. 147) | RBNG stores **one** pattern of ≤ 64 steps for one device | Rework RBNG (§7.1) |
| Pattern length | 1–16 sixteenths, per pattern and per section. Each section loops independently. Changes take effect immediately (pp. 36, 147) | Fixed `nsteps` per song | Per-pattern length. Per-section loop counters in the scheduler |
| Section on/off | "Deactivating" switch per section (p. 147) | None | Add |
| Shuffle | Per-section **on/off** switch plus one **global** Shuffle knob on the transport. "Two o'clock" gives the classic triplet feel. Affects even 16ths (pp. 21, 147) | One `shuffle_pct` per song option | Split into per-section flag + global amount. Map the knob so ≈ 2 o'clock is 2:1 triplet swing (66.7 %) |
| Tempo | **20–500 BPM** (p. 145) | 30–300 in RBNG, FX and PCF clamps | Widen everywhere. Size delay and PCF buffers for 20 BPM |
| Transport | Play = continue. Stop: 1st click stops, 2nd goes to loop start, 3rd to song start. Rew/FF 10 bars (held = continuous). Record. Loop on/off, Loop Start/Length. Bar display (pp. 145–146) | Play/Stop controls only | Implement the documented state machine exactly (it is testable as E1) |
| Song mode | Up to 999 bars. Pattern changes recorded per bar and section. Automation of **all knobs and switches except Tempo, Mute and Master level**. Real-time or step recording. Loop. "Initialize Song from Pattern mode" (pp. 111ff) | AUTO lane (≤ 256 events) on one pattern | Song track per section + automation lanes (§7.1) |
| Pattern edit | Cut/Copy/Paste, Clear, Shift Left/Right, Transpose, Random Pattern / Pitches / Accents, Alter Pattern (pp. 51–54) | Not implemented | Add as pure functions over the pattern model (easy to test) |
| 303 step entry | Pitch buttons, **Pitch Mode** auto-advance, Back/Step, Note/Pause, Down, Up, Accent, Slide. Clear sets Low C, Pause, all flags off (pp. 153–154) | Toggle-only step proof (`app/stepproof.c`) | Full 303 step editor |
| 808/909 step entry | Instrument selector, 16 step buttons, AC row. 909: click twice = accented hit, Flam mode button, modifier-key shortcuts (pp. 28–35, 226) | 16-button proof | Add |
| OH/CH exclusivity | 808: CH cuts OH, and OH+CH on the same step gives a **very short OH**. 909: CH cuts OH, and OH+CH on the same step gives **OH** (p. 35) | 808: no choke at all. 909: symmetric steal by trigger order | Implement both documented rules exactly, and test them (E1) |
| Mixer (per section) | On/off, output meter, volume fader, **Pan**, Delay send, Dist / PCF / Comp switches, Mute (not automated) (pp. 157–158) | 4 mono buses: fader, send, mute, solo | Add pan, per-section insert switches, section meter. ReBirth has **no solo** (drop it or mark it as a UX extension) |
| Master | Stereo meters, master fader, Comp switch (stereo master comp) (p. 165) | Master fader, mono meter | Stereo master and master-comp routing |
| Mods (.rbm) | Replace **all drum sounds in both rhythm sections** plus the panel graphics. Cannot change synths or FX. Songs reference a mod by name, with a missing-mod dialog (pp. 88–96) | RBNM mods swap 909 layers only. MODR references exist | Extend mod coverage to **808 sounds** (see §4.2 on sample vs. synthesis) and to skins |
| MIDI | Pattern select, remote control of all parameters, sync (clock), Live Sync (pp. 20, 112, 120, 127) | Learn map, MMC, clock counter; CAMD bridge 79 lines | Implement the documented maps (E1) |
| Export | AIFF/WAV, songs and patterns (p. 142) | WAV/AIFF, 16/24-bit, 44.1/48 kHz, **mono** | Stereo export |
| Vintage switch | Edit menu "Vintage ReBirth Synth Sound": shorter 303 decay (≤ v1.5 behaviour). Auto-on for old songs (p. 49) | Absent | Add as a per-song flag scaling the MEG decay law |
| ReWire | Per-section channels into a host | N/A on AROS | Document as out of scope. The AHI multi-channel export could stand in |

### 3.2 Effects (all four units)

| Unit | ReBirth 2.0.1 | Current | Action |
|------|---------------|---------|--------|
| Chain | Inserts in series **Dist → PCF → Comp**, each usable by **one section at a time** (Comp: one section *or* master). Delay is a send from all sections (pp. 59–70) | Isolated units, no routing | Implement the routing matrix, with exclusivity enforced as ReBirth does (radio behaviour) |
| Delay | On/off, meter, **Steps 1–32**, **Straight 16ths / 8th-triplet** switch, **F.Back** (bottom = one repeat, top = **infinite**), **Pan** (pp. 70, 161) | beats ∈ {0.5, 0.75, 1, 1.5}, fb ≤ 0.8, mono, no pan | Match the parameter set and laws. Feedback must reach 1.0 (infinite). Sustain after Stop (the manual says the delay keeps sounding after stop) |
| Dist | On/off, meter, **Amount**, **Shape** (p. 163) | Drive + shape (asymmetric tanh) | Names match. Add oversampling and a measured transfer curve (§8) |
| PCF | On/off, meter, **Pattern 0–53**, **Mode LP/BP**, **Freq**, **Q**, **Amt**, **Decay**. 2-pole (12 dB) filter plus **Attack/Decay envelope**. Patterns control retriggering, per-step dynamics and attack. Lengths vary (≤ 32 steps). Patterns 34–53 are **32nd-note** patterns (pp. 61–66, 159–160, 205–220) | 3 modes (LP/BP/**HP**), no envelope, no Decay, 54 × 16 neutral table | See §4.4 |
| Comp | On/off, meter, **Threshold**, **Ratio**, **Level-reduction meter** (p. 164) | Threshold only, fixed 4:1, fixed attack/release, auto make-up | Add Ratio and the GR meter. Mono per section, stereo linked on master |

---

## 4. Device fidelity: the original hardware, and ReBirth's rendition of it

Order of truth: the ReBirth behaviour (E1 manual, or E2/E3 black-box measurement of the freely available ReBirth 2.0.1, §8) sets the **target**. Circuit-level knowledge of the original units says **how to model** it.

### 4.1 TB-303 (`engine/dsp/rb303.*`, `params.c`)

**Missing controls.** ReBirth has Waveform, **Tune (two octaves, semitone steps; p. 155)**, Cutoff, Reso, Env Mod, Decay, Accent. The engine has no Tune, and the panel has no Waveform (see §2.2).

**Envelope architecture (biggest sonic gap).** The code has one `env` driving both the VCA and the cutoff (`rb303.c:178-199`). The original has:
- **MEG** (main envelope, drives the filter). Decay is set by the Decay pot on normal notes. **On accented notes it uses its shortest time**, the same as Decay fully anti-clockwise (Whittle, *TB-303's unique characteristics*). ReBirth matches this: accented steps are "shorter, louder, different character", and at Accent = 0 accented steps are "only shorter" (manual p. 156).
- **VEG** (volume envelope): fast attack, **fixed, long** exponential decay (about 3–4 s per the Devil Fish manual). The Decay knob does **not** shorten the amp. Notes end because the **gate closes**, not because the VEG decays.
- **Accent sweep circuit:** the accent pulse charges a capacitor through a lag network whose smoothing depends on the **Resonance** pot. Consecutive accents do not fully discharge it, so each peak rises higher (the "wow" on runs of accents). The code's accent envelope resets to 1.0 on each accent, so it cannot reproduce this.

**Recommendation:** split the voice into MEG (filter), VEG (VCA), gate, and an accent-sweep state (an RC with charge accumulation and a Reso-dependent lag). Tie accent to "MEG decay := minimum". Keep the existing P-01/P-02/P-06 ledger rows but re-parameterise them to this topology. Add the ReBirth "Vintage" decay law as a switch.

**Gate timing.** `sched.c` holds the gate high until the next step boundary (full-length gate). On the 303 the gate falls partway through each step, and only Slide (tie to the next step) keeps it high. Open303's AcidSequencer models this as a step-length fraction, and x0xb0x firmware as a sub-step clock count. Without it, consecutive non-slide notes never get the amp release between them, and the staccato 303 phrasing is lost. **Action:** add a gate-length rule (E0 now, measured later: §8), emit NOTE_OFF at `step_start + gate_frac·step`, and let shuffle move both edges.

**Slide.** The code slews toward the target with a fixed τ (a 1-pole in Hz). The 303 slide is an RC lag on the **pitch CV**, which is linear in octaves. So it should be exponential in log-frequency, which makes the glide time independent of interval direction in pitch space. **Action:** slew `log2(f)`, not `f`. Keep the P-03 A/B for τ.

**Oscillator.**
- Naive saw/square (`rb303.c:151-165`) aliases badly at 48 kHz, especially after octave-up. Use PolyBLEP/BLAMP or minBLEP. Deterministic implementations are simple and allocation-free.
- The 303 square is **derived from the saw by a transistor waveshaper**, not an ideal 50 % square, and its duty cycle and edge curvature differ from a textbook square. Model it as a shaped saw (Open303 does), so the square keeps the 303's characteristic spectrum.
- The code sets a square amplitude of ±0.5 against a saw of ±1.0, a −6 dB step on waveform switching that the 303 does not have. Calibrate both to the measured relative level.

**Filter.**
- The candidate is a 3-state tanh ladder. The 303's diode ladder has four poles, with the first pole's coupling behaving so that the response is closer to 18 dB/oct (the Stinchcombe analysis is already register entry 3). Keep the Appendix B candidate as a measurement-gated option, but add a 4-pole diode-ladder variant (Stinchcombe/Zavalishin-style, ZDF or 2× oversampled) and pick by A/B.
- Resonance is linear 0..3.8 across the knob. On the 303 the resonance pot also changes the accent-sweep lag (above), and its taper is audibly non-linear. Take the taper from ReBirth measurement (§8).
- Cutoff 100 Hz – 8 kHz, EnvMod 0–2 octaves: these are E0 guesses. Measure them from ReBirth captures.

**Zipper noise.** `rb303_set_param` jumps parameters instantly, and knob automation arrives at 30 Hz GUI granularity (`rbng.h` note). Add a 1-pole or linear smoother per continuous parameter at control rate (every 8–16 samples). This is deterministic, so goldens stay reproducible.

### 4.2 TR-808 (`engine/dsp/rb808.*`)

**Structure (ReBirth, manual pp. 148–150):** 11 instrument slots, 16 sounds. Switch pairs: **LT/LC, MT/MC, HT/HC, RS/CL, CP/MA**. Per-sound controls:

| Sound | ReBirth controls | Current |
|-------|------------------|---------|
| BD | Level, **Tone**, **Decay** | Kit-wide tune ±7 st (the 808 BD has no Tune), kit-wide decay |
| SD | Level, **Tune** (body only), **Snappy** (noise level) | None wired (`params.c:119-123` placeholders) |
| LT/MT/HT, LC/MC/HC | Level, **Tune** | Tune ignored (render passes `0.0f`, `rb808.c:262`) |
| RS/CL, CP/**MA** | Level | **MA missing entirely** |
| CB | Level | ok |
| CY | Level, **Tone**, **Decay** | Fixed |
| OH | Level, **Decay**; cut by CH | No choke |
| CH | Level; cuts OH; same step as OH gives a very short OH | No choke |
| AC | Global accent row plus **Accent level** knob, equal effect on all sounds | Binary ×1.5 fixed; panel knob is a no-op |

**Actions:**
1. Rebuild the 808 around **11 slots + 5 switches** (the spec's "15 voices" lock in §2.3 item 1 conflicts with ReBirth: the playable set is 11 at a time and the sound set is 16 including maracas). Add **MA** (the 808 maracas are high-passed noise with a short, sharp envelope).
2. Per-sound Level plus the per-sound parameters above. Remove the kit-wide Tune/Decay/Snappy/Tone knobs.
3. **Metal section:** replace `RI_808_METAL_RATIO × 1000/250 Hz` with the six fixed oscillators **205.3, 304.4, 369.6, 522.7, 540, 800 Hz** (Werner, Abel & Smith, ICMC 2014), shared by CY, OH and CH through their respective band-pass/high-pass networks. CB uses **540 + 800 Hz** (the code already has this pair). Band-limit the squares (PolyBLEP): six naive squares summed and high-passed at 7 kHz are mostly aliasing at 48 kHz.
4. **BD:** the 808 BD is a bridged-T resonator pinged by a pulse, with a small, fast pitch bend and a fundamental near 50–60 Hz. The P-07 candidate (170 → 48 Hz sweep, τ 22 ms) is closer to a 909 kick. Use the Werner/Abel/Smith DAFx-14 bridged-T model (circuit-bendable, physically informed). Tone is the output low-pass (click brightness). Decay is the resonator damping (the famous long 808 boom).
5. **SD:** two bridged-T resonators (body, tuned by Tune) plus high-passed noise mixed by Snappy. Re-measure P-08 on ReBirth captures before locking.
6. **RS/CL/CB/CP:** RS 800 Hz and CL 1100 Hz look low for the 808. Claves ring in the low-kHz range. Keep them as HYPOTHESIS and measure ReBirth (§8).
7. **Accent:** one global Accent-level knob (continuous), applied as an excitation scale (the register-entry-9 pre-envelope model is fine). Replace the binary 1.5 with the knob law.
8. **Envelopes:** move to recursive per-sample envelopes and deactivate voices (§2.1, §2.3).

**Sample vs. synthesis (decision needed).** ReBirth's rhythm sections are **sample playback**: mods "replace all the drum sounds with new ones, in **both** rhythm sections" (p. 88). The project synthesises the 808 in real time and sample-plays only the 909. Options:
- (a) Keep real-time 808 synthesis (closer to the hardware, costs CPU on AROS), *and* allow mods to override any 808 sound with samples (closer to ReBirth).
- (b) Render the modelled 808 offline into a clean-room sample pack at load time (like the 909 path, `tools/render.c:674 bake_default909`) and play samples. That is cheapest at runtime and closest to ReBirth, but Tone/Decay/Tune then need layer morphs or pitch-rate playback plus envelope shaping.

Recommend **(a) now plus mod override**, with (b) as the low-CPU fallback profile (§17 degraded mode). Either way the RBNM format must cover 808 sounds.

### 4.3 TR-909 (`engine/dsp/rb909.*`)

**ReBirth 909 (pp. 151–152):** 11 instruments.

| Sound | ReBirth controls | Current |
|-------|------------------|---------|
| BD | Level, **Tune**, **Attack** (click), **Decay** | Tune (sample rate) only |
| SD | Level, **Tune** (body), **Tone** (rattle decay), **Snap** | Tune only |
| LT/MT/HT | Level, Tune, Decay | **Missing** |
| RS, CP | Level | **Missing** |
| CH, OH | **Shared Level**, Decay each; CH cuts OH | Present, no decay, no shared level |
| CC (Crash), RC (Ride) | Level, **Tune** | Present (tune) |
| AC | Accent row + Accent level; steps can be **low/high** (click twice) | acc1/acc2 used as accent / flam |
| Flam | Flam mode button + **Flam knob** (width) | Flam via `accent==2`, knob is a no-op |

**Actions:**
1. Add LT, MT, HT, RS and CP.
2. **Follow the hardware split:** on the real 909, BD, SD, toms, RS and CP are **analog** voices, and only the hats and cymbals are **6-bit sample ROM** voices. ReBirth sample-plays everything (mod-replaceable). Suggested model: synthesise the analog voices (909 BD = sine VCO with pitch envelope, an attack click from a pulse plus noise burst, and waveshaping; SD = two VCOs plus filtered noise with Tone/Snappy) into sample layers at load time, or render them live. Keep the hats and cymbals as ROM-style samples with 6-bit companding character and a Tune that changes the sample clock (already modelled).
3. **Decouple accent from flam.** Model per-step velocity (off/low/high) plus a separate flam bit plus a global Flam knob (the scheduler already emits `RI_EV_FLAM` with a width in samples, `sched.c`). The `accent==2 ⇒ flam` overloading conflicts with ReBirth's UI model.
4. **Per-voice Decay and Level** (the `params.c:153-165` placeholders): implement Decay as an amplitude envelope applied over the sample. That is how the 909 hat decay pots act on the ROM playback VCA.
5. **Shared CH/OH Level** plus the documented same-step rule (OH wins on the 909).
6. **Layer morph (P-13):** keep it as an implementation technique for Tune-dependent timbre, but it is not a ReBirth feature. It should not be user-visible.

### 4.4 PCF (`engine/fx/pcf.*`)

The current model is a Chamberlin SVF with cutoff = `base·2^((v−64)/64·amt)` stepped per 16th, v always 64, and LP/BP/HP modes. The ReBirth model (manual pp. 61–66, 159–160, 205–220) is:

```
trigger(step) when pattern has a hit at step:  env.start(velocity=bar_height, attack=pattern_attack)
env: Attack (from pattern) → Decay (Decay knob), retriggered per hit
fc = Freq · 2^(Amt · env · K)          (Freq all the way up ⇒ Amt has no effect)
filter: 12 dB/oct 2-pole, LP or BP only, resonance = Q
pattern clock: 16th-note steps (patterns 0–33) or 32nd-note steps (34–53), loop length 1..32 steps, restart with the sequence
```

**Actions:**
1. **Extract the 54 patterns** from Appendix D. They are vector PDF graphics: parse the PDF drawing operators, or render with `pdftoppm` and measure bar x positions and heights against the step axis and the loop-end triangle marker. Store them as a ledger (length, resolution, velocity per step, attack flag where the diagram marks one) in `reference/pcf-patterns.*`, with a checksum and the page reference. This is **E1** and resolves OPEN-04 without any black-box capture. Cross-check a subset against ReBirth captures (E3).
2. Replace the stepped-cutoff model with the envelope model above. Add the **Decay** control. **Drop HP mode** (not in ReBirth; keep it only behind Power Mode).
3. Change the data model: `RI_PCF_NSTEPS 16` is wrong. Lengths vary up to 32, and patterns 34–53 run at 32nd-note resolution.
4. Lock the PCF clock to transport (§2.7). "Triggers every time the current sequence restarts."
5. Keep the 2-pole filter stable at high Q with a ZDF/TPT SVF instead of Chamberlin. Chamberlin's `fs/6` clamp limits Freq to 8 kHz at 48 kHz.
6. Retire the `pcf_table_load` / `PCF_TABLE_VERIFIED` compile gate once the pattern ledger exists. The current table only restates the E0 response law.

---

## 5. Engine architecture

### 5.1 One integrated render graph (spec §5 compliance)

Target per engine block (64 frames), single render task as in spec §4.1:

```
scheduler (4 section sequencers, independent loop lengths, song track)
  ├─ 303A ─┐
  ├─ 303B ─┤  per section: [Dist?] → [PCF?] → [Comp?] → level·pan (stereo) ─┬─→ master sum (stereo)
  ├─ 808  ─┤      (each insert owned by at most one section: radio rule)      └─→ delay send (mono in)
  └─ 909  ─┘
delay (steps / triplet, fb ≤ 1.0, pan) ──→ master sum
master: [Comp stereo-linked?] → master fader → meters → hard clip at DAC/export
```

- Replace the three duplicated render loops (`render_song`, `render_rbngsong`, `au_render_frames`) with **one** `ri_engine_render(engine, out_l, out_r, n)` shared by the CLI, export and the AHI render task. `tools/render.c` would then shrink to argument parsing plus sinks.
- Keep the `--808/--909/--fx/--mix` fixtures, but build them on the engine API with only one section active, so the goldens exercise production code.
- Voices take **per-sample event offsets** inside a block (the render loops already split at event positions). Move that splitting into the engine.

### 5.2 Scheduler for four sections and song mode

`ri_sched_emit_timed` handles one section and one pattern, capped at 256 events. It emits the whole pattern up front and insertion-sorts it (O(n²)). For ReBirth:
- Walk each section's current pattern incrementally per block (a lookahead window), with an independent loop length per section.
- Apply pattern changes at bar boundaries (song track or queued user selection). ReBirth switches on the next bar.
- Apply shuffle per section (flag) from the global amount.
- Emit 303 gate-off at the gate fraction (§4.1).
- Drop the 256-event static cap. Streaming emission needs only a small per-block buffer.

### 5.3 Stereo

Convert `RiMixer` to stereo (equal-power pan law, to be E3-verified against ReBirth), and make the delay return and the master stereo. Export and the AHI path become 2 channels. `audio_ahi.c:40` currently requests `AHIA_Channels, 1`.

### 5.4 Sample rate independence

Every coefficient must derive from the render `sr`. There are hard-coded 48000 constants in `fx.c:225,273`, `RI_909_REF_RATE` (legitimate as a layer domain, but see §2.9), `RI_FXDELAY_MAX`, and `RI_808_SR_DEFAULT`. Add a CI render at 44.1 kHz that compares spectra and timing against 48 kHz, not bytes.

### 5.5 Parameter smoothing and automation rate

- Continuous parameters use a smoother at control rate (block/8). Switches (waveform, modes) change at sample-accurate event positions, with the existing click-flag semantics.
- Automation recording: ReBirth records knob moves in Song mode. The code quantises the 30 Hz GUI moves at ppq/24. Document the resulting maximum automation rate and check it against ReBirth captures (knob sweeps in exported audio).

---

## 6. Performance (AROS CPU budget, OPEN-05)

The per-sample transcendental load is heavy for the E6320 reference box, and it grows linearly with active voices:

| Hot spot | Cost today | Cheaper equivalent (still deterministic, D1) |
|----------|-----------|-----------------------------------------------|
| `ri_exp` | Double-precision Horner + a **32-iteration** `ri_scale2` loop per call | Exponent-field construction (`2^k` via bit pattern, `k` in [−126, 127]) instead of 32 multiplies |
| `ri_tanh` | Calls `ri_exp` in double, per sample, per voice, 7× per 303 sample (ladder + input) | Rational (Padé) or spline approximation with a measured error bound; or ADAA |
| 303 filter | 4× `ri_sin` (two `ri_tan_small`) + `ri_pow2` per sample | Coefficients at control rate (every 8–16 samples), interpolated |
| 808 | 15 voices active forever × `ri_exp` + `ri_sin`/`tanh` | Deactivation (§2.3), recursive envelopes, phase-increment sine (rotating phasor or table) |
| 808 metal | 6× `ri_sin` + sign per sample for squares | Direct phase-accumulator squares (plus PolyBLEP) |
| 909 | `rb909_pitch_mult` (`ri_pow2`) and a layer scan per sample | Compute at trigger or on tune change |
| Mixer | Solo scan + 2 `ri_fader_gain` per bus per sample | Per block |

Add `tools/bench` output per section to the evidence ledger as the W1.1 CPU report, and set a per-section budget (for example ≤ 10 % of one core at 64-frame blocks on the E6320).

---

## 7. Data, file formats, persistence

### 7.1 RBNG must model a ReBirth song

The current RBNG holds one tempo, one step list of ≤ 64 steps, ≤ 256 automation events and mod refs. A ReBirth song needs at least:
- 4 sections × 32 patterns (4 banks × 8), each with length 1–16, shuffle flag and active flag.
- Per-step data: 303 (pitch, note/pause, up, down, accent, slide); 808 (per-slot hits + accent row); 909 (per-slot off/low/high, flam bit, accent row).
- Front-panel state for every knob and switch (including the 808/909 sound switches, FX on/off, routing switches, pan, sends), tempo, shuffle amount, Vintage flag, mod reference.
- Song track: ≤ 999 bars, per bar per section pattern number, plus automation lanes for all automatable controls (not Tempo, Mute or Master), loop start/length.

Suggested chunk plan: `VERS`, `SONG` (tempo, shuffle, flags), `SECT`×4 (panel state), `PATS`×4 (32 patterns each), `TRAK` (pattern-per-bar), `AUTO` (per-control lanes), `MODR`, `SKIN`, `CPRG`. Keep the existing unknown-chunk rules.

### 7.2 Optional: ReBirth `.rbs` import

Users own large `.rbs` libraries. An importer (clean-room, from documented behaviour and user-owned files, with a legal review as spec §1 requires) would be the single largest compatibility win (FC dimension). At minimum, file this as a spec OPEN item.

### 7.3 Mods (RBNM)

Extend RBNM to cover all 16 808 sounds and all 11 909 sounds, plus skin assets (§9). ReBirth's contract: mods change sounds and graphics, **never** functionality, and never the synths or FX (p. 88). Enforce that in the loader. Also add the missing-mod flow: "Standard", "Cancel", and a mod-provided URL (pp. 93–94), shown as a GUI requester.

---

## 8. Measurement plan against ReBirth itself (turning HYPOTHESIS into E3)

Propellerhead discontinued ReBirth and released it as a free download (the "ReBirth Museum", 2005), so the **reference application is obtainable for black-box measurement**. This is consistent with the spec's clean-room rules: measure behaviour, redistribute nothing.

Proposed capture harness (host side, outside the AROS lane):
1. Run ReBirth 2.0.1 (Windows build under an emulator, or real legacy hardware) and export audio via its own AIFF/WAV export (p. 142). This is deterministic and needs no loopback.
2. Build a library of **probe songs**, one variable at a time:
   - 303: single notes at each Cutoff/Reso/EnvMod/Decay/Accent knob position (a 9–17 point sweep) → cutoff/reso/envmod tapers, MEG/VEG times, accent sweep accumulation (runs of 1–8 accents), gate length, slide time, Tune range, Vintage on/off.
   - 808/909: each sound at each knob position → pitch, decay and spectral centroid tables. OH/CH same-step rules. Accent levels. 909 low/high step levels. Flam knob → flam width law.
   - Shuffle knob → offset law (verify "2 o'clock ≈ triplet").
   - Delay steps, triplet switch, feedback law (confirm fb = max → infinite). Pan law.
   - PCF: each pattern × Decay × Amt → cross-check the Appendix D extraction.
   - Mixer: fader law (P-17) and pan law. Compressor ratio/threshold curves and time constants. Distortion Amount/Shape transfer curves (sine sweeps).
3. Add each measured table to `docs/evidence/<unit>/` as an E3 row, with the probe song SHA-256 and the capture SHA-256. Then re-paste the `params.c` anchor tables. They are already structured for exactly this (`params.c:1-6`).
4. Add **comparison TCs**: our render vs. the ReBirth capture per probe (envelope RMS error, pitch error in cents, spectral distance), with tolerances in the ledger.

This plan converts most of Appendix A (P-01..P-17) from E0 guesses into E3 measurements, and it is the most efficient path to parity.

---

## 9. GUI and UX fidelity

- **Panel inventory** must match ReBirth: 303 (7 knobs + waveform + step editor), 808 (instrument selector, 11 slot legends + 5 sound switches, red Level knobs + white parameter knobs, AC, 16 steps), 909 (legends, Level + parameter knobs, Flam button + knob, 16 steps with two-level entry), mixers (on/off, meter, fader, pan, delay, Dist/PCF/Comp switches, mute), FX panels (§3.2), transport (§3.1), pattern sections (bank A–D, 1–8, length, shuffle on/off, section off). The current 6-panel table has 29 controls in total. ReBirth has well over 150.
- **Focus bar** (orange indicator), keyboard focus with up/down, and keyboard pattern selection (pp. 22, 224–226).
- **Meters** on every section and FX input (the manual uses them as diagnostics), plus the compressor gain-reduction meter (centre-zero).
- **Skins** from mods: the owned-pixels MCC classes (`rknb`, `rstp`, `rlbl`, `rfdr`, `rlvl`) are the right base. Load bitmaps from mod assets instead of procedural art, and keep procedural art as the clean-room default skin (spec §1).
- **Knob behaviour:** the owner-approved drag arc is fine. Also add ReBirth's modifier behaviours: fine adjust, and click-to-default if ReBirth has it (verify in the manual's key-command appendix).
- **Transport and playhead** from the audio clock (§2.9 last row).

---

## 10. Code quality and maintainability

1. **One control registry** (§2.2) removes four hand-maintained copies: `panels.c`, the `rb*.h` IDs, the guide rows and the audit greps.
2. **Placeholders that silently no-op:** `rb808_set_param` LEVEL/SNAPPY/TONE/ACCENT and `rb909_set_param` LEVEL/DECAY/FLAMRES accept input and do nothing, while the user manual lists these controls as working. Either wire them or have the manual and GUI show them as disabled. Spec rule 5 says unknowns stay marked.
3. **Duplicated render loops** (`tools/render.c:450-538` vs `:1037-1170`, plus `audio.c`) are about 200 lines of near-copy. Fold them into the engine (§5.1).
4. **Static process-global state**: `g_depth`, `g_rate` and `g_format` in `render.c`, the FX pools in `fx.c`, and `RI_MIDI_STATE` in `midi.c`. Move them into context structs so a live app can hold several songs or renders.
5. **Magic constants in voices** (for example `0.16f`, `0.08f` and `2500 Hz` in the BD click; `0.13f`/`0.09f`/`0.11f` in SD). Move them to named ledger-linked constants like the P-xx rows, so a measurement update touches one table.
6. **`ftz` via compare on every state variable, per sample**: fine on the host, but on x86-64 AROS consider setting FTZ/DAZ in MXCSR once in the render task. It is cheaper, and deterministic for a given binary.
7. **Event struct** carries `value`/`flags` with type-dependent meaning. That is fine, but add a static-assert table per type (the payload map in §8 of the spec) and a debug validator in `ri_event_less` consumers.
8. **Spec §2.3 item 1** ("15 voices LOCKED") conflicts with ReBirth's 11-slot/16-sound model and misses maracas. Re-open it as a decision (§11).

---

## 11. Spec and document alignment (decisions to make)

| # | Decision | Recommendation |
|---|----------|----------------|
| D-a | Add the **ReBirth 2.0.1 Owner's Manual** as an E1 source in the Prior-Art Register | Yes. Many P-rows and UX rules above come straight from it |
| D-b | 808 voice set | 11 slots / 16 sounds with 5 switches (supersedes the §2.3 item 1 lock) |
| D-c | 808 model | Real-time synthesis plus mod sample override (§4.2 option a) |
| D-d | PCF | Envelope model, LP/BP only, patterns from Appendix D (E1); HP moves to Power Mode |
| D-e | Tempo range | 20–500 BPM (E1) |
| D-f | Stereo | Stereo engine, export and AHI output |
| D-g | Solo | Not in ReBirth: either drop it, or mark it as a UX extension that is never persisted in RBNG |
| D-h | Gate length | E0 fraction now, measured from ReBirth later (§8) |
| D-i | `.rbs` import | New OPEN row, legal review first |
| D-j | Kernel domains | Kernels are total over all finite floats. The domain is a precision statement, not a safety one |

After adoption, update the parity matrix (§14 of the spec) with the new rows: PCF envelope, delay steps/triplet, compressor ratio, 808 switches, 909 instruments, song mode, and pattern edit functions.

---

## 12. Suggested order of work (Terry rule: finish one thing before starting the next)

1. **Kernel totality + 808 envelope/deactivation + section clip removal** (§2.1, §2.3, §2.4). Includes the long-silence tests.
2. **Control registry + 303 ID fix + 303B dispatch + Tune** (§2.2).
3. **Integrated stereo engine skeleton** (§5.1, §5.3), with only the 303s at first. First-light goldens stay byte-identical for mono 303A by construction (centre pan, equal-power law normalised at centre), or get re-baselined deliberately with a ledger note.
4. **303 fidelity pass:** MEG/VEG split, accent sweep, gate length, log-domain slide, band-limited oscillator (§4.1). Then the ReBirth measurement probes for the 303 (§8).
5. **808 rebuild:** slots and switches, MA, metal oscillators, per-sound controls, OH/CH rules, accent level (§4.2).
6. **909 completion:** 11 instruments, controls, step levels, flam knob (§4.3).
7. **Scheduler for 4 sections, pattern banks, lengths, shuffle flags; RBNG v2** (§5.2, §7.1).
8. **FX routing + Delay/Dist/Comp parameter parity + PCF envelope with the Appendix D patterns** (§3.2, §4.4).
9. **Song mode + transport state machine + pattern edit functions** (§3.1).
10. **GUI inventory to ReBirth parity + mod skins + MIDI maps** (§9, §3.1).
11. Performance pass against the W1.1 budget (§6). Keep it continuous, but gate it before the beta exit.

---

## 13. Sources

- Propellerhead Software, *ReBirth RB-338 2.0.1 Owner's Manual* (E1). Text: <https://archive.org/stream/synthmanual-propellerhead-rebirth-2.0.1-owners-manual/propellerheadrebirth2.0.1ownersmanual_djvu.txt>. PDF: <https://deepsonic.ch/deep/docs_manuals/propellerhead_rebirth_rb338_v2.01_manual.pdf>. Pages cited: 21, 23, 35–37, 49, 51–54, 59–70, 88–96, 142, 145–165, 205–220.
- ReBirth 2.0.1 update notes: <https://www.deepsonic.ch/deep/docs_manuals/propellerhead_rebirth_rb338_v2.01_update.pdf>
- ReBirth RB-338 overview: <https://en.wikipedia.org/wiki/ReBirth_RB-338>
- K. J. Werner, J. S. Abel, J. O. Smith III, *The TR-808 Cymbal: a Physically-Informed, Circuit-Bendable, Digital Model*, ICMC 2014 (six oscillators 205.3/304.4/369.6/522.7/540/800 Hz, shared by CY/OH/CH): <https://quod.lib.umich.edu/i/icmc/bbp2372.2014.221/3/--tr-808-cymbal-a-physically-informed-circuit-bendable-digital?page=root%3Bsize%3D50%3Bview%3Dtext>
- Baratatronix, *TR-808 Cymbal & Hi-Hat Synthesis*: <https://www.baratatronix.com/blog/cascadia-808-cymbal-hi-hat-synthesis>
- K. J. Werner, J. S. Abel, J. O. Smith III, *A Physically-Informed, Circuit-Bendable, Digital Model of the Roland TR-808 Bass Drum Circuit*, DAFx-14 (already in the prior-art register lineage)
- R. Whittle, *TB-303's unique characteristics* (MEG vs. VEG, accented MEG decay = minimum, accent-sweep accumulation): <https://www.firstpr.com.au/rwi/dfish/303-unique.html>
- R. Whittle, *Devil Fish User Manual* (VEG fixed ≈ 3–4 s on the stock 303; accent sweep circuit and Resonance interaction): <https://www.firstpr.com.au/rwi/dfish/Devil-Fish-Manual.pdf>
- Open303 (register entry 1; AcidSequencer gate-length model, saw-derived square)
- In-repo: spec v5 (`docs/superpowers/specs/2026-09-20-reincarnation-spec.md`), prior-art register (`reference/prior-art.md`), evidence ledger (`docs/evidence/`), llm-wiki records through m43.
