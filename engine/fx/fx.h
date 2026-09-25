/* fx.h — Delay / Distortion / Compressor + generic RIFX wrapper
 * (Task 10, gate G10). Spec §13 (FX: parameter mapping, processing
 * equation, state init, latency, interpolation, sample-rate behavior,
 * regression fixture) + Appendix D sketch names.
 *
 * Delay: BPM-sync (delay_smp = round(beats*60*sr/bpm)), feedback +
 * mix, caller-owned line buffer (non-owning, no allocation).
 * Distortion: asymmetric tanh (drive + shape), 2x oversampled (§12.8:
 * linear-mid evaluate + box decimate, prev-sample state streams across
 * blocks); drive 0 + shape 0 is an exact bypass so unity holds
 * by construction.
 * Compressor: threshold + ratio (default 4:1, knob 0..127 -> 1..20),
 * attack 10 ms / release 100 ms peak follower, fixed auto make-up
 * premultiplied at set time
 * (MU_lin = 2^(-thresh_dB*(1-1/R)*log2(10)/20) via ri_pow2),
 * plus a gain-reduction meter (block min gain in dB, read/reset).
 * Every knob is 0..127; all state init explicit; latency: delay =
 * delay_smp, dist/comp = 0. No allocation; no IO; kernels ri_* only.
 * Exact C signatures below are executor-defined (spec carries interface
 * sketches only, NOT frozen ABI).
 */
#ifndef RI_FX_H
#define RI_FX_H
#include <stdint.h>
#include "engine/fx/pcf.h"

/* Generic wrapper types (Appendix D fx_type domain). */
#define RI_FX_DELAY 0u
#define RI_FX_DIST 1u
#define RI_FX_COMP 2u
#define RI_FX_PCF 3u

/* Control IDs (0x0Axx FX intent block, spec §13). */
#define RI_FXID_DELAY_BEATS 0x0A00u /* 0..3 -> 0.5/0.75/1.0/1.5 beats (steps 2/3/4/6 straight) */
#define RI_FXID_DELAY_STEPS 0x0A0Bu /* 1..32 delay steps (§12.8b1) */
#define RI_FXID_DELAY_TRIPLET 0x0A0Cu /* 0 straight 16ths, nonzero 8th-triplets (§12.8b1) */
#define RI_FXID_DELAY_FB 0x0A01u /* 0..127 -> 0..0.8 */
#define RI_FXID_DELAY_MIX 0x0A02u /* 0..127 -> 0..1 */
#define RI_FXID_DIST_DRIVE 0x0A03u /* 0..127 */
#define RI_FXID_DIST_SHAPE 0x0A04u /* 0..127 */
#define RI_FXID_COMP_THRESH 0x0A05u /* 0..127 -> -40..0 dB */
#define RI_FXID_COMP_RATIO 0x0A0Eu /* 0..127 -> 1..20 (§12.8: was fixed 4:1) */
#define RI_FXID_PCF_BASE 0x0A06u /* 0..127 -> 100..8000 Hz exp */
#define RI_FXID_PCF_Q 0x0A07u /* 0..127 -> 0.7..8 */
#define RI_FXID_PCF_AMT 0x0A08u /* 0..127 -> -4..+4 oct */
#define RI_FXID_PCF_MODE 0x0A09u /* 0..2 low/band/high */
#define RI_FXID_PCF_PATTERN 0x0A0Au /* 0..53 stored state */
#define RI_FXID_PCF_DECAY 0x0A0Du /* 0..127 envelope decay (§12.8c1) */
#define RI_FXCOMP_RATIO 4.0f
#define RI_FXCOMP_ATTACK_S 0.010f
#define RI_FXCOMP_RELEASE_S 0.100f

struct RiFXDelay {
    float *buf; /* non-owning line, capacity cap */
    uint32_t cap;
    uint32_t pos;
    uint32_t delay_smp; /* live tap (slews toward target_smp) */
    uint32_t target_smp; /* retargeted tap (§12.8a; no zipper jumps) */
    float beats; /* musical length, beats (wrapper-owned knob state) */
    uint8_t steps; /* 1..32 delay steps (§12.8b1; BEATS maps onto these) */
    uint8_t triplet; /* nonzero = 8th-triplet steps (1/3 beat each) */
    float fb; /* 0..0.8 */
    float mix; /* 0..1 */
};

struct RiFXDist {
    uint8_t drive; /* 0..127 */
    uint8_t shape; /* 0..127 */
    float prev; /* previous input sample (2x oversample state, §12.8) */
};

