/* t49_pcf_patterns.c — Appendix-D pattern data drives the envelope (§12.8c2).
 * (a) Installed table: pattern_step velocities come from the table
 *     (incl. wrap: step length+k == step k).
 * (b) Render follows the table: a patterned render differs from neutral;
 *     hits retrigger (env rises at boundaries).
 * (c) 32nd resolution: hits arrive 2x per 16th (rise count over a span).
 * (d) Neutral fallback (no table): legacy sustain unchanged (guard).
 * RED-first: neutral-only engine.
 */
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "engine/fx/pcf.h"

#define SR 48000.0f

static float IN[4800u];
static float OUT[4800u];

static void flat_in(void) {
    uint32_t i;
    for (i = 0; i < 4800u; i++)
        IN[i] = 0.5f;
}

/* Hand table: pattern 0 = 16th, length 4, hits at 0 (vel 100) and 2 (vel 50);
 * pattern 1 = 32nd, length 8, hits every step at 80. */
static void hand_table(struct PCFPatterns *t) {
    uint32_t i;
    t->n = 2u;
    t->pat[0].res = RI_PCF_RES_16TH;
    t->pat[0].length = 4u;
    for (i = 0; i < 32u; i++)
        t->pat[0].vel[i] = 0u;
    t->pat[0].vel[0] = 100u;
    t->pat[0].vel[2] = 50u;
    t->pat[1].res = RI_PCF_RES_32ND;
    t->pat[1].length = 8u;
    for (i = 0; i < 32u; i++)
        t->pat[1].vel[i] = (i < 8u) ? 80u : 0u;
}

int main(void) {
    static struct PCFPatterns T;
    struct PCF p;
    uint32_t i;
    flat_in();
    hand_table(&T);
    /* (a) table velocities + wrap. */
    RI_ASSERT(pcf_pattern_vel(&T, 0u, 0u) == 100u, "vel00");
    RI_ASSERT(pcf_pattern_vel(&T, 0u, 2u) == 50u, "vel02");
    RI_ASSERT(pcf_pattern_vel(&T, 0u, 1u) == 0u, "rest nonzero");
    RI_ASSERT(pcf_pattern_vel(&T, 0u, 4u) == 100u, "wrap broken");
    RI_ASSERT(pcf_pattern_vel(&T, 0u, 40u) == 100u, "wrap10 broken");
    RI_ASSERT(pcf_pattern_vel(0, 0u, 0u) == 0u, "null table nonzero");
    RI_ASSERT(pcf_pattern_vel(&T, 9u, 0u) == 0u, "oor pattern nonzero");
    /* (b) render follows the table (Amt=1: below the fmax clamp on both
     * sides, so the envelope swing shows; Amt=4 would clamp both). */
    {
        struct PCF q;
        static float ON[4800u], OP[4800u];
        int diff = 0;
        pcf_init(&p);
        p.base_fc = 800.0f;
        p.q = 2.0f;
        p.amt_oct = 1.0f;
        pcf_set_tempo(&p, 140.0f);
        pcf_render(&p, IN, ON, 4800u, SR);
        pcf_init(&q);
        q.base_fc = 800.0f;
        q.q = 2.0f;
        q.amt_oct = 1.0f;
        pcf_install_patterns(&q, &T);
        q.pattern = 0;
        pcf_set_tempo(&q, 140.0f);
        pcf_render(&q, IN, OP, 4800u, SR);
        for (i = 0; i < 4800u; i++)
            if (ON[i] != OP[i]) {
                diff = 1;
                break;
            }
        RI_ASSERT(diff, "patterned render == neutral render");
    }
    /* (c) 32nd hits arrive 2x per 16th (env-rise count over 2 sixteenths). */
    {
        struct PCF a, b;
        uint32_t ra = 0, rb = 0;
        float pe = 64.0f, qe = 64.0f;
        pcf_init(&a);
        a.base_fc = 800.0f;
        pcf_install_patterns(&a, &T);
        a.pattern = 0;
        pcf_set_tempo(&a, 140.0f);
        pcf_set_decay(&a, 0); /* fast: env falls between hits */
        pcf_init(&b);
        b.base_fc = 800.0f;
        pcf_install_patterns(&b, &T);
        b.pattern = 1;
        pcf_set_tempo(&b, 140.0f);
        pcf_set_decay(&b, 0);
        /* three sixteenths = steps 0,1,2 (16th) and 0..5 (32nd). */
        for (i = 0; i < 15429u; i++) {
            float x = 0.5f, oa, ob;
            uint32_t n = 1u;
            pcf_render(&a, &x, &oa, n, SR);
            if (a.env > pe + 1.0f)
                ra++;
            pe = a.env;
            pcf_render(&b, &x, &ob, n, SR);
            if (b.env > qe + 1.0f)
                rb++;
            qe = b.env;
        }
        RI_ASSERT(ra == 2u, "16th rises %u want 2", ra);
        RI_ASSERT(rb == 6u, "32nd rises %u want 6", rb);
    }
    /* (d) neutral fallback unchanged (guard). */
    {
        struct PCF n;
        pcf_init(&n);
        n.base_fc = 800.0f;
        pcf_set_tempo(&n, 140.0f);
        pcf_render(&n, IN, OUT, 4800u, SR);
        RI_ASSERT(n.env > 0.0f, "neutral env dead");
    }
    RI_RESULT("pcf_patterns");
}
