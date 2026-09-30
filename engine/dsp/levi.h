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
#define RI_LEVI_ALGO_N 64u      /* own bank (fidelity P3); 0..7 = the v1 presets */
#define RI_LEVI_ALGO_CUSTOM 64u /* custom routing: readable, not a preset */
/* Algo modes (manual p. 58): one preset, a morph across up to 8 slots,
 * or a custom routing grid. */
#define RI_LEVI_AMODE_SINGLE 0u
#define RI_LEVI_AMODE_MORPH 1u
#define RI_LEVI_AMODE_CUSTOM 2u
#define RI_LEVI_SLOT_SILENCE 64u /* morph slot: no oscillators */
#define RI_LEVI_SLOT_OFF 65u     /* morph slot unused (slot 1 never) */
#define RI_LEVI_NSLOTS 8u

/* Operator modes (manual p. 43 list; own definitions below). */
#define RI_LEVI_FM 0u     /* frequency wobble by modulator */
#define RI_LEVI_PM 1u     /* phase offset by modulator */
#define RI_LEVI_PWM 2u    /* square, duty 0.5 + 0.4*m (intrinsic pulse) */
#define RI_LEVI_SYNC 3u   /* saw, hard reset on modulator rising edge */
#define RI_LEVI_PDSAW 4u  /* sine tilted saw-ish by modulator drive */
#define RI_LEVI_PDSQ 5u   /* sine folded toward square by drive */
#define RI_LEVI_PDPULSE 6u        /* sine morphed to narrow pulse by |m| */
#define RI_LEVI_NMODES 7u

/* v1 filter types (legacy FTYPE key; mapped onto the P4 models). */
#define RI_LEVI_FTYPE_LP 0u
#define RI_LEVI_FTYPE_HP 1u
#define RI_LEVI_FTYPE_BP 2u
#define RI_LEVI_FTYPE_NOTCH 3u
#define RI_LEVI_NFTYPES 4u
/* Digital filter models (fidelity P4, manual p. 62; own designs and
 * own names): two morphing state-variable filters, 3 HP, 2 BP, 10 LP,
 * a formant (vowel) filter. */
#define RI_LEVI_DF_SVF_LBH 0u     /* morph LP > BP > HP */
#define RI_LEVI_DF_SVF_LNH 1u     /* morph LP > notch > HP */
#define RI_LEVI_DF_HP_GRIT 2u
#define RI_LEVI_DF_HP_MOD 3u
#define RI_LEVI_DF_HP_12 4u
#define RI_LEVI_DF_BP_MOD 5u
#define RI_LEVI_DF_BP_12 6u
#define RI_LEVI_DF_LP_L12 7u      /* ladder, uncompensated */
#define RI_LEVI_DF_LP_L24 8u
#define RI_LEVI_DF_LP_F12 9u      /* ladder, bass-compensated */
#define RI_LEVI_DF_LP_F24 10u
#define RI_LEVI_DF_LP_GATE 11u
#define RI_LEVI_DF_LP_GRIT 12u
#define RI_LEVI_DF_LP_MOD 13u
#define RI_LEVI_DF_LP_12 14u
#define RI_LEVI_DF_LP_6 15u
#define RI_LEVI_DF_LP_48 16u
#define RI_LEVI_DF_VOWEL 17u
#define RI_LEVI_NDF 18u

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
#define RI_CTL_LEVI_LFO0RATE 0x0E1Du   /* LFO rate 0..127 (v2 feature 4d) */
#define RI_CTL_LEVI_LFO1RATE 0x0E1Eu
#define RI_CTL_LEVI_LFO2RATE 0x0E1Fu
#define RI_CTL_LEVI_LFO3RATE 0x0E20u
#define RI_CTL_LEVI_LFO4RATE 0x0E21u
#define RI_CTL_LEVI_LFO0SHAPE 0x0E22u  /* LFO shape 0/1 (v2 feature 4d) */
#define RI_CTL_LEVI_LFO1SHAPE 0x0E23u
#define RI_CTL_LEVI_LFO2SHAPE 0x0E24u
#define RI_CTL_LEVI_LFO3SHAPE 0x0E25u
#define RI_CTL_LEVI_LFO4SHAPE 0x0E26u

