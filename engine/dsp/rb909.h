/* rb909.h — 909 layered sampler (Task 9, gate G9).
 * Spec §11 + Appendix C skeleton (E0 candidate, HYPOTHESIS P-13..P-15):
 *   909 layer voice: sample = SUM_layers lerp(data).triangular_weight(tune);
 *   pos += rate.pitch_mult/sr;
 *   pitch_mult, layer_weight, and accent/flam paths are separate
 *   definitions (P-13..P-15) — never conflated (see accessors below).
 *
 * Prior-art model (register entry 14, E0+E2): the reference is a hybrid —
 * analog dual-oscillator layers plus 6-bit companded ROM layers. Tune acts
 * as sample clock (rb909_pitch_mult), accent as VCA scale
 * (rb909_accent_gain), crash/ride decay shortens with Tune
 * (rb909_decay_scale, extra envelope on CR/RD only), and open/closed hats
 * share one ROM so they cannot sound together (voice-steal rule:
 * rb909_trigger on CH kills OH and vice versa).
 * Accent model [HYPOTHESIS]: none x1.0; acc1 accent-layers x1.15 + shelf;
 * acc2 flam on capable voices else = acc1; crash/ride accent-no-op quirk
 * (CR/RD ignore accent gain). Monophonic retrigger (trigger resets both
 * playheads). Idle-only mod swap (rb909_set_layers refuses while active).
 * Clean-room rule (entry 16, E5): shipped layers are synthesized from
 * scratch (sine/noise recipes, reference/packs/classic-01/); the engine
 * holds non-owning pointers and never samples anything itself.
 *
 * Kernels ri_* only; no allocation; no libm; bounded loops.
 * Exact C signatures below are executor-defined (spec carries interface
 * sketches only, NOT frozen ABI).
 */
#ifndef RI_RB909_H
#define RI_RB909_H
#include <stdint.h>

#define RI_909_NVOICES 6u

/* Voice ids, classic order subset shipped in the clean pack. */
#define RB909_BD 0u
#define RB909_SD 1u
#define RB909_CH 2u
#define RB909_OH 3u
#define RB909_CR 4u
#define RB909_RD 5u

#define RI_909_MAX_LAYERS 4u
/* P-13: layer interpolation is a triangular crossfade whose feather spans
 * ~8 knob positions (4 each side of a lo/hi boundary). */
#define RI_909_XFADE_HALF 4u
/* P-14: acc1 = accent-layers x1.15 + shelf (shelf = small HF lift, the
 * test band x1.15 +/-0.2 dB admits it); crash/ride accent-no-op quirk. */
#define RI_909_ACC1_GAIN 1.15f
/* P-05: flam second hit x0.75 at the scheduler flam delay (35 ms nominal,
 * value arrives in samples; default below is 35 ms @ 48 kHz). */
#define RI_909_FLAM_GAIN 0.75f
#define RI_909_FLAM_DEFAULT_SMP 1680
/* Idle-only swap refusal + trigger fade-in (click-free idle swap). */
#define RI_909_BUSY 1
#define RI_909_BADARG 2
#define RI_909_TRIG_RAMP 32u

/* One baked layer. data is a non-owning pointer to frames mono f32
 * samples at rate Hz; the voice never writes through it. lo..hi is the
 * tune span (0..127) the layer covers; accent_layer marks layers the
 * acc1 gain applies to (all pack layers carry 1 except CR/RD layers). */
struct RISampleLayer {
    const float *data;
    uint32_t frames;
    uint32_t rate;
    uint8_t lo;
    uint8_t hi;
    uint8_t accent_layer;
    uint8_t pad[2];
};

struct RB909Voice {
    uint8_t id; /* RB909_* */
    uint8_t tune; /* 0..127 knob */
    uint8_t accent; /* 0 none, 1 acc1, 2 acc2(flam or =acc1) */
    uint8_t active; /* nonzero from trigger until both playheads end */
    float pos; /* main playhead, layer-frame domain at 48 kHz ref */
    float pos2; /* flam second playhead, <0 when inactive */
    float flam_delay; /* second-hit delay, layer-frame domain */
    float shelf_lp; /* accent shelf one-pole state */
    uint32_t age; /* samples since trigger (fade-in ramp) */
    const struct RISampleLayer *layers; /* non-owning, idle-swap only */
    uint8_t n_layers;
    uint8_t flam_capable; /* BD/SD: acc2 = flam; else acc2 = acc1 */
    uint8_t accent_noop; /* CR/RD quirk: accent gain pinned to 1.0 */
    uint8_t pad;
};

struct RB909Set {
    struct RB909Voice v[RI_909_NVOICES];
};

/* Voice short names for goldens/logs ("bd", "sd", ...). */
const char *rb909_name(uint32_t voice);

void rb909_init_set(struct RB909Set *s);
/* Install layer maps (idle-only: returns RI_909_BUSY while the voice is
 * active, RI_909_BADARG on n==0/n>4/NULL/short frames/lo>hi, else 0).
 * The set copies the descriptors, NOT the sample data. */
int rb909_set_layers(struct RB909Set *s, uint32_t voice,
    const struct RISampleLayer *layers, uint32_t n);
/* Monophonic trigger: resets both playheads (retrigger cuts previous),
 * stores tune/accent, applies the shared-hat-ROM steal rule (CH<->OH).
 * flam_delay_smp = scheduler second-hit delay in output samples
 * (RI_EV_FLAM value domain); pass RI_909_FLAM_DEFAULT_SMP when the
 * scheduler did not emit one. */
void rb909_trigger(struct RB909Set *s, uint32_t voice, uint32_t accent,
    uint8_t tune, int32_t flam_delay_smp);

/* Split Tune model, three separate definitions (spec §11): */
/* knob -> playback rate (sample clock): 2^((tune-64)/48). */
float rb909_pitch_mult(uint8_t tune);
/* knob -> layer blend: triangular feather, RI_909_XFADE_HALF each side
 * of the lo/hi boundary (P-13); un-normalized single-layer weight. */
float rb909_layer_weight(uint8_t tune, uint8_t lo, uint8_t hi);
/* accent -> VCA scale (P-14): 1.0 / 1.15 / (flam voices: flam path,
 * else 1.15); CR/RD quirk voices always 1.0. */
float rb909_accent_gain(const struct RB909Voice *v);
/* Tune -> extra decay scale for crash/ride only (1.0 elsewhere):
 * 2^(-(tune-64)/48), applied as an extra exp envelope. */
float rb909_decay_scale(uint8_t voice, uint8_t tune);

/* Sampler utils (same TU): linear-interpolated fetch (past-end reads 0)
 * and the normalized triangular layer mix at pos (48 kHz ref domain;
 * per-layer rate scaling is applied inside). Normalization is
 * equal-power (u_i = sqrt(w_i/wsum) via bounded Newton arithmetic, no
 * libm): the P-13 RMS-continuity mechanism; single-layer output is
 * identical to sum normalization. */
float ri_resample_linear(const float *d, uint32_t n, float pos);
float ri_layer_mix(const struct RISampleLayer *L, uint32_t n,
    uint8_t tune, float pos);

/* One sample from a single voice (advances playheads by pitch_mult). */
float rb909_voice_render(struct RB909Voice *v, float sr);
/* Sum of all active voices. */
void rb909_render_mix(struct RB909Set *s, float *out, uint32_t n, float sr);
#endif
