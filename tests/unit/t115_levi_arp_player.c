/* t115_levi_arp_player — arp in the song path (v2 feature 3b-ii/2).
 * Player-owned arp cfg (default off): off = baseline Levi emission;
 * on = occurrence window rewritten (subdivided strikes, chord notes,
 * balanced gates); deterministic across re-init; fail-closed setter.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/player.h"
#include "engine/seq/pattern.h"
#include "engine/dsp/levi_arp.h"

#define SRU 48000u
static const struct RISegment SEG0[] = { { 0, 428571428ULL } }; /* 140 BPM */
static const struct RITempoMap MAP = { SEG0, 1, 96, SRU };
static struct RIPatternBank B4[5];
static struct RIEvent EV[512];

static uint32_t levi_ons(const struct RIEvent *ev, uint32_t n) {
    uint32_t i, c = 0u;
    for (i = 0u; i < n; i++)
        if (ev[i].device == 4u && ev[i].type == RI_EV_NOTE_ON)
            c++;
    return c;
}

static void gates_balanced(const struct RIEvent *ev, uint32_t n) {
    uint32_t v, i;
    for (v = 0u; v < 6u; v++) {
        uint32_t on = 0u, off = 0u;
        for (i = 0u; i < n; i++)
            if (ev[i].device == 4u && ev[i].voice == v) {
                on += ev[i].type == RI_EV_NOTE_ON;
                off += ev[i].type == RI_EV_NOTE_OFF;
            }
        RI_ASSERT(on == off, "levi voice %u gate %u/%u", v, on, off);
    }
}

int main(void) {
    const struct RIPatternBank *c5[5];
    struct RISongTrack tr;
    struct RIPlayer pl;
    struct RILeviArpCfg up = { 1u, RI_LEVI_ARP_UP, 127u, 0u };
    uint32_t i, n0, n1, k;
    uint64_t bar = 384u; /* ppq 96, 4/4 */
    for (i = 0u; i < 5u; i++) {
        uint32_t kind = i < 2u ? RI_PATTERN_KIND_303 : i < 4u ? RI_PATTERN_KIND_DRUM : RI_PATTERN_KIND_LEVI;
        uint32_t cls = i == 2u ? RI_DRUM_CLASS_808 : i == 3u ? RI_DRUM_CLASS_909 : 0u;
        ri_bank_init(&B4[i], i, kind, cls);
        ri_pattern_set_length(&B4[i].pat[0], 16u);
        c5[i] = &B4[i];
    }
    RI_ASSERT(ri_levi_set(&B4[4].pat[0], 0u, 0u, 60u, 1) == 0, "set");
    RI_ASSERT(ri_levi_set(&B4[4].pat[0], 0u, 1u, 64u, 1) == 0, "set");
    RI_ASSERT(ri_levi_set(&B4[4].pat[0], 0u, 2u, 67u, 1) == 0, "set");
    ri_track_init(&tr);
    /* Fail-closed setter. */
    RI_ASSERT(ri_player_levi_arp(0, &up) == 2, "null player");
    /* Baseline: arp default off. */
    ri_player_init(&pl, c5, &tr, 0u);
    n0 = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, bar, EV, 512u);
    RI_ASSERT(n0 > 0u && levi_ons(EV, n0) == 3u, "baseline 3 levi ons, got %u", levi_ons(EV, n0));
    /* On: same block subdivides (more strikes, chord notes, balanced). */
    ri_player_init(&pl, c5, &tr, 0u);
    RI_ASSERT(ri_player_levi_arp(&pl, &up) == 0, "set arp");
    n1 = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, bar, EV, 512u);
    RI_ASSERT(levi_ons(EV, n1) > 3u, "subdivided %u", levi_ons(EV, n1));
    for (k = 0u; k < n1; k++)
        if (EV[k].device == 4u && EV[k].type == RI_EV_NOTE_ON)
            RI_ASSERT(EV[k].value == 60 || EV[k].value == 64 || EV[k].value == 67,
                "strike in chord");
    gates_balanced(EV, n1);
    /* Deterministic: re-init + same cfg replays identically. */
    {
        struct RIEvent EV2[512];
        uint32_t n2;
        ri_player_init(&pl, c5, &tr, 0u);
        RI_ASSERT(ri_player_levi_arp(&pl, &up) == 0, "reset arp");
        n2 = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, bar, EV2, 512u);
        RI_ASSERT(n2 == n1 && !memcmp(EV, EV2, (size_t)n1 * sizeof EV[0]), "replay");
    }
    RI_RESULT("leviarpplayer");
}
