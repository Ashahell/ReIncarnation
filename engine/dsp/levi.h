/* levi.h — Levi FM voice bank (owner 2026-09-28, v1 slice 3a).
 * Faithful subset of the manual (pp. 35-36, 43): 2-operator FM core
 * (Freq Mod + Phase Mod; PD/HTE/morphing later), per-operator DAHDSR
 * amplitude contour, resonant lowpass, fixed 6-voice polyphony (lane ==
 * voice slot, like drum lanes). Clean-room: own code, own waves (sine
 * only v1), no ASM content. Determinism: float ri_* kernels only, no
 * RNG, no globals; idle voices return exact 0 without advancing.
 */
#ifndef RI_LEVI_H
#define RI_LEVI_H
#include <stdint.h>

#define RI_LEVI_NVOICES 6u

/* Operator modes (manual p. 43 subset). */
#define RI_LEVI_FM 0u
#define RI_LEVI_PM 1u

/* Voice params (set_param ids). */
#define RI_LEVI_CUTOFF 0u /* Hz, 40..18000 */
#define RI_LEVI_RESO 1u   /* 0..1 */
#define RI_LEVI_MODE 2u   /* RI_LEVI_FM/PM (modulator role) */
#define RI_LEVI_RATIO 3u  /* modulator ratio 0.25..64 */

/* E0 defaults (ledgered here; panel exposes later slices). */
#define RI_LEVI_DEF_CUTOFF 12000.0f
#define RI_LEVI_DEF_RESO 0.15f
#define RI_LEVI_DEF_RATIO 1.0f
#define RI_LEVI_MOD_INDEX 0.5f /* fixed modulator depth v1 */

/* DAHDSR stages (manual: Delay Attack Hold Decay Sustain Release). */
#define RI_LEVI_SEG_D 0u
#define RI_LEVI_SEG_A 1u
#define RI_LEVI_SEG_H 2u
#define RI_LEVI_SEG_D2 3u
#define RI_LEVI_SEG_S 4u
#define RI_LEVI_SEG_R 5u
#define RI_LEVI_SEG_IDLE 6u

struct RILeviEnv {
    float times[6]; /* seconds per stage (D/A/H/D/S ignored for S) */
    float sustain;  /* 0..1 hold level */
    float value;    /* current 0..1 */
    uint8_t stage;  /* RI_LEVI_SEG_* */
    uint8_t pad[3];
    float stage_t;  /* seconds in stage */
};

struct RILeviOp {
    float phase; /* 0..1 */
    float freq;  /* Hz at trigger (note x ratio) */
    float ratio;
    float level; /* 0..1 (Initial Level; Env Level full v1) */
    uint8_t mode;
    uint8_t pad[3];
    struct RILeviEnv env;
};

struct RILeviVoice {
    uint8_t active;
    uint8_t note; /* MIDI */
    uint8_t pad[2];
    struct RILeviOp car, mod;
    float cutoff; /* Hz */
    float reso;   /* 0..1 */
    float level;  /* voice trim */
    float lp1, lp2; /* resonant-LP state */
};

struct RILeviSet {
    struct RILeviVoice v[RI_LEVI_NVOICES];
};

void levi_init_set(struct RILeviSet *s);
/* Trigger (note 0..127) / release a voice. Returns 0 ok, 2 bad. */
int levi_trigger(struct RILeviSet *s, uint32_t voice, uint8_t note);
void levi_release(struct RILeviSet *s, uint32_t voice);
/* Render one sample; idle voices return exact 0. */
/* Render one sample; idle voices return exact 0. */
float levi_voice_render(struct RILeviVoice *v, float sr);
/* Sum all voices into out (render mix, rb909 pattern). */
void levi_voice_render_sum(struct RILeviSet *s, float *out, uint32_t n,
    float sr);
/* Voice params; returns 0 ok, 2 bad id/voice/range/NULL. */
int levi_set_param(struct RILeviSet *s, uint32_t voice, uint32_t id,
    float value);

#endif
