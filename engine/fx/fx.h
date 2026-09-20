/* fx.h — Delay / Distortion / Compressor + generic RIFX wrapper
 * (Task 10, gate G10). Spec §13 (FX: parameter mapping, processing
 * equation, state init, latency, interpolation, sample-rate behavior,
 * regression fixture) + Appendix D sketch names.
 *
 * Delay: BPM-sync (delay_smp = round(beats*60*sr/bpm)), feedback +
 * mix, caller-owned line buffer (non-owning, no allocation).
 * Distortion: asymmetric tanh (drive + shape), drive 0 + shape 0 is an
 * exact bypass so unity holds by construction.
 * Compressor: fixed 4:1 above threshold, attack 10 ms / release 100 ms
 * peak follower, fixed auto make-up premultiplied at set time
 * (MU_lin = 2^(-thresh_dB*(1-1/4)*log2(10)/20) via ri_pow2).
 * Every knob is 0..127; all state init explicit; latency: delay =
 * delay_smp, dist/comp = 0. No allocation; no IO; kernels ri_* only.
 * Exact C signatures below are executor-defined (spec carries interface
 * sketches only, NOT frozen ABI).
 */
#ifndef RI_FX_H
#define RI_FX_H
#include <stdint.h>

/* Generic wrapper types (Appendix D fx_type domain). */
#define RI_FX_DELAY 0u
#define RI_FX_DIST 1u
#define RI_FX_COMP 2u
#define RI_FX_PCF 3u

/* Control IDs (0x0Axx FX intent block, spec §13). */
#define RI_FXID_DELAY_BEATS 0x0A00u /* 0..3 -> 0.5/0.75/1.0/1.5 beats */
#define RI_FXID_DELAY_FB 0x0A01u /* 0..127 -> 0..0.8 */
#define RI_FXID_DELAY_MIX 0x0A02u /* 0..127 -> 0..1 */
#define RI_FXID_DIST_DRIVE 0x0A03u /* 0..127 */
#define RI_FXID_DIST_SHAPE 0x0A04u /* 0..127 */
#define RI_FXID_COMP_THRESH 0x0A05u /* 0..127 -> -40..0 dB */
#define RI_FXID_PCF_BASE 0x0A06u /* 0..127 -> 100..8000 Hz exp */
#define RI_FXID_PCF_Q 0x0A07u /* 0..127 -> 0.7..8 */
#define RI_FXID_PCF_AMT 0x0A08u /* 0..127 -> -4..+4 oct */
#define RI_FXID_PCF_MODE 0x0A09u /* 0..2 low/band/high */
#define RI_FXID_PCF_PATTERN 0x0A0Au /* 0..53 stored state */

#define RI_FXDELAY_MAX 96000u /* 2 s at 48 kHz */
#define RI_FXCOMP_RATIO 4.0f
#define RI_FXCOMP_ATTACK_S 0.010f
#define RI_FXCOMP_RELEASE_S 0.100f

struct RiFXDelay {
    float *buf; /* non-owning line, capacity cap */
    uint32_t cap;
    uint32_t pos;
    uint32_t delay_smp;
    float fb; /* 0..0.8 */
    float mix; /* 0..1 */
};

struct RiFXDist {
    uint8_t drive; /* 0..127 */
    uint8_t shape; /* 0..127 */
};

struct RiFXComp {
    float env; /* peak follower state */
    float atk_a; /* per-sample attack coeff */
    float rel_a; /* per-sample release coeff */
    float thresh_lin;
    float mu_lin; /* auto make-up, premultiplied */
    uint8_t thresh; /* 0..127 knob echo */
};

/* Generic handle (Appendix D RIFX sketch, executor-defined shape). */
struct RIFX {
    uint32_t type;
    uint8_t busy;
    uint8_t pad[3];
    struct RiFXDelay delay;
    struct RiFXDist dist;
    struct RiFXComp comp;
    uint8_t pcf_pattern;
    uint8_t pcf_mode;
    uint8_t pcf_base; /* knob echoes for the wrapped PCF */
    uint8_t pcf_q;
    uint8_t pcf_amt;
    uint8_t pad2[3];
};

/* Delay. buf/cap caller-owned (cap >= 64). Returns 0 ok, 2 bad arg. */
int ri_fxdelay_init(struct RiFXDelay *d, float *buf, uint32_t cap);
uint32_t ri_fxdelay_sync(struct RiFXDelay *d, float bpm, float beats,
    float sr);
void ri_fxdelay_set(struct RiFXDelay *d, uint8_t fb128, uint8_t mix128);
void ri_fxdelay_reset(struct RiFXDelay *d);
void ri_fxdelay_render(struct RiFXDelay *d, const float *in, float *out,
    uint32_t n);

/* Distortion. */
void ri_fxdist_init(struct RiFXDist *d);
void ri_fxdist_set(struct RiFXDist *d, uint8_t drive, uint8_t shape);
void ri_fxdist_render(struct RiFXDist *d, const float *in, float *out,
    uint32_t n);

/* Compressor. sr > 0 required at init (coeffs); returns 0 ok, 2 bad. */
int ri_fxcomp_init(struct RiFXComp *c, float sr);
void ri_fxcomp_set(struct RiFXComp *c, uint8_t thresh);
void ri_fxcomp_reset(struct RiFXComp *c);
void ri_fxcomp_render(struct RiFXComp *c, const float *in, float *out,
    uint32_t n);

/* Generic wrapper (Appendix D names, executor-defined shape). */
struct RIFX *RiFXCreate(uint32_t fx_type);
void RiFXSetParam(struct RIFX *x, uint32_t id, uint8_t value);
void RiFXRender(struct RIFX *x, float *in, float *out, uint32_t frames,
    float sr, float bpm);
void RiFXReset(struct RIFX *x);
#endif