/* E0 defaults (ledgered here; panel exposes later slices). */
#define RI_LEVI_DEF_CUTOFF 12000.0f
#define RI_LEVI_DEF_RESO 0.15f
#define RI_LEVI_DEF_RATIO 1.0f
#define RI_LEVI_MOD_INDEX 0.5f /* v1 modulator depth (= RI_LEVI_MOD_DEPTH x 1/8) */

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
    /* Fidelity P2 (manual pp. 38-40): curves, quantize, counted loops
     * over a stage range, freerun. Zeros = the v1 envelope exactly. */
    int8_t curve[3];   /* attack, decay, release: -64..+63, 0 = linear */
    uint8_t quant;     /* output steps, 0 = off */
    uint8_t loopn;     /* 0 off (legacy loop flag above), 2..50, 255 = infinite */
    uint8_t loopleft;  /* loops remaining this note */
    uint8_t loopend;   /* last stage in the loop: RI_LEVI_SEG_A/H/D2 */
    uint8_t freerun;   /* note-off waits for the sustain stage */
    uint8_t relpend;   /* freerun: release pending */
    int8_t cmod[3];    /* matrix curve offsets (P5b), 0 = none */
    float seg_from;    /* value at segment start (curved segments) */
    float susmod;      /* matrix sustain offset (P5b), 0 = none */
};

/* LFOs (v2 feature 4d, owner order): 5 per voice, trigger-reset
 * phases (deterministic, no RNG). Rate 0.01..30 Hz exp-mapped
 * (100 s/cycle floor per spec); shape 0 smooth sine, 1 quantized
 * 3-step {-1,0,+1} (own interpretation of "quantized to 3 steps"). */
#define RI_LEVI_NLFO 5u
#define RI_LEVI_LFO_SMOOTH 0u
#define RI_LEVI_LFO_STEPS 1u

/* Full LFO (fidelity P5, manual pp. 76-82; own wave set and laws).
 * Params ride keys 0x10A0 | lfo << 4 | param (block 0x10), 0..127. */
#define RI_LEVI_LW_SINE 0u
#define RI_LEVI_LW_TRI 1u
#define RI_LEVI_LW_SAWUP 2u
#define RI_LEVI_LW_SAWDN 3u
#define RI_LEVI_LW_SQUARE 4u
#define RI_LEVI_LW_PULSE27 5u
#define RI_LEVI_LW_PULSE13 6u
#define RI_LEVI_LW_SH 7u      /* sample & hold, one value per cycle */
#define RI_LEVI_LW_NOISE 8u   /* a new value every sample */
#define RI_LEVI_LW_RANDOM 9u  /* smooth random: glides between per-cycle values */
#define RI_LEVI_LW_STEP 10u   /* step table (default ramp -1..+1) */
#define RI_LEVI_NLW 11u
#define RI_LEVI_LP_WAVE 0u
#define RI_LEVI_LP_RATE 1u
#define RI_LEVI_LP_SPEED 2u    /* 0 slow (0..25 Hz), 1 fast (5..150 Hz) */
#define RI_LEVI_LP_TRIG 3u     /* 0 poly, 1 single, 2 off (free, shared) */
#define RI_LEVI_LP_DELAY 4u
#define RI_LEVI_LP_FADE 5u
#define RI_LEVI_LP_QUANT 6u    /* 0 off, 1..15 step tables */
#define RI_LEVI_LP_LEVEL 7u    /* 0..127, 127 = full */
#define RI_LEVI_LP_STEPS 8u    /* 2..64 */
#define RI_LEVI_LP_SMOOTH 9u
#define RI_LEVI_LP_BPM 10u     /* stored; tempo sync arrives with the clock (P8) */
#define RI_LEVI_LP_ONESHOT 11u /* 0 off, 1 on (one cycle), 2 step (one step per note) */
#define RI_LEVI_LP_PHASE 12u   /* start phase 0..360 deg */
#define RI_LEVI_LP_STAGGER 13u /* per-voice phase offset (trig sync off) */
#define RI_LEVI_LP_N 14u
#define RI_LEVI_MAXSTEPS 64u

