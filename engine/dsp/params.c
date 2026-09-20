/* params.c — 303 knob 0..127 -> float curves (Task 4, gate G4).
 * Each curve is an EXPLICIT 9-anchor table (executor-written hypothesis
 * values) with linear interpolation; the table IS the curve, so a future
 * measurement TC re-pastes anchors instead of re-fitting code. Linear
 * control (accent, volume) uses 2-anchor tables for the same reason.
 * Accent placeholder (spec §2.3 item 5) lives here: single gain multiplier
 * applied by the voice, one place, ledger row in filter-candidate.md D7.
 *
 * E0 fader law (Task 11, gate G11; P-17, TC-2.6.1): mixer bus faders,
 * master, and sends share gain = (v/127)^2, implemented as ri_fader_gain
 * in engine/mixer/mixer.c — an exact closed form, NOT a table here:
 * linear interpolation between square-law anchors would sit up to ~6 dB
 * off the square between anchors (v=8: half the v=16 gain vs the true
 * (8/127)^2), so a 9-anchor table cannot honor its own formula. Voice
 * VOLUME above stays a linear trim; the mixer owns loudness. Ledger:
 * docs/evidence/sequencer/fader-law.md.
 */
#include "engine/dsp/rb303.h"

#define RI_N_ANCHOR 9

static const unsigned char RI_KNOB[RI_N_ANCHOR] = { 0, 16, 32, 48, 64, 80, 96, 112, 127 };

/* CUTOFF: fc_base Hz, exponential feel 100..8000 (P-01..P-06 family). */
static const float RI_CUTOFF_TBL[RI_N_ANCHOR] = {
    100.0f, 180.0f, 320.0f, 560.0f, 1000.0f, 1800.0f, 3200.0f, 5600.0f, 8000.0f
};

/* RESO: coefficient 0..3.8 linear (P-04). */
static const float RI_RESO_TBL[RI_N_ANCHOR] = {
    0.0f, 0.475f, 0.95f, 1.425f, 1.9f, 2.375f, 2.85f, 3.325f, 3.8f
};

/* ENVMOD: octaves of env -> fc, 0..2 linear [HYPOTHESIS]. */
static const float RI_ENVMOD_TBL[RI_N_ANCHOR] = {
    0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f
};

/* DECAY: tau seconds 0.08..4.0 exponential (P-06). */
static const float RI_DECAY_TBL[RI_N_ANCHOR] = {
    0.08f, 0.12f, 0.2f, 0.35f, 0.6f, 1.0f, 1.7f, 2.7f, 4.0f
};

/* ACCENT amount 0..1 linear (spec §2.3 item 5 placeholder lives here). */
static const float RI_ACCENT_TBL[RI_N_ANCHOR] = {
    0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 1.0f
};

/* VOLUME 0..1 linear (voice trim; mixer fader law is Task 11, not this). */
static const float RI_VOLUME_TBL[RI_N_ANCHOR] = {
    0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 1.0f
};

/* Linear interpolation over the 9-anchor table. Bounded loop (constant 9),
 * kernels.c style: fixed trip count with an in-range guard. */
static float interp9(const float *tbl, uint8_t value) {
    int s;
    for (s = 0; s < RI_N_ANCHOR - 1; s++) {
        if (value <= RI_KNOB[s + 1] || s == RI_N_ANCHOR - 2) {
            unsigned span = (unsigned)(RI_KNOB[s + 1] - RI_KNOB[s]);
            float t = span ? (float)(value - RI_KNOB[s]) / (float)span : 0.0f;
            return tbl[s] + t * (tbl[s + 1] - tbl[s]);
        }
    }
    return tbl[RI_N_ANCHOR - 1];
}

void rb303_set_param(struct RB303Voice *v, uint32_t ctl_id, uint8_t value) {
    switch (ctl_id) {
    case RI_CTL_303A_CUTOFF:
        v->cutoff_hz = interp9(RI_CUTOFF_TBL, value);
        break;
    case RI_CTL_303A_RESO:
        v->reso_k = interp9(RI_RESO_TBL, value);
        break;
    case RI_CTL_303A_ENVMOD:
        v->env_mod = interp9(RI_ENVMOD_TBL, value);
        break;
    case RI_CTL_303A_DECAY:
        v->decay_tau = interp9(RI_DECAY_TBL, value);
        break;
    case RI_CTL_303A_ACCENT:
        v->accent_amt = interp9(RI_ACCENT_TBL, value);
        break;
    case RI_CTL_303A_WAVE:
        v->wave_square = (value >= 64) ? 1 : 0;
        break;
    case RI_CTL_303A_VOLUME:
        v->volume = interp9(RI_VOLUME_TBL, value);
        break;
    default:
        break; /* unknown control: ignored by the voice; tools/render
                * rejects bad song lines loudly, so nothing fails silent */
    }
}
