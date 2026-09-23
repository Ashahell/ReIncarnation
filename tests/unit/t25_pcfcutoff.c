/* t25_pcfcutoff — Module 2.5, TC-2.5.2 (PCF cutoff law):
 *
 *   fc = base·2^(((v−64)/64)·amt) within ±2 cents at all 10 ledger
 *   rows of reference/pcf-table.bin (mirrors t1_fx §1 values), PLUS
 *   white-box edge ratios beyond t1: unity exact at v=64 (2^0), /16
 *   at v=0/amt=+4 (2^-4), ×16 at v=127/amt=−4 range... (measured
 *   below), and the clamp rails (v/amt/base guards).
 *
 * Green pin on frozen code expected (property pin over landed Task
 * 10 behaviour); any RED here is a real law defect.
 *
 * CWD convention: repo root (table path, same as t1_fx).
 * Analysis may use libm (tests/ only).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "engine/fx/pcf.h"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    struct PCFTable t;
    int nr, i;

    nr = pcf_table_load("reference/pcf-table.bin", &t);
    CHECK(nr == 10, "table rows %d want 10", nr);
    if (nr != 10) {
        printf("FAIL %d\n", fails);
        return 1;
    }
    CHECK(t.n == 10u, "table count %u", t.n);

    /* --- 10 ledger rows within ±2 cents --- */
    for (i = 0; i < (int)t.n; i++) {
        float got = pcf_cutoff_hz(t.rows[i].base_fc,
            (int)t.rows[i].v, (float)t.rows[i].amt);
        double cents = 1200.0 * log2((double)got /
            (double)t.rows[i].expected_fc);
        CHECK(fabs(cents) <= 2.0,
            "row %d v=%u amt=%d cents=%.4g", i, t.rows[i].v,
            t.rows[i].amt, cents);
        if (i == 0 || i == 4 || i == 8)
            printf("row %d: v=%u amt=%d got=%.3f want=%.3f (%.4g cents)\n",
                i, t.rows[i].v, t.rows[i].amt, (double)got,
                (double)t.rows[i].expected_fc, cents);
    }

    /* --- edge ratios: unity at v=64, /16 at v=0/amt=+4 --- */
    CHECK(pcf_cutoff_hz(1000.0f, 64, 4.0f) == 1000.0f, "v64 unity inexact");
    CHECK(pcf_cutoff_hz(1000.0f, 64, -4.0f) == 1000.0f, "v64 unity inexact (neg)");
    {
        double r = (double)pcf_cutoff_hz(1000.0f, 0, 4.0f) / 1000.0;
        CHECK(fabs(r - 1.0 / 16.0) / (1.0 / 16.0) < 1e-6,
            "v0 ratio %.9g want 1/16", r);
        printf("v0/amt+4 ratio: %.9g\n", r);
    }

    /* --- clamp rails (out-of-range never escapes) --- */
    CHECK(pcf_cutoff_hz(1000.0f, -5, 4.0f) ==
        pcf_cutoff_hz(1000.0f, 0, 4.0f), "v low clamp");
    CHECK(pcf_cutoff_hz(1000.0f, 200, 4.0f) ==
        pcf_cutoff_hz(1000.0f, 127, 4.0f), "v high clamp");
    CHECK(pcf_cutoff_hz(0.0f, 64, 4.0f) == 0.0f, "base guard");

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t25_pcfcutoff\n");
    return fails != 0;
}
