# 2026-09-24 — Codebase review: ReBirth 2.0 + hardware fidelity, verified defects

> Source: full-tree review at `cfb3d4e` + host experiments + ReBirth RB-338 2.0.1 Owner's Manual (E1) + published circuit analyses
> Collected: 2026-09-24
> Published: 2026-09-24
> Full document: [../../../docs/2026-09-24-improvement-opportunities.md](../../../docs/2026-09-24-improvement-opportunities.md)

## Disposition
New. First whole-codebase review against two targets: ReBirth RB-338 2.0 as a
product, and the original TB-303 / TR-808 / TR-909. Findings only — nothing
normative until adopted into the spec. This record keeps the load-bearing
facts; the full list (≈ 60 items, priorities, work order) is in the doc.

## Verified defects (host-measured)

**1. `ri_exp` breaks below ≈ −25 → 808 voices blow up (CRITICAL).**
`engine/dsp/kernels.c` clamps k to ±32 and then runs a 9th-order Taylor on an
unreduced remainder. Measured:

```
exp(-20)=2.06e-09   exp(-23)=1.03e-10   exp(-30)=-3.15e-08
exp(-50)=-0.0048    exp(-100)=-60.1     exp(-200)=-108525
exp(-1000)=-5.2e+11 exp(-3000)=-1.18e+16
```

808 voices call `ri_exp(-t/tau)` per sample and never deactivate. After ONE
trigger, peak per 100 ms window (index:peak):

```
rs: 0:0.172 ... 24:1.000 28:1.394 29..39:1.400
cl: 0:0.182 ... 33:1.000 36:1.335 39:1.400
ch: 0:0.143 ... 27:0.930 30:1.298 33..39:1.400
oh: 0:0.182 ... 33:0.997 36:1.283 39:1.398
```

i.e. RS/CL/CH/OH ramp to full-scale buzz 2–3 s after their last hit. Goldens
are too short to see it. `ri_sin` has a similar cliff: `ri_sin(500) = −7.8e15`
(cycle clamp ±64). Fix: total kernels, recursive envelopes, voice
deactivation, 30 s long-silence regression test.

**2. Control-ID mismatch.** Panel/manual 0x0305 = 303A "volume"; engine
0x0305 = `RI_CTL_303A_WAVE`, 0x0306 = VOLUME. 303B IDs (0x031x) are handled
nowhere (`rb303_set_param` switches on 0x030x; `render.c:442` forwards only
0x03xx&0xff00==0x0300). Fix: one control registry generating panels,
dispatch, ARexx, manual and audit rows.

**3. 808 mix soft-clip discontinuity.** `rb808.c:352`: |m|>1 →
`tanh(m*0.5)*1.4` drops 1.0 → 0.647 at the threshold.

**4. Delay.** `RiFXRender` re-syncs to 0.75 beat every render (`fx.c:332`),
overriding the beats param; `RiFXSetParam` syncs at 140 BPM / 48000
(`fx.c:273`); comp init at 48000 (`fx.c:225`); line capped at 48001 samples.

**5. FX pool leak.** `RI_FX_INST[8]` / `RI_FX_DBUF[2]` never released; no
`RiFXDestroy`.

**6. PCF clock.** Float `beat_pos` never wraps: runs ≈ 26 % fast near 2048
sixteenths, stalls above 4096 (≈ 7 min at 140 BPM); free-running, not
transport-locked. `pcf_pattern_step` returns 64 always (no modulation).

**7. One-renderer scope.** Song/live paths render one 303 only; 808/909/FX/
mixer are isolated `--808/--909/--fx/--mix` fixtures (audio.c tripwire
comment says so). Output is mono end to end.

**8. Smaller.** 909 `decay_env` mixes 48 kHz-ref frames with output sr;
`RI_EVFLAG_OCTAVE` never emitted; MIDI note→step = `note % 16`; mixer scans
solo per sample; stepproof playhead from an independent timer (+347 µs/fire,
m43) instead of the audio clock.

## ReBirth 2.0.1 manual facts (E1, not previously cited in repo)

- Tempo **20 to 500 bpm** (p. 145). Code clamps 30–300.
- **32 patterns per section**, 4 banks × 8; length 1–16 per pattern, each
  section loops independently (pp. 36, 147).
