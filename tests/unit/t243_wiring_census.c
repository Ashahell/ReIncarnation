/* t243_wiring_census — every registry row through every door that can be
 * measured host-side, and a ratchet gate on the result.
 *
 * Rewritten 2026-11-21 (advisor review of the first version). The first
 * version reported verdicts it never measured:
 *   - D3 (MIDI in) and D9 (keyboard) called ri_panel_ctl_send directly, so
 *     they could only ever agree with D1;
 *   - "feedback" was "any meter peak after a 64-sample render with no notes",
 *     which is silence on every row, so 2010 NOFEEDBACK verdicts were the
 *     check itself;
 *   - the gate ran only with --gate, which the audit never passed, and the
 *     allow-list was generated from the verdicts it was meant to judge.
 *
 * What this version measures, per row (the reference is D1):
 *   D1 PANEL    ri_panel_ctl_send -> control plane -> engine.     OK | DEAD
 *   D3 MIDI_IN  the real map: ri_midi_msg on a bound panel, then the push
 *               RIAPP does (ri_panel_ctl_send of the reported touch).
 *                                                 OK | DEAD | DIVERGE
 *   D5 AUTO     ri_auto_touch record, then the event replayed.
 *                                                 OK | DEAD | DIVERGE
 *   D6 SAVELOAD the automation event through an RBNG write + read (the ATRK
 *               lane RIAPP loads), then replayed.  OK | NOROUNDTRIP | DIVERGE
 *   D7 STARTUP  the panel default sent to a fresh engine must not move it.
 *                                                 OK | DEFAULT
 * Not measurable here, reported as NOTMEASURED, never as a pass or a fail:
 *   D2 Levi page encoders (docs/evidence/levi-wiring/.../levi_audit.c),
 *   D4 MIDI out, D8 lamp/LED/meter/repaint, D9 keyboard, D10 menus,
 *   D11 undo, D12 ARexx (the RIAPP effects are AROS-side; lane proof X3).
 * RI_BIND_NONE rows get one class instead of door verdicts: PRESSPATH
 * (transport / pattern sections), PANELLOCAL (button, step, display, meter,
 * LED, the Levi Master group) or UNEXPLAINED.
 *
 * GATE (always on): every non-OK verdict must be in the committed baseline
 * docs/evidence/wiring/2026-11-21-census/baseline.tsv. A NEW bad verdict
 * fails; a baseline entry that now reads OK fails too (remove it). The
 * baseline is a ratchet, not a judgement: its rows are the W1 backlog.
 * RI_CENSUS_WRITE_BASELINE=1 writes the current set to RI_TMP for review.
 *
 * Controls: a row with a bogus reg_id must read DEAD through the same
 * function the census uses; a D3 result compared against the wrong value
 * must read DIVERGE; t224's strip-mute shape (a mixer On/Off switch with
 * bind NONE) must read UNEXPLAINED; and at least one row must read OK.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>
#include "tests/helpers/ri_assert.h"
#include "platform/pal/ri_tmp.h"
#include "gui/ctlreg.h"
#include "gui/panelctl.h"
#include "gui/midimap.h"
#include "gui/sectui.h"
#include "gui/sectfx.h"
#include "gui/sectmix.h"
#include "gui/keymap.h"
#include "engine/engine.h"
#include "engine/seq/autolane.h"
#include "engine/seq/ctlplane.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_fx.h"
#include "project/rbng.h"

#define T243_DIR RI_TMP_ROOT "/run/t243"
#define T243_BASELINE "docs/evidence/wiring/2026-11-21-census/baseline.tsv"

enum { D_PANEL, D_LEVI_ENC, D_MIDI_IN, D_MIDI_OUT, D_AUTO, D_SAVELOAD,
       D_STARTUP, D_FEEDBACK, D_KEYBOARD, D_MENUS, D_UNDO, D_AREXX, D_COUNT };
static const char *const k_door[D_COUNT] = {
    "D1_PANEL", "D2_LEVI_ENC", "D3_MIDI_IN", "D4_MIDI_OUT", "D5_AUTO",
    "D6_SAVELOAD", "D7_STARTUP", "D8_FEEDBACK", "D9_KEYBOARD", "D10_MENUS",
    "D11_UNDO", "D12_AREXX" };

enum { V_OK, V_NA, V_NOTMEASURED, V_DEAD, V_DIVERGE, V_DEFAULT, V_NOROUNDTRIP,
       V_PRESSPATH, V_PANELLOCAL, V_UNEXPLAINED, V_COUNT };
static const char *const k_verdict[V_COUNT] = {
    "OK", "NA", "NOTMEASURED", "DEAD", "DIVERGE", "DEFAULT", "NOROUNDTRIP",
    "BINDNONE_PRESSPATH", "BINDNONE_PANELLOCAL", "BINDNONE_UNEXPLAINED" };

static int is_bad(int v) {
    return v == V_DEAD || v == V_DIVERGE || v == V_DEFAULT || v == V_NOROUNDTRIP ||
        v == V_UNEXPLAINED;
}

/* Same engine equality as t116 (skips the Levi reverb line pointers). */
static int engines_equal(const struct RIEngine *a, const struct RIEngine *b) {
    size_t r0 = offsetof(struct RIEngine, slevi) +
        offsetof(struct RILeviSet, fx) + offsetof(struct RILeviFx, rvl);
    size_t r1 = r0 + 2u * sizeof(struct RIReverb);
    const struct RILeviFx *fa = &a->slevi.fx, *fb = &b->slevi.fx;
    if (memcmp(a, b, r0))
        return 0;
    if (memcmp((const unsigned char *)a + r1, (const unsigned char *)b + r1, sizeof *a - r1))
        return 0;
    return fa->rvl.fb == fb->rvl.fb && fa->rvr.fb == fb->rvr.fb &&
        fa->rvl.cap == fb->rvl.cap && fa->rvr.cap == fb->rvr.cap &&
        !memcmp(fa->rvl.len, fb->rvl.len, sizeof fa->rvl.len) &&
        !memcmp(fa->rvr.len, fb->rvr.len, sizeof fa->rvr.len);
}

