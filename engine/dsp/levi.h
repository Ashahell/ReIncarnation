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
#include "engine/dsp/levi_matrix.h"

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

/* Operator modes (manual p. 43 list; own definitions below). */
#define RI_LEVI_FM 0u     /* frequency wobble by modulator */
#define RI_LEVI_PM 1u     /* phase offset by modulator */
#define RI_LEVI_PWM 2u    /* square, duty 0.5 + 0.4*m (intrinsic pulse) */
#define RI_LEVI_SYNC 3u   /* saw, hard reset on modulator rising edge */
#define RI_LEVI_PDSAW 4u  /* sine tilted saw-ish by modulator drive */
#define RI_LEVI_PDSQ 5u   /* sine folded toward square by drive */
#define RI_LEVI_PDPULSE 6u        /* sine morphed to narrow pulse by |m| */
#define RI_LEVI_NMODES 7u

/* Filter types (own subset: SVF taps + driven 24 dB second stage). */
#define RI_LEVI_FTYPE_LP 0u
#define RI_LEVI_FTYPE_HP 1u
#define RI_LEVI_FTYPE_BP 2u
#define RI_LEVI_FTYPE_NOTCH 3u
#define RI_LEVI_NFTYPES 4u

/* Voice params (set_param ids). */
#define RI_LEVI_CUTOFF 0u /* Hz, 40..18000 */
#define RI_LEVI_RESO 1u   /* 0..1 */
#define RI_LEVI_MODE 2u   /* RI_LEVI_FM/PM (modulator role) */
#define RI_LEVI_RATIO 3u  /* modulator ratio 0.25..64 */
#define RI_LEVI_FTYPE 4u  /* RI_LEVI_FTYPE_* */
#define RI_LEVI_DRIVE 5u  /* 0..1 pre-drive saturation */
#define RI_LEVI_CUTOFF2 6u        /* stage-2 LP Hz, 40..18000 */
#define RI_LEVI_RESO2 7u  /* stage-2 reso 0..1 */
#define RI_LEVI_ATTACK 8u /* s, 0.001..2 */
#define RI_LEVI_DECAY 9u  /* s, 0.001..2 (second decay) */
#define RI_LEVI_SUSTAIN 10u       /* 0..1 hold level */
#define RI_LEVI_RELEASE 11u       /* s, 0.001..2 */
#define RI_LEVI_LOOP 12u  /* 0/1 sustain loops back to attack */
/* Control-block ids (0x0E, recorded in the requirement before code). */
#define RI_CTL_LEVI_CUTOFF 0x0E00u
#define RI_CTL_LEVI_RESO 0x0E01u
#define RI_CTL_LEVI_MODE 0x0E02u
#define RI_CTL_LEVI_RATIO 0x0E03u
#define RI_CTL_LEVI_ALGO 0x0E04u  /* voice algorithm 0..7 */
#define RI_CTL_LEVI_MORPH 0x0E05u /* morph position 0..100 */
#define RI_CTL_LEVI_OPMODE 0x0E06u        /* packed op*16+mode (op 0..7, mode 0..6) */
#define RI_CTL_LEVI_ALGOB 0x0E07u /* morph-target algorithm 0..7 */
#define RI_CTL_LEVI_FTYPE 0x0E08u /* filter type 0..3 */
#define RI_CTL_LEVI_DRIVE 0x0E09u /* drive 0..127 */
#define RI_CTL_LEVI_CUTOFF2 0x0E0Au       /* stage-2 cutoff */
#define RI_CTL_LEVI_RESO2 0x0E0Bu /* stage-2 reso */
#define RI_CTL_LEVI_ATTACK 0x0E0Cu       /* attack time */
#define RI_CTL_LEVI_DECAY 0x0E0Du /* decay time */
#define RI_CTL_LEVI_SUSTAIN 0x0E0Eu       /* sustain level */
#define RI_CTL_LEVI_RELEASE 0x0E0Fu       /* release time */
#define RI_CTL_LEVI_LOOP 0x0E10u  /* envelope loop 0/1 */
#define RI_CTL_LEVI_ARPON 0x0E11u       /* arp gate 0/1 (v2 feature 3) */
#define RI_CTL_LEVI_ARPRATE 0x0E12u     /* arp rate 0..127 (v2 feature 3) */
#define RI_CTL_LEVI_SEQON 0x0E13u       /* seq gate 0/1 (v2 feature 3) */
#define RI_CTL_LEVI_SEQLEN 0x0E14u      /* seq length 1..16 (v2 feature 3) */
#define RI_CTL_LEVI_ROUTE0 0x0E15u      /* matrix slot 0 gate (v2 feature 4) */
#define RI_CTL_LEVI_ROUTE1 0x0E16u
#define RI_CTL_LEVI_ROUTE2 0x0E17u
#define RI_CTL_LEVI_ROUTE3 0x0E18u
#define RI_CTL_LEVI_ROUTE4 0x0E19u
#define RI_CTL_LEVI_ROUTE5 0x0E1Au
#define RI_CTL_LEVI_ROUTE6 0x0E1Bu
#define RI_CTL_LEVI_ROUTE7 0x0E1Cu

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
    uint8_t loop;   /* sustain loops back to attack (contour loop) */
    uint8_t pad[2];
    float stage_t;  /* seconds in stage */
};