struct RILeviLFO {
    float rate;    /* Hz */
    uint8_t shape; /* legacy view: RI_LEVI_LFO_* (0 sine, 1 = step x3) */
    uint8_t wave;  /* RI_LEVI_LW_* */
    uint8_t oneshot, trig;
    float phase;   /* 0..1 */
    float value;   /* last stepped value (after level/quantize/smooth) */
    uint8_t ui[RI_LEVI_LP_N];
    uint8_t steps, quant, wrapped, done;
    uint8_t stepk, shared, pad[2];
    float level, delay, fade, smooth; /* smooth: one-pole coefficient, 0 = off */
    float t;       /* seconds since the trigger */
    float phase0;  /* start phase 0..1 (+ stagger) */
    float held, from, sy;    /* S&H / random values, smoother state */
    uint32_t rng;
    float rmul, lmod, smod, stmod; /* matrix: rate x, level/smooth/steps + (P5b) */
};

/* Modulation envelopes ENV 1-5 (fidelity P5, manual pp. 71-75): the
 * oscillator DAHDSR with its own UI list; keys 0x1000 | env << 5 |
 * param, params numbered like the oscillator envelope (RI_LEVI_OP_DELAY
 * .. RI_LEVI_OP_FREERUN, VELENV), plus trigger sources and level. */
#define RI_LEVI_NMENV 5u
#define RI_LEVI_ME_TRIG1 0u     /* trigger sources 1-4 (params 0..3) */
#define RI_LEVI_ME_LEVEL 6u     /* 0..127 = 0.0..128.0 (the osc ENVL slot) */
#define RI_LEVI_ME_VELCRV 29u   /* stored (velocity arrives in P9) */
#define RI_LEVI_TS_OFF 0u
#define RI_LEVI_TS_NOTE 1u
#define RI_LEVI_TS_LFO1 2u      /* 2..6 = LFO 1-5 cycle start */
#define RI_LEVI_TS_N 10u        /* 7 ribbon on, 8 ribbon release, 9 sustain pedal: stored (P8/P9) */

/* Per-oscillator parameters (fidelity plan P2, manual pp. 35-41). UI
 * values ride control keys 0x0F00 | op << 5 | param (owner 2026-09-30:
 * a second Levi block), 0..127 on the wire, each param in its range
 * (ri_levi_op_range). Defaults reproduce the v1 voice bit for bit:
 * sine, ratio 1, init 0, env level +128 on op 0 and +16 on ops 1..7
 * (with depth 4: index 0.5, the v1 law), keytrack 100 %, phase 0. */
#define RI_LEVI_OP_WAVE 0u
#define RI_LEVI_OP_INVERT 1u
#define RI_LEVI_OP_PMODE 2u      /* 0 semitone, 1 ratio, 2 frequency */
#define RI_LEVI_OP_COARSE 3u     /* semi 28..100 (-36..+36), ratio idx 0..65, freq 0..127 */
#define RI_LEVI_OP_FINE 4u       /* 64 = centre (cent / ratio fine); freq: +0..0.99 Hz */
#define RI_LEVI_OP_INIT 5u       /* Initial Level 0..128 */
#define RI_LEVI_OP_ENVL 6u       /* Env Level -128..+128, 64 = 0 */
#define RI_LEVI_OP_FEEDBACK 7u   /* 0..100 % (Phase/Freq Mod only) */
#define RI_LEVI_OP_KEYTRK 8u     /* -200..+200 %, 64 = 0, 96 = 100 % */
#define RI_LEVI_OP_PHASE 9u      /* start phase 0..360 deg */
#define RI_LEVI_OP_DIRECT 10u    /* Direct Out: a modulator also sounds */
#define RI_LEVI_OP_MODE 11u      /* how it modulates others: RI_LEVI_FM.. */
#define RI_LEVI_OP_DELAY 12u     /* envelope times 0..127 (speed range) */
#define RI_LEVI_OP_ATTACK 13u
#define RI_LEVI_OP_HOLD 14u
#define RI_LEVI_OP_DECAY 15u
#define RI_LEVI_OP_SUSTAIN 16u   /* 0..128 */
#define RI_LEVI_OP_RELEASE 17u
#define RI_LEVI_OP_SPEED 18u     /* 0 fast, 1 slow */
#define RI_LEVI_OP_ACURVE 19u    /* curves: 64 = linear */
#define RI_LEVI_OP_DCURVE 20u
#define RI_LEVI_OP_RCURVE 21u
#define RI_LEVI_OP_QUANT 22u     /* 0 off, 1..15 step tables */
#define RI_LEVI_OP_LOOP 23u      /* 0 off, 1..49 = 2..50 loops, 50 = infinite */
#define RI_LEVI_OP_STAGELOOP 24u /* 0 D>A, 1 D>H, 2 D>D */
#define RI_LEVI_OP_LEGATO 25u
#define RI_LEVI_OP_RESET 26u
#define RI_LEVI_OP_FREERUN 27u
#define RI_LEVI_OP_VELENV 28u    /* stored; velocity arrives with MIDI (P9) */
#define RI_LEVI_OP_TGT1 29u      /* custom routing: up to 3 targets, 0 none, 1..8 = OSC 1..8 */
#define RI_LEVI_OP_TGT2 30u
#define RI_LEVI_OP_TGT3 31u
#define RI_LEVI_OP_NPARAM 32u
#define RI_LEVI_OPKEY(op, p) ((uint16_t)(0x0F00u | (((uint32_t)(op) & 7u) << 5) | ((uint32_t)(p) & 31u)))
#define RI_LEVI_NWAVES 128u      /* own authored set: 8 families x 16 */
#define RI_LEVI_MOD_DEPTH 4.0f   /* Phase/Freq Mod index at full level (E0) */

