/* t114_levi_arp_emit — arp bind at emission (v2 feature 3b, owner order).
 * Post-pass over a Levi event window: arp off (or sub-audible rate) is
 * bit-identical passthrough (demo safety); arp on subdivides each
 * chord group into stepper strikes (lanes cycle, legato, gates
 * balanced); deterministic; fail-closed.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/pattern.h"
#include "engine/seq/sched.h"
#include "engine/seq/clock.h"
#include "engine/dsp/levi_arp.h"

#define SRU 48000u
static const struct RISegment SEG0[] = { { 0, 428571428ULL } }; /* 140 BPM */
static const struct RITempoMap MAP = { SEG0, 1, 96, SRU };
static struct RIEvent OB[256], RB[512], RB2[512];

static uint32_t emit_all(const struct RIPattern *p, uint16_t dev) {
    return ri_sched_emit_pattern(p, dev, &MAP, 0u, 96u, NULL, 0, 0, OB, 256u);
}

static void mkchord(struct RIPattern *p) {
    ri_pattern_init(p, RI_PATTERN_KIND_LEVI, 0u);
    ri_pattern_set_length(p, 16u);
    RI_ASSERT(ri_levi_set(p, 0u, 0u, 60u, 1) == 0, "set");
    RI_ASSERT(ri_levi_set(p, 0u, 1u, 64u, 1) == 0, "set");
    RI_ASSERT(ri_levi_set(p, 0u, 2u, 67u, 1) == 0, "set");
}

/* Per voice: every NOTE_ON has a later NOTE_OFF (no stuck gates). */
static void gates_balanced(const struct RIEvent *ev, uint32_t n) {
    uint32_t v, i;
    for (v = 0u; v < 6u; v++) {
        uint32_t on = 0u, off = 0u;
        for (i = 0u; i < n; i++)
            if (ev[i].voice == v) {
                on += ev[i].type == RI_EV_NOTE_ON;
                off += ev[i].type == RI_EV_NOTE_OFF;
            }
        RI_ASSERT(on == off, "voice %u gate %u/%u", v, on, off);
    }
}

int main(void) {
    struct RIPattern p;
    struct RILeviArpCfg off = { 0u, 0u, 0u, 0u };
    struct RILeviArpCfg up = { 1u, RI_LEVI_ARP_UP, 127u, 0u };
    uint32_t n, m, m2, i;
    uint64_t end_sample;
    mkchord(&p);
    n = emit_all(&p, 4u);
    RI_ASSERT(n > 0u, "have events");
    end_sample = OB[n - 1u].sample;
    /* Fail-closed. */
    RI_ASSERT(ri_levi_arp_rewrite(0, n, RB, 512u, &up, end_sample) == 0u, "null in");
    RI_ASSERT(ri_levi_arp_rewrite(OB, n, 0, 512u, &up, end_sample) == 0u, "null out");
    RI_ASSERT(ri_levi_arp_rewrite(OB, n, RB, 512u, 0, end_sample) == 0u, "null cfg");
    /* Off = bit-identical passthrough (demo safety). */
    m = ri_levi_arp_rewrite(OB, n, RB, 512u, &off, end_sample);
    RI_ASSERT(m == n, "off count %u/%u", m, n);
    RI_ASSERT(!memcmp(OB, RB, (size_t)n * sizeof OB[0]), "off identical");
    /* Sub-audible rate = passthrough too (knob accident never silences). */
    {
        struct RILeviArpCfg slow = { 1u, RI_LEVI_ARP_UP, 0u, 0u };
        m = ri_levi_arp_rewrite(OB, n, RB, 512u, &slow, end_sample);
        RI_ASSERT(m == n && !memcmp(OB, RB, (size_t)n * sizeof OB[0]), "slow passthrough");
    }
    /* On: strikes cycle the chord notes, voices rotate, gates balance. */
    m = ri_levi_arp_rewrite(OB, n, RB, 512u, &up, end_sample);
    RI_ASSERT(m > 0u && m <= 512u, "on emits %u", m);
    for (i = 0u; i < m; i++)
        if (RB[i].type == RI_EV_NOTE_ON)
            RI_ASSERT(RB[i].value == 60 || RB[i].value == 64 || RB[i].value == 67,
                "strike in chord");
    gates_balanced(RB, m);
    /* Deterministic: same window twice, byte-identical. */
    m2 = ri_levi_arp_rewrite(OB, n, RB2, 512u, &up, end_sample);
    RI_ASSERT(m2 == m && !memcmp(RB, RB2, (size_t)m * sizeof RB[0]), "deterministic");
    /* No output capacity: fail-closed, never partial. */
    RI_ASSERT(ri_levi_arp_rewrite(OB, n, RB, 0u, &up, end_sample) == 0u, "no cap");
    RI_RESULT("leviarpemit");
}
