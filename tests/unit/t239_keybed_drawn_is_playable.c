/* t239_keybed_drawn_is_playable — the panel DRAWS a three-octave keyboard, so
 * all of it must play (owner 2026-10-24).
 *
 * THE REPORT, verbatim: "In the Levi panel, the 1st seven and the last seven
 * white piano keys do not produce anything audible. The other white piano
 * keys do produce a tone. Also, only the middle 5 black piano keys produce
 * anything audible." The rule that follows from it is the same one P-KEY2
 * established: a key you can see is a key you can hit. Decoration that looks
 * playable is a defect, not styling.
 *
 * WHAT THE MAP SAID BEFORE THIS PHASE, in Q: the art laid 22 white keys from
 * x 440 at pitch 59 and skipped its black keys only for k 7..13, so the panel
 * showed 22 whites and 10 blacks. The registry had THIRTEEN keys (ids 24..36)
 * and RI_GEO_LEVI put them at cx 882..1295 -- one octave in the middle. So 14
 * whites (the first seven and the last seven) and 5 blacks were drawn with
 * nothing behind them: exactly the groups the owner pressed.
 *
 * WHAT THIS PINS. The owner chose the three-octave bed, not a smaller picture:
 * 37 keys = 22 white + 15 black, C4..C7, one semitone per key.
 *
 *   1. the band the art lays out IS the piano: 22 whites and 15 blacks at one
 *      pitch, and the black pattern of a keyboard;
 *   2. every slot the band draws has exactly one control, and that control's
 *      geometry box is the slot -- no drawn key without a control, and no
 *      control off its slot;
 *   3. the census is closed both ways: 37 key-shaped geometry rows and 37
 *      key-shaped registry rows, so neither table can grow a dead key;
 *   4. the ids are the owner's stability rule: keys 0..12 keep 24..36 and keys
 *      13..36 take 228..251, one id per key, and the notes run 60..96 in bed
 *      order, so a press plays the pitch under the finger;
 *   5. the paint rule answers for every key id and refuses a non-key, which is
 *      the half t238 compares against the hit box.
 *
 * HOW IT AVOIDS AGREEING WITH THE BUG: the slot list is computed here from
 * the art's own band constants (RI_LEVI_KB_*), NOT from RI_GEO_LEVI, and the
 * controls are read back out of RI_GEO_LEVI and the registry. Two independent
 * descriptions, compared box for box. A second copy of the geometry table
 * would only prove the copy matched.
 *
 * The art no longer draws a black key at all: every black key is a control
 * (P-KEY3), and the bed it still lays is the white the black keys sit on.
 * An art key added back without a control changes the raster, which t93
 * pins -- this file covers the map side.
 */
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "gui/draw/art.h"
#include "gui/panelgeo.h"
#include "gui/ctlreg.h"
#include "gui/sectlevi.h"

#define L(id) ((uint16_t)((uint16_t)(RI_SEC_LEVI << 8) | (uint16_t)(id)))

/* The white sequence of a keyboard octave and the black keys as "after white
 * k", with their semitones. The test states the piano itself; if the product
 * and this disagree, the product is drawing a different instrument. */
static const int WSEMI[7] = { 0, 2, 4, 5, 7, 9, 11 };
static const int BAFTER[5] = { 0, 1, 3, 4, 5 };
static const int BSEMI[5] = { 1, 3, 6, 8, 10 };

static const struct RIGeoItem *geo_row(const struct RIGeoSection *g, uint16_t id) {
    int j;
    for (j = 0; j < (int)g->nitems; j++)
        if (g->items[j].reg_id == (uint16_t)L(id))
            return &g->items[j];
    return 0;
}

/* A box in Q, from the geometry row the app hit-tests. */
static void hit_box(const struct RIGeoItem *it, int *x0, int *y0, int *x1, int *y1) {
    *x0 = (int)it->cx - (int)it->w / 2;
    *x1 = (int)it->cx + (int)it->w / 2;
    *y0 = (int)it->cy - (int)it->h / 2;
    *y1 = (int)it->cy + (int)it->h / 2;
}

