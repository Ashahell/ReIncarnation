/* t21_evwin — per-buffer event windowing unit (WBS 2.1 playback side).
 * Hand-made sorted list (5 events @0,100,200,300,400): full/partial/
 * empty/past-end windows, cap truncation, degenerate (s1<=s0) + NULL
 * edges. Window is [s0,s1); input order preserved (caller keeps lists
 * sorted — walker output is sorted by contract).
 */
#include <stdio.h>
#include <stdint.h>
#include "engine/seq/riseq.h"
#include "engine/seq/sched.h"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    struct RIEvent ev[5], out[8];
    uint32_t n, i;
    for (i = 0u; i < 5u; i++) {
        ev[i].sample = (uint64_t)i * 100ULL;
        ev[i].type = RI_EV_NOTE_ON;
        ev[i].device = 0u;
        ev[i].voice = 0u;
        ev[i].value = (uint16_t)(40u + i);
        ev[i].flags = 0u;
        ev[i].seq = i;
    }

    n = ri_events_in_window(ev, 5u, 0ULL, 400ULL, out, 8u);
    CHECK(n == 4u, "full count %u want 4", n);
    CHECK(out[0].sample == 0ULL && out[3].sample == 300ULL, "full bounds");

    n = ri_events_in_window(ev, 5u, 100ULL, 300ULL, out, 8u);
    CHECK(n == 2u, "mid count %u want 2", n);
    CHECK(out[0].value == 41u && out[1].value == 42u, "mid values");

    n = ri_events_in_window(ev, 5u, 200ULL, 201ULL, out, 8u);
    CHECK(n == 1u && out[0].sample == 200ULL, "single %u", n);

    n = ri_events_in_window(ev, 5u, 500ULL, 600ULL, out, 8u);
    CHECK(n == 0u, "past-end %u", n);

    n = ri_events_in_window(ev, 5u, 200ULL, 200ULL, out, 8u);
    CHECK(n == 0u, "degenerate %u", n);

    n = ri_events_in_window(ev, 5u, 0ULL, 500ULL, out, 2u);
    CHECK(n == 2u, "cap count %u want 2", n);
    CHECK(out[1].sample == 100ULL, "cap order");

    CHECK(ri_events_in_window(NULL, 5u, 0ULL, 500ULL, out, 8u) == 0u,
          "null ev nonzero");
    CHECK(ri_events_in_window(ev, 5u, 0ULL, 500ULL, NULL, 8u) == 0u,
          "null out nonzero");
    CHECK(ri_events_in_window(ev, 0u, 0ULL, 500ULL, out, 8u) == 0u,
          "empty nonzero");

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t21_evwin\n");
    return fails != 0;
}