static struct RIEngine g_base, g_ref, g_e; /* large: static, not stack */

static void fresh(struct RIEngine *e) {
    memset(e, 0, sizeof *e);
    ri_engine_init(e);
    ri_engine_defaults(e);
}

/* Drain the plane into the engine; returns the number of events applied. */
static uint32_t drain_into(struct RIControlPlane *pl, struct RIEngine *e) {
    struct RIEvent ev[16];
    uint32_t seq = 0, nd, q, total = 0;
    while ((nd = ri_ctl_drain(pl, ev, 16u, 0u, &seq)) > 0u) {
        for (q = 0; q < nd; q++)
            ri_engine_apply_event(e, &ev[q]);
        total += nd;
    }
    return total;
}

/* D1: the reference door. 1 when anything reached the engine. */
static int via_panel(struct RIEngine *e, uint16_t reg_id, int value) {
    struct RIControlPlane pl;
    ri_ctl_init(&pl);
    if (ri_panel_ctl_send(&pl, reg_id, value) != 0)
        return 0;
    return drain_into(&pl, e) > 0u;
}

/* The value the bridge puts on the plane for a panel value: what live.c
 * records (it records after the bridge), so D5/D6 record this, not the
 * raw panel value. -1 when nothing is sent. */
static int bridged(uint16_t reg_id, int value) {
    struct RIControlPlane pl;
    struct RIEvent ev[4];
    uint32_t seq = 0;
    ri_ctl_init(&pl);
    if (ri_panel_ctl_send(&pl, reg_id, value) != 0)
        return -1;
    if (ri_ctl_drain(&pl, ev, 4u, 0u, &seq) == 0u)
        return -1;
    return (int)ev[0].flags;
}

static void apply_auto(struct RIEngine *e, uint16_t ctl, uint8_t val) {
    struct RIEvent re;
    memset(&re, 0, sizeof re);
    re.type = RI_EV_AUTOMATION;
    re.value = ctl;
    re.flags = val;
    ri_engine_apply_event(e, &re);
}

/* ---- the MIDI fixture: real section objects, bound as riapp.c binds them
 * (t232's fixture; a zeroed RIPanelUI makes ui_of() return NULL). ---- */
static struct RIPanelUI g_panel;
static struct RIMidiIn g_midi;
static struct RISectUI g_fx[4], g_mix[6], g_tr, g_synth[2], g_drum[2], g_pat[RI_FOCUS_COUNT];
static struct RIMixBoard g_board;
static struct RIFxRack g_rack;