struct RILeviOp {
    float ratio;
    float level; /* 0..1 legacy trim (1) */
    uint8_t mode;
    uint8_t wave;     /* 0..127 */
    uint8_t invert;
    uint8_t pmode;    /* 0 semitone, 1 ratio, 2 frequency */
    float pitchmul;   /* semitone mode: 2^((semi+cent/100)/12) */
    float hz;         /* frequency mode: fixed Hz */
    float init;       /* 0..1 */
    float envl;       /* -1..1 */
    float fb;         /* 0..1 */
    float kt;         /* keytrack, 1 = 100 % */
    float phase0;     /* 0..1 */
    uint8_t direct;
    uint8_t speed;
    uint8_t pad2[2];
    uint8_t ui[RI_LEVI_OP_NPARAM]; /* last UI values (readback) */
};

struct RILeviOpState {
    float phase; /* 0..1 */
    float freq;  /* Hz at trigger (note x ratio) */
    float ps;    /* previous modulator (SYNC edge detect) */
    struct RILeviEnv env;
    float last;  /* last output (feedback) */
    float amp;   /* last level 0..1 (modulation amount for PW/Sync/PD) */
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
    uint8_t feeds[RI_LEVI_NOPS];  /* bank A: ops op i modulates (bit t = op t); 0 = carrier */
    uint8_t feedsB[RI_LEVI_NOPS]; /* bank B routing */
    uint8_t amode;                /* RI_LEVI_AMODE_* */
    uint8_t slot[RI_LEVI_NSLOTS]; /* morph list: preset, SILENCE or OFF */
    uint8_t mslotA;               /* morph: list index loaded into bank A */
    uint8_t mute;                 /* carriers muted (bit per op) */
    uint8_t solo;                 /* 0 none, 1..8 soloed op */
    uint8_t cfeeds[RI_LEVI_NOPS]; /* custom grid (Custom mode) */
    uint16_t mpos;                /* morph position, 100 per slot step */
    uint8_t padm[2];
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
    float df[12];   /* digital filter state (model-dependent) */
    float af[5];    /* analog ladder: 4 stages + last output */
    struct RILeviLFO lfo[RI_LEVI_NLFO]; /* per-voice LFOs (4d) */
    float bias_envl;  /* Osc Env Level bias, -1..1 (device knob, P2) */
    float bias_t[4];  /* attack/decay/release/hold time scales (1 = none; hold has no knob) */
    /* Filters + VCA (fidelity P4, manual pp. 62-70). */
    uint8_t dtype;    /* RI_LEVI_DF_* */
    uint8_t dmorph;   /* morph (SVF, vowel) or drive (others), 0..127 */
    uint8_t dpost;    /* drive after the filter */
    uint8_t vorder;   /* vowel order 0..7 */
    float dkt, akt;   /* keytrack, octaves per octave around C2 (-2..2) */
    float dktm, aktm; /* keytrack cutoff multipliers for the held note */
    float dlfo, alfo, vlfo;  /* LFO 1/2/3 amounts, -1..1 */
    float dlevel, osclvl, vcalvl, patchlvl; /* stage gains, 1 = unity */
    /* Modulation envelopes (P5): ENV 1 > digital cutoff, ENV 2 > analog
     * cutoff, ENV 3 > VCA (pre-wired, p. 71). */
    struct RILeviEnv menv[RI_LEVI_NMENV];
    uint8_t meui[RI_LEVI_NMENV][RI_LEVI_OP_NPARAM];
    uint8_t melfo;            /* some envelope listens to an LFO cycle (steps the LFOs) */
    uint8_t mepad[2];
    float melevel[RI_LEVI_NMENV];
    float denv, aenv, vinit;  /* ENV 1/2 amounts -1..1, VCA initial level 0..1 */
    /* Matrix / macro modulation of oscillator and envelope params (P5b):
     * RI_LEVI_DO_* per oscillator, RI_LEVI_DE_* per ENV 1-5; -1..1. */
    float opm[RI_LEVI_NOPS][RI_LEVI_DO_N];
    float mem[RI_LEVI_NMENV][9];
    uint8_t opm_on, mem_on, padm2[2];
    float melmod[RI_LEVI_NMENV];   /* ENV level offsets (P5b) */
    /* Voice params (fidelity P6b, manual pp. 87-96; pan/width/mode go
     * live with the P6c stereo sum, bend with the P9 MIDI data). */
    float vdetune;  /* 0..1 ordinal spread, +/-50 c full scale */
    float vafeel;   /* 0..1 per-voice drift */
    float vrndph;   /* 0..1 random start-phase amount */
    float vpan;     /* -1..1 */
    float vwidth;   /* 0..1 panner width */
    uint8_t vpanmode; /* 0..2 */
    float vbendrng; /* semitones 0..24 (bend src reads 0 until P9) */
    float vvibrate; /* Hz 0.1..20 */
    float vvibamt;  /* semitones 0..4 */
    float vvibdly;  /* seconds 0..5 */
    uint8_t vglide; /* 0 off, 1 glide, 2 glissando */
    float vgltime;  /* seconds 0..5 */
    float vglcurve; /* glide exponent */
    float vibphase, vibtime; /* vibrato state (trigger-reset) */
    float glsemi, glt;       /* glide state (semitone offset, progress) */
    float wtime;             /* analog-feel wander clock */
    uint8_t vidx;            /* voice index (VoiceMod ordinal) */
    uint8_t vpad[3];
    float vom[RI_LEVI_DVO_N]; /* DM_VOICE offsets (P6b); -1..1 */
    uint8_t vom_on;
};