/* The slot the art's band lays out, in Q, from the band constants only. */
static void white_slot(int k, int *x0, int *y0, int *x1, int *y1) {
    int cx = (int)RI_LEVI_KB_X0 + (int)RI_LEVI_KB_PITCH * k + (int)RI_LEVI_KB_PITCH / 2;
    *x0 = cx - (int)RI_LEVI_KB_W / 2;
    *x1 = cx + (int)RI_LEVI_KB_W / 2;
    *y0 = (int)RI_LEVI_KB_Y0;
    *y1 = (int)RI_LEVI_KB_Y1;
}

static void black_slot(int k, int *x0, int *y0, int *x1, int *y1) {
    int cx = (int)RI_LEVI_KB_X0 + (int)RI_LEVI_KB_PITCH * (k + 1);
    *x0 = cx - (int)RI_LEVI_BK_W / 2;
    *x1 = cx + (int)RI_LEVI_BK_W / 2;
    *y0 = (int)RI_LEVI_BK_Y0;
    *y1 = (int)RI_LEVI_BK_Y1;
}

int main(void) {
    const struct RIGeoSection *g = ri_geo_section(RI_SEC_LEVI);
    int k, i, nseen = 0;
    int kx[64], knote[64];
    int nw = 0, nb = 0;

    RI_ASSERT(g != 0, "the Levi section is laid out");

    /* ---- 1. THE BAND IS THE PIANO THE ANSWER ASKED FOR ----
     * 3 octaves + the closing C = 22 whites; the keyboard's 5 blacks per
     * octave = 15; and the whole bed is one semitone per key. These are the
     * counts the owner's ear described as groups of 7 and 5. */
    RI_ASSERT(RI_LEVI_KB_NW == 22, "the band draws 22 white keys (%d)", (int)RI_LEVI_KB_NW);
    for (k = 0; k <= 20; k++)
        for (i = 0; i < 5; i++)
            if (BAFTER[i] == k % 7)
                nb++;
    nw = (int)RI_LEVI_KB_NW;
    RI_ASSERT(nw + nb == (int)RI_SLEVI_KEYS,
        "the drawn bed is the mapped bed (%d whites + %d blacks, map says %u)",
        nw, nb, (unsigned)RI_SLEVI_KEYS);
    RI_ASSERT(RI_SLEVI_KEYS == 37, "the bed is 37 keys C4..C7 (%u)", (unsigned)RI_SLEVI_KEYS);

    /* The band must fit the section. The art's pitch is 59 and the section
     * is 1756 Q wide, so the 22nd white ends at 1735 -- this pins that the
     * bed is inside the panel rather than trusting a remembered width (an
     * earlier note in this phase said 1684, which would have put the last
     * key off the edge; the measurement here is the one that counts). */
    {
        int x0, y0, x1, y1;
        white_slot(nw - 1, &x0, &y0, &x1, &y1);
        RI_ASSERT(x1 <= (int)g->w,
            "the last white key ends inside the section (%d <= %d Q)", x1, (int)g->w);
    }

    /* ---- 2. EVERY DRAWN SLOT HAS EXACTLY ONE CONTROL, ON THAT SLOT ---- */
    for (k = 0; k < nw; k++) {
        int note = (int)RI_SLEVI_MIDDLE_C + (k / 7) * 12 + WSEMI[k % 7];
        uint16_t id = ri_levi_key_id((unsigned)(note - (int)RI_SLEVI_MIDDLE_C));
        const struct RIGeoItem *it = geo_row(g, id);
        int sx0, sy0, sx1, sy1, hx0, hy0, hx1, hy1, cnt = 0;
        const struct RICtlDef *d;
        white_slot(k, &sx0, &sy0, &sx1, &sy1);
        RI_ASSERT(it != 0, "white slot %d (note %d, id %u) has a control", k, note, id);
        if (!it)
            continue;
        hit_box(it, &hx0, &hy0, &hx1, &hy1);
        RI_ASSERT(hx0 == sx0 && hx1 == sx1 && hy0 == sy0 && hy1 == sy1,
            "white slot %d sits where it is drawn (control %d..%d x %d..%d, drawn %d..%d x %d..%d)",
            k, hx0, hx1, hy0, hy1, sx0, sx1, sy0, sy1);
        d = ri_ctlreg_find(L(id));
        RI_ASSERT(d != 0, "white slot %d (id %u) is registered", k, id);
        if (d) {
            RI_ASSERT(d->kind == RI_CK_BUTTON, "white slot %d is a BUTTON (%u)", k, d->kind);
            RI_ASSERT((int)ri_ctlreg_auto_id(d) == 0,
                "white slot %d is a key, not an engine control (%04x)", k, ri_ctlreg_auto_id(d));
        }
        for (i = 0; i < (int)g->nitems; i++)
            if (g->items[i].reg_id == (uint16_t)L(id))
                cnt++;
        RI_ASSERT(cnt == 1, "white slot %d has exactly one geometry row (%d)", k, cnt);
        RI_ASSERT(nseen < 64, "bed census fits");
        kx[nseen] = (int)it->cx;
        knote[nseen] = note;
        nseen++;
    }
    for (k = 0; k <= 20; k++) {
        int bi = -1;
        for (i = 0; i < 5; i++)
            if (BAFTER[i] == k % 7)
                bi = i;
        if (bi < 0)
            continue;
        {
            int note = (int)RI_SLEVI_MIDDLE_C + (k / 7) * 12 + BSEMI[bi];
            uint16_t id = ri_levi_key_id((unsigned)(note - (int)RI_SLEVI_MIDDLE_C));
            const struct RIGeoItem *it = geo_row(g, id);
            int sx0, sy0, sx1, sy1, hx0, hy0, hx1, hy1, cnt = 0;
            const struct RICtlDef *d;
            black_slot(k, &sx0, &sy0, &sx1, &sy1);
            RI_ASSERT(it != 0, "black slot %d (note %d, id %u) has a control", k, note, id);
            if (!it)
                continue;
            hit_box(it, &hx0, &hy0, &hx1, &hy1);
            RI_ASSERT(hx0 == sx0 && hx1 == sx1 && hy0 == sy0 && hy1 == sy1,
                "black slot %d sits where it is drawn (control %d..%d x %d..%d, drawn %d..%d x %d..%d)",
                k, hx0, hx1, hy0, hy1, sx0, sx1, sy0, sy1);
            d = ri_ctlreg_find(L(id));
            RI_ASSERT(d != 0, "black slot %d (id %u) is registered", k, id);
            if (d) {
                RI_ASSERT(d->kind == RI_CK_BUTTON, "black slot %d is a BUTTON (%u)", k, d->kind);
                RI_ASSERT((int)ri_ctlreg_auto_id(d) == 0,
                    "black slot %d is a key, not an engine control (%04x)", k, ri_ctlreg_auto_id(d));
            }
            for (i = 0; i < (int)g->nitems; i++)
                if (g->items[i].reg_id == (uint16_t)L(id))
                    cnt++;
            RI_ASSERT(cnt == 1, "black slot %d has exactly one geometry row (%d)", k, cnt);
            RI_ASSERT(nseen < 64, "bed census fits");
            kx[nseen] = (int)it->cx;
            knote[nseen] = note;
            nseen++;
        }
    }
    RI_ASSERT(nseen == (int)RI_SLEVI_KEYS,
        "every drawn slot was measured (%d slots, map says %u)", nseen, (unsigned)RI_SLEVI_KEYS);

    /* ---- 3. THE PITCH RISES LEFT TO RIGHT, ONE SEMITONE PER KEY ----
     * A black key's centre lies between the two whites it separates, so
     * sorting the 37 rows by centre x must produce the chromatic run 60..96.
     * This is the keyboard property itself, not the map's own arithmetic: a
     * key drawn at the wrong boundary, or two keys sharing a note, breaks the
     * run. */
    for (k = 1; k < nseen; k++) {
        int x = kx[k], nt = knote[k], jj = k - 1;
        while (jj >= 0 && kx[jj] > x) {
            kx[jj + 1] = kx[jj];
            knote[jj + 1] = knote[jj];
            jj--;
        }
        kx[jj + 1] = x;
        knote[jj + 1] = nt;
    }
    for (k = 0; k < nseen; k++)
        RI_ASSERT(knote[k] == (int)RI_SLEVI_MIDDLE_C + k,
            "the %dth key from the left plays %d, want %d", k, knote[k], (int)RI_SLEVI_MIDDLE_C + k);

    /* ---- 4. THE CENSUS IS CLOSED BOTH WAYS ----
     * A key-shaped row the band does not draw is a key with no slot (an extra
     * control, an invisible key); a key id must be exactly the 37. */
    {
        int geo_keys = 0, reg_keys = 0;
        for (i = 0; i < (int)g->nitems; i++)
            if (ri_levi_key_n((uint16_t)(g->items[i].reg_id & 0xFFu)) >= 0)
                geo_keys++;
        for (i = 0; i < (int)ri_ctlreg_count(); i++) {
            const struct RICtlDef *d = ri_ctlreg_at((uint32_t)i);
            if (d && d->section == RI_SEC_LEVI && ri_levi_key_n((uint16_t)(d->reg_id & 0xFFu)) >= 0)
                reg_keys++;
        }
        RI_ASSERT(geo_keys == (int)RI_SLEVI_KEYS,
            "the geometry has exactly the 37 key rows (%d)", geo_keys);
        RI_ASSERT(reg_keys == (int)RI_SLEVI_KEYS,
            "the registry has exactly the 37 key rows (%d)", reg_keys);
    }

    /* ---- 5. THE ID RULE THE OWNER SET, AND THE NOTES IT PLAYS ----
     * Keys 0..12 are the old keybed (24..36) and must not move: songs, skins
     * and automation are keyed on the ids, not on the picture. The rest take
     * 228..251, the section's only free run. The two are one map, not a range,
     * which is why idx - KEY0 must no longer appear anywhere. */
    for (k = 0; k < (int)RI_SLEVI_KEYS; k++) {
        uint16_t id = ri_levi_key_id((unsigned)k);
        if (k < 13)
            RI_ASSERT(id == (uint16_t)(RI_SLEVI_KEY0 + k),
                "key %d keeps its old id (%u, want %u)", k, id, (unsigned)(RI_SLEVI_KEY0 + k));
        else
            RI_ASSERT(id == (uint16_t)(RI_SLEVI_KEY_EXT0 + k - 13),
                "key %d takes an extension id (%u, want %u)", k, id,
                (unsigned)(RI_SLEVI_KEY_EXT0 + k - 13));
        RI_ASSERT(ri_levi_key_n(id) == k, "id %u maps back to key %d (%d)", id, k, ri_levi_key_n(id));
        RI_ASSERT((int)ri_levi_key_note((unsigned)k) == (int)RI_SLEVI_MIDDLE_C + k,
            "key %d plays %d (%u)", k, (int)RI_SLEVI_MIDDLE_C + k, ri_levi_key_note((unsigned)k));
        {
            int white = (k % 12 == 0 || k % 12 == 2 || k % 12 == 4 || k % 12 == 5 ||
                         k % 12 == 7 || k % 12 == 9 || k % 12 == 11);
            RI_ASSERT((ri_levi_key_black((unsigned)k) != 0) == !white,
                "key %d is classified white/black like a keyboard", k);
        }
    }
    for (k = 0; k < (int)RI_SLEVI_KEYS; k++)
        for (i = k + 1; i < (int)RI_SLEVI_KEYS; i++)
            RI_ASSERT(ri_levi_key_id((unsigned)k) != ri_levi_key_id((unsigned)i),
                "keys %d and %d share an id", k, i);

    /* ---- 6. THE PAINT RULE KNOWS THE WHOLE BED ----
     * t238 compares paint to hit; this only says the rule answers for every
     * key and refuses a non-key (37 is the id just past the old keybed). */
    for (k = 0; k < (int)RI_SLEVI_KEYS; k++) {
        const struct RIGeoItem *it = geo_row(g, ri_levi_key_id((unsigned)k));
        int px0, py0, px1, py1;
        RI_ASSERT(it != 0, "key %d has a row to paint", k);
        if (!it)
            continue;
        RI_ASSERT(ri_levi_key_paint_q(it, &px0, &py0, &px1, &py1) == 0,
            "the paint rule answers for key %d", k);
    }
    {
        const struct RIGeoItem *it = geo_row(g, RI_SLEVI_STEP0);
        int px0, py0, px1, py1;
        RI_ASSERT(it != 0, "a step segment has a row");
        if (it)
            RI_ASSERT(ri_levi_key_paint_q(it, &px0, &py0, &px1, &py1) != 0,
                "the paint rule refuses a control that is not a key");
        RI_ASSERT(ri_levi_key_n(37u) < 0, "37 maps to no key");
    }

    RI_RESULT("keybed drawn is playable");
    return 0;
}
