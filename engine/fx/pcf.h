/* pcf.h — PCF 12 dB SVF + ledger-gated response table (Task 10, gate G10).
 * Spec §12 (demoted black-box plan) + Appendix A P-15 + Appendix D sketch.
 *
 * locked requirements regardless of route: 12 dB SVF engine shape
 * (candidate, textbook Chamberlin two-pole form — the register carries no
 * PCF-specific prior-art row, so no lineage is claimed), free-running
 * transport clock (beat_pos advances every render), fixed-seed
 * determinism (zero init, no RNG), 16th-grid stepping (step index derives
 * from beat_pos), 54-tile picker UI (count constant only).
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

#define RI_PCF_NPATTERNS 54u
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

struct PCFSVF {
    float low;
    float band;
};

struct PCF {
    struct PCFSVF svf;
    uint8_t pattern; /* 0..53 stored state (spec §2.3 item 3) */
    uint8_t mode; /* 0 = low, 1 = band, 2 = high */
    uint8_t pad[2];
    float base_fc;
    float q;
    float amt_oct; /* ±4 response amount (P-15) */
    float bpm; /* free-running transport clock rate */
    float beat_pos; /* 16th-note position, advances every render */
};

/* Load + validate the ledger-verified table. Returns row count, else
 * negative: -1 IO, -2 magic, -3 version, -4 count, -5 row range. */
int pcf_table_load(const char *path, struct PCFTable *t);
/* P-15 response law via ri_pow2 (v clamped 0..127, amt ±4, base > 0). */
float pcf_cutoff_hz(float base_fc, int v, float amt_oct);
/* Pattern step on the 16th grid. UNVERIFIED: returns RI_PCF_STEP_NEUTRAL
 * for every (pattern, step) until per-pattern ledger rows lock. */
uint8_t pcf_pattern_step(uint8_t pattern, uint32_t step16);
void pcf_init(struct PCF *p);
void pcf_set_tempo(struct PCF *p, float bpm);
void pcf_render(struct PCF *p, const float *in, float *out, uint32_t n,
    float sr);
#endif
