/* rb808.h — 808 fifteen-voice set (Task 8, gate G8).
 * Spec §10 + Appendix C skeleton (E0 candidates, HYPOTHESIS P-07..P-12):
 *   TRIGGER: phase=0; env_pitch=1; env_amp=1 (+ voice click where listed)
 *   PER SAMPLE:
 *     f   = PITCH(f_start(tune), f_end, env_pitch)
 *     osc = OSCILLATOR(phase, f)
 *     nz  = NOISE() -> FILTER(chain)
 *     y   = MIX(osc, nz, env) -> SATURATE(point)
 *     out = y · env_amp · EXCITE(accent)      // accent pre-envelope (E0)
 * Empty blocks are N/A in the per-voice ledger rows docs/evidence/808/.
 * Accent (register entry 9, E0 service-manual model): excitation scaled
 * PRE-envelope (osc/noise amplitudes × EXCITE at the source, so saturation
 * drive follows); EXCITE(0)=1.0, EXCITE(1)=1.5. Saturation drive is kept
 * low (peak |x| <= ~0.35) so the pre-envelope model coincides with an
 * output gain within 0.4 dB. P-12 three-state (off->weak->strong, entry 24,
 * E3) is OPEN: accent field carries 0/1/2, level 2 currently maps to 1.5
 * (binary hold) until M2.1 verifies the mapping.
 * Kernels only (ri_*); no platform transcendentals; no allocation.
 * Exact C signatures below are executor-defined (spec carries interface
 * sketches only, NOT frozen ABI).
 */
#ifndef RI_RB808_H
#define RI_RB808_H
#include <stdint.h>

#define RI_808_NSOUNDS 16u /* sixteen sounds (§2.3 item 6, §12.5a). */
#define RI_808_NSLOTS 11u /* eleven slots; pairs share (§2.3 item 6). */

/* Voice ids, classic order (spec §2.3 item 6: 16 sounds, 11 slots). */

/* Voice ids, classic order (spec §2.3 LOCKED set). */
#define RB808_BD 0u
#define RB808_SD 1u
#define RB808_LT 2u
#define RB808_MT 3u
#define RB808_HT 4u
#define RB808_LC 5u
#define RB808_MC 6u
#define RB808_HC 7u
#define RB808_RS 8u
#define RB808_CL 9u
#define RB808_CP 10u
#define RB808_CH 11u
#define RB808_OH 12u
#define RB808_CY 13u
#define RB808_CB 14u
#define RB808_MA 15u /* maracas (§12.5a; HP noise, §4.2 action 1) */

/* Appendix A nominals [HYPOTHESIS] (full rows in docs/evidence/808/).
 * BD (§12.5c 808 regime): ~62 Hz start with a 4 ms sigh to the 48 Hz boom
 * (E1 Werner/Abel/Smith: ~56 Hz center; the old 170 Hz/22 ms candidate was
 * 909-like). Full WDF bridged-T topology awaits measurement (ledger BD). */
#define RI_808_BD_F_START 62.0f /* Hz at tune 0 (P-07 as re-based; tune ±7 st) */
#define RI_808_BD_F_END 48.0f
#define RI_808_BD_TAU_PITCH 0.004f /* s sigh (P-07 as re-based) */
#define RI_808_SD_F1 185.0f /* Hz (P-08; 173.3 also passes: revision tol) */
#define RI_808_SD_F2 330.0f /* Hz (P-08; 336.0 also passes: revision tol) */
#define RI_808_TOM_LT_F0 75.0f /* P-09 sweep starts */
#define RI_808_TOM_MT_F0 115.0f
#define RI_808_TOM_HT_F0 155.0f
#define RI_808_TOM_F1_RATIO 0.65f /* sweep end ~= 0.65 * start (LT 75->50 ...) */
#define RI_808_CONGA_LC_F 200.0f /* P-09 fixed */
#define RI_808_CONGA_MC_F 250.0f
#define RI_808_CONGA_HC_F 310.0f
#define RI_808_METAL_BASE 1000.0f /* Hz; 6-osc cluster base (P-10, TC-2.3.1).
                                   * Classic 808 metal set: partials =
                                   * base x {0.83,1.48,2.26,2.92,3.94,5.31}
                                   * (WBS ratios; deep-dive part 3: f, 1.48f,
                                   * 2.26f, 2.92f, 3.94f, 5.3f, f=826-830) */
#define RI_808_HP_METAL 7000.0f /* Hz one-pole HP (P-10) */
#define RI_808_CH_TAU 0.035f /* s (P-10) */
#define RI_808_CY_TAU 1.6f /* s (P-10) */
#define RI_808_CB_F1 540.0f /* Hz squares (P-10) */
#define RI_808_CB_F2 800.0f
#define RI_808_CLAP_BURST_GAP 0.009f /* s 9 ms (P-11) */
#define RI_808_CLAP_BURST_W 0.002f /* s 2 ms bursts @1.1 kHz Q2.5 (P-11) */
#define RI_808_CLAP_BP_F 1100.0f
#define RI_808_CLAP_TAIL_TAU 0.120f /* s (P-11) */
#define RI_808_EXCITE_ACC 1.5f /* EXCITE(1): ×1.5 over EXCITE(0)=1.0 */
#define RI_808_REST_LEVEL 0.00001f /* -100 dBFS: voice rest threshold (§2.3).
                                    * A voice deactivates once its envelope
                                    * (clap: tail level) falls below this with
                                    * all transients past; its output bound is
                                    * then below threshold by construction. */
