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

#define RI_808_NVOICES 15u

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

/* Appendix A nominals [HYPOTHESIS] (full rows in docs/evidence/808/). */
#define RI_808_BD_F_START 170.0f /* Hz at tune 0 (P-07; tune ±7 st) */
#define RI_808_BD_F_END 48.0f
#define RI_808_BD_TAU_PITCH 0.022f /* s (P-07) */
#define RI_808_SD_F1 185.0f /* Hz (P-08; 173.3 also passes: revision tol) */
#define RI_808_SD_F2 330.0f /* Hz (P-08; 336.0 also passes: revision tol) */
#define RI_808_TOM_LT_F0 75.0f /* P-09 sweep starts */
#define RI_808_TOM_MT_F0 115.0f
#define RI_808_TOM_HT_F0 155.0f
#define RI_808_TOM_F1_RATIO 0.65f /* sweep end ~= 0.65 * start (LT 75->50 ...) */
#define RI_808_CONGA_LC_F 200.0f /* P-09 fixed */
#define RI_808_CONGA_MC_F 250.0f
#define RI_808_CONGA_HC_F 310.0f
#define RI_808_METAL_BASE 400.0f /* Hz; 6-osc cluster base (P-10) */
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
#define RI_808_FLOOR_HZ 35.0f /* P-07 35 Hz floor: no voice ever below */
#define RI_808_SR_DEFAULT 48000.0f

/* Six-oscillator metal cluster ratios (P-10 E0 nominals; ledger row CH). */
extern const float RI_808_METAL_RATIO[6];

/* Voice short names for goldens/logs ("bd", "sd", ...). */
const char *rb808_name(uint32_t voice);

struct RB808Voice {
    uint8_t id; /* RB808_* */
    uint8_t accent; /* 0/1 binary; 2 reserved (P-12 OPEN, maps to 1.5) */
    uint8_t active; /* nonzero after trigger until caller mutes */
    uint8_t pad;
    float t; /* s since trigger */
    float tune_st; /* semitones, BD ±7 (P-07) */
    float tau_amp; /* s amplitude decay (per-voice default or max) */
    float phase; /* main osc 0..1 */
    float phase2; /* second partial 0..1 */
    float mph[6]; /* metal cluster phases 0..1 */
    uint32_t rng; /* deterministic LFSR (fixed seed at trigger: D1) */
    float st_hp; /* one-pole states for the voice filter chain */
    float st_lp;
    float st_lp2;
    float tail; /* clap tail level (separate ~100 ms param, P-11) */
};

struct RB808Set {
    struct RB808Voice v[RI_808_NVOICES];
    uint16_t triggered; /* bit i = voice i triggered */
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
float rb808_excite(uint32_t voice, uint32_t accent);
/* One sample from a single voice (advances state by 1/sr). */
float rb808_voice_render(struct RB808Voice *v, float sr);
/* Sum of all triggered voices (advances each by 1/sr per sample). */
void rb808_render_mix(struct RB808Set *s, float *out, uint32_t n, float sr);
#endif
