/* t238_levi_key_paint — the keybed's painted slots and its hit boxes were
 * two different rectangles, so most of the keys the owner could SEE were not
 * the keys the app could HIT (2026-10-23, owner ear round on build 8e98a66).
 *
 * THE EVIDENCE, BEFORE THE CLAIM. The owner reported: of the 13 keys "only
 * a few produce something audible", "the volume is low", and "one of the
 * black keys (the 6th from left) is only rendered half". The presses came
 * from the owner's hands, so the lane logged them: 18 presses, and every
 * one logged a WHITE-key note (60,62,64,65,67,69,71,72 = C D E F G A B C+)
 * with ZERO lines for any of the five black keys (61,63,66,68,70). So the
 * bed was not "half broken by luck" — the aim and the picture disagreed.
 *
 * The geometry and the registry are NOT at fault. Driven off RI_GEO_LEVI,
 * all 13 keys have a RECT row and a BUTTON row, and a press at each row's
 * centre hit-tests to that row at every zoom (the first two blocks below —
 * they were GREEN before this phase, and they stay GREEN after it: that is
 * the point. The aim bug was never a missing box.)
 *
 * WHAT THE ARITHMETIC SAYS, in Q units off the map:
 *
 *   white rows  cx 882..1295 step 59, cy 510, w 55, h 80  -> own box y 470..550
 *   black rows  cx 912,971,1089,1148,1207,   cy 412, w 36, h 116 -> own y 354..470
 *   art band    LEVI_KEY_Y0 355 .. LEVI_KEY_Y1 550, blacks end at 470
 *
 * So the black rows and the art's black band COINCIDE exactly (354..470 vs
 * 355..470), and a black key sits centred on its white boundary: (882+941)/2
 * = 911.5 vs 912, (941+1000)/2 = 970.5 vs 971, then a gap at 1029.5 (E/F —
 * correctly no black key), 1088.5 vs 1089, 1147.5 vs 1148, 1206.5 vs 1207.
 * The map is a correct piano.
 *
 * The paint is what is wrong, and it is one line of it: art_levi.c's live-
 * white branch re-centres a white key's paint over the WHOLE band (y 355..550)
 * instead of the slot its hit box owns (470..550), and the geometry table
 * paints blacks before whites, so every white repaints the blacks beside it.
 * White C paints x 854..910 over black C# (x 894..930); white D paints 913..
 * 969 over the rest — leaving C# about 4 Q wide (909..913). A 36 Q key drawn
 * 4 Q wide is what the owner did not see, so they aimed at the full-width
 * black keys the ART draws OUTSIDE the live band (x < 853 and x > 1325) —
 * decoration, with nothing behind them. Every black-key press therefore hit a
 * live WHITE box: which is exactly the 18-for-8 log above, including six
 * presses logged as E.
 *
 * And the half key the owner counted: the art blacks are skipped for k 7..13,
 * so from the left the visible black keys are the art octave's five (499, 558,
 * 676, 735, 794) and then the art black at x 1325 — the SIXTH — which the live
 * white C+ paints over up to 1322, leaving 1322..1343 = about 21 Q. Measured
 * in the lane's scale-2 capture, four live black slots sat at 2 image px and
 * that sixth one at 8 px against 14 for a full key. Same number, same key,
 * found two ways.
 *
 * WHAT THIS PINS — the invariant, not the symptom. Two things must agree, and
 * today they do not:
 *
 *   1. every key's PAINT box equals its HIT box — a key paints the slot you
 *      can hit, and nothing else;
 *   2. no live key's paint box covers the centre of ANOTHER live key's hit
 *      box — nothing you can hit is painted by something else.
 *
 * Both are driven from RI_GEO_LEVI and from ri_levi_key_paint_q, which is the
 * rule the DRAWING reads (art_levi.c computes its rect from it, so the test
 * cannot pass against a rule the drawing does not use — that is the shape
 * that would have made this a tautology).
 *
 * The third block is the owner's velocity ruling (2026-10-23: raise it). The
 * keybed sends RI_LEVI_KEY_VEL, a fixed value, and the note path clamps 0..127
 * (t146 pins that clamp). The claim pinned here is the one that can only be
 * earned by the engine: the value the keybed sends normalises to FULL velocity
 * at the voice (vel01 = 1.0f), so the keybed is not quietly sending 79 % of the
 * amplitude the path can express — which is what "the volume is low" was.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/draw/art.h"
#include "gui/panelgeo.h"
#include "gui/ctlreg.h"
#include "gui/sectlevi.h"
#include "engine/dsp/levi.h"

#define L(id) ((uint16_t)((uint16_t)(RI_SEC_LEVI << 8) | (uint16_t)(id)))
#define KEY0 ((int)RI_SLEVI_KEY0)
#define NKEY ((int)RI_SLEVI_KEYS)

/* One static set: the velocity claim is about the voice, not about a pair. */
static struct RILeviSet A;