#define RI_808_FLOOR_HZ 35.0f /* P-07 35 Hz floor: no voice ever below */
#define RI_808_SR_DEFAULT 48000.0f

/* 808 control IDs [WBS interface line 81; order mirrors panels.c] */
#define RI_CTL_808_LEVEL  0x0400u
#define RI_CTL_808_TUNE   0x0401u
#define RI_CTL_808_DECAY  0x0402u
#define RI_CTL_808_SNAPPY 0x0403u
#define RI_CTL_808_TONE   0x0404u
#define RI_CTL_808_ACCENT 0x0405u

/* Six fixed metal oscillators, Hz (E1 Werner/Abel/Smith ICMC 2014, §12.5b;
 * shared by CY, OH and CH through their per-voice HP networks). Supersedes
 * RI_808_METAL_BASE/RATIO (WBS-ratio cluster) below, kept for the record. */
extern const float RI_808_METAL_HZ[6];

/* Slot assignment: index = slot 0..10, value = default sound (§2.3 item 6:
 * BD SD LT MT HT RS CP CB CY OH CH; switched slots default to the upper
 * row: LT MT HT RS CP). rb808_trigger re-points a slot at the triggered
 * sound (last-wins); render_mix walks slots in order (the mix law). */
extern const uint8_t RI_808_SLOT_DEFAULT[RI_808_NSLOTS];
/* Sound -> slot lookup (pair members share). */
uint32_t rb808_slot_of(uint32_t sound);

/* Voice short names for goldens/logs ("bd", "sd", ...). */
const char *rb808_name(uint32_t voice);

struct RB808Voice {
    uint8_t id; /* RB808_* */
    uint8_t accent; /* 0/1 binary; 2 reserved (P-12 OPEN, maps to level) */
    uint8_t active; /* nonzero after trigger until the envelope rest
                     * (§2.3: dies on its own; retrigger resets) */
    uint8_t pad;
    float t; /* s since trigger */
    float tune_st; /* semitones, BD ±7 (P-07) */
    float tau_amp; /* s amplitude decay (per-voice default or max) */
    float level; /* per-sound linear trim, knob 0..127/127 (§12.5a) */
    float accent_amt; /* excitation amount 0..1, knob/127 (default 0.5) */
    float snappy; /* SD noise mix ratio, knob/64 (default 1.0, §12.5b) */
    float tone; /* BD click / CY HP knob value 0..127 (default 64, §12.5b) */
    float phase; /* main osc 0..1 */
    float phase2; /* second partial 0..1 */
    float mph[6]; /* metal cluster phases 0..1 */
    uint32_t rng; /* deterministic LFSR (fixed seed at trigger: D1) */
    float st_hp; /* one-pole states for the voice filter chain */
    float st_lp;
    float tail; /* clap tail level (separate ~100 ms param, P-11) */
};

struct RB808Set {
    struct RB808Voice v[RI_808_NSOUNDS];
    uint32_t triggered; /* bit i = sound i triggered (16 bits used) */
    uint8_t slot[RI_808_NSLOTS]; /* slot -> selected sound */
};

void rb808_init_set(struct RB808Set *s);
/* Trigger one voice (accent 0/1; 2 reserved). tune_st in semitones. */
void rb808_trigger(struct RB808Set *s, uint32_t voice, uint32_t accent, float tune_st);
/* Override amplitude decay (storm uses maxima); clamped to [0.005, 4.0]. */
void rb808_set_decay(struct RB808Set *s, uint32_t voice, float tau_amp);
/* Set every voice to its ledger max decay (storm fixture). */
void rb808_max_decay(struct RB808Set *s);
/* Pitch model query (sweep voices: exp curve; fixed voices: constant).
 * Same function the renderer uses; clamped to RI_808_FLOOR_HZ. */
float rb808_pitch_hz(uint32_t voice, float t, float tune_st);
/* Excitation gain: 1.0 (accent 0) or 1.5 (accent 1/2). */
float rb808_excite(uint32_t voice, uint32_t accent, float amt);
/* Knob 0..127 -> voice parameter (WBS interface line 81; 808 section of
 * engine/dsp/params.c). Ids are the RI_CTL_808_* block above; curves are the
 * 9-anchor tables in params.c (mirrors rb303_set_param). */
void rb808_set_param(struct RB808Voice *v, uint32_t ctl_id, uint8_t value);
/* One sample from a single voice (advances state by 1/sr). */
float rb808_voice_render(struct RB808Voice *v, float sr);
/* Sum of all triggered voices (advances each by 1/sr per sample). */
void rb808_render_mix(struct RB808Set *s, float *out, uint32_t n, float sr);
#endif
