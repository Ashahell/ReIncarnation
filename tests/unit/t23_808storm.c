/* t23_808storm — Module 2.3, TC-2.3.5 (all-voice full-decay storm): with every
 * accent-capable voice triggered at max decay and accent, the synthetic render
 * must fit in <= 0.3x the buffer duration of CPU time, and re-rendering must
 * be bit-identical (determinism). Mirror of t1_808 §6.
 *
 * The WBS contract says "14-voice"; the engine hosts RI_808_NVOICES = 15
 * voices, and this pin exercises all 15 (a superset whose trigger mask is
 * 0x7FFF), which strictly covers the TC.
 *
 * Clock-only timing (no libm needed).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/utsname.h>
#include "engine/dsp/rb808.h"

#define T23_SR 48000.0f
#define T23_SRU 48000u

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    struct RB808Set s;
    uint32_t n = 2u * T23_SRU, blk = 64u, pos = 0;
    static float storm[96000];
    static float storm2[96000];
    clock_t c0, c1;
    double cpu, buf_dur = 2.0;
    uint32_t v;
    struct utsname un;

    rb808_init_set(&s);
    rb808_max_decay(&s);
    for (v = 0; v < RI_808_NVOICES; v++)
        rb808_trigger(&s, v, 1, 0.0f);
    CHECK(s.triggered == 0x7FFFu, "storm mask 0x%04x (want 0x7fff, all 15)",
        s.triggered);
    c0 = clock();
    while (pos < n) {
        uint32_t cc = (n - pos > blk) ? blk : (n - pos);
        rb808_render_mix(&s, storm + pos, cc, T23_SR);
        pos += cc;
    }
    c1 = clock();
    cpu = (double)(c1 - c0) / (double)CLOCKS_PER_SEC;
    uname(&un);
    printf("INFO storm machine=%s/%s cpu=%.4gs buffer=%.2fs ratio=%.4f flags=%s\n",
        un.sysname, un.machine, cpu, buf_dur, cpu / buf_dur, __VERSION__);
    CHECK(cpu / buf_dur <= 0.3, "storm ratio %.4f > 0.3", cpu / buf_dur);
    /* determinism: identical re-trigger re-renders bit-exact */
    rb808_init_set(&s);
    rb808_max_decay(&s);
    for (v = 0; v < RI_808_NVOICES; v++)
        rb808_trigger(&s, v, 1, 0.0f);
    pos = 0;
    while (pos < n) {
        uint32_t cc = (n - pos > blk) ? blk : (n - pos);
        rb808_render_mix(&s, storm2 + pos, cc, T23_SR);
        pos += cc;
    }
    CHECK(memcmp(storm, storm2, sizeof storm) == 0,
        "storm not bit-identical across re-render");

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t23_808storm\n");
    return fails != 0;
}