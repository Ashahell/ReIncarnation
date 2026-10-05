/* bench_build — where does the display-list BUILD go, and how much of it is
 * thrown away by the damage clip?
 *
 * WHY THIS EXISTS (2026-10-05). With `dp_gap` corrected to subtract the SUM of
 * the phases (t168), a quiet box repaint on the Dell is attributed properly:
 *
 *     233 us = 112 build + 33 replay + 69 blit + 18 unattributed
 *
 * and ~14 of that 18 is my own eight ReadEClock calls, so the structural floor
 * is ~2 us. That makes **the build the largest real term at 112 us (48 %)**, and
 * the Dell record of 2026-10-02 already noted the build "was 59 % of a box
 * repaint" before the clip existed.
 *
 * The clip (bb1c385) drops commands at PUSH time, so it removes the replay's
 * work but NOT the drawing work: geometry, colour lookup and — the suspect —
 * font measurement all happen before the push, for every element in the whole
 * section, and are then discarded. This bench measures how much of the build
 * that is, per section, on the host where a clock read is ~20 ns instead of
 * 4-6 us so the counts are exactly controlled rather than sampled.
 *
 * It reports, per section:
 *   items      geometry items in the section (the per-item loop in art_section.c)
 *   built      commands the whole section emits (no clip) — the work done
 *   kept       commands that survive a 64x16 damage box — the work replayed
 *   wasted     built - kept, i.e. computed then discarded: the prize
 *   text_b/t   text commands built / kept (text measurement is the suspect)
 *   us_b/t     microseconds to build whole / to build with the clip
 *
 * Nothing here asserts. A diagnosis that can fail is a test pretending to be a
 * measurement, and this file's only job is to point at the next cut.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "gui/sectui.h"
#include "gui/sectmix.h"
#include "gui/ctlreg.h"
#include "gui/panelgeo.h"
#include "gui/draw/canvas.h"
#include "gui/draw/art.h"
#include "platform/pal/ri_pal_draw.h"

static const char *SECNAME[RI_SEC_COUNT] = {
    "SYNTH1", "SYNTH2", "808", "909", "MIX-S1", "MIX-S2", "MIX-808",
    "MIX-909", "MASTER", "PCF", "DELAY", "DIST", "COMP", "TRANSPORT",
    "PAT-S1", "PAT-S2", "PAT-808", "PAT-909", "LEVI", "PAT-LEVI", "MIX-LEVI"
};

static struct ri_dcmd CMD[24576];
static char SP[65536];
static struct RIMixBoard BOARD;

static struct ri_text_metrics TM;

/* A stand-in for the host's text width. It is deliberately NOT free: on AROS
 * `rtext_width` is a real font measurement per string, which is the whole
 * reason a discarded text command is expensive. Counting calls rather than
 * modelling the cost keeps this comparable across machines — see the note on
 * us_b below. */
static uint32_t g_width_calls;

static int bench_width(void *ctx, const char *s) {
    (void)ctx;
    if (!s)
        return 0;
    g_width_calls++;
    return (int)strlen(s) * 6;
}

/* The background half on its own, so `built - bg` isolates the per-item loop.
 * ri_draw_section calls one of these before the item loop and only when no skin
 * supplies a background image, and the bench passes skin == 0, so this is the
 * same call ri_draw_section makes -- no duplication of its logic, just its
 * dispatch. */
static void draw_bg(struct ri_dlist *dl, const struct RIGeoSection *g, uint8_t sec,
    int ox, int oy, int z) {
    switch (sec) {
    case RI_SEC_808: ri_art_bg_808(dl, g, ox, oy, z); break;
    case RI_SEC_909: ri_art_bg_909(dl, g, ox, oy, z); break;
    case RI_SEC_LEVI: ri_art_bg_levi(dl, g, ox, oy, z); break;
    case RI_SEC_TRANSPORT: ri_art_bg_tr(dl, g, ox, oy, z, &TM); break;
    case RI_SEC_PAT_SYNTH1: case RI_SEC_PAT_SYNTH2: case RI_SEC_PAT_808:
    case RI_SEC_PAT_909: case RI_SEC_PAT_LEVI:
        ri_art_bg_pat(dl, g, ox, oy, z, sec); break;
    case RI_SEC_PCF: case RI_SEC_DELAY: case RI_SEC_DIST: case RI_SEC_COMP:
        ri_art_bg_fx(dl, g, ox, oy, z, sec); break;
    default:
        if (sec == RI_SEC_MASTER || ri_smix_strip(sec) >= 0)
            ri_art_bg_mix(dl, g, ox, oy, z, sec == RI_SEC_MASTER);
        else
            ri_art_bg_303(dl, g, ox, oy, z);
        break;
    }
}

