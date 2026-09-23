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
#include "engine/dsp/rb808.h"
#include "engine/dsp/rb909.h"

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

/* --- 808 control curves (WBS interface line 81; Module 2.3) --- *
 * DECAY: amplitude tau seconds 0.18..2.8, exponential feel (TC-2.3.2:
 * 0.18 s at knob 0 .. 2.8 s at knob 127, both +-10%; knob 64 = 0.50 s, the
 * exact BD ledger default). */
static const float RI_808_DECAY_TBL[RI_N_ANCHOR] = {
    0.18f, 0.22f, 0.28f, 0.38f, 0.50f, 0.72f, 1.10f, 1.80f, 2.80f
};

void rb808_set_param(struct RB808Voice *v, uint32_t ctl_id, uint8_t value) {
    switch (ctl_id) {
    case RI_CTL_808_DECAY:
        v->tau_amp = interp9(RI_808_DECAY_TBL, value);
        break;
    case RI_CTL_808_TUNE:
        /* +/-7 st across the knob (0 -> -7, 127 -> +7); engine clamps. */
        v->tune_st = ((float)value - 64.0f) * 14.0f / 127.0f;
        break;
    case RI_CTL_808_LEVEL:
        /* Placeholder: no engine gain field yet (auto/panel trim); the
         * ledger rows keep EXCITE as the only 808 level control. */
        break;
    case RI_CTL_808_SNAPPY:
    case RI_CTL_808_TONE:
        /* Placeholder: no engine field for either (P-10 E0 keeps fixed
         * 2 ms clap burst width / fixed cluster HP). */
        break;
    case RI_CTL_808_ACCENT:
        /* Placeholder: trigger-time binary (rb808_trigger accent arg);
         * a panel 0..127 knob is not wired into the engine state. */
        break;
    default:
        break; /* unknown control: ignored by the voice */
    }
}

/* --- 909 control curves (WBS interface line for Module 2.4) ---
 * TUNE writes the engine tune byte directly: the panel knob domain
 * 0..127 IS the voice tune domain (rb909_pitch_mult / layer_weight
 * read it raw), so no curve table is needed. LEVEL/DECAY/FLAMRES are
 * documented placeholders (TC-2.4 pins the engine behaviour; the
 * panel path only needs a total, honest entry point). */
void rb909_set_param(struct RB909Voice *v, uint32_t ctl_id, uint8_t value) {
    switch (ctl_id) {
    case RI_CTL_909_TUNE:
        v->tune = value;
        break;
    case RI_CTL_909_LEVEL:
        /* Placeholder: no engine gain field (voice VCA is the accent
         * path rb909_accent_gain; static level lives in the mixer). */
        break;
    case RI_CTL_909_DECAY:
        /* Placeholder: decay is baked into the layers; the only
         * tune-driven decay (CR/RD rb909_decay_scale) derives from
         * TUNE, not from a second knob. */
        break;
    case RI_CTL_909_FLAMRES:
        /* Placeholder: flam delay is trigger-time (scheduler domain,
         * rb909_trigger flam_delay_smp arg), not voice state. */
        break;
    default:
        break; /* unknown control: ignored by the voice */
    }
}
