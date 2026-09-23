/* t25_pcfopen — Module 2.5, TC-2.5.1 status pin (OPEN-04):
 *
 * Per-pattern captures have NOT landed (0/54 rows): pcf_pattern_step
 * explicitly refuses to claim and returns RI_PCF_STEP_NEUTRAL (64)
 * for every (pattern, step). This pin LOCKS THE REFUSAL — it fails
 * if any pattern/step returns anything else, so a future capture
 * landing must update this pin deliberately (not silently):
 *
 *   - all 54 patterns × 16 steps read neutral 64;
 *   - RI_PCF_NPATTERNS == 54, RI_PCF_NSTEPS == 16;
 *   - the ledger table loads (10 calibration rows) and loads
 *     identically twice (restart determinism of the loader path).
 *
 * This pin passing does NOT pass TC-2.5.1 (that needs the 54
 * black-box captures); it pins the documented OPEN state so the
 * gate cannot be mistaken for pattern coverage.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "engine/fx/pcf.h"
static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    struct PCFTable t1, t2;
    int n1, n2;
    uint32_t p, s;

    CHECK(RI_PCF_NPATTERNS == 54u, "npatterns %u", RI_PCF_NPATTERNS);
    CHECK(RI_PCF_NSTEPS == 16u, "nsteps %u", RI_PCF_NSTEPS);

    /* --- refusal: every (pattern, step) reads neutral --- */
    for (p = 0; p < RI_PCF_NPATTERNS; p++) {
        for (s = 0; s < RI_PCF_NSTEPS; s++)
            CHECK(pcf_pattern_step((uint8_t)p, s) == RI_PCF_STEP_NEUTRAL,
                "pattern %u step %u claims %u", p, s,
                pcf_pattern_step((uint8_t)p, s));
    }
    printf("refusal: 54x16 all neutral\n");

    /* --- loader path: 10 calibration rows, identical twice.
     * (memset first: the loader writes rows[0..n) + n only; the
     * unwritten tail is caller memory, not engine state — comparing
     * it would test my stack garbage, not the loader.) --- */
    memset(&t1, 0, sizeof t1);
    memset(&t2, 0, sizeof t2);
    n1 = pcf_table_load("reference/pcf-table.bin", &t1);
    n2 = pcf_table_load("reference/pcf-table.bin", &t2);
    CHECK(n1 == 10, "rows %d", n1);
    CHECK(n2 == 10, "rows2 %d", n2);
    if (n1 == 10 && n2 == 10)
        CHECK(memcmp(&t1, &t2, sizeof t1) == 0, "loader nondet");

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t25_pcfopen\n");
    return fails != 0;
}