static void midi_fixture(void) {
    unsigned i;
    memset(&g_panel, 0, sizeof g_panel);
    ri_smix_init(&g_board);
    ri_sfx_rack_init(&g_rack);
    for (i = 0; i < 4; i++) {
        ri_sui_init(&g_fx[i], (uint8_t)(RI_SEC_PCF + i));
        ri_sui_bind_fx_rack(&g_fx[i], &g_rack);
        g_panel.fx[i] = &g_fx[i];
    }
    for (i = 0; i < 6; i++) {
        uint8_t sec = i == 5u ? (uint8_t)RI_SEC_MIX_LEVI
            : (uint8_t)(i == 4u ? RI_SEC_MASTER : (uint32_t)RI_SEC_MIX_SYNTH1 + i);
        ri_sui_init(&g_mix[i], sec);
        ri_sui_bind_board(&g_mix[i], &g_board);
        g_panel.mix[i] = &g_mix[i];
    }
    ri_sui_init(&g_synth[0], RI_SEC_SYNTH1);
    ri_sui_init(&g_synth[1], RI_SEC_SYNTH2);
    g_panel.synth[0] = &g_synth[0];
    g_panel.synth[1] = &g_synth[1];
    ri_sui_init(&g_drum[0], RI_SEC_808);
    ri_sui_init(&g_drum[1], RI_SEC_909);
    g_panel.drum[0] = &g_drum[0];
    g_panel.drum[1] = &g_drum[1];
    for (i = 0; i < RI_FOCUS_COUNT; i++) {
        ri_sui_init(&g_pat[i], (uint8_t)(RI_SEC_PAT_SYNTH1 + i));
        g_panel.pat[i] = &g_pat[i];
    }
    ri_sui_init(&g_tr, RI_SEC_TRANSPORT);
    g_panel.tr = &g_tr;
    ri_midi_init(&g_midi, 0u);
}

/* The value a door drives a row to: an end of its range (max first, then
 * min) that moves a fresh engine. A row whose default sits where the engine
 * already is would otherwise read DEAD at the one end that is a no-op. */
static int g_alt;
static int alt_of(const struct RICtlDef *d) {
    (void)d;
    return g_alt;
}
static void choose_alt(const struct RICtlDef *d) {
    fresh(&g_e);
    g_alt = d->max_v;
    if (via_panel(&g_e, d->reg_id, g_alt) && !engines_equal(&g_e, &g_base))
        return;
    g_alt = d->min_v;
}

/* RIAPP's startup burst (app/riapp.c, after the demo mix): these rows are
 * sent at their panel value before the first buffer, so the engine adopts
 * the panel and a default that differs from the engine's own is by design.
 * Keep in step with that block. */
static int in_startup_burst(const struct RICtlDef *d) {
    uint32_t i = d->reg_id & 0xFFu;
    switch (d->section) {
    case RI_SEC_SYNTH1: case RI_SEC_SYNTH2: return i <= 6u;
    case RI_SEC_808: return i < 44u;   /* RI_S808_NCTL */
    case RI_SEC_909: return i < 46u;   /* RI_S909_NCTL */
    case RI_SEC_PCF: case RI_SEC_DELAY: case RI_SEC_DIST: case RI_SEC_COMP:
        return i < 8u;                 /* RI_SFX_NCTL */
    case RI_SEC_MIX_SYNTH1: case RI_SEC_MIX_SYNTH2: case RI_SEC_MIX_808:
    case RI_SEC_MIX_909: case RI_SEC_MIX_LEVI:
        return i >= 2u && i <= 7u;
    case RI_SEC_MASTER: return i == 0u;
    case RI_SEC_LEVI: return 1;        /* ri_panel_levi_adopt: every value */
    default: return 0;
    }
}

static int bindnone_class(const struct RICtlDef *d) {
    switch (d->section) {
    case RI_SEC_TRANSPORT: case RI_SEC_PAT_SYNTH1: case RI_SEC_PAT_SYNTH2:
    case RI_SEC_PAT_808: case RI_SEC_PAT_909: case RI_SEC_PAT_LEVI:
        return V_PRESSPATH;
    default:
        break;
    }
    if (d->kind == RI_CK_BUTTON || d->kind == RI_CK_STEP || d->kind == RI_CK_DISPLAY ||
        d->kind == RI_CK_METER || d->kind == RI_CK_LED)
        return V_PANELLOCAL;
    if (d->section == RI_SEC_LEVI && d->group && strcmp(d->group, "Master") == 0)
        return V_PANELLOCAL;
    /* 303 step-edit keys (Down/Up/Accent/Slide/Note/Pitch Mode): the
     * pattern editor's press path, not a sound parameter. */
    if ((d->section == RI_SEC_SYNTH1 || d->section == RI_SEC_SYNTH2) && d->group &&
        strcmp(d->group, "Step") == 0)
        return V_PRESSPATH;
    /* Levi edit-target selectors (lane / operator / module): choose what
     * the page shows, never recorded. */
    if (d->section == RI_SEC_LEVI && d->kind == RI_CK_SELECTOR && !d->automatable)
        return V_PANELLOCAL;
    return V_UNEXPLAINED;
}