struct RiFXComp {
    float env; /* peak follower state */
    float atk_a; /* per-sample attack coeff */
    float rel_a; /* per-sample release coeff */
    float thresh_lin;
    float mu_lin; /* auto make-up, premultiplied */
    float ratio; /* compression ratio (default 4; knob 0..127 -> 1..20) */
    float gr_min_g; /* min compression gain this block (GR meter state) */
    uint8_t thresh; /* 0..127 knob echo */
    uint8_t ratio_knob; /* 0..127 knob echo */
};

/* Generic handle (Appendix D RIFX sketch, executor-defined shape).
 * Fix round 1: the handle OWNS a persistent struct PCF (streaming-safe:
 * SVF state + clock survive across RiFXRender block calls; the render
 * path never re-inits it). §12.8a: delay lines are CALLER-OWNED (no static
 * pool — it leaked by never releasing): RiFXCreateDelay takes the buffer;
 * plain RiFXCreate(RI_FX_DELAY) fails closed (NULL). RiFXDestroy releases
 * the slot for reuse (double-destroy and NULL safe). */
struct RIFX {
    uint32_t type;
    uint8_t busy;
    uint8_t pad[3];
    struct RiFXDelay delay;
    struct RiFXDist dist;
    struct RiFXComp comp;
    struct PCF pcf; /* owned voice: init at create, reused per render */
    uint8_t pcf_pattern;
    uint8_t pcf_mode;
    uint8_t pcf_base; /* knob echoes for the wrapped PCF */
    uint8_t pcf_q;
    uint8_t pcf_amt;
    uint8_t pcf_decay; /* envelope decay knob (§12.8c1) */
    uint8_t pad2[2];
};

/* Delay. buf/cap caller-owned (cap >= 64, sized for the worst case:
 * 32 triplet-8ths at 20 BPM = 32 s — the caller allocates at load time,
 * never on the render path). Returns 0 ok, 2 bad arg. */
int ri_fxdelay_init(struct RiFXDelay *d, float *buf, uint32_t cap);
uint32_t ri_fxdelay_sync(struct RiFXDelay *d, float bpm, float beats,
    float sr);
/* Retarget without jumping (render slews to it); returns the target. */
uint32_t ri_fxdelay_retarget(struct RiFXDelay *d, float bpm, float beats,
    float sr);
void ri_fxdelay_set(struct RiFXDelay *d, uint8_t fb128, uint8_t mix128);
void ri_fxdelay_reset(struct RiFXDelay *d);
void ri_fxdelay_render(struct RiFXDelay *d, const float *in, float *out,
    uint32_t n);

/* Distortion. */
void ri_fxdist_init(struct RiFXDist *d);
void ri_fxdist_set(struct RiFXDist *d, uint8_t drive, uint8_t shape);
void ri_fxdist_reset(struct RiFXDist *d); /* prev=0, knobs preserved */
void ri_fxdist_render(struct RiFXDist *d, const float *in, float *out,
    uint32_t n);

/* Compressor. sr > 0 required at init (coeffs); returns 0 ok, 2 bad. */
int ri_fxcomp_init(struct RiFXComp *c, float sr);
void ri_fxcomp_set(struct RiFXComp *c, uint8_t thresh);
void ri_fxcomp_set_ratio(struct RiFXComp *c, uint8_t ratio128);
void ri_fxcomp_reset(struct RiFXComp *c);
void ri_fxcomp_render(struct RiFXComp *c, const float *in, float *out,
    uint32_t n);
/* Gain-reduction meter: most negative compression gain this block in dB
 * (<= 0; 0 when nothing compressed). gr_reset restarts the block peak. */
float ri_fxcomp_gr_db(const struct RiFXComp *c);
void ri_fxcomp_gr_reset(struct RiFXComp *c);

/* Generic wrapper (Appendix D names, executor-defined shape).
 * RiFXCreate returns 0 on bad type or full pool. RI_FX_DELAY needs a
 * caller-owned line: use RiFXCreateDelay (plain Create fails closed).
 * RiFXDestroy releases the slot (reuse, double-destroy and NULL safe).
 * RiFXValid returns 0 when render-ready, 2 when a DELAY handle has no
 * line buffer (only reachable by construction, never by Create). */
struct RIFX *RiFXCreate(uint32_t fx_type);
struct RIFX *RiFXCreateDelay(float *buf, uint32_t cap);
void RiFXDestroy(struct RIFX *x);
int RiFXValid(const struct RIFX *x);
void RiFXSetParam(struct RIFX *x, uint32_t id, uint8_t value);
void RiFXRender(struct RIFX *x, float *in, float *out, uint32_t frames,
    float sr, float bpm);
void RiFXReset(struct RIFX *x);
#endif