/* Osc Env Level & Bias (manual p. 54): device-wide offsets over every
 * oscillator envelope. 64 = no bias. Keys 0x0E27..0x0E2A. */
#define RI_CTL_LEVI_BIAS_ENVL 0x0E27u
#define RI_CTL_LEVI_BIAS_ATK 0x0E28u
#define RI_CTL_LEVI_BIAS_DEC 0x0E29u
#define RI_CTL_LEVI_BIAS_REL 0x0E2Au
/* Algorithm modes, morph list and solo/mute (fidelity P3, manual pp.
 * 58-61). Slot values 0..63 preset, 64 SILENCE, 65 OFF; morph position
 * 0..127 spans the active slots; mute masks split op 0..6 / op 7. */
#define RI_CTL_LEVI_AMODE 0x0E2Bu
#define RI_CTL_LEVI_SLOT0 0x0E2Cu  /* .. 0x0E33 */
#define RI_CTL_LEVI_MPOS 0x0E34u
#define RI_CTL_LEVI_SOLO 0x0E35u
#define RI_CTL_LEVI_MUTELO 0x0E36u
#define RI_CTL_LEVI_MUTEHI 0x0E37u
/* Filters + VCA (fidelity P4, manual pp. 62-70). 7-bit values; amounts
 * and keytrack 64 = 0 (keytrack 32 per 100 %), levels 64 = unity. The
 * v1 keys stay: CUTOFF/RESO digital, DRIVE = analog pre-drive,
 * CUTOFF2/RESO2 analog, FTYPE legacy (maps onto a model). */
