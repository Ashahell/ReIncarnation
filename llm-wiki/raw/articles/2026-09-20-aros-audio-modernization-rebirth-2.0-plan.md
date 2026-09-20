# Comprehensive Plan: AROS Audio Modernization + "ReBirth 2.0 for AROS"

**Ingested:** 2026-09-20 into ReIncarnation `llm-wiki`
**Source:** user-provided plan text, stored verbatim below (only this header added).
**Status:** plan / design input — not yet executed.

---

Comprehensive Plan: AROS Audio Modernization + "ReBirth 2.0 for AROS"0. Scope, Constraints, and One Critical Legal Note
The legal constraint first (it shapes everything). "ReBirth" and the Propellerhead name are trademarks. Roland's TB-303/TR-808/TR-909 panel layouts are also protected — which is exactly why original ReBirth got away with it only via the licensing deal with Roland, and why modern clones (e.g. D16 Group's Phoscyon, AudioRealism's Bass Line) look similar-but-not-identical and model the circuits rather than copy UI artwork.
Recommended approach: build the project as an AROS-native acid-house groovebox that is functionally and behaviorally identical to ReBirth RB-338 v2.0.1 (same devices, same workflow, same sound), with original artwork that strongly evokes the original layout without being a 1:1 pixel copy of Roland's panel designs or ReBirth's JPEG assets. We'll refer to it internally as ReBirth-compatible and externally under a distinct name. Do not reuse Propellerhead/Roland image or sample assets; synthesize everything or record/licence new material. This is the standard clean-room-adjacent practice used by the modern 303/909 clones and keeps the project legally shippable.
Everything below assumes that constraint is accepted.
1. The Three Workstreams
Table
#	Workstream	Goal	Outcome
W1	AROS audio foundation	Make AROS's audio stack a first-class, low-latency, modern foundation	All AROS apps benefit; W2/W3 built on top
W2	ReBirth 2.0 recreation	Faithful look/feel + sound of RB-338 v2.0.1 on AROS	Flagship application
W3	Modern-hardware expansion	Exploit modern PCs (SMP, SIMD, multichannel, float audio)	W1 subsystem + extended ReBirth feature layer
2. Workstream 1 — AROS Audio Foundation
2.1 Current state assessment
AROS's audio path today is: ahi.device (retargetable audio, originally by Martin Blom, ported to AROS) mixing into a host driver (HDA/AC97/USB audio on x86-64, plus Paula emulation on m68k). AHI historically provided driver-based retargeting, mixing routines for 8/16-bit channels, multi-channel output (8 channels), and recording — but the mixing core is legacy single-threaded C aimed at 68k-era CPUs. AROS proper also has its own newer native HDA driver, but the mixing/routing/effects layer is thin.
Deliverable A1 — Audio foundation specification. Audit ahi.device and the native HDA driver on x86-64 AROS: latency figures, mixing precision, sample formats supported, driver architecture, and what host-OS abstraction exists. Output: spec document + benchmark suite (simple CLI test app that measures end-to-end latency, max voices @ 48 kHz/24-bit, underrun behavior).
2.2 Proposed new "AROS Audio Services" layer
Rather than replacing ahi.device (breaking compat), add a modern layer above it:
plain
Applications
  ├── legacy: ahi.device API (unchanged, back-compat)
  └── new: audio.library / audio.service
        ├─ float32 engine (per-stream gain/pan/FX graphs)
        ├─ worker thread pool (SMP) → SIMD mixdown (SSE2/AVX2, NEON)
        ├─ low-latency path → ahi.device (16/24/32-bit modes)
        └─ AHI-Mixer helper library for devs (one-call "play sample w/ gain/pan")