/* Render blocks: the velocity term is copied into the voice per block, not
 * per call (t146's law), so the value only lands after a render. */
static void run(struct RILeviSet *s, uint32_t nblocks) {
    static float l[256], r[256];
    uint32_t i;
    for (i = 0u; i < nblocks; i++)
        levi_voice_render_sum_stereo(s, l, r, 256u, 48000.0f);
}

static int feq(float a, float b) {
    float d = a > b ? a - b : b - a;
    float m = a > b ? a : b;
    m = m < 0.0f ? -m : m;
    return d <= m * 1e-5f + 1e-7f && d < 1e30f;
}

int main(void) {
    const struct RIGeoSection *g = ri_geo_section(RI_SEC_LEVI);
    int z, k, j;

    RI_ASSERT(g != 0, "the Levi section is laid out");

    /* ---- 1. THE CENSUS, DRIVEN OFF THE MAP (already true: the pin) ----
     * Every key the map claims must exist in the registry, as a control
     * shape, or a press there is a press on nothing. */
    for (k = 0; k < NKEY; k++) {
        int idx = KEY0 + k;
        int found = 0;
        const struct RICtlDef *d = ri_ctlreg_find(L(idx));
        for (j = 0; j < (int)g->nitems; j++)
            if ((g->items[j].reg_id & 0xFFu) == (uint32_t)idx)
                found++;
        RI_ASSERT(d != 0, "key %u has a registry row (%s)", idx, d ? d->legend : "-");
        RI_ASSERT(found == 1, "key %u has exactly one geometry row (%d)", idx, found);
        if (d)
            RI_ASSERT(d->kind == RI_CK_BUTTON, "key %u is a BUTTON (%u)", idx, d->kind);
    }

    /* ---- 2. THE AIM, AT THE BOX LEVEL (already true: the pin) ----
     * A press at a key's own box centre hit-tests to that key, at every zoom.
     * This is the half that was already right, and it is why the report was
     * not "the black keys are missing from the map". */
    for (z = 0; z < 4; z++) {
        for (k = 0; k < NKEY; k++) {
            const struct RIGeoItem *it = 0;
            int opt = -2;
            uint16_t h;
            for (j = 0; j < (int)g->nitems; j++)
                if ((g->items[j].reg_id & 0xFFu) == (uint32_t)(KEY0 + k)) {
                    it = &g->items[j];
                    break;
                }
            RI_ASSERT(it != 0, "key %u present (zoom %d)", KEY0 + k, z);
            if (!it)
                continue;
            h = ri_geo_hit_opt(g, ri_geo_px(it->cx, z), ri_geo_px(it->cy, z), z, &opt);
            RI_ASSERT(h == L(KEY0 + k), "hit at centre of key %u = %04x, want %04x",
                KEY0 + k, h, L(KEY0 + k));
        }
    }

    /* ---- 3. PAINT == HIT, AND NO PAINT OVER ANOTHER KEY (RED today) ----
     * Compared in Q units, which is where the map lives: converting to px at
     * a zoom rounds, and a comparison that mixes the two would pass at one
     * zoom and fail at another while the product stayed the same. The zoom
     * sweep belongs to the aim block above, not to this one. */
    for (k = 0; k < NKEY; k++) {
        const struct RIGeoItem *it = 0;
        int hx0, hy0, hx1, hy1, px0, py0, px1, py1;
        for (j = 0; j < (int)g->nitems; j++)
            if ((g->items[j].reg_id & 0xFFu) == (uint32_t)(KEY0 + k)) {
                it = &g->items[j];
                break;
            }
        RI_ASSERT(it != 0, "key %u measured in Q", KEY0 + k);
        if (!it)
            continue;
        /* The slot the map says can be hit. */
        hx0 = (int)it->cx - (int)it->w / 2;
        hx1 = (int)it->cx + (int)it->w / 2;
        hy0 = (int)it->cy - (int)it->h / 2;
        hy1 = (int)it->cy + (int)it->h / 2;
        /* The rect the drawing paints. */
        RI_ASSERT(ri_levi_key_paint_q(it, &px0, &py0, &px1, &py1) == 0,
            "key %u has a paint rule", KEY0 + k);
        RI_ASSERT(px0 == hx0 && px1 == hx1,
            "key %u paints its own width (paint %d..%d Q, hit %d..%d Q)",
            KEY0 + k, px0, px1, hx0, hx1);
        RI_ASSERT(py0 == hy0 && py1 == hy1,
            "key %u paints its own height (paint %d..%d Q, hit %d..%d Q)",
            KEY0 + k, py0, py1, hy0, hy1);

        /* Nothing you can hit may be painted by a DIFFERENT key: that is the
         * shape that makes a visible slot un-hittable, and a hittable
         * slot invisible. Driven from the map, so it covers all 13 x 12
         * pairs rather than the two the report happened to name. */
        for (j = 0; j < NKEY; j++) {
            const struct RIGeoItem *ot = 0;
            int m;
            if (j == k)
                continue;
            for (m = 0; m < (int)g->nitems; m++)
                if ((g->items[m].reg_id & 0xFFu) == (uint32_t)(KEY0 + j)) {
                    ot = &g->items[m];
                    break;
                }
            if (!ot)
                continue;
            RI_ASSERT(!(px0 <= (int)ot->cx && (int)ot->cx <= px1 &&
                       py0 <= (int)ot->cy && (int)ot->cy <= py1),
                "key %u's paint must not cover key %u's centre (paint %d..%d x %d..%d Q, centre %d,%d)",
                KEY0 + k, KEY0 + j, px0, px1, py0, py1,
                (int)ot->cx, (int)ot->cy);
        }
    }

    /* ---- 4. THE VELOCITY THE KEYBED SENDS (owner 2026-10-23: raise it) ----
     * The claim in engine terms: the fixed value the keybed sends must
     * normalise to full velocity at the voice. The old 100 reaches 100/127
     * of the amplitude the path can express, which is the report's "the
     * volume is low"; t146 owns the vel01 -> level route, so the route is
     * not re-proved here. */
    {
        levi_init_set(&A);
        RI_ASSERT(levi_note_vel(&A, (uint8_t)RI_SLEVI_MIDDLE_C,
                                (uint8_t)RI_LEVI_KEY_VEL) == 1,
            "the keybed's note path fires (vel %u)", RI_LEVI_KEY_VEL);
        RI_ASSERT(A.v[0].nvel == (uint8_t)RI_LEVI_KEY_VEL,
            "the voice takes the keybed's velocity (%u, want %u)", A.v[0].nvel, RI_LEVI_KEY_VEL);
        run(&A, 5u);
        RI_ASSERT(feq(A.v[0].vel01, 1.0f),
            "the keybed sends full velocity, not a fraction (%f)", A.v[0].vel01);
    }

    RI_RESULT("levi key paint");
    return 0;
}
