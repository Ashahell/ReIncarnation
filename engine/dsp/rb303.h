/* rb303.h — 303 voice + Appendix B candidate filter (Task 4, gate G4).
 * Spec §9 chain: oscillator -> envelope -> VCA -> nonlinear filter -> output.
 * All numeric defaults are Appendix A hypotheses (P-01..P-06), NOT contracts.
 * Control IDs live in the 0x030x intent block (spec §13); knob 0..127 curves
 * are owned by engine/dsp/params.c. Exact C signatures below are executor-
 * defined (the spec carries them as interface sketch only, NOT frozen ABI).
 */
#ifndef RI_RB303_H
#define RI_RB303_H
#include <stdint.h>

/* 303A control IDs [E0 decision, Task 4] */
#define RI_CTL_303A_CUTOFF 0x0300u
#define RI_CTL_303A_RESO   0x0301u
#define RI_CTL_303A_ENVMOD 0x0302u
#define RI_CTL_303A_DECAY  0x0303u
#define RI_CTL_303A_ACCENT 0x0304u
#define RI_CTL_303A_WAVE   0x0305u /* 0 = saw, else square */
#define RI_CTL_303A_VOLUME 0x0306u
#define RI_CTL_303A_TUNE   0x0307u /* semitones, value - 64, clamped ±24 */

/* 303B control IDs (§12.2: same layout, section block 0x031x; the dispatch
 * normalizes to 303A so both sections share one implementation). */
#define RI_CTL_303B_CUTOFF 0x0310u
#define RI_CTL_303B_RESO   0x0311u
#define RI_CTL_303B_ENVMOD 0x0312u
#define RI_CTL_303B_DECAY  0x0313u
#define RI_CTL_303B_ACCENT 0x0314u
#define RI_CTL_303B_WAVE   0x0315u /* 0 = saw, else square */
#define RI_CTL_303B_VOLUME 0x0316u
#define RI_CTL_303B_TUNE   0x0317u /* semitones, value - 64, clamped ±24 */

/* MEG/VEG envelope laws (E1 Open303-lineage starting values, spec §10-area
 * lineage list; M2.2 A/B decides. Devil-Fish tension — VEG fixed 3–4 s —
 * recorded for the measurement pass, see the §12.4b article). */
#define RI_303_MEG_ACC_TAU 0.2f /* MEG decay on accented notes (minimum) */
#define RI_303_VEG_TAU 1.23f /* VEG fixed decay, normal notes */
#define RI_303_VEG_ACC_TAU 0.2f /* VEG decay on accented notes */
#define RI_303_REL_TAU 0.0005f /* release, normal notes (0.5 ms) */
#define RI_303_REL_ACC_TAU 0.05f /* release, accented notes (50 ms) */

struct RB303Voice {
    /* Parameters (rb303_set_param curves, or direct setters for tests). */
    float cutoff_hz;  /* fc_base, Hz */
    float reso_k;      /* 0..~3.8 (P-04) */
    float env_mod;     /* octaves of env -> fc */
    float decay_tau;   /* s, 0.08..4 (P-06) */
    float accent_amt;  /* 0..1 */
    float slide_tc;    /* s; default 0.040 (P-03 tension: 40 vs 60, M2.2 A/B) */
    float volume;      /* 0..1 linear */
    float tune_st;     /* semitones off center, ±24 (§12.2; ReBirth Tune) */
    int wave_square;   /* 0 = saw, 1 = square */
    int classic_click; /* 1 = hard wave switch with deterministic click
                        * (classic behavior, default); 0 = 0.5 ms
                        * crossfade on switch (TC-2.2.5) */
    int wave_rendered; /* shape currently rendered (crossfade source) */
    uint32_t xfade_n;  /* crossfade progress samples */
    /* Ladder + chain state (all zero at init). */
    float s0, s1, s2;
    float fb_lp;       /* feedback-loop HPF (150 Hz) lowpass state */
    float post_lp1;    /* post HP 44.486 Hz lowpass state */
    float post_lp2;    /* post HP 24.167 Hz lowpass state */
    float phase;       /* VCO phase 0..1 */
    float freq;        /* slewed freq, Hz (== pow2(logfreq) while gliding) */
    float target_freq; /* slide target, Hz */
    float logfreq;     /* log2(freq): the glide state (RC on pitch CV) */
    float log_target;  /* log2(target_freq) */
    float meg;         /* filter (MEG) envelope 0..1: Decay knob, min on accent */
    float veg;         /* amp (VEG) envelope 0..1: fixed long decay */
    float sweep;       /* accent-sweep state: saturating buildup, reso lag */
    int accented;      /* nonzero when the current note is accented */
    float accent_env;  /* accent amount envelope 0..1 (P-02, 60 ms) */
    float gate;        /* 1 = gate high (sustain/decay), 0 = release ramp */
    float prev_in;     /* oversample interpolation memory */
};

void rb303_init(struct RB303Voice *v);
void rb303_set_param(struct RB303Voice *v, uint32_t ctl_id, uint8_t value);
void rb303_set_cutoff_hz(struct RB303Voice *v, float fc); /* test/scaffold direct */
void rb303_set_reso(struct RB303Voice *v, float k);       /* test/scaffold direct */
/* One Appendix B core-ladder step (2x substeps inside). Test stimulus point. */
float rb303_filter_step(struct RB303Voice *v, float in, float sr);
void rb303_note(struct RB303Voice *v, uint8_t midi, int slide, int accent);
void rb303_slide_to(struct RB303Voice *v, uint8_t midi); /* CONTINUE: target only */
void rb303_accent(struct RB303Voice *v);                 /* ACCENT event: env only */
void rb303_release(struct RB303Voice *v);
void rb303_render(struct RB303Voice *v, float *out, uint32_t n, float sr);
#endif