#define RI_CTL_LEVI_DTYPE 0x0E38u
#define RI_CTL_LEVI_DMORPH 0x0E39u
#define RI_CTL_LEVI_DPOST 0x0E3Au
#define RI_CTL_LEVI_VORDER 0x0E3Bu
#define RI_CTL_LEVI_DKEYTRK 0x0E3Cu
#define RI_CTL_LEVI_DLFO1 0x0E3Du
#define RI_CTL_LEVI_DLEVEL 0x0E3Eu
#define RI_CTL_LEVI_AKEYTRK 0x0E3Fu
#define RI_CTL_LEVI_ALFO2 0x0E40u
#define RI_CTL_LEVI_OSCLVL 0x0E41u
#define RI_CTL_LEVI_VCALVL 0x0E42u
#define RI_CTL_LEVI_PATCHLVL 0x0E43u
#define RI_CTL_LEVI_VLFO3 0x0E44u
/* Pre-wired envelope amounts and VCA initial level (fidelity P5). */
#define RI_CTL_LEVI_DENV1 0x0E45u    /* 64 = 0 */
#define RI_CTL_LEVI_AENV2 0x0E46u    /* 64 = 0 */
#define RI_CTL_LEVI_VINIT 0x0E47u    /* 0..127 */
/* Macros (fidelity P5b, pp. 120-123): knobs 0x0E48..4F, buttons
 * 0x0E50..57. Matrix routes ride block 0x11 (slot << 2 | field: 0
 * source (UI list), 1 module, 2 param, 3 depth 64 = 0); macro routes
 * block 0x12 (macro << 5 | route << 2 | field: 0 module, 1 param,
 * 2 depth 64 = 0, 3 button value). */
#define RI_CTL_LEVI_MKNOB0 0x0E48u
#define RI_CTL_LEVI_MBTN0 0x0E50u
/* Voice allocator (fidelity P6a, manual pp. 87-96): polyphony mode,
 * unison density and poly limit. Device-wide, section-wide apply. */
#define RI_CTL_LEVI_POLYMODE 0x0E58u
#define RI_CTL_LEVI_UDENSITY 0x0E59u
#define RI_CTL_LEVI_ULIMIT 0x0E5Au
/* Voice params (fidelity P6b, manual pp. 87-96): detune, analog feel,
 * random phase, pan/width/mode (stored; stereo P6c), bend range,
 * vibrato rate/amt/delay, glide mode/time/curve. Device-wide. */
#define RI_CTL_LEVI_VDETUNE 0x0E5Bu
#define RI_CTL_LEVI_VAFEEL 0x0E5Cu
#define RI_CTL_LEVI_VRNDPH 0x0E5Du
#define RI_CTL_LEVI_VPAN 0x0E5Eu
#define RI_CTL_LEVI_VWIDTH 0x0E5Fu
#define RI_CTL_LEVI_VPANMODE 0x0E60u
#define RI_CTL_LEVI_VBENDRNG 0x0E61u
#define RI_CTL_LEVI_VVIBRATE 0x0E62u
#define RI_CTL_LEVI_VVIBAMT 0x0E63u
#define RI_CTL_LEVI_VVIBDLY 0x0E64u
#define RI_CTL_LEVI_VGLIDE 0x0E65u
#define RI_CTL_LEVI_VGLTIME 0x0E66u
#define RI_CTL_LEVI_VGLCURVE 0x0E67u
#define RI_LEVI_POLY_ROTATE 0u
#define RI_LEVI_POLY_REASSIGN 1u
#define RI_LEVI_POLY_MONO 2u
#define RI_LEVI_POLY_MONOLO 3u
#define RI_LEVI_POLY_MONOHI 4u
#define RI_LEVI_POLY_UNISON 5u
#define RI_LEVI_POLY_UNISONLO 6u
#define RI_LEVI_POLY_UNISONHI 7u
#define RI_LEVI_POLY_UNISONPOLY 8u
#define RI_LEVI_POLY_N 9u
#define RI_LEVI_MXKEY(sl, f) ((uint16_t)(0x1100u | ((uint32_t)(sl) << 2) | (uint32_t)(f)))
#define RI_LEVI_MRKEY(m, r, f) ((uint16_t)(0x1200u | ((uint32_t)(m) << 5) | ((uint32_t)(r) << 2) | (uint32_t)(f)))
/* Mod envelope / LFO params (block 0x10, P5). */
#define RI_LEVI_MEKEY(e, p) ((uint16_t)(0x1000u | ((uint32_t)(e) << 5) | (uint32_t)(p)))
#define RI_LEVI_LFOKEY(l, p) ((uint16_t)(0x10A0u | ((uint32_t)(l) << 4) | (uint32_t)(p)))
/* Valid param range for a mod env / LFO param; 0 ok, 2 not a param. */
int ri_levi_menv_range(uint32_t param, int *lo, int *hi);
int ri_levi_lfo_range(uint32_t param, int *lo, int *hi);
int ri_levi_menv_default(uint32_t env, uint32_t param);
int ri_levi_lfo_default(uint32_t param);
const char *ri_levi_lfo_wave_name(uint32_t w);
/* LFO rate in Hz for a speed range (0 slow 0..25, 1 fast 5..150) and UI value. */
float ri_levi_lfo_hz(uint32_t fast, uint8_t ui);
/* Envelope value 0..1 x level (render truth; tests). */
float levi_menv_value(const struct RILeviVoice *v, uint32_t env);
/* Digital model name (own, upper case, never NULL). */
const char *ri_levi_df_name(uint32_t t);

