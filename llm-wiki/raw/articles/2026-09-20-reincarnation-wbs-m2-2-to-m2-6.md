# ReIncarnation — Work Breakdown Structure (M2.2 → M2.6)

**Ingested:** 2026-09-20 into ReIncarnation `llm-wiki`
**Source:** user-provided WBS text, stored verbatim below (only this header added).
**Status:** design input — execution breakdown for M2.2–M2.6, companion to the Comprehensive Plan and Deep Dives 1–3.

---

ReIncarnation — Work Breakdown Structure (M2.2 → M2.6)0. Architecture Pre-Decision: The "Device Framework"
Your requirement — "future modules must resemble their originals, visually and audibly" — turns the skin engine from a reskinning feature into a device plug-in framework. Everything below is structured around one core abstraction:
plain
┌──────────────────────────── DEVICE DESCRIPTOR ─────────────────────────────┐
│ struct RIDevice {                                                          │
│   struct RIDeviceClass *class;        /* device type ID, version */        │
│   struct RIDSP   *dsp;                /* render callbacks (section bus) */  │
│   struct RIPanel *panel;              /* visual descriptor                */│
│   struct RIMap   *control_map;        /* control IDs ↔ DSP params         */│
│   struct RIModHooks *mod_hooks;       /* sample/parameter override points */│
│ }                                                                          │
└─────────────────────────────────────────────────────────────────────────────┘
RIDSP: opaque engine. The mixer, sequencer, automation, and MIDI layers talk to every device through the same four entry points (render, note event, param set, meter tap). ReBirth's four sections are just the first four registered devices.
RIPanel: geometry + widget list + artwork binding. A "TB-303" panel and a future "JX-3P" panel are both instances; the GUI framework never hardcodes a device.
Control IDs: one stable ABI (from the skin-format dive) shared by GUI, automation, MIDI-learn, and ARexx.
This decision is locked in task 2.1 and gates everything else.
Visual fidelity policy: all artwork is original but measured against original reference panels (geometry from high-res reference photos; control positions within ±2 px at 1024×768 reference scale; typography matched in style, not copied glyph-for-glyph). A/B acceptance = side-by-side screenshots at 50% zoom, silhouette/overlap test. Behavior fidelity = the interaction spec from M2.1 is the contract.
1. WBS Overview & Dependency Graph
plain
2.1 Device framework + scheduler core          ████▶ gates: all DSP + GUI
2.2 DSP: 303A/303B                              ▶ needs 2.1
2.3 DSP: 808                                    ▶ needs 2.1
2.4 DSP: 909                                    ▶ needs 2.2 (shared sampler utils)
2.5 FX engine (delay/dist/comp/PCF)             ▶ needs 2.1 (bus insert points)
2.6 Mixer + metering                            ▶ needs 2.1, 2.5
2.7 audio.library backend glue                  ▶ needs W1 (parallel)
2.8 MIDI (CAMD) I/O                             ▶ needs 2.1
2.9 GUI framework: panels, knobs, faders, steps ▶ needs 2.1 (panel descriptors)
2.10 Sequencer GUI + transport                  ▶ needs 2.9
2.11 FX GUI                                     ▶ needs 2.9
2.12 Skin/mod engine                            ▶ needs 2.9 + 2.4 + 2.5
2.13 Song format load/save + WAV/AIFF export    ▶ needs all DSP + 2.10
2.14 Pattern/song editing completeness          ▶ needs 2.10
2.15 Automation record/playback + MIDI learn    ▶ needs 2.8 + 2.13
2.16 Polish, perf, docs, beta                   ▶ needs everything
Milestone mapping: M2.2 = 2.1+2.2+2.7 (303 sounds right, end-to-end) · M2.3 = 2.3, 2.4, 2.9 (808+909+first panel) · M2.4 = 2.5–2.6, 2.9–2.11 (full classic GUI) · M2.5 = 2.12–2.14 (formats, skins, export) · M2.6 = 2.15–2.16 (beta).
2. Module Specifications
Each module: Interface (the contract) + Test criteria (acceptance gates; all must pass to merge).
2.1 Device Framework & Scheduler Core — SEQ/FRAME
Interface:
c
/* framework/ridevice.h */
struct RIDevice *RiDeviceRegister(const struct RIDeviceClass *class,
                                  const struct RIDSPOps *dsp,
                                  const struct RIPanelDesc *panel,
                                  struct Library *AudioBase);