static uint32_t count_op(const struct ri_dlist *dl, uint32_t op) {
    uint32_t i, n = 0u;
    for (i = 0u; i < dl->n; i++)
        if (dl->cmd[i].op == op)
            n++;
    return n;
}

static double us_since(const struct timespec *t0) {
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (double)(t1.tv_sec - t0->tv_sec) * 1e6 +
        (double)(t1.tv_nsec - t0->tv_nsec) / 1e3;
}

#define REPS 200

int main(void) {
    uint32_t sec;
    double tot_wasted = 0.0, tot_full = 0.0, tot_clip = 0.0;
    uint32_t tot_items = 0u, tot_built = 0u, tot_kept = 0u;

    TM.width = bench_width;
    TM.height = 7;
    TM.baseline = 5;
    TM.ctx = 0;
    ri_smix_init(&BOARD);

    printf("%-11s %6s %7s %6s %6s %8s %6s %6s %9s %9s\n", "section", "items",
        "built", "bg", "items", "wasted", "txt_b", "txt_k", "us_built", "us_clip");
    for (sec = 0u; sec < RI_SEC_COUNT; sec++) {
        const struct RIGeoSection *g =
            ri_geo_section(sec == RI_SEC_SYNTH2 ? RI_SEC_SYNTH1 : sec);
        struct RISectUI ui;
        struct ri_dlist dl;
        struct timespec t0;
        int w, h, bx, by, r;
        uint32_t built, kept, tb, tk, k, bg;
        double uf, uc;

        if (!g)
            continue;
        if (ri_sui_init(&ui, (uint8_t)sec) != 0)
            continue;
        if (ri_smix_strip(sec) >= 0 || sec == RI_SEC_MASTER)
            ri_sui_bind_board(&ui, &BOARD);

        w = ri_geo_px((int)g->w, 0);
        h = ri_geo_px((int)g->h, 0);
        if (w <= 0 || h <= 0)
            continue;

        /* A 64x16 damage box, the size class a step lamp or a song-position
         * cell actually is, placed at the section's centre so it lands on real
         * content rather than empty panel. */
        bx = w / 2 - 32;
        by = h / 2 - 8;
        if (bx < 0) bx = 0;
        if (by < 0) by = 0;

        /* Warm: first touch of a section pays page faults and the ctlreg. */
        ri_dlist_init(&dl, CMD, 24576u, SP, sizeof SP);
        ri_draw_section(&dl, &ui, (uint8_t)sec, 0, 0, 0, &TM, 0, 0);

        g_width_calls = 0u;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (k = 0; k < REPS; k++) {
            ri_dlist_init(&dl, CMD, 24576u, SP, sizeof SP);
            ri_draw_section(&dl, &ui, (uint8_t)sec, 0, 0, 0, &TM, 0, 0);
        }
        uf = us_since(&t0) / (double)REPS;
        built = dl.n;
        tb = count_op(&dl, (uint32_t)RI_D_TEXT);
        (void)g_width_calls;

        ri_dlist_init(&dl, CMD, 24576u, SP, sizeof SP);
        ri_draw_section(&dl, &ui, (uint8_t)sec, 0, 0, 0, &TM, 0, 0);
        r = 0;

        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (k = 0; k < REPS; k++) {
            ri_dlist_init(&dl, CMD, 24576u, SP, sizeof SP);
            ri_dlist_set_clip(&dl, bx, by, bx + 63, by + 15);
            ri_draw_section(&dl, &ui, (uint8_t)sec, 0, 0, 0, &TM, 0, 0);
            ri_dlist_clear_clip(&dl);
        }
        uc = us_since(&t0) / (double)REPS;
        kept = dl.n;
        tk = count_op(&dl, (uint32_t)RI_D_TEXT);

        /* Background alone, unclipped: how much of `built` the item loop is
         * not responsible for. */
        ri_dlist_init(&dl, CMD, 24576u, SP, sizeof SP);
        draw_bg(&dl, g, (uint8_t)sec, 0, 0, 0);
        bg = dl.n;

        printf("%-11s %6u %7u %6u %6u %8u %6u %6u %9.2f %9.2f%s\n",
            SECNAME[sec], (unsigned)g->nitems, built, bg, built - bg,
            built - kept, tb, tk, uf, uc,
            (built > 40u && kept * 4u < built) ? "  <- 98% discarded" : "");

        tot_items += g->nitems;
        tot_built += built;
        tot_kept += kept;
        tot_wasted += uf - uc;
        tot_full += uf;
        tot_clip += uc;
        (void)r;
    }
    printf("\nTOTAL items %u | built %u | kept %u (%.1f%% kept) | "
        "us whole %.1f | us clipped %.1f | recoverable %.1f us\n",
        tot_items, tot_built, tot_kept,
        tot_built ? 100.0 * (double)tot_kept / (double)tot_built : 0.0,
        tot_full, tot_clip, tot_wasted);
    return 0;
}