struct RILeviSet {
    struct RILeviVoice v[RI_LEVI_NVOICES];
    uint8_t arpon;   /* device arp gate (v2 feature 3; UI/automation truth) */
    uint8_t arprate; /* device arp rate 0..127 */
    uint8_t seqon;   /* device seq gate (v2 feature 3; UI/automation truth) */
    uint8_t seqlen;  /* device seq length 1..16 */
    struct RILeviMatrix mx; /* device matrix program (v2 feature 4) */
    uint8_t bias[4];        /* env level, attack, decay, release; 64 = 0 (voices hold the floats) */
    struct RILeviLFO glfo[RI_LEVI_NLFO]; /* shared LFOs (trig sync single / off, P5) */
    /* Voice allocator (fidelity P6a, manual pp. 87-96): device-wide.
     * Direct levi_trigger/release stay lane==voice for songs (bit-identical);
     * live notes go through levi_note_on/off below. */
    uint8_t polymode;       /* RI_LEVI_POLY_* */
    uint8_t udensity;       /* stacked voices per note 1..8 (UnisonPoly) */
    uint8_t ulimit;         /* poly voice cap 1..6 (Unison, UnisonPoly) */
    uint8_t arot;           /* rotate cursor */
    uint8_t anotes[16];     /* held notes in arrival order */
    uint8_t an;             /* held count */
    uint8_t apad[2];
};

/* Matrix route / macro route fields (P5b), 7-bit UI values. 0 ok, 2 bad. */
int levi_set_mx_ui(struct RILeviSet *s, uint32_t slot, uint32_t field, uint8_t val);
int levi_set_mr_ui(struct RILeviSet *s, uint32_t macro, uint32_t route, uint32_t field, uint8_t val);
/* Mod envelope / LFO UI params (P5), 0..127 clamped to the param. 0 ok, 2 bad. */
int levi_set_menv_ui(struct RILeviSet *s, uint32_t voice, uint32_t env, uint32_t param, uint8_t val);
int levi_set_lfo_ui(struct RILeviSet *s, uint32_t voice, uint32_t lfo, uint32_t param, uint8_t val);

void levi_init_set(struct RILeviSet *s);
/* Trigger (note 0..127) / release a voice. Returns 0 ok, 2 bad. */
int levi_trigger(struct RILeviSet *s, uint32_t voice, uint8_t note);
void levi_release(struct RILeviSet *s, uint32_t voice);
/* Voice allocator (fidelity P6a): live note on/off through the poly mode.
 * Returns the number of voices applied, -1 on bad set/note/NULL.
 * Direct levi_trigger/release above stay lane==voice (songs, bit-identical). */
