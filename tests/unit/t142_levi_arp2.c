/* t142_levi_arp2 — Levi P8b device arp params + phrases.
 * Laws: arp off bit-identical (tempo set); UP cycles chord order;
 * deterministic repeat; gate trims vs legato; octave transposes;
 * phrase follows factory rule; swing shifts odd strikes; ratchet
 * multiplies; chance thins; length wraps; latch sustains; entropy
 * leaps; stepoff rotates; clocklock restarts grid; DM_ARP routes;
 * factory content pinned; mode clamps; keys/pages/texts; null-safe;
 * finite extremes.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_arp.h"
#include "engine/dsp/levi_matrix.h"
#include "engine/seq/autolane.h"
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"

#define SR 48000.0f
#define BS 256u

static struct RILeviSet A, B;

static void hold_chord(struct RILeviSet *s) {
    levi_note_on(s, 60u);
    levi_note_on(s, 64u);
    levi_note_on(s, 67u);
}

/* Voice-0 note trajectory over nblocks (mono: voice 0 = last strike). */
static void traj(struct RILeviSet *s, uint8_t *out, uint32_t nblocks) {
    static float l[BS], r[BS];
    uint32_t i;
    for (i = 0u; i < nblocks; i++) {
        levi_arp_block(s, SR, BS);
        levi_voice_render_sum_stereo(s, l, r, BS, SR);
        out[i] = s->v[0].note;
    }
}

static void run_blocks(struct RILeviSet *s, uint32_t nblocks) {
    static float l[BS], r[BS];
    uint32_t i;
    for (i = 0u; i < nblocks; i++) {
        levi_arp_block(s, SR, BS);
        levi_voice_render_sum_stereo(s, l, r, BS, SR);
    }
}

static void arp_fast(struct RILeviSet *s) {
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_ARPON & 0xFFu, 1u);
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_ARPRATE & 0xFFu, 127u);
    levi_set_tempo(s, 140.0f);
}

static uint32_t active_count(struct RILeviSet *s) {
    uint32_t i, n = 0u;
    for (i = 0u; i < RI_LEVI_NVOICES; i++)
        n += s->v[i].active ? 1u : 0u;
    return n;
}

