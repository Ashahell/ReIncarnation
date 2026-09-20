# ReIncarnation — Deep Technical Dives

**Ingested:** 2026-09-20 into ReIncarnation `llm-wiki`
**Source:** user-provided deep-dive text, stored verbatim below (only this header added).
**Status:** design input — companion to the Comprehensive Plan.

---

ReIncarnation — Deep Technical Dives1. The TB-303 DSP Model
1.1 Why this matters most
Everything in ReBirth lives or dies on two sounds: the 303's filter sweep and the 808's kick. A "correct" 303 note is ~5% waveform, ~95% filter behavior and timing. The architecture below follows the Open303 lineage of models (which are public, well-analyzed, and A/B-tested against hardware) rather than naive synth tutorials.
1.2 Voice block diagram
plain
        ├─ SAW path ─┐
 GATE ──┤            ├─ VCO mux ──► [VCA 1: accent/env amp] ──► [18dB LP filter]
        └─ SQR path ─┘                        ▲                        ▲
                                              │                        │
                                     [Slide/portamento]      [Cutoff + Resonance]
                                                              driven by:
                                                          ┌── ENV (decay, env-mod amount)
                                                          └── ACCENT envelope (fast attack,
                                                              longer decay, adds resonance boost)
1.3 The oscillator (the easy 5%)
plain
VCO: naive saw + pulse wave (fixed 50% duty square), amplitude ±1.0f
Both waves are trivially correct and alias-free only up to ~2 kHz at 44.1 kHz without oversampling — because the 303's notes are low (E0–C5) this is borderline fine, but for the power-mode (W3) 96 kHz path, oversample the VCO 2× and decimate. The VCO in the original drifts slightly and glitches on waveform switch mid-note; ReBirth actually modeled the waveform-switch click, and it's audible in acid basslines. Implement a 0.5 ms waveform crossfade plus a tiny deterministic click on toggle — flag it in classic mode.
1.4 The VCA — accent and envelope (the first 45%)
ReBirth v2.0.1 gives you three envelope/amp behaviors via knobs: Env Mod (how much the filter envelope opens the filter), Decay (envelope decay time), and Accent (how hard accented steps hit). The v2.0 303 also has per-303 Shuffle which lives in the sequencer, not here.
Classic (non-slide, non-accent) note:
plain
amp_env(t) = exp(-t / τ_amp)          τ_amp ≈ 350 ms baseline
VCA gain  = clamp(amp_env, 0, 1)      (303 VCA is linear, not exponential-loudness)
Accented note — this is the acid "ouch":
plain
accent_env(t) = 1 + A · exp(-t / τ_acc)    τ_acc ≈ 60 ms, A = accent amount (0..1 from knob)
VCA gain     = clamp(amp_env · accent_env, 0, 1.2)   // slight overdrive past 1.0
Slide (portamento, v1 = tied note, v2 = explicit slide flag): the oscillator's target frequency changes instantly but the current frequency slews:
plain
f_cur += (f_target - f_cur) · (1 - exp(-1 / (SR · τ_slide)))   τ_slide ≈ 30–50 ms
Critical detail: during a slide, the VCA and filter envelopes do NOT retrigger — the note gate stays high, only pitch and (in hardware) a small accent-like filter nudge moves. ReBirth emulated this faithfully and acid players depend on it.
1.5 The 18 dB filter — the soul (the other 50%)
A three-stage (18 dB/oct) transistor-ladder lowpass. The linear version:
plain
// y = filter output, x = input, per sample:
stage[0] = stage[0] + g·(tanh(x − k·stage[2]) − tanh(stage[0]))
stage[1] = stage[1] + g·(tanh(stage[0]) − tanh(stage[1]))
stage[2] = stage[2] + g·(tanh(stage[1]) − tanh(stage[2]))
y = stage[2]
Where:
g = 1 − exp(−2π·fc/SR) — the per-stage cutoff coefficient
k = resonance feedback (0..~3.8; hardware self-oscillates near 3.5+, ReBirth's range topped just below self-oscillation in classic mode, slightly into it in power mode)
tanh() nonlinearity is mandatory — it's where the squelch, the "bark" on accented high-resonance notes, and the pleasant saturation come from. A linear ladder filter sounds like a soft synth; a tanh ladder filter sounds like a 303.
Cutoff modulation per step:
plain
fc = fc_base · 2^( (CV_env + CV_accent) / 1200 · env_mod_amount )      // cents
CV_env(t)    = step_amplitude · exp(−t / τ_decay)   // τ_decay from Decay knob, ~80 ms..4 s
CV_accent(t) = accent_flag · A · exp(−t / τ_acc)
Plus the accent resonance boost: accented steps push k up by ~15% and open the VCA as above. The interaction of VCA-overdrive + resonance boost + fast accent decay is the acid sound.
Tuning the constants is the M2.2 milestone's real work: start from Open303's published values (they're derived from hardware measurement), then A/B against reference recordings — the main candidates for taste-adjustment are τ_acc, the tanh drive amount, and how far the cutoff opens on accent.
1.6 Render-thread contract
plain
// engine/dsp/rb303.h — the ONLY surface the sequencer/mixer sees
struct RB303Voice {
    // all state, cache-line aligned, owned by render thread exclusively
    float stage[3];          // filter states
    float phase, f_cur, f_target;
    float amp_env, acc_env, filter_env_cv;
    uint32_t gate : 1, slide : 1, accent : 1, waveform : 1;
    float cutoff, resonance, env_mod, decay, accent_amt;
};


// Called with a step event list; renders into out[], n samples.
// Must never allocate, never lock, never call DOS/exec.
void rb303_render(struct RB303Voice *v, const struct RBStepEvent *ev,
                  float *out, uint32_t n, float sample_rate);
2. audio.library — API Sketch
Design goal: an Amiga-native, AROS-convention layer over ahi.device that gives float32 graphs, SMP rendering, and low latency — without breaking a single existing AHI app.
2.1 Object model (Amiga-style, tag-based)
plain
audioObject → AuCreateObject()          // a render graph: sources → buses → outputs
AuSource    → AuLoadSource()            // buffer, streaming file, or app callback (ReIncarnation)
AuBus       → AuCreateBus()             // named strip w/ gain/pan/sends (mixer channel)
AuEffect    → AuInsertEffect()          // DSP node in a bus
AuOutput    → AuOpenOutput()            // binds graph tail to AHI modes / channels
Everything is struct Library dispatch with tags — no C++ objects cross the API:
c
/* audio/audio.h — public include */
struct AudioObject *AuCreateObject(struct Library *AudioBase, struct TagItem *tags);
/* tags: AUO_SampleRate, F (44100.0)
         AUO_BufferFrames,  (64..4096, latency control)
         AUO_RenderThreads, (1..N, SMP pool)
         AUO_Precision,     (AU_PRECISION_FLOAT32 | INT24 | INT16)
         AUO_Outputs,       (AU_OUT_STEREO | AU_OUT_MULTICHANNEL(n))  */


/* ReIncarnation registers 4 render callbacks — one per section */
uint32 AuAddSource(struct AudioObject *ao, struct TagItem *tags);
/* tags: AUS_Callback, (void (*)(void *userdata, float **ch, uint32_t frames))
         AUS_UserData,  AUS_Channels, (1|2|...)  */


uint32 AuAddBus(struct AudioObject *ao, const STRPTR name, struct TagItem *tags);
/* tags: AUB_GainF, AUB_PanF, AUB_Mute, AUB_Sends (array of {bus, level}) */


BOOL AuConnect(struct AudioObject *ao, uint32 src, uint32 bus);
BOOL AuRoute(struct AudioObject *ao, uint32 bus, uint32 out);
2.2 Lifecycle (exec-task discipline)
c
AuStart(ao);   // spawns render threads from the library's SMP pool
AuLock(ao);    // pauses graph mutation — GUI thread must hold this to tweak knobs
AuSetBusAttr(ao, bus, AUB_GainF, 0.8);
AuUnlock(ao);
AuStop(ao);    // drains, joins threads, never drops a callback in-flight
Rules enforced by the library (documented in autodocs, asserted in debug builds):
Callbacks run on render threads: no AllocVec, no DOS, no locks, no Forbid/Disable.
All parameter changes go through AuSetBusAttr/AuSetSourceAttr — the library applies them with a lock-free snapshot at buffer boundaries, so GUI knob drags never glitch audio.
Buffer sizes chosen by the app; AuQueryAttr(ao, AUQA_LatencyFrames) reports real end-to-end latency including the AHI driver's chunk.
2.3 Render/export
c
/* Offline render — ReIncarnation's WAV/AIFF export and the "bounce pattern" feature */
BOOL AuRenderToFile(struct AudioObject *ao, const STRPTR path,
                    uint32 seconds_x1000, uint32 *fmtTags);  // AIFF/WAV datatype hook


/* Live meter tap for GUI VU meters — lock-free SPSC ring */
uint32 AuTapMeters(struct AudioObject *ao, struct AUMeterBlock *blk);
2.4 The mixer helper (low-level convenience for other AROS apps)
c
/* One-call sample playback for simple apps — the "easy mode" that keeps
   ahi.device usage sane for games/utils */
uint32 AuPlaySample(struct Library *AudioBase, CONST APTR data, uint32 frames,
                    struct TagItem *tags);   // AUFS_VolumeF, AUFS_PanF, AUFS_Loop, AUFS_RateF
2.5 Internals sketch
plain
 audio.library internals (per AudioObject):
 ┌─────────────────────────────────────────────┐
 │ graph: sources → buses(→effects) → outputs  │
 │ render pool: N exec tasks (SMP)             │
 │ per-thread: render callback → float bus mix │
 │ mixer tail: SIMD (SSE2/AVX2) → AHI chunk    │
 │ lock-free: param snapshot ring (GUI→render) │
 │ meter SPSC ring (render→GUI)                │
 └─────────────────────────────────────────────┘
              ↓ integer conversion (float→s24/s16)
         ahi.device (existing, untouched)
ReIncarnation's instantiation: 4 sources (303A, 303B, 808, 909) → 4 buses with sends → master bus (hosting delay/dist/comp/PCF as AuEffect nodes — which also means the W3 power-mode "separate outputs" feature is just extra AuRoute calls) → stereo output, 64-frame buffers.
3. Skin / Mod File Format ("ReIncarnation Mods")
Goals: reproduce the v2.0 "Mod" culture (skins + sample sets) on AROS-native IFF, be open and documented, let users share mods without legal exposure (mods contain our format, not ripped assets).
3.1 Container: FORM RBNM (Reincarnation Mod)
IFF, big-endian, chunk-based — Amiga-native, self-describing, extendable:
plain
FORM RBNM                        /* "Reincarnation Mod" */
├── VERS  { UWORD version=2, UWORD revision=0, UWORD flags }
├── NAME  { STRPTR mod display name }
├── AUTH  { STRPTR author, URL/email }
├── SKIN
│   ├── PDEF  { panel geometry: UWORD width,height; panel count; layout rects }
│   ├── IMGS  { FORM ILBM images: background, knob strip (rotated frames),
│   │           fader strip, step-button on/off, LED sprites, VU meter frames }
│   ├── MAPS  { control map: per-control chunk of
│   │           { UBYTE id, UWORD panel, rect, knob type, strip frame count,
│   │             default value, fine-shift factor } }
│   └── THME  { color LUTs: accent LED colors, display phosphor color, dim/bright }
├── SNDS                          /* sound set = a "ReBirth mod" sample bank */
│   ├── S909  { for each 909 voice: sample layers }
│   │     per voice chunk: ID, UWORD layers, per layer:
│   │     { ULONG rate, UWORD bits(16/24), ULONG frames, ULONG offset, UBYTE keyrange }
│   ├── S808  { 808 parameters override chunk (synth, but tune/decay tables live here) }
│   └── S303  { waveform set flag, optional alternate saw/square oversampled tables }
├── SONG  { optional demo song (reuse ReIncarnation's native song format) }
└── CPRG  { STRPTR license/attribution text — enforced displayed on mod load }
Key design decisions:
909 samples as layers, not single hits. A ReBirth-style tune knob crossfades 2–3 velocity/character layers per voice (the 909's tune knob isn't pitch — it morphs the sample). The layer chunk layout above maps directly onto the engine's SampleVoice struct.
808 stays synthesized. The mod provides only tables (tune curves, decay ranges, level trims per voice), because that's where the 808's character knobs actually live.
Knobs/faders are strip animations, exactly like the original: one image per angle step. MAPS declares how many frames per control so the skin engine doesn't hardcode anything. Minimum viable: 32 frames; classic ReBirth look = 64.
CPRG is mandatory — when a user loads a mod, ReIncarnation shows the author's attribution screen. This keeps mod culture healthy and keeps us clean legally.
3.2 Control IDs (stable ABI — MAPS references these)
plain
#define CTL_303A_TUNE    0x0301
#define CTL_303A_CUTOFF  0x0302
#define CTL_303A_RESO    0x0303
#define CTL_303A_ENVMOD  0x0304
#define CTL_303A_DECAY   0x0305
#define CTL_303A_ACCENT  0x0306
#define CTL_303A_WAVEFORM 0x0307   /* toggle, not a strip */
#define CTL_303A_STEPS   0x0308   /* the 16 step buttons + LEDs */
... (303B = 0x04xx, 808 = 0x08xx, 909 = 0x09xx, mixer = 0x0Axx, FX = 0x0Bxx)
Adding power-mode (W3) controls later just allocates new IDs — old mods still load, new controls fall back to the default skin's controls ("skin hole" behavior, documented).
3.3 Distribution & loading
Mods are a single file, installable to SYS:Classes/ReIncarnation/Mods/ or anywhere (file requester / drag-and-drop onto the window).
Datatype: an RBNM datatype gives thumbnails (first skin image) in Wanderer.
A built-in mod browser lists installed mods; switching a mod is safe at any time because the engine only replaces sample tables and skin, never the playing pattern state.
Versioning: VERS ≥ engine version → load with warnings; engine writes which mod a song used into the song file, so reopening a song offers to fetch the mod.
3.4 What ships on day one
"Classic" — the default, evoking v2.0's black-panel look with original artwork.
"808 State homage" — as a nod to the A. R. Kane/808 State connection in your project's roots... (worth deciding the actual default naming).
One blank mod template + full format documentation in AmigaGuide so the community can build mods on day one — that was half of ReBirth's longevity.