void  RiDeviceEvent(struct RIDevice *dev, const struct RIEvent *ev); /* note/accent/slide/flam */
void  RiDeviceSetParam(struct RIDevice *dev, uint32_t ctl_id, uint8_t value); /* 0..127 */
void  RiDeviceRender(struct RIDevice *dev, float **bus, uint32_t frames, float sr);


/* framework/riseq.h — master clock + event scheduler (spec §2 of Part 3) */
struct RISeq *RiSeqCreate(struct AudioObject *ao, uint32_t ppq /*=96*/);
void  RiSeqLoadSnapshot(struct RISeq *s, const struct RISeqSnapshot *snap); /* lock-free, GUI side */
int64_t RiSeqMasterClock(const struct RISeq *s);   /* samples, render side */
Implementations: snapshot builder (song → immutable event lists), event walker, shuffle/legato/flam resolution, PCF clock feed, loop/song-mode cursor. Hard rules: no alloc/lock in render path; deterministic.
Test criteria:
TC-2.1.1: 10-minute playback at 44.1 kHz accumulates ≤ 0 sample clock error against ideal (samples·PPQ·BPM)/(60·SR) (float64 check each buffer).
TC-2.1.2: song-mode loop of 8 pattern steps loops sample-exactly (rendered PCM of loop iteration 2 equals iteration 1 bit-for-bit).
TC-2.1.3: snapshot swap mid-bar applies at next buffer boundary, no underrun, no event loss (injected 10k swaps soak test, zero clicks > −80 dBFS).
TC-2.1.4: full event storm (all 4 sections, all 16 steps + flams + automation in one 64-frame buffer) renders within 0.5× buffer budget on a 2-core AROS box.
TC-2.1.5: second dummy device class registers/renders/unregisters with no code changes outside its own module (proves framework generality).
2.2 DSP: TB-303 ×2 — DSP-303
Interface: as in Part 1 (rb303_render), plus rb303_set_param(v, CTL_303A_*, value) mapping knob 0..127 → float ranges via per-param curves (tables in dsp/params.c).
Test criteria:
TC-2.2.1: Filter response: LP magnitude response at resonance knob = min matches modeled 18 dB ladder within ±1 dB at 20 test frequencies (C0–C6 sweep), 48 kHz.
TC-2.2.2: Accent envelope: accent peak amplitude and τ_acc within ±10% of spec (60 ms) via step-response measurement.
TC-2.2.3: Slide legato: two-note slide retrigger test — envelopes do not reset (assert env state continuity), pitch reaches target within 5·τ_slide.
TC-2.2.4: A/B listening panel (3 acid-experienced testers, blind): render vs. reference recording — 2/3 identify which is which at ≤ chance-confusion rate; documented sign-off per voice.
TC-2.2.5: Waveform-switch click present in classic mode, absent when classic-click flag off.
TC-2.2.6: Two 303 instances render independently (shared code, separate state), cross-talk < −90 dB.
2.3 DSP: TR-808 — DSP-808
Interface: rb808_render(voice_set, events, bus, frames, sr) with per-voice structs from Part 3 §1; param curves in dsp/params.c (808 section).
Test criteria:
TC-2.3.1: Per-voice unit tests: BD pitch trajectory (f_start, f_end, τ) within ±5%; hat 6-osc spectrum matches ratios (0.83, 1.48, 2.26, 2.92, 3.94, 5.31 ±0.5%) via FFT of isolated hit.
TC-2.3.2: BD decay range covers 0.18–2.8 s (±10%) across Decay knob travel; clamp at 35 Hz verified.
TC-2.3.3: Clap shows 4-burst structure (3 × 9 ms + tail) in envelope analysis.
TC-2.3.4: Accent raises level ×1.5 (±0.5 dB) on all accent-capable voices.
TC-2.3.5: 14-voice polyphony storm (every voice max decay, full accent) renders within 0.3× buffer budget.
2.4 DSP: TR-909 — DSP-909
Interface: rb909_render(voice, events, bus, frames, sr) per Part 2 §2; shared sampler/ utils (linear-interp resampler, layer crossfader) — this is the reusable piece future sample-based devices need.
Test criteria:
TC-2.4.1: Layer crossfade: tune-knob sweep across full range yields continuous RMS (no >1 dB steps at layer boundaries).
TC-2.4.2: Accent levels: acc1 amp ×1.15 ±0.2 dB; flam = second hit at +35 ms ±5 ms, ×0.75 amp.
TC-2.4.3: Cymbal/ride accent quirk: accent input produces no level change (parity with v2.0) — assert in test.
TC-2.4.4: Retrigger cuts previous hit sample-exactly (no voice overlap).
TC-2.4.5: Mod swap guard: changing S909 sample set while any voice active is refused (returns error, no click); when idle, swap is click-free.
2.5 FX Engine — FX
Interface:
c
struct RIFX *RiFXCreate(struct Library *AudioBase, uint32_t fx_type, struct TagItem *);
/* types: RI_FX_DELAY, RI_FX_DISTORTION, RI_FX_COMPRESSOR, RI_FX_PCF */
void RiFXSetParam(struct RIFX *, uint32_t id, uint8_t value);
void RiFXRender(struct RIFX *, float *in, float *out, uint32_t frames, float sr, float bpm);
Delay: tempo-sync via BPM feed (spec § earlier). Distortion: shape curve from v2.0 (asymmetric tanh-family, 0..127 → drive+shape). Compressor: fixed ratio ~4:1, threshold from knob, make-up auto. PCF: SVF + 54-pattern table + determinism (Part 2 §1).
Test criteria:
TC-2.5.1: PCF table: all 54 patterns present, byte-diff against spec table = 0; PRNG patterns deterministic across engine restarts (fixed seed test).
TC-2.5.2: PCF cutoff law: fc = base·2^((v−64)/64·amt) verified at 10 table values ±2 cents.
TC-2.5.3: Delay tempo sync at 120/140/174 BPM: echo interval error < 0.1%; no drift over 5 min (master-clock check).
TC-2.5.4: Distortion: unity gain at drive=0 ±0.2 dB; monotonic loudness growth; no NaN/Inf across full input range × parameter grid (fuzz sweep test).
TC-2.5.5: FX order swap in mixer config applies without zipper (param-snapshot mechanism, 1 buffer max).
2.6 Mixer & Metering — MIX
Interface: RiMixer — 4 section buses + master; AuConnect-compatible source registration; meter tap (AuTapMeters-fed SPSC ring); mute/solo logic.
Test criteria:
TC-2.6.1: Gain staging: fader law matches v2.0 feel curve (map table in spec), ±0.5 dB at 9 anchor points.
TC-2.6.2: Solo/mute matrix: all 2⁴ combinations correct; no zipper when toggled live.
TC-2.6.3: Meters: peak-hold decay 20 dB/s; ballistics feel validated in GUI milestone (subjective sign-off).
2.7 audio.library Backend — IO-AUDIO (parallel with W1)
Interface: per Part 1 §2 API; ReIncarnation instantiates: 4 sources + 4 buses + FX inserts + master + stereo out, 64-frame buffers, render threads = min(4, cores).
Test criteria:
TC-2.7.1: End-to-end latency AuQueryAttr(AUQA_LatencyFrames) ≤ 15 ms @ 64 frames/48 kHz on reference AROS x86-64 box.
TC-2.7.2: 30-min soak, 4 sections + 4 FX active: zero underruns on 2-core, < 0.01% on 1-core.
TC-2.7.3: Param spam: 1000 knob changes/s via AuSetBusAttr — no audio artifacts, no lock contention (render stays within budget).
TC-2.7.4: AHI fallback path (if audio.library missing): app boots with reduced features, documented message — graceful degradation test.
2.8 MIDI I/O — IO-MIDI
Interface: CAMD backend; RiMidi maps note→303 note entry, CC→ctl_id (learn), MIDI clock in/out + song position, MMC transport. Config: DEVS:ReIncarnation/MIDI/ IFF maps.
Test criteria:
TC-2.8.1: MIDI note recorded into pattern lands on correct step after quantization (round-trip test).
TC-2.8.2: Clock sync: 100 bars slave-to-master drift < 1 tick.
TC-2.8.3: CC learn: assign, save map, reload, persist across reboot.
TC-2.8.4: Hot-unplug USB MIDI mid-playback: no crash, no hang (error path test).
2.9 GUI Framework — GUI-CORE (the visual-fidelity heart)
Interface:
c
struct RIPanelDesc {           /* device panel, skin-independent */
    uint32_t width, height, zoom_levels[3];   /* 1x/1.5x/2x */
    struct RIWidget widgets[];                 /* knob/fader/step/led/display */
    CONST_STRPTR art_binds[];                  /* logical name → skin image */
};
/* widgets implemented as Zune custom classes: RKnb (vertical drag, shift=fine,
   right-click=default, dblclick=type-in), RStp (step button + LED), RFdr, RVUm,
   RDsp (7-seg/pattern display) */
