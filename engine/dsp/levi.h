/* levi.h — Levi FM voice bank (owner 2026-09-28, v1 slice 3a + v2 slice 1).
 * Faithful subset of the manual (pp. 35-36, 43) growing toward the
 * hardware: 8-operator algorithms (own topologies) + custom routing,
 * per-operator FM/PM modes (PD/PWM/Sync later), per-operator DAHDSR
 * amplitude contour, resonant lowpass, fixed 6-voice polyphony (lane ==
 * voice slot, like drum lanes). Clean-room: own code, own waves (sine
 * only v1), own topologies, no ASM content. Determinism: float ri_*
 * kernels only, no RNG, no globals; idle voices return exact 0
 * without advancing.
 */
#ifndef RI_LEVI_H
#define RI_LEVI_H
#include <stdint.h>

#define RI_LEVI_NVOICES 6u
#define RI_LEVI_NOPS 8u
/* Algorithm ids (own presets; 8 = custom routing, readable not settable). */
#define RI_LEVI_ALGO_DUO 0u     /* one 2-op pair (v1 sound), rest idle */
#define RI_LEVI_ALGO_ALLPAR 1u  /* 8 parallel carriers */
#define RI_LEVI_ALGO_STACK8 2u  /* single 8-op chain */
#define RI_LEVI_ALGO_STACK44 3u /* two 4-op chains */
#define RI_LEVI_ALGO_STACK422 4u        /* 4-chain + two pairs */
#define RI_LEVI_ALGO_PAIRS4 5u  /* four 2-op pairs */
#define RI_LEVI_ALGO_STACK332 6u        /* 3+3+2 chains */
#define RI_LEVI_ALGO_STACK62 7u /* 6-chain + pair */
#define RI_LEVI_ALGO_CUSTOM 8u
#define RI_LEVI_ALGO_N 8u

/* Operator modes (manual p. 43 subset). */
#define RI_LEVI_FM 0u
#define RI_LEVI_PM 1u

/* Voice params (set_param ids). */
#define RI_LEVI_CUTOFF 0u /* Hz, 40..18000 */
#define RI_LEVI_RESO 1u   /* 0..1 */
#define RI_LEVI_MODE 2u   /* RI_LEVI_FM/PM (modulator role) */
#define RI_LEVI_RATIO 3u  /* modulator ratio 0.25..64 */
/* Control-block ids (0x0E, recorded in the requirement before code). */
#define RI_CTL_LEVI_CUTOFF 0x0E00u
#define RI_CTL_LEVI_RESO 0x0E01u
#define RI_CTL_LEVI_MODE 0x0E02u
#define RI_CTL_LEVI_RATIO 0x0E03u

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
    uint8_t algo; /* preset id, or RI_LEVI_ALGO_CUSTOM */
    uint8_t pad;
    struct RILeviOp op[RI_LEVI_NOPS];
    int8_t mod_src[RI_LEVI_NOPS]; /* modulator op index, -1 = carrier */
    uint8_t order[RI_LEVI_NOPS];  /* render order (modulators first) */
    uint8_t live[RI_LEVI_NOPS];   /* 1 = in the graph */
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
/* UI-value mapper (panel/automation 0..127 -> voice params, rb303
 * set_param shape): cutoff exponential 40..18000 Hz, reso linear,
 * mode >= 64 PM else FM, ratio 0.25..64 over 8 octaves. Applies to one
 * voice; returns 0 ok, 2 bad. */
int levi_set_param_ui(struct RILeviSet *s, uint32_t voice, uint32_t id,
    uint8_t val);
/* Render one sample; idle voices return exact 0. */
float levi_voice_render(struct RILeviVoice *v, float sr);
/* Sum all voices into out (render mix, rb909 pattern). */
void levi_voice_render_sum(struct RILeviSet *s, float *out, uint32_t n,
    float sr);
/* Algorithm select (preset 0..7; 8/custom is readable, not settable).
 * Returns 0 ok, 2 bad. Selecting a preset replaces custom routing. */
int levi_set_algo(struct RILeviSet *s, uint32_t voice, uint32_t algo);
/* Current algorithm id (0..8); negative on bad voice/NULL. */
int levi_algo_get(const struct RILeviSet *s, uint32_t voice);
/* Custom routing: op's modulator (op index) or -1 for carrier; the
 * voice becomes custom. Acyclic only: cycles/self-routes refused
 * (returns 2, routing unchanged). Returns 0 ok, 2 bad. */
int levi_set_route(struct RILeviSet *s, uint32_t voice, uint32_t op,
    int src);
/* Routing read: modulator op index, -1 carrier; -2 on bad voice/op/NULL. */
int levi_route_get(const struct RILeviSet *s, uint32_t voice,
    uint32_t op);
/* Voice params; returns 0 ok, 2 bad id/voice/range/NULL. */
int levi_set_param(struct RILeviSet *s, uint32_t voice, uint32_t id,
    float value);

#endif