struct RILeviOp {
    float ratio;
    float level; /* 0..1 (Initial Level; Env Level full v1) */
    uint8_t mode;
    uint8_t pad[3];
};

struct RILeviOpState {
    float phase; /* 0..1 */
    float freq;  /* Hz at trigger (note x ratio) */
    float ps;    /* previous modulator (SYNC edge detect) */
    struct RILeviEnv env;
};

struct RILeviVoice {
    uint8_t active;
    uint8_t note; /* MIDI */
    uint8_t algo; /* bank-A preset id, or RI_LEVI_ALGO_CUSTOM */
    uint8_t algoB;        /* morph-target preset id */
    uint8_t morph;        /* bank blend 0..100 (A->B) */
    uint8_t pad[3];
    struct RILeviOp op[RI_LEVI_NOPS]; /* params, shared by both banks */
    struct RILeviOpState st[2][RI_LEVI_NOPS]; /* render states, bank A/B */
    int8_t mod_src[RI_LEVI_NOPS]; /* bank A: who i feeds, -1 = mix */
    int8_t mod_srcB[RI_LEVI_NOPS];        /* bank B routing */
    uint8_t order[RI_LEVI_NOPS];  /* bank-A render order */
    uint8_t orderB[RI_LEVI_NOPS]; /* bank-B render order */
    uint8_t live[RI_LEVI_NOPS];   /* bank A graph */
    uint8_t liveB[RI_LEVI_NOPS];  /* bank B graph */
    float cutoff; /* Hz */
    float reso;   /* 0..1 */
    float level;  /* voice trim */
    float drive;  /* 0..1 pre-drive saturation */
    float cutoff2;        /* stage-2 LP Hz */
    float reso2;  /* stage-2 reso 0..1 */
    uint8_t ftype;        /* RI_LEVI_FTYPE_* */
    uint8_t padlp[3];
    float lp1, lp2; /* stage-1 SVF state */
    float lp3, lp4; /* stage-2 LP state (24 dB cascade) */
};

struct RILeviSet {
    struct RILeviVoice v[RI_LEVI_NVOICES];
    uint8_t arpon;   /* device arp gate (v2 feature 3; UI/automation truth) */
    uint8_t arprate; /* device arp rate 0..127 */
    uint8_t seqon;   /* device seq gate (v2 feature 3; UI/automation truth) */
    uint8_t seqlen;  /* device seq length 1..16 */
    struct RILeviMatrix mx; /* device matrix program (v2 feature 4) */
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
/* Render one sample; idle voices return exact 0. mx NULL (or an
 * empty program) renders the legacy path bit-identically. */
float levi_voice_render(struct RILeviVoice *v, const struct RILeviMatrix *mx,
    float sr);
/* Sum all voices into out (render mix, rb909 pattern). */
void levi_voice_render_sum(struct RILeviSet *s, float *out, uint32_t n,
    float sr);
/* Algorithm select (preset 0..7; 8/custom is readable, not settable).
 * Returns 0 ok, 2 bad. Selecting a preset replaces custom routing. */
int levi_set_algo(struct RILeviSet *s, uint32_t voice, uint32_t algo);
/* Current algorithm id (0..8); negative on bad voice/NULL. */
int levi_algo_get(const struct RILeviSet *s, uint32_t voice);
/* Custom routing: op feeds src (target op index) or -1 to the mix;
 * the voice becomes custom. Acyclic only: src's forward chain must not
 * reach op (cycles/self-routes refused, returns 2, routing unchanged).
 * Returns 0 ok, 2 bad. */
int levi_set_route(struct RILeviSet *s, uint32_t voice, uint32_t op,
    int src);
/* Routing read: feed target op index, -1 mix; -2 on bad voice/op/NULL. */
int levi_route_get(const struct RILeviSet *s, uint32_t voice,
    uint32_t op);
/* Morph target select (preset 0..7) + blend position 0..100. Bank-B
 * states start as a copy of bank A (seamless join); both banks tick
 * every sample. Returns 0 ok, 2 bad. */
int levi_set_morph(struct RILeviSet *s, uint32_t voice, uint32_t algoB,
    uint32_t pos);
/* Blend position 0..100; negative on bad voice/NULL. */
int levi_morph_get(const struct RILeviSet *s, uint32_t voice);
/* Voice params; returns 0 ok, 2 bad id/voice/range/NULL. */
int levi_set_param(struct RILeviSet *s, uint32_t voice, uint32_t id,
    float value);
/* Per-operator mode (panel slice owns names; DSP owns behaviour).
 * Returns 0 ok, 2 bad. */
int levi_set_op_mode(struct RILeviSet *s, uint32_t voice, uint32_t op,
    uint32_t mode);

#endif