/* D1 for one row against a given definition (the controls call this with
 * doctored copies). */
static int door_panel(const struct RICtlDef *d) {
    fresh(&g_e);
    if (!via_panel(&g_e, d->reg_id, alt_of(d)) || engines_equal(&g_e, &g_base))
        return V_DEAD;
    return V_OK;
}

/* D3, compared against D1 at the value the map reports it settled at. */
static int door_midi(const struct RICtlDef *d, int cmp_bias) {
    const struct RICtlDef *hit;
    if (d->midi_cc == RI_MIDI_CC_NONE)
        return V_NA;
    hit = ri_ctlreg_by_cc(d->midi_cc);
    if (!hit || hit->reg_id != d->reg_id)
        return V_DIVERGE; /* the CC reaches another row first */
    midi_fixture();
    ri_midi_msg(&g_midi, &g_panel, 0xB0u, d->midi_cc, alt_of(d) == d->max_v ? 127u : 0u);
    if (!g_midi.touched || g_midi.touch_reg != d->reg_id)
        return V_DEAD;
    fresh(&g_e);
    if (!via_panel(&g_e, g_midi.touch_reg, (int)g_midi.touch_val) || engines_equal(&g_e, &g_base))
        return V_DEAD;
    fresh(&g_ref);
    via_panel(&g_ref, d->reg_id, (int)g_midi.touch_val + cmp_bias);
    return engines_equal(&g_e, &g_ref) ? V_OK : V_DIVERGE;
}

static int door_auto(const struct RICtlDef *d, uint16_t key) {
    struct RIAutoEv ev[4];
    struct RIAutoLane lane;
    struct RIAutoPass pass;
    if (!d->automatable)
        return V_NA;
    memset(ev, 0, sizeof ev);
    memset(&pass, 0, sizeof pass);
    lane.ev = ev;
    lane.cap = 4u;
    lane.n = 0u;
    lane.flags = 0u;
    if (ri_auto_touch(&lane, &pass, RI_TR_RECORD, 0u, 96u, key,
            (uint8_t)bridged(d->reg_id, alt_of(d))) != 0 ||
        lane.n == 0u)
        return V_DEAD;
    fresh(&g_e);
    apply_auto(&g_e, lane.ev[0].ctl, lane.ev[0].val);
    if (engines_equal(&g_e, &g_base))
        return V_DEAD;
    return engines_equal(&g_e, &g_ref) ? V_OK : V_DIVERGE;
}

static struct RISong g_song, g_song2;
static struct RBAutoEv g_atrk[4], g_atrk2[4];

static int door_saveload(const struct RICtlDef *d, uint16_t key) {
    char err[160];
    uint32_t i;
    int found = -1;
    if (!d->automatable)
        return V_NA;
    rbng_song_init(&g_song);
    g_song.tempo = 120u;
    g_song.ppq = 96u;
    g_song.nsteps = 16u;
    g_song.atrk = g_atrk;
    g_song.atrk_cap = 4u;
    g_song.atrk[0].tick = 0u;
    g_song.atrk[0].ctl = key;
    g_song.atrk[0].val = (uint8_t)bridged(d->reg_id, alt_of(d));
    g_song.natrk = 1u;
    if (rbng_write_song(T243_DIR "/rt.rbng", &g_song, err, sizeof err) != 0) {
        if (getenv("T243_DEBUG"))
            printf("write %04x: %s\n", d->reg_id, err);
        return V_NOROUNDTRIP;
    }
    rbng_song_init(&g_song2);
    g_song2.atrk = g_atrk2;
    g_song2.atrk_cap = 4u;
    if (rbng_read_song(T243_DIR "/rt.rbng", &g_song2, err, sizeof err) != 0) {
        if (getenv("T243_DEBUG"))
            printf("read %04x: %s\n", d->reg_id, err);
        return V_NOROUNDTRIP;
    }
    for (i = 0; i < g_song2.natrk; i++)
        if (g_song2.atrk[i].ctl == key)
            found = (int)i;
    if (found < 0 || g_song2.atrk[found].val != g_song.atrk[0].val)
        return V_NOROUNDTRIP;
    fresh(&g_e);
    apply_auto(&g_e, g_song2.atrk[found].ctl, g_song2.atrk[found].val);
    return engines_equal(&g_e, &g_ref) ? V_OK : V_DIVERGE;
}

