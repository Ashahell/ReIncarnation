/* t113_levi_arp — Levi arpeggiator stepper (v2 feature 3, owner order).
 * Own 8 modes over the held chord; deterministic Entropy (same seed =
 * same walk, no RNG/globals); fail-closed on bad input; off = silent.
 */
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi_arp.h"

static const uint8_t CHORD[3] = { 60u, 64u, 67u };

static void start_ok(struct RILeviArp *a, uint32_t mode, uint32_t seed) {
    RI_ASSERT(ri_levi_arp_start(a, CHORD, 3u, mode, seed) == 0, "start m%u", mode);
}

static void collect(struct RILeviArp *a, uint8_t *out, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++) {
        int rc = ri_levi_arp_step(a, &out[i]);
        RI_ASSERT(rc == 0, "step %u ready", i);
    }
}

int main(void) {
    struct RILeviArp a, b;
    uint8_t out[12], out2[12];
    uint32_t i;
    ri_levi_arp_init(&a);
    /* Fail-closed. */
    RI_ASSERT(ri_levi_arp_start(0, CHORD, 3u, RI_LEVI_ARP_UP, 1u) == 2, "start null");
    RI_ASSERT(ri_levi_arp_start(&a, 0, 3u, RI_LEVI_ARP_UP, 1u) == 2, "notes null");
    RI_ASSERT(ri_levi_arp_start(&a, CHORD, 0u, RI_LEVI_ARP_UP, 1u) == 2, "empty");
    RI_ASSERT(ri_levi_arp_start(&a, CHORD, 7u, RI_LEVI_ARP_UP, 1u) == 2, "too many");
    RI_ASSERT(ri_levi_arp_start(&a, CHORD, 3u, RI_LEVI_ARP_NMODES, 1u) == 2, "bad mode");
    RI_ASSERT(ri_levi_arp_step(0, out) == 2, "step null");
    RI_ASSERT(ri_levi_arp_step(&a, 0) == 2, "out null");
    /* Off = silent passthrough (fresh init is off). */
    RI_ASSERT(ri_levi_arp_step(&a, out) == 1, "off silent");
    /* UP: 60 64 67 60 64 67. */
    a.on = 1u;
    start_ok(&a, RI_LEVI_ARP_UP, 11u);
    collect(&a, out, 6u);
    RI_ASSERT(out[0] == 60 && out[1] == 64 && out[2] == 67 &&
        out[3] == 60 && out[4] == 64 && out[5] == 67, "up order");
    /* DOWN: 67 64 60 67. */
    start_ok(&a, RI_LEVI_ARP_DOWN, 11u);
    collect(&a, out, 4u);
    RI_ASSERT(out[0] == 67 && out[1] == 64 && out[2] == 60 && out[3] == 67, "down order");
    /* UPDOWN: 60 64 67 64 60 64 (ends not repeated). */
    start_ok(&a, RI_LEVI_ARP_UPDOWN, 11u);
    collect(&a, out, 6u);
    RI_ASSERT(out[0] == 60 && out[1] == 64 && out[2] == 67 &&
        out[3] == 64 && out[4] == 60 && out[5] == 64, "updown order");
    /* CHORD: root struck every step. */
    start_ok(&a, RI_LEVI_ARP_CHORD, 11u);
    collect(&a, out, 3u);
    RI_ASSERT(out[0] == 60 && out[1] == 60 && out[2] == 60, "chord restrike");
    /* OCTUP: 60 72 60 72; OCTDOWN: 60 48 60 48. */
    start_ok(&a, RI_LEVI_ARP_OCTUP, 11u);
    collect(&a, out, 4u);
    RI_ASSERT(out[0] == 60 && out[1] == 72 && out[2] == 60 && out[3] == 72, "octup");
    start_ok(&a, RI_LEVI_ARP_OCTDOWN, 11u);
    collect(&a, out, 4u);
    RI_ASSERT(out[0] == 60 && out[1] == 48 && out[2] == 60 && out[3] == 48, "octdown");
    /* RANDOM + ENTROPY: notes stay in the chord; same seed replays
     * identically (determinism law); random actually moves. */
    start_ok(&a, RI_LEVI_ARP_RANDOM, 42u);
    collect(&a, out, 12u);
    for (i = 0u; i < 12u; i++)
        RI_ASSERT(out[i] == 60 || out[i] == 64 || out[i] == 67, "random in chord");
    RI_ASSERT(out[0] != out[1] || out[1] != out[2] || out[2] != out[3], "random moves");
    ri_levi_arp_init(&b);
    b.on = 1u;
    RI_ASSERT(ri_levi_arp_start(&b, CHORD, 3u, RI_LEVI_ARP_RANDOM, 42u) == 0, "reseed");
    collect(&b, out2, 12u);
    for (i = 0u; i < 12u; i++)
        RI_ASSERT(out[i] == out2[i], "random deterministic %u", i);
    start_ok(&a, RI_LEVI_ARP_ENTROPY, 7u);
    collect(&a, out, 12u);
    for (i = 0u; i < 12u; i++)
        RI_ASSERT(out[i] >= 60 && out[i] <= 79, "entropy near chord");
    ri_levi_arp_init(&b);
    b.on = 1u;
    RI_ASSERT(ri_levi_arp_start(&b, CHORD, 3u, RI_LEVI_ARP_ENTROPY, 7u) == 0, "reseed e");
    collect(&b, out2, 12u);
    for (i = 0u; i < 12u; i++)
        RI_ASSERT(out[i] == out2[i], "entropy deterministic %u", i);
    /* Unsorted latch sorts low -> high. */
    {
        static const uint8_t rev[3] = { 67u, 64u, 60u };
        ri_levi_arp_init(&a);
        a.on = 1u;
        RI_ASSERT(ri_levi_arp_start(&a, rev, 3u, RI_LEVI_ARP_UP, 1u) == 0, "sort latch");
        collect(&a, out, 3u);
        RI_ASSERT(out[0] == 60 && out[1] == 64 && out[2] == 67, "sorted");
    }
    /* Rate map: full = 4/quarter, mid bands, low = off(0). */
    RI_ASSERT(RI_LEVI_ARP_STEPSQ(127) == 4u, "rate full");
    RI_ASSERT(RI_LEVI_ARP_STEPSQ(96) == 4u, "rate hi");
    RI_ASSERT(RI_LEVI_ARP_STEPSQ(64) == 2u, "rate mid");
    RI_ASSERT(RI_LEVI_ARP_STEPSQ(32) == 1u, "rate lo");
    RI_ASSERT(RI_LEVI_ARP_STEPSQ(0) == 0u, "rate zero");
    RI_RESULT("leviarp");
}