Key features to implement:
Float32 internal mixing — all ReBirth signal paths (and most modern audio apps) want float; convert to AHI's integer output formats at the end of the chain, supporting 16/24/32-bit output modes.
SMP-aware audio engine — ReBirth class apps need a realtime render thread per device section + a mixing thread pool. Crucial AROS-convention point: never use Forbid()/Disable() in audio code — AROS has SMP, so use semaphores/mutexes; document this as a hard rule.
Low-latency device.open modes — verify/tune AHI buffer sizes; expose a query for the app's supported latency range.
Multichannel output — route sections to discrete outputs (foundation for W3).
MIDI: standardize on CAMD (the Amiga MIDI standard AROS already has) + a USB-MIDI driver if not already solid.
Codec/render helpers — WAV/AIFF export (ReBirth v2.0 exported songs to WAV/AIFF — required for feature parity).
Developer conventions: Amiga-style autodocs, C header with __AHIUTILS__-style tags, catalog-based localization for prefs/tools, and an "Audio Dev Kit" with examples.
2.3 W1 milestones
Table
Milestone	Content	Est. effort
M1.1	Audit + spec + latency benchmark	4–6 weeks
M1.2	Float mixing core + SIMD mixdown in audio.library	8–10 weeks
M1.3	SMP thread pool, latency tuning, multichannel routing	6–8 weeks
M1.4	USB-MIDI/CAMD polish, WAV/AIFF render API, dev kit + docs	4–6 weeks
3. Workstream 2 — ReBirth 2.0 Recreation for AROS
3.1 Feature target (RB-338 v2.0.1 parity)
Per the historical spec, parity means:
Two TB-303 sections — monophonic, sawtooth/square VCO, 18 dB lowpass filter, Tune/Cutoff/Resonance/Env Mod/Decay/Accent controls, slide (portamento), accent, shuffle per 303 (v2.0 addition).
TR-808 section — modeled/analog-synth drums: bass drum, snare, 3 toms, 3 congas, rimshot, clave, clap, closed/open hat, cymbal, cowbell, accent, per-part level/tune/decay knobs.
TR-909 section — sample-based engine (multi-layer, tuneable) with per-step accent (two levels) and flams; bass drum (tune/attack/decay), snare (tone/snap), toms, rim, clap, hats, crash/ride with tune.
Sequencer: per-section 32 patterns × up to 16 steps, variable pattern length, song mode up to ~999 pattern steps, pattern/song modes, tap programming via mouse/keyboard/MIDI, real-time knob tweak recording into songs.
Mixer: per-section fader/pan/mute, delay send, compressor/distortion/PCF inserts, master section, level meters.
Effects (v2.0 set): Delay, Distortion (with the v2.0 Shape parameter), Compressor, PCF (12 dB LP/BP filter, 54 fixed envelope patterns — replicate the pattern table), per-303 Shuffle.
Transport bar, tempo control, and WAV/AIFF export.
Mods: replaceable skins + drum sample sets (the v2.0 "Mod" concept), since mods were a core part of ReBirth culture.
3.2 Software architecture (AROS conventions)
plain
ReBirthAROS/
├── app/                  # main program, Intuition window, MUI/Zune GUI
├── engine/               # realtime DSP — no OS GUI calls inside
│   ├── dsp/              # float32 render cores (303 voice, 808 synth, 909 sampler)
│   ├── mixer/            # per-section strips, sends, master bus
│   ├── fx/               # delay, distortion, compressor, PCF
│   └── seq/              # pattern/song sequencer, automation recorder
├── audio_io/             # audio.library backend + AHI fallback + WAV export
├── midi_io/              # CAMD backend (note in, CC→knob assignment, sync)
├── gui/                  # Zune/MUI custom widgets: knob, fader, VU meter, step LEDs
│   └── skins/            # skin engine (original ReBirth-inspired artwork)
├── project/              # song file format: IFF/RIFF container, AROS datatypes
└── docs/                 # AmigaGuide manual + autodocs
AROS conventions to follow:
C (AROS SDK), Amiga API style (struct Library bases, IExec, ObtainSemaphore), no C++ exceptions crossing library boundaries.
GUI in Zune (MUI) with BOOPSI custom gadgets for knobs/faders/step buttons — the knob must be draggable vertically, snapping, shift=fine, right-click reset: exact ReBirth interaction feel.
Song files as an IFF FORM (Amiga-native) and optionally RIFF for cross-platform mod compatibility.
Localization via catalogs; docs as AmigaGuide; installer script; icons in Amiga style.
IPC discipline: GUI runs on the main task; render threads on the SMP thread pool; message passing via exec ports/AllocVec, never shared unsynchronized state. Signal-based audio-thread ↔ GUI-thread metering.
Datatype for previewing/exporting songs; commodity/ARexx port for remote control (ADDRESS REBIRTHAROS).
3.3 The "look and feel" plan
Interaction spec first: document every control's behavior from original RB-338 v2.0.1 (this is legal to reproduce — behavior isn't copyrighted, expression is). Knob travel curves, LED glow behavior (dim/bright accent, green flam LEDs on the 909), the "click the 909 sound abbreviation" v2.0 change, double-click accent entry, drag fader feel, step-button programming UX, pattern/song switch behavior.
Original artwork produced to evoke the v2.0 layout: same panel geometry/ergonomics, new pixel art. One default skin + a "classic" homage skin.
Skin engine loads skins the way ReBirth mods worked (named image set + sample set), so the community can extend it — this reproduces the modding culture legally.
3.4 Sound engine plan
303: port/generate a known-good open 303 model (e.g. Open303-style): naive saw/square + one-pole filter is not enough — the ladder filter's nonlinear resonance and the accent envelope are the soul of the sound. A/B against reference recordings.
808: synthesize each voice (808 kick = pitched sine w/ exponential pitch sweep + noise click; snare = 2 band-passed oscillators + noise; hats = 6 square oscillators through HP filter, etc.) — this is well-documented open knowledge (e.g. the "sound of the 808" analyses).
909: multi-sample layers with crossfades under the tune knobs, velocity/accent layers — record or synthesize new clean 909-style samples (do not rip ReBirth's AIFFs).
FX: standard float implementations, tuned to v2.0 parameter ranges (delay tempo-sync, distortion shape curve, compressor with the RB gain structure, PCF with the exact 54 patterns).
3.5 W2 milestones
Table
Milestone	Content
M2.1	Interaction/behavior spec + sound design spec (303 filter params, sample list, PCF patterns)
M2.2	Audio core: render threads via W1 engine; 303#1 sounds right
M2.3	808 + 909 engines, mixer, one GUI panel prototype with real knobs
M2.4	Full GUI: all four sections, mixer, FX panels, transport; pattern programming works
M2.5	Song mode, automation, WAV/AIFF export, skin engine, mod system
M2.6	MIDI in (CAMD), polish, AmigaGuide docs, beta
4. Workstream 3 — Modern PC Expansion Layer
Only after W2 parity is solid, add a "Power Mode" that the original 1998 software could never do — keeping classic mode bit-exact for purists:
Voice scaling: 303s become per-note polyphonic layers; unlimited pattern memory (classic mode stays 32×16).
Per-instrument outputs → multichannel AHI routing (separate 303/808/909 buses or per-drum-sound channels — the Rewire-style routing the original only achieved via Cubase).
Extended precision: 96 kHz / float render path, oversampled distortion/filter (anti-aliased knobs).
SMP/SIMD: per-section parallel rendering (4 sections on 4 cores), AVX2 mixdown.
New FX for power mode: syncable LFO on every knob (the one thing everyone missed in ReBirth), a second PCF, sidechain-style modulation.
Modern I/O: USB-MIDI hot-plug, MIDI clock + MMC sync, CV/gate via serial if adventurous, network jam session later.
Optional extras: pattern randomize/alter v2.0 already had — extend with generative fills; sample-swap per drum voice (v2.0 Mods already did this — make it a first-class feature).
5. Risks & Mitigations
Table
Risk	Mitigation
Legal/trademark exposure	Functional recreation + original artwork; legal review before any public release; never distribute Roland/Propellerhead assets
AHI legacy limitations block low latency	The audio.library layer + benchmark suite in M1.1 reveals this before W2 depends on it; fallback = direct HDA driver access documented in spec
303 filter "soul" hard to nail	Early A/B milestone (M2.2) with reference recordings; budget iteration time
GUI feel mismatch	Interaction spec in M2.1 is a written contract; knob widget prototyped against it early
SMP bugs (deadlocks, clicks)	Strict "no Forbid/Disable" rule; audio thread never allocates; CI with underrun detector
Scope creep (power mode bleeding into classic mode)	Hard split: classic/ and power/ code paths behind one mode switch
6. Suggested Timeline & Team
Assuming 2–3 developers plus a graphics artist:
Table
Phase	Duration	Output
Phase A (W1 M1.1–M1.2)	3 months	Audio foundation spec + float mixing core
Phase B (W1 M1.3–M1.4 ∥ W2 M2.1)	3 months	Audio layer complete; ReBirth spec done
Phase C (W2 M2.2–M2.4)	4–5 months	Playable recreation, GUI complete
Phase D (W2 M2.5–M2.6)	3 months	Feature-complete beta
Phase E (W3)	ongoing	Power Mode releases