static int door_startup(const struct RICtlDef *d) {
    if (in_startup_burst(d))
        return V_OK; /* adopted at startup; t95 pins the burst itself */
    fresh(&g_e);
    via_panel(&g_e, d->reg_id, d->def_v);
    return engines_equal(&g_e, &g_base) ? V_OK : V_DEFAULT;
}

/* ---- census + baseline ---- */
#define NROWS_MAX 600u
static uint16_t g_reg[NROWS_MAX];
static uint8_t g_v[NROWS_MAX][D_COUNT];
static uint32_t g_n;

struct BaseEnt { uint16_t reg; uint8_t door, verdict, hit; };
static struct BaseEnt g_bl[8192];
static uint32_t g_nbl;

static int verdict_of(const char *s) {
    int i;
    for (i = 0; i < V_COUNT; i++)
        if (!strcmp(s, k_verdict[i]))
            return i;
    return -1;
}
static int door_of(const char *s) {
    int i;
    for (i = 0; i < D_COUNT; i++)
        if (!strcmp(s, k_door[i]))
            return i;
    return -1;
}

static void baseline_load(void) {
    char line[256], reg[16], door[32], ver[48];
    FILE *f = fopen(T243_BASELINE, "r");
    RI_ASSERT(f != NULL, "baseline %s missing (run from the repo root)", T243_BASELINE);
    if (!f)
        return;
    while (fgets(line, sizeof line, f)) {
        int dv, vv;
        if (line[0] == '#' || line[0] == '\n')
            continue;
        if (sscanf(line, "%15s %31s %47s", reg, door, ver) != 3)
            continue;
        dv = door_of(door);
        vv = verdict_of(ver);
        RI_ASSERT(dv >= 0 && vv >= 0, "baseline line not understood: %s", line);
        if (dv < 0 || vv < 0 || g_nbl >= sizeof g_bl / sizeof g_bl[0])
            continue;
        g_bl[g_nbl].reg = (uint16_t)strtoul(reg, NULL, 16);
        g_bl[g_nbl].door = (uint8_t)dv;
        g_bl[g_nbl].verdict = (uint8_t)vv;
        g_bl[g_nbl].hit = 0u;
        g_nbl++;
    }
    fclose(f);
}

static int baseline_has(uint16_t reg, int door, int v) {
    uint32_t i;
    for (i = 0; i < g_nbl; i++)
        if (g_bl[i].reg == reg && g_bl[i].door == door && g_bl[i].verdict == v) {
            g_bl[i].hit = 1u;
            return 1;
        }
    return 0;
}

