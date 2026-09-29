/* reverb.h — algorithmic reverb core (v2 feature 5a, owner order).
 * Own network (clean-room: own code, own delay lengths/gains): four
 * parallel combs into two series allpasses, wet-only output. Caller
 * owns the six delay lines (non-owning, no allocation — RIDelay
 * precedent). Decay knob 0..127 -> uniform comb feedback 0..0.85;
 * decay 0 is exact silence (bypass law). Kernels ri_* only.
 */
#ifndef RI_REVERB_H
#define RI_REVERB_H
#include <stdint.h>

/* Own delay lengths (samples @48 kHz; scaled by sr/48000 at init).
 * Chosen spread-out odd values, not copied from any published set. */
#define RI_REV_C0 1123u
#define RI_REV_C1 1201u
#define RI_REV_C2 1291u
#define RI_REV_C3 1361u
#define RI_REV_A0 401u
#define RI_REV_A1 271u
#define RI_REV_MIN_CAP 2048u /* lines must hold the longest tap + margin */

/* Control IDs (0x0Axx FX intent block, after the delay/dist/comp/pcf
 * assignments in fx.h). */
#define RI_FXID_REV_DECAY 0x0A10u /* 0..127 -> feedback 0..0.85 */

struct RIReverb {
    float *lc[4];  /* comb lines (non-owning) */
    float *la[2];  /* allpass lines (non-owning) */
    uint32_t cap;  /* per-line capacity (samples) */
    uint32_t len[6]; /* scaled tap lengths (C0..C3, A0, A1) */
    uint32_t pos[6]; /* write positions */
    float fb;      /* comb feedback 0..0.85 (decay knob) */
    uint32_t sr;   /* init sample rate */
};

/* Init (clears state, scales taps, decay 0). Lines bound separately
 * so tests own their backing. Returns 0 ok, 2 bad. */
int ri_reverb_init(struct RIReverb *r, uint32_t sr);
/* Bind the six lines (cap each >= RI_REV_MIN_CAP). 0 ok, 2 bad. */
int ri_reverb_lines(struct RIReverb *r, float *c0, float *c1, float *c2,
    float *c3, float *a0, float *a1, uint32_t cap);
/* Decay knob 0..127. 0 ok, 2 bad. */
int ri_reverb_set_decay(struct RIReverb *r, uint8_t decay);
/* Render one wet sample. 0.0f on bad args/unbound. */
float ri_reverb_render(struct RIReverb *r, float in);
/* Clear lines + positions (keeps knobs). NULL-safe no-op. */
void ri_reverb_reset(struct RIReverb *r);

#endif