- Shuffle: per-section on/off + one global knob; "two o'clock" ≈ triplet feel.
- Transport Stop: 1st click stops, 2nd → loop start, 3rd → song start; Rew/FF
  10 bars (p. 146).
- Song mode automates all knobs/switches **except Tempo, Mute and Master**.
- Pattern edit: Cut/Copy/Paste, Clear, Shift L/R, Transpose, Random
  Pattern/Pitches/Accents, Alter Pattern (pp. 51–54).
- 303: Waveform, **Tune (two octaves, semitone steps)**, Cutoff, Reso, Env
  Mod, Decay, Accent; steps Pitch, Note/Pause, Down, Up (both = neither),
  Accent, Slide; "Vintage ReBirth Synth Sound" switch = shorter decay (≤ v1.5).
  Accent at 0: accented steps "will only be shorter" (p. 156).
- **808:** 16 sounds in 11 slots; switch pairs LT/LC, MT/MC, HT/HC, RS/CL,
  **CP/MA (maracas — missing in code)**. BD Tone+Decay; SD Tune+Snappy;
  toms/congas Tune; CY Tone+Decay; OH Decay; global Accent level (pp. 148–150).
- **909:** 11 instruments: BD Tune/Attack/Decay; SD Tune/Tone/Snap; LT/MT/HT
  Tune+Decay; RS, CP none; CH/OH Decay + shared Level; Crash, Ride Tune.
  Click twice = accented hit; Flam button + Flam knob (pp. 151–152). Code has
  6 voices.
- OH/CH: CH cuts OH in both; same step → 808 very short OH, 909 OH (p. 35).
- "Clipping cannot occur within an individual section" (p. 23) — code
  soft-clips 808/909 sections.
- Mixer per section: on/off, meter, fader, **Pan**, Delay send, Dist/PCF/Comp
  switches, Mute; no solo (pp. 157–158). Master stereo + comp switch.
- Inserts in series **Dist → PCF → Comp**, one section each (Comp: section or
  master stereo). Delay send: Steps (shortest 1 sixteenth, longest 32 eighth
  triplets), straight/triplet switch, F.Back (max = infinite), Pan (pp. 70,
  161). Comp: Threshold, Ratio, level-reduction meter. Dist: Amount, Shape.
- **PCF:** 12 dB 2-pole LP or BP (no HP), Freq, Q, Amt, **Decay**;
  Attack/Decay envelope retriggered by the pattern with per-step dynamics and
  attack. **Appendix D (pp. 205–220) draws patterns 0–53 as vector bar
  charts**: 0–33 are 16th-note, 34–53 32nd-note, lengths vary (max 32 steps;
  "Pattern 3 … 12 steps long", "Pattern 40 … 28 steps long"). Extractable →
  candidate E1 closure of OPEN-04.
- Mods (.rbm) replace drum sounds in **both** rhythm sections + graphics;
  cannot change synths or effects (p. 88) → ReBirth's 808 is sample playback.

## Hardware facts used

- TR-808 metal: six oscillators **205.3, 304.4, 369.6, 522.7, 540, 800 Hz**
  shared by CY/OH/CH (Werner/Abel/Smith, ICMC 2014). Code uses base 1000 ×
  {0.83..5.31} (CY base 250). CB 540+800 already correct.
- TB-303 (Whittle): MEG (filter) decay from the Decay pot, accented notes use
  the shortest MEG time; VEG (amp) fixed, long; accent-sweep capacitor makes
  successive accent peaks go higher. Code uses one envelope for amp + filter
  and resets the accent envelope per accent.

## Decisions proposed (see doc §11)
Add manual to prior-art register (E1); 808 = 11 slots / 16 sounds (re-opens
spec §2.3 item 1 "15 voices"); PCF envelope model, LP/BP only; tempo 20–500;
stereo engine; drop or flag solo; gate-length rule; `.rbs` import as new OPEN
row; kernels total over all finite floats.

## Measurement route
ReBirth was released as a free download (ReBirth Museum) → black-box probe
songs exported via its own AIFF/WAV export can convert most P-01..P-17 rows to
E3.

## Side note (working tree)
Uncommitted `gui/widgets/rknb.mcc.c` +2 lines sets
`s_inreq->io_Message.mn_ReplyPort = s_inport` after `CreateIORequest` —
redundant (CreateIORequest already sets the reply port) but harmless; not part
of this review.