int main(void) {
    uint32_t i, k;
    int d, nbad = 0, nnew = 0, nstale = 0, has_ok = 0;
    uint32_t count[V_COUNT];
    FILE *tsv;
    mkdir(RI_TMP_ROOT "/run", 0755);
    mkdir(T243_DIR, 0755);
    fresh(&g_base);
    memset(count, 0, sizeof count);

    for (k = 0; k < ri_ctlreg_count() && g_n < NROWS_MAX; k++) {
        const struct RICtlDef *r = ri_ctlreg_at(k);
        uint16_t key;
        if (!r)
            continue;
        g_reg[g_n] = r->reg_id;
        for (d = 0; d < D_COUNT; d++)
            g_v[g_n][d] = V_NOTMEASURED;
        key = ri_ctlreg_auto_id(r);
        if (r->bind == RI_BIND_NONE || key == 0u) {
            /* key 0 on a bound row: tempo, owned by the transport path */
            int c = r->bind == RI_BIND_NONE ? bindnone_class(r)
                : (r->section == RI_SEC_TRANSPORT ? V_PRESSPATH : V_DEAD);
            for (d = 0; d < D_COUNT; d++)
                g_v[g_n][d] = V_NA;
            g_v[g_n][D_PANEL] = (uint8_t)c;
        } else {
            choose_alt(r);
            g_v[g_n][D_PANEL] = (uint8_t)door_panel(r);
            fresh(&g_ref); /* D1 reference state for D5/D6 */
            via_panel(&g_ref, r->reg_id, alt_of(r));
            g_v[g_n][D_MIDI_IN] = (uint8_t)door_midi(r, 0);
            g_v[g_n][D_AUTO] = (uint8_t)door_auto(r, key);
            fresh(&g_ref);
            via_panel(&g_ref, r->reg_id, alt_of(r));
            g_v[g_n][D_SAVELOAD] = (uint8_t)door_saveload(r, key);
            g_v[g_n][D_STARTUP] = (uint8_t)door_startup(r);
            if (r->section != RI_SEC_LEVI)
                g_v[g_n][D_LEVI_ENC] = V_NA;
        }
        g_n++;
    }
    printf("rows %u\n", g_n);

    /* ---- controls ---- */
    {
        const struct RICtlDef *good = ri_ctlreg_find((uint16_t)((RI_SEC_SYNTH1 << 8) | 2u));
        struct RICtlDef bad;
        if (good)
            choose_alt(good);
        RI_ASSERT(good && door_panel(good) == V_OK, "control: 303A Cutoff reads OK");
        if (good) {
            bad = *good;
            bad.reg_id = 0xFFFEu; /* no such row: nothing can reach the engine */
            RI_ASSERT(door_panel(&bad) == V_DEAD, "control: a bogus reg_id reads DEAD");
            fresh(&g_ref);
            via_panel(&g_ref, good->reg_id, alt_of(good));
            RI_ASSERT(door_midi(good, -40) == V_DIVERGE,
                "control: D3 compared against the wrong value reads DIVERGE");
        }
    }
    {
        const struct RICtlDef *mute = ri_ctlreg_find((uint16_t)((RI_SEC_MIX_SYNTH1 << 8) | 0u));
        struct RICtlDef t224;
        RI_ASSERT(mute && mute->kind == RI_CK_SWITCH, "control: MIX_SYNTH1 row 0 is the strip switch");
        if (mute) {
            t224 = *mute;
            t224.bind = RI_BIND_NONE; /* t224's strip mute before its fix */
            RI_ASSERT(bindnone_class(&t224) == V_UNEXPLAINED,
                "control: t224's unbound strip switch reads UNEXPLAINED");
        }
    }

    /* ---- table, counts, gate ---- */
    tsv = fopen(T243_DIR "/census.tsv", "w");
    RI_ASSERT(tsv != NULL, "open %s", T243_DIR "/census.tsv");
    if (tsv) {
        fprintf(tsv, "reg_id");
        for (d = 0; d < D_COUNT; d++)
            fprintf(tsv, "\t%s", k_door[d]);
        fprintf(tsv, "\n");
    }
    baseline_load();
    for (i = 0; i < g_n; i++) {
        if (tsv)
            fprintf(tsv, "%04x", g_reg[i]);
        for (d = 0; d < D_COUNT; d++) {
            int v = g_v[i][d];
            count[v]++;
            if (v == V_OK)
                has_ok = 1;
            if (tsv)
                fprintf(tsv, "\t%s", k_verdict[v]);
            if (is_bad(v)) {
                nbad++;
                if (!baseline_has(g_reg[i], d, v)) {
                    printf("NEW %04x %s %s\n", g_reg[i], k_door[d], k_verdict[v]);
                    nnew++;
                }
            }
        }
        if (tsv)
            fprintf(tsv, "\n");
    }
    if (tsv)
        fclose(tsv);
    for (i = 0; i < g_nbl; i++)
        if (!g_bl[i].hit) {
            printf("FIXED (remove from baseline) %04x %s %s\n", g_bl[i].reg,
                k_door[g_bl[i].door], k_verdict[g_bl[i].verdict]);
            nstale++;
        }
    if (getenv("RI_CENSUS_WRITE_BASELINE")) {
        FILE *b = fopen(T243_DIR "/baseline.tsv", "w");
        if (b) {
            for (i = 0; i < g_n; i++)
                for (d = 0; d < D_COUNT; d++)
                    if (is_bad(g_v[i][d]))
                        fprintf(b, "%04x\t%s\t%s\n", g_reg[i], k_door[d], k_verdict[g_v[i][d]]);
            fclose(b);
            printf("wrote %s\n", T243_DIR "/baseline.tsv");
        }
    }
    for (d = 0; d < V_COUNT; d++)
        printf("%-22s %u\n", k_verdict[d], count[d]);
    printf("bad %d, new %d, fixed-but-listed %d, baseline %u\n", nbad, nnew, nstale, g_nbl);
    RI_ASSERT(has_ok, "at least one row reads OK");
    RI_ASSERT(nnew == 0, "%d bad verdicts are not in the baseline", nnew);
    RI_ASSERT(nstale == 0, "%d baseline entries now read OK: remove them (ratchet)", nstale);
    RI_RESULT("wiring_census");
    return 0;
}