int main(void) {
    uint32_t i;

    /* Factory phrase rule pinned (own rule; guards table edits). */
    RI_ASSERT(ri_levi_arp_phrase(0u, 0u) == 100, "phrase rest");
    RI_ASSERT(ri_levi_arp_phrase(0u, 1u) == 5, "phrase step1");
    RI_ASSERT(ri_levi_arp_phrase(63u, 15u) == 12, "phrase last");
    RI_ASSERT(ri_levi_arp_phrase(64u, 0u) == 0, "bad bank refused");

    /* Arp off (default): bit-identical even with tempo + held chord. */
    levi_init_set(&A);
    hold_chord(&A);
    levi_set_tempo(&A, 140.0f);
    run_blocks(&A, 40u);
    {
        uint8_t na[RI_LEVI_NVOICES];
        for (i = 0u; i < RI_LEVI_NVOICES; i++)
            na[i] = A.v[i].note;
        levi_init_set(&B);
        hold_chord(&B);
        levi_set_tempo(&B, 140.0f);
        run_blocks(&B, 40u);
        for (i = 0u; i < RI_LEVI_NVOICES; i++)
            RI_ASSERT(B.v[i].note == na[i], "arp off deterministic %u", i);
    }

    /* UP cycles chord order over the first strikes (mono: voice 0
     * carries every strike; the user note 67 gives way to 60). */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    hold_chord(&A);
    arp_fast(&A);
    {
        uint8_t tr[70], i, seen = 0u;
        traj(&A, tr, 70u);
        RI_ASSERT(tr[0] == 60u, "first strike %u", tr[0]);
        for (i = 1u; i < 70u; i++) {
            if (tr[i] == 64u && seen == 0u)
                seen = 1u;
            if (tr[i] == 67u && seen == 1u)
                seen = 2u;
        }
        RI_ASSERT(seen == 2u, "up order cycles");
    }

    /* Division knob sets strike density (6 vs 2 strikes/120 blocks). */
    levi_init_set(&A);
    hold_chord(&A);
    arp_fast(&A);
    run_blocks(&A, 120u);
    RI_ASSERT(A.arp_nstr == 6u, "fast division %u", A.arp_nstr);
    levi_init_set(&B);
    hold_chord(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPON & 0xFFu, 1u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPRATE & 0xFFu, 32u);
    levi_set_tempo(&B, 140.0f);
    run_blocks(&B, 120u);
    RI_ASSERT(B.arp_nstr == 2u, "slow division %u", B.arp_nstr);

    /* Nonzero phrase selects another factory row (63: 72, 64, ...). */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    hold_chord(&A);
    arp_fast(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPMODE & 0xFFu, RI_LEVI_ARP_PHRASE);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPPHRASE & 0xFFu, 63u);
    {
        uint8_t tr[50];
        traj(&A, tr, 50u);
        RI_ASSERT(tr[0] == 72u && tr[21] == 64u, "phrase63 %u %u", tr[0], tr[21]);
    }

    /* Scrambled hold order still plays low -> high (chord sort). */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    levi_note_on(&A, 67u);
    levi_note_on(&A, 60u);
    levi_note_on(&A, 64u);
    arp_fast(&A);
    {
        uint8_t tr[70], i, seen = 0u;
        traj(&A, tr, 70u);
        RI_ASSERT(tr[0] == 60u, "sorted first %u", tr[0]);
        for (i = 1u; i < 70u; i++) {
            if (tr[i] == 64u && seen == 0u)
                seen = 1u;
            if (tr[i] == 67u && seen == 1u)
                seen = 2u;
        }
        RI_ASSERT(seen == 2u, "sorted order cycles");
    }

    /* Deterministic repeat of a full musical run. */
    levi_init_set(&A);
    hold_chord(&A);
    arp_fast(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPSWING & 0xFFu, 64u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPRATCHET & 0xFFu, 40u);
    run_blocks(&A, 120u);
    {
        uint8_t na[RI_LEVI_NVOICES], aa[RI_LEVI_NVOICES];
        for (i = 0u; i < RI_LEVI_NVOICES; i++) {
            na[i] = A.v[i].note;
            aa[i] = A.v[i].active;
        }
        levi_init_set(&B);
        hold_chord(&B);
        arp_fast(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPSWING & 0xFFu, 64u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPRATCHET & 0xFFu, 40u);
        run_blocks(&B, 120u);
        for (i = 0u; i < RI_LEVI_NVOICES; i++)
            RI_ASSERT(B.v[i].note == na[i] && B.v[i].active == aa[i], "repeat %u", i);
    }

    /* Gate: min trims (voices released mid-cycle), max legato. */
    levi_init_set(&A);
    hold_chord(&A);
    arp_fast(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPGATE & 0xFFu, 0u);
    run_blocks(&A, 60u);
    {
        uint32_t atrim = active_count(&A);
        levi_init_set(&B);
        hold_chord(&B);
        arp_fast(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPGATE & 0xFFu, 127u);
        run_blocks(&B, 60u);
        RI_ASSERT(active_count(&B) > atrim, "gate trims %u vs %u", atrim, active_count(&B));
    }

    /* Octave UP range 4 reaches two octaves up. */
    levi_init_set(&A);
    hold_chord(&A);
    arp_fast(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPOCTMODE & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPOCTRANGE & 0xFFu, 127u);
    run_blocks(&A, 200u);
    {
        uint32_t hi = 0u;
        for (i = 0u; i < RI_LEVI_NVOICES; i++)
            if (A.v[i].note > hi)
                hi = A.v[i].note;
        RI_ASSERT(hi >= 67u + 24u, "octave reaches %u", hi);
    }

    /* Phrase mode follows the factory rule (phrase 0: rest, 65, 71). */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    hold_chord(&A);
    arp_fast(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPMODE & 0xFFu, RI_LEVI_ARP_PHRASE);
    {
        uint8_t tr[50];
        traj(&A, tr, 50u);
        RI_ASSERT(tr[0] == 67u, "phrase rest first %u", tr[0]);
        RI_ASSERT(tr[21] == 65u && tr[41] == 71u, "phrase notes %u %u", tr[21], tr[41]);
    }

    /* Swing shifts odd strikes (slow division for block margins;
     * mono: voice 0 carries the strike notes). */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    hold_chord(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPON & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPRATE & 0xFFu, 32u);
    levi_set_tempo(&A, 140.0f);
    {
        uint8_t tr[400], b = 0u;
        uint32_t i;
        traj(&A, tr, 130u);
        for (i = 1u; i < 130u; i++)
            if (tr[i] == 64u) {
                b = (uint8_t)i;
                break;
            }
        RI_ASSERT(b > 0u, "straight strike1 lands");
        levi_init_set(&B);
        levi_set_alloc_ui(&B, RI_LEVI_POLY_MONO);
        hold_chord(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPON & 0xFFu, 1u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPRATE & 0xFFu, 32u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPSWING & 0xFFu, 127u);
        levi_set_tempo(&B, 140.0f);
        {
            uint8_t tr2[400];
            traj(&B, tr2, b + 1u);
            RI_ASSERT(tr[b] == 64u && tr2[b] == 60u, "swing shifts %u vs %u", tr[b], tr2[b]);
        }
    }

    /* Ratchet multiplies strikes (further allocator rotation). */
    levi_init_set(&A);
    hold_chord(&A);
    arp_fast(&A);
    run_blocks(&A, 120u);
    {
        uint8_t na[RI_LEVI_NVOICES];
        for (i = 0u; i < RI_LEVI_NVOICES; i++)
            na[i] = A.v[i].note;
        levi_init_set(&B);
        hold_chord(&B);
        arp_fast(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPRATCHET & 0xFFu, 127u);
        run_blocks(&B, 120u);
        {
            uint32_t same = 1u;
            for (i = 0u; i < RI_LEVI_NVOICES; i++)
                if (B.v[i].note != na[i])
                    same = 0u;
            RI_ASSERT(!same, "ratchet moves");
        }
    }

    /* Chance 127 thins to nearly nothing (strike counter). */
    levi_init_set(&A);
    hold_chord(&A);
    arp_fast(&A);
    run_blocks(&A, 120u);
    RI_ASSERT(A.arp_nstr == 6u, "plain strikes %u", A.arp_nstr);
    levi_init_set(&B);
    hold_chord(&B);
    arp_fast(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPCHANCE & 0xFFu, 127u);
    run_blocks(&B, 120u);
    RI_ASSERT(B.arp_nstr <= 1u, "chance thins %u", B.arp_nstr);

    /* Length 1 repeats the chord root (mono: voice 0 never leaves C). */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    hold_chord(&A);
    arp_fast(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPLEN & 0xFFu, 0u);
    {
        uint8_t tr[130], i, ok = 1u;
        traj(&A, tr, 130u);
        for (i = 1u; i < 130u; i++)
            if (tr[i] != 60u)
                ok = 0u;
        RI_ASSERT(ok, "length wraps");
    }

    /* Latch sustains after release; unlatch goes quiet (short gate so
     * old strikes expire; the strike counter tells the tale). */
    levi_init_set(&A);
    hold_chord(&A);
    arp_fast(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPGATE & 0xFFu, 0u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPLATCH & 0xFFu, 1u);
    run_blocks(&A, 60u);
    levi_note_off(&A, 60u);
    levi_note_off(&A, 64u);
    levi_note_off(&A, 67u);
    {
        uint32_t n0 = A.arp_nstr;
        run_blocks(&A, 60u);
        RI_ASSERT(A.arp_nstr > n0, "latch sustains %u", A.arp_nstr - n0);
    }
    levi_init_set(&B);
    hold_chord(&B);
    arp_fast(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPGATE & 0xFFu, 0u);
    run_blocks(&B, 60u);
    levi_note_off(&B, 60u);
    levi_note_off(&B, 64u);
    levi_note_off(&B, 67u);
    {
        uint32_t n0 = B.arp_nstr;
        run_blocks(&B, 120u);
        RI_ASSERT(B.arp_nstr == n0, "unlatch stops");
        RI_ASSERT(active_count(&B) == 0u, "unlatch quiet %u", active_count(&B));
    }

    /* Entropy leaps where none stood. */
    levi_init_set(&A);
    hold_chord(&A);
    arp_fast(&A);
    run_blocks(&A, 200u);
    {
        uint8_t na[RI_LEVI_NVOICES];
        for (i = 0u; i < RI_LEVI_NVOICES; i++)
            na[i] = A.v[i].note;
        levi_init_set(&B);
        hold_chord(&B);
        arp_fast(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPENTROPY & 0xFFu, 127u);
        run_blocks(&B, 200u);
        {
            uint32_t same = 1u;
            for (i = 0u; i < RI_LEVI_NVOICES; i++)
                if (B.v[i].note != na[i])
                    same = 0u;
            RI_ASSERT(!same, "entropy leaps");
        }
    }

    /* Step offset rotates the start (mono: off 4 starts on E). */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    hold_chord(&A);
    arp_fast(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPSTEPPOFF & 0xFFu, 34u);
    {
        uint8_t tr[8];
        traj(&A, tr, 8u);
        RI_ASSERT(tr[0] == 64u, "stepoff starts %u", tr[0]);
    }

    /* Clock lock restarts the grid on chord change (mono: locked ends
     * on D after D,F,A,D; free ends on F after A,D,F). */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    hold_chord(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPON & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPRATE & 0xFFu, 32u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPCLOCK & 0xFFu, 1u);
    levi_set_tempo(&A, 140.0f);
    run_blocks(&A, 100u);
    levi_note_off(&A, 60u);
    levi_note_off(&A, 64u);
    levi_note_off(&A, 67u);
    levi_note_on(&A, 62u);
    levi_note_on(&A, 65u);
    levi_note_on(&A, 69u);
    {
        uint8_t tr[300];
        traj(&A, tr, 300u);
        RI_ASSERT(tr[299] == 62u, "locked ends %u", tr[299]);
    }
    levi_init_set(&B);
    levi_set_alloc_ui(&B, RI_LEVI_POLY_MONO);
    hold_chord(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPON & 0xFFu, 1u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPRATE & 0xFFu, 32u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_ARPCLOCK & 0xFFu, 0u);
    levi_set_tempo(&B, 140.0f);
    run_blocks(&B, 100u);
    levi_note_off(&B, 60u);
    levi_note_off(&B, 64u);
    levi_note_off(&B, 67u);
    levi_note_on(&B, 62u);
    levi_note_on(&B, 65u);
    levi_note_on(&B, 69u);
    {
        uint8_t tr[300];
        traj(&B, tr, 300u);
        RI_ASSERT(tr[299] == 65u, "free ends %u", tr[299]);
    }

    /* DM_ARP module: 11 params; route fills lead offsets; pinned
     * offsets move the gate timing (notes + gates observed). */
    RI_ASSERT(ri_levi_dm_nparam(RI_LEVI_DM_ARP) == 11u, "arp nparam");
    levi_init_set(&B);
    hold_chord(&B);
    arp_fast(&B);
    levi_set_mx_ui(&B, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_LFO0));
    levi_set_mx_ui(&B, 0u, 1u, RI_LEVI_DM_ARP);
    levi_set_mx_ui(&B, 0u, 2u, RI_LEVI_DA_GATE);
    levi_set_mx_ui(&B, 0u, 3u, 100u);
    run_blocks(&B, 30u);
    {
        uint32_t any = 0u;
        for (i = 0u; i < RI_LEVI_NVOICES; i++)
            any |= B.v[i].axm_on;
        RI_ASSERT(any, "dm arp route fills");
    }
    levi_init_set(&A);
    hold_chord(&A);
    arp_fast(&A);
    run_blocks(&A, 120u);
    {
        uint8_t na[RI_LEVI_NVOICES], aa[RI_LEVI_NVOICES];
        for (i = 0u; i < RI_LEVI_NVOICES; i++) {
            na[i] = A.v[i].note;
            aa[i] = A.v[i].active;
        }
        levi_init_set(&B);
        hold_chord(&B);
        arp_fast(&B);
        for (i = 0u; i < 120u; i++) {
            uint32_t v;
            for (v = 0u; v < RI_LEVI_NVOICES; v++) {
                B.v[v].axm[RI_LEVI_DA_GATE] = -1.0f;
                B.v[v].axm_on = 1u;
            }
            run_blocks(&B, 1u);
        }
        {
            uint32_t same = 1u;
            for (i = 0u; i < RI_LEVI_NVOICES; i++)
                if (B.v[i].note != na[i] || B.v[i].active != aa[i])
                    same = 0u;
            RI_ASSERT(!same, "dm arp gate moves");
        }
    }

    /* Mode clamps to PHRASE (8) at the engine door. */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    hold_chord(&A);
    arp_fast(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPMODE & 0xFFu, 200u);
    {
        uint8_t tr[50];
        traj(&A, tr, 50u);
        RI_ASSERT(tr[0] == 67u && tr[21] == 65u && tr[41] == 71u, "clamped phrase %u %u %u",
            tr[0], tr[21], tr[41]);
    }

    /* Keys / allow-list / pages / texts. */
    {
        uint32_t k;
        for (k = 0x0EA0u; k <= 0x0EACu; k++)
            RI_ASSERT(ri_auto_allowed((uint16_t)k), "arp key %04x allowed", k);
    }
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_ARPMODE), "arpmode allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_ARPSTEPPOFF), "stepoff allowed");
    RI_ASSERT(!ri_auto_allowed(0x0EBFu), "0x0EBF refused (P8d)");
    {
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | 186u));
        RI_ASSERT(d && d->engine_id == RI_CTL_LEVI_ARPMODE, "arpmode row binds");
    }
    {
        struct RISectLevi lv;
        char tx[16];
        memset(&lv, 0, sizeof lv);
        ri_slevi_init(&lv);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_ARP) == 1, "arp module");
        RI_ASSERT(ri_slevi_page_count(&lv) == 2u, "arp 2 pages");
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && !strcmp(ri_slevi_enc_name(&lv, 0u), "DIVISION"), "slot0 div");
        RI_ASSERT(ri_slevi_enc_live(&lv, 4u) && !strcmp(ri_slevi_enc_name(&lv, 4u), "MODE"), "slot4 mode");
        lv.page = 0u;
        ri_slevi_enc_text(&lv, 4u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "UP"), "mode text %s", tx);
        RI_ASSERT(ri_slevi_press(&lv, RI_SLEVI_PAGEDN) == 1, "page 2");
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && !strcmp(ri_slevi_enc_name(&lv, 0u), "ENTROPY"), "p2 entropy");
    }

    /* Null-safe entry points. */
    levi_arp_block(0, SR, BS);
    RI_ASSERT(levi_note_strike(0, 60u) < 0, "strike null refused");
    RI_ASSERT(levi_arp_release_note(0, 60u) < 0, "release null refused");

    /* Finite/bounded extremes storm (all modes, max everything). */
    {
        uint32_t t, bad = 0;
        static float l[BS], r[BS];
        for (t = 0u; t < RI_LEVI_ARP_NMODES; t++) {
            int i;
            levi_init_set(&A);
            hold_chord(&A);
            arp_fast(&A);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPMODE & 0xFFu, (uint8_t)t);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPGATE & 0xFFu, 127u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPSWING & 0xFFu, 127u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPRATCHET & 0xFFu, 127u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPCHANCE & 0xFFu, 64u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_ARPENTROPY & 0xFFu, 127u);
            levi_trigger(&A, 0u, 60u);
            for (i = 0; i < 200; i++) {
                levi_arp_block(&A, SR, BS);
                levi_voice_render_sum_stereo(&A, l, r, BS, SR);
                {
                    uint32_t k;
                    for (k = 0u; k < BS; k++) {
                        float a = l[k], b = r[k];
                        if (!((a > -8.0f && a < 8.0f) && (b > -8.0f && b < 8.0f)))
                            bad = 1;
                    }
                }
            }
        }
        RI_ASSERT(!bad, "extremes bounded");
    }

    RI_RESULT("t142_levi_arp2");
}
