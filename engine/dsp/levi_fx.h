/* levi_fx.h — Levi per-device FX chain (fidelity P7, manual pp. 83-86).
 * Own algorithms throughout (clean-room: no ASM content). The chain is
 * per-device (one instance in RILeviSet, after the voice sum): Pre-FX
 * and Post-FX engines arrive in P7c, the full reverb in P7b; P7a ships
 * the delay with the bypass/dry-wet framework. No allocation anywhere
 * (static lines, cleared in levi_init_set); ri_* kernels only.
 */
#ifndef RI_LEVI_FX_H
#define RI_LEVI_FX_H
#include <stdint.h>
#include "engine/dsp/levi_matrix.h"  /* DM_DELAY destination ids */

/* Delay types (own). */
#define RI_LEVI_DT_CLEAN 0u
#define RI_LEVI_DT_ANALOG 1u
#define RI_LEVI_DT_TAPE 2u
#define RI_LEVI_DT_PINGPONG 3u
#define RI_LEVI_DT_N 4u

/* Delay line capacity: 2 s at 48 kHz per channel (E0). */
#define RI_LEVI_DLY_MAX 96000u

/* Matrix destination params for DM_DELAY live in levi_matrix.h
 * (RI_LEVI_DD_*), next to the other destination groups. */

struct RILeviFx {
    float dl[RI_LEVI_DLY_MAX]; /* delay lines L/R (device pair) */
    float dr[RI_LEVI_DLY_MAX];
    uint32_t dpos;      /* shared write position */
    uint8_t dtype;      /* RI_LEVI_DT_* */
    uint8_t dbypass;    /* 1 = exact dry */
    uint8_t dbpm;       /* BPM sync flag (stored; live with the P8 clock) */
    uint8_t dpad;
    float dtime;        /* seconds, 0.001..2 */
    float dfb;          /* 0..0.95 feedback */
    float dwtone;       /* Hz wet lowpass */
    float dfbtone;      /* Hz loop lowpass */
    float ddrywet;      /* 0..1 wet */
    float wl, wr;       /* wet-tone one-pole states */
    float fl, fr;       /* loop-tone states */
    float wphase;       /* tape wow phase */
    float dfxm[RI_LEVI_DD_N]; /* DM_DELAY offsets (lead voice) */
    uint8_t dfxm_on;
    uint8_t fxpad[3];
};

/* Delay time in seconds for a UI value (0..127 -> 1 ms..2 s). Pure. */
float ri_levi_delay_time(uint8_t ui);
/* One stereo sample through the device delay (bypass = exact dry).
 * dfxm (matrix) folds first when dfxm_on. lenne = line length bound. */
void levi_fx_delay(struct RILeviFx *f, float sr, float in_l, float in_r,
    float *out_l, float *out_r);
/* Clear lines + states (keeps knobs). NULL-safe no-op. */
void levi_fx_clear(struct RILeviFx *f);

#endif
