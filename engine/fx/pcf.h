/* pcf.h — PCF 12 dB SVF + attack/decay envelope (§12.8c1) + ledger-gated
 * response table (Task 10, gate G10).
 * Spec §12 (demoted black-box plan) + Appendix A P-15 + Appendix D sketch.
 *
 * locked requirements regardless of route: 12 dB SVF engine shape
 * (candidate, textbook Chamberlin two-pole form — the register carries no
 * PCF-specific prior-art row, so no lineage is claimed), integer sample
 * clock (pos_smp; the step index is a pure function of samples rendered —
 * never drifts), pattern retrigger (neutral: every 16th at velocity 64
 * until per-pattern rows lock), fixed-seed determinism (zero init, no RNG),
 * 16th-grid stepping, 54-tile picker UI (count constant only). No HP mode
 * (ReBirth has LP/BP only; mode 2 maps to band).
 *
 * The 54x16 pattern contents are UNVERIFIED (OPEN-04): no implementation
 * may hardcode them as fact. Until per-pattern ledger rows lock, every
 * pattern step reads the explicit neutral placeholder (64) via
 * pcf_pattern_step — a refusal to claim, not a claim. The engine reads
 * response rows from reference/pcf-table.bin (pcf_table_load); the test
 * reads the SAME file as oracle (single source). P-15 law (HYPOTHESIS,
 * locks at TC-2.5.2): fc = base * 2^(((v-64)/64)*amt), amt within ±4 oct,
 * Q 0.7..8.
 *
 * Table load is init-time (control plane) only; pcf_render performs no
 * allocation, no IO, no unbounded loops. Kernels ri_* only; no libm.
 * Exact C signatures below are executor-defined (spec carries interface
 * sketches only, NOT frozen ABI).
 */
#ifndef RI_PCF_H
#define RI_PCF_H
#include <stdint.h>

#define RI_PCF_NPATTERNS 55u
#define RI_PCF_TABLE_MAX 64u
#define RI_PCF_TABLE_VERSION 1u
/* Pattern steps per row (max 32) and resolution codes. */
#define RI_PCF_PSTEPS_MAX 32u
#define RI_PCF_RES_16TH 0u
#define RI_PCF_RES_32ND 1u
#define RI_PCF_RES_8TH 2u
#define RI_PCF_NSTEPS 16u
#define RI_PCF_TABLE_MAX 64u
#define RI_PCF_TABLE_VERSION 1u
#define RI_PCF_Q_MIN 0.7f
#define RI_PCF_Q_MAX 8.0f
#define RI_PCF_AMT_MAX 4.0f
/* Neutral step value while pattern rows are unverified (OPEN-04). */
#define RI_PCF_STEP_NEUTRAL 64u
/* Chamberlin stability bound (Appendix B): fc never reaches fs/6. */
#define RI_PCF_FC_MIN_HZ 10.0f

/* One ledger-verified response row (binary layout of pcf-table.bin). */
struct PCFTableRow {
    uint8_t v;
    int8_t amt;
    uint8_t pad[2];
    float base_fc;
    float expected_fc;
};

struct PCFTable {
    struct PCFTableRow rows[RI_PCF_TABLE_MAX];
    uint32_t n;
};

/* One extracted Appendix-D pattern (E1; see docs/evidence/pcf/patterns.md).
 * vel holds per-step velocities 0..127 (0 = rest); only length entries
 * are meaningful. res selects the step grid (16th/32nd/8th). */
struct PCFPattern {
    uint8_t res;
    uint8_t length;
    uint8_t vel[RI_PCF_PSTEPS_MAX];
};

/* Installed pattern set (caller-owned, e.g. loaded from
 * reference/pcf-patterns.bin; NULL = neutral legacy sustain). */
struct PCFPatterns {
    struct PCFPattern pat[RI_PCF_TABLE_MAX];
    uint32_t n;
};

struct PCFSVF {
    float low;
    float band;
};

struct PCF {
    struct PCFSVF svf;
    uint8_t pattern; /* 0..53 stored state (spec §2.3 item 3) */
    uint8_t mode; /* 0 = low, 1 = band (2 maps to band: no HP in ReBirth) */
    uint8_t pad[2];
    float base_fc;
    float q;
    float amt_oct; /* ±4 response amount (P-15) */
    float bpm; /* transport clock rate */
    uint64_t pos_smp; /* integer sample clock (§12.8c1: never drifts) */
    uint32_t last_step; /* last pattern-step rendered (hit detection) */
    float env; /* attack/decay envelope, velocity units 0..127 */
    float decay; /* envelope decay tau, s (Decay knob) */
    const struct PCFPatterns *ptab; /* installed patterns (NULL = neutral) */
};

/* Load + validate the ledger-verified table. Returns row count, else
 * negative: -1 IO, -2 magic, -3 version, -4 count, -5 row range. */
int pcf_table_load(const char *path, struct PCFTable *t);
/* P-15 response law via ri_pow2 (v clamped 0..127, amt ±4, base > 0). */
float pcf_cutoff_hz(float base_fc, int v, float amt_oct);
/* Pattern step on the 16th grid. UNVERIFIED: returns RI_PCF_STEP_NEUTRAL
 * for every (pattern, step) until per-pattern ledger rows lock. */
uint8_t pcf_pattern_step(uint8_t pattern, uint32_t step16);
/* Load + validate the extracted pattern set (magic PCFP, version 1).
 * Returns pattern count, else negative: -1 IO, -2 magic, -3 version,
 * -4 count, -5 row range. Mirrors pcf_table_load's contract. */
int pcf_patterns_load(const char *path, struct PCFPatterns *t);
/* Install (or uninstall with NULL) the pattern set. Neutral behavior
 * without a table is unchanged (sustain). */
void pcf_install_patterns(struct PCF *p, const struct PCFPatterns *t);
/* Velocity at a pattern step (0 = rest/out of range/no table installed
 * rows beyond length read 0 — the wrap lives in render). */
uint8_t pcf_pattern_vel(const struct PCFPatterns *t, uint32_t pattern,
    uint32_t pstep);
void pcf_init(struct PCF *p);
void pcf_set_tempo(struct PCF *p, float bpm);
/* Decay knob 0..127 -> tau 0.05*2^((v-64)/16) s (3 ms .. 0.8 s, E0). */
void pcf_set_decay(struct PCF *p, uint8_t v);
/* Transport restart: clock + envelope reset, SVF + params preserved. */
void pcf_restart(struct PCF *p);
/* 16th step index at an absolute sample position (pure function: the
 * integer clock — exact at any horizon, never drifts). */
uint32_t pcf_step_index(uint64_t pos_smp, float bpm, float sr);
void pcf_render(struct PCF *p, const float *in, float *out, uint32_t n,
    float sr);
#endif