int levi_note_on(struct RILeviSet *s, uint8_t note);
int levi_note_off(struct RILeviSet *s, uint8_t note);
/* Allocator mode/density/limit UI (keys 0x0E58..5A, device-wide). 0 ok, 2 bad. */
int levi_set_alloc_ui(struct RILeviSet *s, uint32_t mode);
uint32_t levi_alloc_mode(const struct RILeviSet *s);
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
/* Algorithm select (preset 0..63; 64/custom is readable, not settable).
 * Returns 0 ok, 2 bad. Selecting a preset replaces custom routing. */
int levi_set_algo(struct RILeviSet *s, uint32_t voice, uint32_t algo);
/* Current algorithm id (0..8); negative on bad voice/NULL. */
int levi_algo_get(const struct RILeviSet *s, uint32_t voice);
/* Custom routing (v1 single-target form): op feeds src (target op) or -1 to the mix;
 * the voice becomes custom. Acyclic only: src's forward chain must not
 * reach op (cycles/self-routes refused, returns 2, routing unchanged).
 * Returns 0 ok, 2 bad. */
int levi_set_route(struct RILeviSet *s, uint32_t voice, uint32_t op,
    int src);
/* Routing read: lowest target op index, -1 carrier; -2 on bad voice/op/NULL. */
int levi_route_get(const struct RILeviSet *s, uint32_t voice,
    uint32_t op);
/* Algorithm mode (RI_LEVI_AMODE_*), morph slot i (preset, SILENCE,
 * OFF; slot 0 is always a preset) and morph position (0 .. 100 x
 * (active slots - 1)). Returns 0 ok, 2 bad. */
int levi_set_amode(struct RILeviSet *s, uint32_t voice, uint32_t mode);
int levi_set_slot(struct RILeviSet *s, uint32_t voice, uint32_t slot, uint32_t val);
int levi_set_mpos(struct RILeviSet *s, uint32_t voice, uint32_t pos);
/* Active morph slots (1..8) and the op-feeds mask of a preset. */
uint32_t levi_morph_slots(const struct RILeviSet *s, uint32_t voice);
uint8_t ri_levi_preset_feeds(uint32_t algo, uint32_t op);
/* Custom grid: op modulates the ops in mask (acyclic only; 2 refused).
 * Selects Custom mode. */
int levi_set_feeds(struct RILeviSet *s, uint32_t voice, uint32_t op, uint32_t mask);
/* Morph target select (preset 0..63) + blend position 0..100. Bank-B
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
/* Per-oscillator parameter (fidelity P2): UI value in the param's
 * range (ri_levi_op_range). Returns 0 ok, 2 bad voice/op/param/NULL. */
int levi_set_op_ui(struct RILeviSet *s, uint32_t voice, uint32_t op, uint32_t param, uint8_t val);
/* UI range of a per-op param (lo..hi, both <= 127); 2 on bad param. */
int ri_levi_op_range(uint32_t param, int *lo, int *hi);
/* UI default of a per-op param for op (op 0 sounds, 1..7 modulate). */
int ri_levi_op_default(uint32_t op, uint32_t param);
/* Envelope segment time in seconds for a UI value (param = one of the
 * time params, speed 0 fast / 1 slow). Pure. */
float ri_levi_env_time(uint32_t param, uint8_t val, uint32_t speed);
/* Ratio for a ratio-mode COARSE index 0..65 (0.25, 0.5, 1..64). */
float ri_levi_ratio(uint8_t idx);
/* Own wave names ("SINE", "PULSE 3", ...); "" on bad index. */
const char *ri_levi_wave_name(uint32_t wave);
/* One sample of wave w at phase 0..1 (dt = cycles per sample, for the
 * band-limit corrections). Pure; exact ri_sin for w = 0. */
float ri_levi_wave(uint32_t w, float phase, float dt);
/* LFO UI map (0..127 -> 0.01..30 Hz exp). Pure. */
float ri_levi_lfo_rate(uint8_t ui);
/* Advance one LFO a sample (wraps phase 0..1); returns its value
 * (wave, level, quantize, smooth, delay/fade). A shared LFO (trig sync
 * single/off) returns its value unchanged: the set steps it. 0.0f on bad. */
float ri_levi_lfo_step(struct RILeviLFO *l, float sr);

#endif
