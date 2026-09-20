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

struct RB303Voice {
    /* Parameters (rb303_set_param curves, or direct setters for tests). */
    float cutoff_hz;  /* fc_base, Hz */
    float reso_k;      /* 0..~3.8 (P-04) */
    float env_mod;     /* octaves of env -> fc */
    float decay_tau;   /* s, 0.08..4 (P-06) */
    float accent_amt;  /* 0..1 */
    float slide_tc;    /* s; default 0.040 (P-03 tension: 40 vs 60, M2.2 A/B) */
    float volume;      /* 0..1 linear */
    int wave_square;   /* 0 = saw, 1 = square */
    /* Ladder + chain state (all zero at init). */
    float s0, s1, s2;
    float fb_lp;       /* feedback-loop HPF (150 Hz) lowpass state */
    float post_lp1;    /* post HP 44.486 Hz lowpass state */
    float post_lp2;    /* post HP 24.167 Hz lowpass state */
    float phase;       /* VCO phase 0..1 */
    float freq;        /* slewed freq, Hz */
    float target_freq; /* slide target, Hz */
    float env;         /* amp envelope 0..1 (decay while gate high) */
    float accent_env;  /* accent envelope 0..1 */
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