Drag physics (interaction spec contract): vertical drag, sensitivity curve, fine mode ×0.1, continuous while dragging + single commit event on release (undo unit = one drag).
Test criteria:
TC-2.9.1: Silhouette test: default skin overlaid on reference panel screenshot at same scale — every control center within ±2 px; panel proportions within ±1%.
TC-2.9.2: Knob: drag 100 px maps to expected value travel ±5%; fine mode ×0.1 ±10%; right-click resets to default; all with mouse capture outside window.
TC-2.9.3: Step button: click toggles, LED update within 1 frame (33 ms) of pattern position; 16th-note LED chase at 174 BPM is stutter-free.
TC-2.9.4: Zoom 1×/1.5×/2× renders crisply (artwork authored at 2×, filtered down).
TC-2.9.5: Second dummy device panel (future-device simulation) renders and binds with zero GUI-framework changes.
2.10 Sequencer GUI + Transport — GUI-SEQ
Pattern/song switch, tap programming via mouse & keyboard (keys 1..8/9..0 row = step entry, like the original's keyboard entry), pattern copy/paste/clear, transport bar, tempo, shuffle per 303, PCF pattern picker grid (54 thumbnails).
Test criteria:
TC-2.10.1: Entire "ReBirth 101" tutorial song (canonical 8-bar acid loop from the spec pack) programmed via keyboard+mouse only, in ≤ 10 minutes, by a tester who used ReBirth — they rate workflow "equivalent" on a 5-point scale ≥ 4.
TC-2.10.2: Pattern/song mode switch mid-playback behaves per spec (switch at next 16th, no cutoff).
TC-2.10.3: Copy/paste pattern preserves accent/slide flags (round-trip through song file).
2.11 FX GUI — GUI-FX
Four FX windows (delay/dist/comp/PCF) with panel descriptors as devices-in-miniature (they're moddable too). PCF picker with 54 mini-waveforms.
Test criteria:
TC-2.11.1: FX window knob edits audible within 1 buffer (≤2 ms @ 48 kHz/64).
TC-2.11.2: PCF picker: selecting pattern #N applies table pattern #N (verified via offline render diff against direct table render).
2.12 Skin/Mod Engine — FMT-MOD
FORM RBNM per Part 1 §3; loader validates VERS, CPRG, CRC; skin-hot-swap on idle; SYS:Classes/ReIncarnation/Mods/ + file requester + drag-drop.
Test criteria:
TC-2.12.1: Round-trip: every shipped mod loads, saves unchanged, re-serializes byte-identical.
TC-2.12.2: Corrupt mod (truncated chunk, bad CRC) → clean error requester, no crash (fuzz suite: 500 mutated files).
TC-2.12.3: Mod missing control art → fallback to default skin art for those controls, rest applies (partial-mod test).
TC-2.12.4: CPRG attribution screen displays on every load (assertion in code review + test hook).
TC-2.12.5: Loading a mod with a 909 sample set changes sound per S909 chunk (render diff against default).
2.13 Song Format + Export — FMT-SONG
FORM RBNG per Part 2 §3; load/save; AuRenderToFile WAV/AIFF export; datatype + Wanderer preview; ARexx OPENSONG/PLAY/EXPORTWAV.
Test criteria:
TC-2.13.1: Determinism: 10-song corpus rendered twice → bit-identical PCM (md5 match).
TC-2.13.2: Serialize→parse→serialize byte-identical for all 10 corpus songs.
TC-2.13.3: Forward-compat: song with unknown future chunk loads, preserves unknown chunk verbatim on re-save.
TC-2.13.4: Export WAV at 44.1/48 kHz, 16/24-bit: header valid (validated by external tools: sox, Audition import), no clipping > 0 dBFS unless song genuinely peaks there.
TC-2.13.5: Mod reference: song saved with mod X warns and offers locate on load when mod missing; loads with default when declined.
2.14 Pattern/Song Editing Completeness — SEQ-EDIT
Undo/redo (per-interaction units), pattern protect, the v2.0 quirks: double-click accent entry on 303s, drag across steps for flam painting on 909, shuffle per 303.
Test criteria:
TC-2.14.1: Undo chain: 200 operations deep, correct state at every step (scripted regression).
TC-2.14.2: v2.0 quirks verified per interaction spec (scripted UI test where possible, checklist sign-off otherwise).
2.15 Automation + MIDI Learn — AUTO
Recorder per Part 3 §2.5 (GUI-thread, 30 Hz poll, ppq/24 quantize); playback; MIDI learn UI; ARexx param access.
Test criteria:
TC-2.15.1: Recorded cutoff sweep plays back within ±1 ctl unit of recorded values after save/load.
TC-2.15.2: Automation + manual knob conflict: live override wins, release returns to automation within 1 buffer.
TC-2.15.3: MIDI-learned CC replays identical automation stream (loopback test).
2.16 Polish, Performance, Docs, Beta — REL
AmigaGuide manual, autodocs, catalogs (EN + one more locale to prove pipeline), installer, icon set; perf targets: 174 BPM, all sections + 4 FX ≤ 50% of one core (render budget), GUI ≤ 10% CPU; beta program with 20 users, bug triage.
Test criteria:
TC-2.16.1: Performance budget met on reference hardware (2-core AROS x86-64, 48 kHz, 64-frame buffers).
TC-2.16.2: Docs complete per checklist (every control documented, every ARexx command exemplified).
TC-2.16.3: Beta exit: zero crashers, zero data-loss bugs, A/B panel rates ReIncarnation ≥ 4/5 on "feels like ReBirth."
3. Consolidated Milestone Acceptance Gates
Table
Milestone	Modules	Gate = sum of TCs
M2.2	2.1, 2.2, 2.7	TC-2.1., TC-2.2., TC-2.7.* + blind A/B sign-off on 303
M2.3	2.3, 2.4, 2.9 (partial: one panel)	+ TC-2.3., TC-2.4., TC-2.9.1–2.9.3
M2.4	2.5, 2.6, 2.9 (full), 2.10, 2.11	+ TC-2.5.–TC-2.11.; full classic GUI playable
M2.5	2.12, 2.13, 2.14	+ TC-2.12.–TC-2.14.; WAV export shipped; mod SDK documented
M2.6	2.8, 2.15, 2.16	+ TC-2.8., TC-2.15., TC-2.16.*; beta exit criteria
Every TC is automated where possible (audio analysis, file round-trips, scripted UI) and checklist-signed where subjective (A/B panels, feel). CI runs all automated TCs on every commit; the spec's determinism guarantees make most audio tests md5-diff-based, so regressions are loud.
4. What This Leaves Open
GUI interaction spec (knob physics numbers, LED timing curves, drag thresholds) — referenced throughout as the M2.1 contract but not yet written; it's the last document to author before M2.3 starts.
808/909 artwork + sample production — asset tasks, tracked in M2.3/M2.4 but requiring the artist pipeline definition.
W3 power-mode — deliberately excluded from every gate above; all power-mode TCs will be a separate appendix so classic parity can never be held hostage to expansion features.
