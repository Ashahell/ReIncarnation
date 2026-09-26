/* app/riapp.c — RIAPP live application with the ReBirth panel (G9b Step 2).
 * AROS-only. Supersedes the bare app/main.c window (kept) and the Step-1
 * text window: one MUI window with Transport + the four Pattern sections
 * + 303A + the 808 mixer strip (the sectproof keys/live layouts, 1x),
 * driving the same live session / control plane / meter snapshot the
 * text shell proved on the Dell.
 *
 * Usage: RIAPP [frames] (device buffer, default 256; 64..4096).
 * Quit: window close gadget, or Shell `Break <cli> C` (Ctrl-C raises the
 * loop break; plain Q is not a quit key under MUI).
 *
 * One path to the engine (no second setter route):
 * - every sounding value change on a canvas goes through
 *   gui/panelctl.h ri_panel_ctl_send (reg_id -> lane key -> ri_ctl_send),
 *   t83-pinned host-side; the render task drains the plane at buffer start;
 * - transport Play/Stop go through au_live_request (Record plays: the
 *   record lane lands in Step 4);
 * - pattern selection goes through ri_track_capture on the GUI-side track
 *   at the snapshot bar (the player changes over at the pattern end);
 *   pattern length/off go through ri_pattern_set_length on the GUI-side
 *   banks (off = length 0, which the player reads as silent); 303A steps
 *   go through ri_p303_set on the GUI-side bank slot. Banks and the track
 *   are live-read by the player at the next buffer, so no snapshot
 *   handshake is needed on this path (the RISeq snapshot API covers the
 *   old event-list model, which the live session does not use);
 * - startup pushes every sounding canvas default through the bridge, so
 *   the engine adopts the panel (no pre-task setter calls);
 * - meters and position come from the render-published snapshot only
 *   (ri_live_meters_read; livestate scales it; a busy read is dropped for
 *   that tick, never blocked on). This closes G6b on the device.
 *
 * Placeholders (canvas responds visually; no engine route yet — see the
 * evidence file, never silent-by-design): transport tempo/shuffle/loop/
 * rewind/FF/song-mode, record lamp, pattern shuffle switches, mixer
 * on/off switches, 303 programming buttons (pending state: they shape the
 * next stepped note, which does reach the engine), drum taps (need the
 * instrument canvases of a later slice), skins (Classic procedural),
 * capture keys (recording UX is Step 5).
 * Startup: built-in demo song (the G9.2 t81 fixture); AHI missing ->
 * null backend + RI_AUDIO_NULL_MSG (panel chases on the null render);
 * unbound 909 pack -> 909 renders silence + notice (§17, never silent).
 */
#ifndef __AROS__
#error "app/riapp.c is AROS-only"
#endif

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <string.h>
#include <libraries/mui.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include <proto/muimaster.h>
#include <proto/timer.h>
#include "engine/live.h"
#include "engine/seq/ctlplane.h"
#include "engine/seq/autolane.h"
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"
#include "engine/seq/transport.h"
#include "gui/ctlreg.h"
#include "gui/sectui.h"
#include "gui/sect303.h"
#include "gui/sectpat.h"
#include "gui/secttr.h"
#include "gui/sectmix.h"
#include "gui/panelui.h"
#include "gui/panelctl.h"
#include "gui/panelgeo.h"
#include "gui/livestate.h"
#include "gui/widgets/rsection.h"
#include "audio_io/audio_ahi_live.h"

extern struct DosLibrary *DOSBase;

#define RIAPP_FRAMES 64u /* null-backend render chunk */
#define RIAPP_DEV_FRAMES 256u /* default device buffer (owner-approved 2026-09-26) */
/* Demo mix (owner, Dell 2026-09-26): the 303 strip starts at 72 (-9.9 dB,
 * P-17) and the 808 downbeats are accented. Set on the board at startup
 * and sent through the bridge like any other fader (one path). */
#define RIAPP_303_LEVEL 72u
#define RIAPP_PPQ 96u
#define RIAPP_TICKS_BAR (4u * RIAPP_PPQ)

/* Panel canvases: transport + 4 pattern sections + 303A + 808 mixer. */
enum { C_TR, C_P0, C_P1, C_P2, C_P3, C_303, C_MIX, C_N };
static const ULONG c_sections[C_N] = {
    RI_SEC_TRANSPORT,
    RI_SEC_PAT_SYNTH1, RI_SEC_PAT_SYNTH2, RI_SEC_PAT_808, RI_SEC_PAT_909,
    RI_SEC_SYNTH1, RI_SEC_MIX_808
};
/* Pattern instance (bank + track slot) behind each PAT canvas. */
static const uint32_t c_pat_instance[C_N] = { 0u, 0u, 1u, 2u, 3u, 0u, 2u };

static struct RIPatternBank s_ba, s_bb, s_b808, s_b909;
static struct RISongTrack s_tr;
static struct RIEvent s_scratch[512];
static struct RIControlPlane s_ctl;
static float s_fl[RIAPP_FRAMES], s_fr[RIAPP_FRAMES];
static struct RILiveSession s_sess;
static struct AuLive s_lv;
static int s_live; /* AHI backend up (render task owns the session) */

static struct RIPanelUI s_panel;
static Object *s_canvas[C_N];
static struct RISectUI *s_ui[C_N];
static const struct RSectionDiag *s_dg[C_N];

/* Sync shadows (state-compare: the panel is the truth, the session follows). */
static int s_tr_state;
static uint8_t s_pat_bank[C_N], s_pat_pat[C_N], s_pat_off[C_N], s_pat_shuf[C_N];
static uint8_t s_pat_len[C_N][32];
static struct RI303Row s_303_row[16];
static uint8_t s_303_slot;
static IPTR s_changes[C_N];
static int s_meter_shown[2];

/* Demo: a 16-step 303 line (accents + slides) over an 808 beat. */
static void riapp_demo_song(void) {
    static const uint8_t keys[16] = { 0, 0, 12, 0, 3, 0, 5, 7, 0, 0, 12, 10, 7, 5, 3, 0 };
    static const uint8_t fl[16] = { RI_STEP_ACCENT, 0, RI_STEP_SLIDE, 0, 0, RI_STEP_REST, RI_STEP_ACCENT, 0,
        0, RI_STEP_SLIDE, RI_STEP_ACCENT, 0, 0, RI_STEP_REST, 0, RI_STEP_SLIDE };
    uint32_t i;
    ri_bank_init(&s_ba, 0u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&s_bb, 1u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&s_b808, 2u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    ri_bank_init(&s_b909, 3u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    for (i = 0u; i < 32u; i++) {
        ri_pattern_set_length(&s_ba.pat[i], 16u);
        ri_pattern_set_length(&s_bb.pat[i], 16u);
        ri_pattern_set_length(&s_b808.pat[i], 16u);
        ri_pattern_set_length(&s_b909.pat[i], 16u);
    }
    for (i = 0u; i < 16u; i++) {
        ri_p303_set(&s_ba.pat[0], i, keys[i], fl[i]);
        if ((i & 3u) == 0u)
            ri_pdrum_set(&s_b808.pat[0], i, RI_L808_BD, RI_HIT_LOW);
        if ((i & 3u) == 2u)
            ri_pdrum_set(&s_b808.pat[0], i, RI_L808_CH, RI_HIT_LOW);
        if (i == 4u || i == 12u)
            ri_pdrum_set(&s_b808.pat[0], i, RI_L808_SD, RI_HIT_LOW);
        if (i == 0u || i == 8u)
            ri_pdrum_set_ac(&s_b808.pat[0], i, 1); /* accented downbeats */
    }
    ri_track_init(&s_tr);
}

/* Status lines also go to RAM:RIAPP.LOG, opened, appended and closed per
 * line so it can be read while RIAPP runs (the Run redirect stays empty). */
static void rlog(const char *fmt, IPTR a, IPTR b, IPTR c, IPTR d, IPTR e) {
    BPTR f;
    ULONG args[5]; /* RawDoFmt %ld/%lu/%lx consume 32-bit LONGs, packed */
    args[0] = (ULONG)a;
    args[1] = (ULONG)b;
    args[2] = (ULONG)c;
    args[3] = (ULONG)d;
    args[4] = (ULONG)e;
    if (!DOSBase)
        return;
    VPrintf((STRPTR)fmt, (RAWARG)args);
    f = Open((STRPTR)"RAM:RIAPP.LOG", MODE_READWRITE);
    if (!f)
        return;
    Seek(f, 0, OFFSET_END);
    VFPrintf(f, (STRPTR)fmt, (RAWARG)args);
    Close(f);
}

static struct RIPatternBank *pat_bank(uint32_t inst) {
    static struct RIPatternBank *b[4];
    b[0] = &s_ba;
    b[1] = &s_bb;
    b[2] = &s_b808;
    b[3] = &s_b909;
    return inst < 4u ? b[inst] : &s_ba;
}

/* Transport state edge -> the render task (or the null session). */
static void sync_transport(void) {
    int st = s_ui[C_TR]->u.tr.tr.state;
    if (st == s_tr_state)
        return;
    s_tr_state = st;
    if (st == RI_TR_PLAYING || st == RI_TR_RECORD) {
        /* RECORD plays: the record lane lands in Step 4. */
        if (s_live)
            au_live_request(&s_lv, AU_LIVE_CMD_PLAY);
        else
            ri_live_play(&s_sess);
        rlog("RIAPP play\n", 0, 0, 0, 0, 0);
    } else {
        if (s_live)
            au_live_request(&s_lv, AU_LIVE_CMD_STOP);
        else
            ri_live_stop(&s_sess);
        rlog("RIAPP stop\n", 0, 0, 0, 0, 0);
    }
}

/* One PAT canvas -> banks/track. Selection captures at the snapshot bar
 * (changeover at the pattern end); length writes the bank slot; off parks
 * the bank length at 0 (silent) and restores the canvas length on on. */
static void sync_pat(int c, uint64_t cursor_ticks) {
    struct RISectUI *u = s_ui[c];
    uint32_t inst = c_pat_instance[c];
    struct RIPatternBank *b = pat_bank(inst);
    int sel = ri_spat_selected(&u->u.pat);
    uint64_t bar;
    if (sel < 0)
        sel = 0;
    if (sel > 31)
        sel = 31;
    bar = cursor_ticks / RIAPP_TICKS_BAR;
    if (bar >= (uint64_t)RI_SONG_BARS)
        bar = (uint64_t)RI_SONG_BARS - 1u;
    if (u->u.pat.bank != s_pat_bank[c] || u->u.pat.pattern != s_pat_pat[c]) {
        s_pat_bank[c] = u->u.pat.bank;
        s_pat_pat[c] = u->u.pat.pattern;
        ri_track_capture(&s_tr, bar, inst, (uint8_t)sel);
        if (c == C_P0) {
            /* 303A shows the selected slot: refresh its steps from the bank. */
            struct RISectUI *u303 = s_ui[C_303];
            uint32_t k;
            s_303_slot = (uint8_t)sel;
            for (k = 0u; k < 16u; k++) {
                s_303_row[k] = b->pat[sel].row.r303[k];
                u303->u.s303.pat.row.r303[k] = b->pat[sel].row.r303[k];
            }
            ri_rsection_refresh(s_canvas[C_303]);
        }
    }
    if (u->u.pat.length[sel] != s_pat_len[c][sel]) {
        s_pat_len[c][sel] = u->u.pat.length[sel];
        if (!u->u.pat.off)
            ri_pattern_set_length(&b->pat[sel], u->u.pat.length[sel]);
    }
    if (u->u.pat.off != s_pat_off[c]) {
        s_pat_off[c] = u->u.pat.off;
        if (u->u.pat.off)
            b->pat[sel].length = 0u; /* single-byte park: player reads 0 as silent */
        else
            ri_pattern_set_length(&b->pat[sel], u->u.pat.length[sel]);
    }
    s_pat_shuf[c] = u->u.pat.shuffle; /* placeholder: shuffle flags are OPEN */
}

/* 303A steps -> the GUI-side bank slot (pure edit functions, one path). */
static void sync_303(void) {
    struct RISectUI *u = s_ui[C_303];
    struct RIPatternBank *b = pat_bank(0u);
    uint32_t k;
    for (k = 0u; k < 16u; k++) {
        if (u->u.s303.pat.row.r303[k].key != s_303_row[k].key ||
            u->u.s303.pat.row.r303[k].flags != s_303_row[k].flags) {
            s_303_row[k] = u->u.s303.pat.row.r303[k];
            ri_p303_set(&b->pat[s_303_slot], k, s_303_row[k].key, s_303_row[k].flags);
        }
    }
}

/* Sounding value controls (mouse incl. drags and arrow repeats report the
 * hit control): exactly one control-plane message when the lane key is
 * nonzero (the bridge owns that law, t83). Transport and PAT canvases
 * travel their state paths above. */
static void sync_values(void) {
    static const int val_canvas[2] = { C_303, C_MIX };
    int i;
    for (i = 0; i < 2; i++) {
        int c = val_canvas[i];
        IPTR ch = 0;
        GetAttr(MUIA_RSection_Changes, s_canvas[c], &ch);
        if (ch == s_changes[c])
            continue;
        s_changes[c] = ch;
        if (s_dg[c] && s_dg[c]->last_hit != 0xFFFFu) {
            uint16_t reg = s_dg[c]->last_hit;
            struct RISectUI *u = s_ui[c];
            ri_panel_ctl_send(&s_ctl, reg, ri_sui_value(u, reg & 0xFFu));
        }
    }
}

/* Meters + position from the published snapshot only (G6b). Levels feed
 * the 808 board strips; the playhead chases the transport cursor. */
static void meter_round(ULONG mix_freq) {
    struct RILiveMeters m;
    int lvl303, lvl808;
    uint64_t sixteenths;
    int playing;
    if (ri_live_meters_read(&s_sess, &m) != 0)
        return;
    lvl303 = ri_live_meter_level(m.sec_peak[0]);
    lvl808 = ri_live_meter_level(m.sec_peak[2]);
    if (lvl303 != s_meter_shown[0] || lvl808 != s_meter_shown[1]) {
        struct RISectUI *u = s_ui[C_MIX];
        s_meter_shown[0] = lvl303;
        s_meter_shown[1] = lvl808;
        if (u && u->u.mix.board) {
            ri_smix_meter_set(u->u.mix.board, RI_SEC_MIX_SYNTH1, 0, lvl303);
            ri_smix_meter_set(u->u.mix.board, RI_SEC_MIX_808, 0, lvl808);
            ri_rsection_refresh(s_canvas[C_MIX]);
        }
    }
    playing = (s_tr_state != RI_TR_STOPPED);
    sixteenths = ri_live_16ths(m.samples, 120u, mix_freq ? mix_freq : 48000u);
    if (ri_panel_live(&s_panel, playing, sixteenths)) {
        int k;
        for (k = 0; k < C_N; k++)
            ri_rsection_refresh(s_canvas[k]);
    }
}

static ULONG riapp_arg_frames(int argc, char **argv) {
    ULONG v = 0u;
    const char *p;
    if (argc < 2 || !argv[1])
        return RIAPP_DEV_FRAMES;
    for (p = argv[1]; *p >= '0' && *p <= '9'; p++)
        v = v * 10u + (ULONG)(*p - '0');
    return (v >= 64u && v <= AU_LIVE_MAXFRAMES) ? v : RIAPP_DEV_FRAMES;
}

int main(int argc, char **argv) {
    Object *app, *win, *row, *pats;
    LONG ret;
    ULONG sigs = 0;
    const struct RIPatternBank *b4[4];
    ULONG frames = riapp_arg_frames(argc, argv);
    float rate = 48000.0f;
    int i, rc;
    struct MsgPort *tport = 0;
    struct timerequest *treq = 0;
    int timer_ok = 0, timer_armed = 0;
    static const char *installed[1] = { "Classic" };

    riapp_demo_song();
    b4[0] = &s_ba;
    b4[1] = &s_bb;
    b4[2] = &s_b808;
    b4[3] = &s_b909;
    ri_ctl_init(&s_ctl);
    rc = au_live_open(&s_lv, frames, 48000u);
    if (rc == 0) {
        s_live = 1;
        rate = (float)s_lv.mix_freq; /* E0 (G9.0): the session runs at the device rate */
    }
    ri_live_init(&s_sess, RIAPP_PPQ, rate, 120.0f, RI_ENGINE_S303A | RI_ENGINE_S808, s_scratch, 512u);
    ri_live_set_banks(&s_sess, b4, &s_tr, 0);
    ri_live_set_ctl(&s_sess, &s_ctl);
    if (s_live && au_live_run(&s_lv, &s_sess) != 0) {
        au_live_close(&s_lv);
        s_live = 0;
    }
    if (DOSBase) {
        if (s_live)
            rlog("audio: AHI low-level mode=0x%08lx mix=%lu Hz buffer=%lu frames period=%lu us\n",
                s_lv.mode_id, s_lv.mix_freq, s_lv.frames, s_lv.period_us, 0);
        else
            rlog("audio: AHI unavailable - null backend active (offline render only) [err %ld]\n",
                (IPTR)s_lv.err, 0, 0, 0, 0);
        rlog("RIAPP 909 pack: unbound - 909 renders silence (load a pack for drums)\n", 0, 0, 0, 0, 0);
    }

    /* Panel: transport (compact) + 4 pattern sections + 303A + 808 mixer. */
    ri_panel_init(&s_panel);
    ri_panel_skins(&s_panel, installed, 0u, "Classic"); /* skins ride later work */
    for (i = 0; i < C_N; i++) {
        LONG zoom = (i == C_TR) ? RI_GEO_ZOOM_COMPACT : 0;
        struct RISectUI *u = 0;
        s_canvas[i] = (Object *)ri_rsection_create(c_sections[i], zoom);
        if (!s_canvas[i]) {
            if (s_live)
                au_live_close(&s_lv);
            return 5;
        }
        GetAttr(MUIA_RSection_State, s_canvas[i], (IPTR *)&u);
        s_ui[i] = u;
        GetAttr(MUIA_RSection_Diag, s_canvas[i], (IPTR *)&s_dg[i]);
        if (i == C_TR)
            s_panel.tr = u;
        else if (i >= C_P0 && i <= C_P3)
            s_panel.pat[i - C_P0] = u;
        else if (i == C_303)
            s_panel.synth[0] = u;
        else if (i == C_MIX)
            s_panel.mix[2] = u;
        SetAttrs(s_canvas[i], MUIA_RSection_Panel, (IPTR)&s_panel,
            MUIA_RSection_KeyOwner, i == C_TR, TAG_DONE);
    }
    /* The panel shows the demo: 303A steps mirror bank slot 0. */
    for (i = 0; i < 16; i++) {
        s_ui[C_303]->u.s303.pat.row.r303[i] = s_ba.pat[0].row.r303[i];
        s_303_row[i] = s_ba.pat[0].row.r303[i];
    }
    s_303_slot = 0u;
    for (i = C_P0; i <= C_P3; i++) {
        int k;
        s_pat_bank[i] = s_pat_pat[i] = s_pat_off[i] = s_pat_shuf[i] = 0u;
        for (k = 0; k < 32; k++)
            s_pat_len[i][k] = 16u;
    }
    /* Demo mix on the board (display = engine truth, set before publish). */
    ri_smix_set_value(s_ui[C_MIX]->u.mix.board, RI_SEC_MIX_SYNTH1, RI_SMIX_LEVEL, RIAPP_303_LEVEL);
    /* Startup burst: every sounding canvas default through the bridge, so
     * the engine adopts the panel at the first drained buffers (the whole
     * board, not just the 808 strip: the demo 303A level 72 lives on
     * strip 0). */
    for (i = 0; i <= 6; i++)
        ri_panel_ctl_send(&s_ctl, (uint16_t)c_sections[C_303] << 8 | (uint16_t)i,
            ri_sui_value(s_ui[C_303], (uint32_t)i));
    for (i = 0; i < 4; i++) {
        int k;
        struct RISectUI *mu = s_ui[C_MIX];
        for (k = 2; k <= 7; k++)
            ri_panel_ctl_send(&s_ctl,
                (uint16_t)(RI_SEC_MIX_SYNTH1 + i) << 8 | (uint16_t)k,
                ri_smix_value(mu->u.mix.board, (uint32_t)(RI_SEC_MIX_SYNTH1 + i),
                    (uint32_t)k));
    }
    s_tr_state = s_ui[C_TR]->u.tr.tr.state;
    for (i = 0; i < C_N; i++) {
        IPTR ch = 0;
        GetAttr(MUIA_RSection_Changes, s_canvas[i], &ch);
        s_changes[i] = ch;
    }
    s_meter_shown[0] = s_meter_shown[1] = -1;
    if (!s_live)
        ri_live_render(&s_sess, s_fl, s_fr, RIAPP_FRAMES); /* drain the burst */

    pats = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 2,
        Child, (IPTR)s_canvas[C_P0], Child, (IPTR)s_canvas[C_P1],
        Child, (IPTR)s_canvas[C_P2], Child, (IPTR)s_canvas[C_P3], TAG_DONE);
    {
        Object *lower;
        lower = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 2,
            Child, (IPTR)s_canvas[C_303], Child, (IPTR)s_canvas[C_MIX], TAG_DONE);
        row = (pats && lower) ? (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Spacing, 2,
            Child, (IPTR)s_canvas[C_TR], Child, (IPTR)pats,
            Child, (IPTR)lower, TAG_DONE) : 0;
    }
    if (!row) {
        if (s_live)
            au_live_close(&s_lv);
        return 5;
    }
    win = (Object *)MUI_NewObject(MUIC_Window,
        MUIA_Window_Title, (IPTR)"RIAPP live panel",
        MUIA_Window_LeftEdge, 0,
        MUIA_Window_TopEdge, 0,
        MUIA_Window_CloseGadget, TRUE,
        MUIA_Window_DepthGadget, TRUE,
        MUIA_Window_DragBar, TRUE,
        MUIA_Window_RootObject, (IPTR)row,
        TAG_DONE);
    if (!win) {
        if (s_live)
            au_live_close(&s_lv);
        return 7;
    }
    app = (Object *)MUI_NewObject(MUIC_Application,
        MUIA_Application_Title, (IPTR)"RIAPP",
        MUIA_Application_Base, (IPTR)"RIAPP",
        SubWindow, (IPTR)win,
        TAG_DONE);
    if (!app) {
        if (s_live)
            au_live_close(&s_lv);
        return 8;
    }
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2,
        MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    rlog("RIAPP panel: transport+patterns+303A+mix808 (Dell-proofed 2026-09-26)\n",
        0, 0, 0, 0, 0);

    /* 100 ms tick: meter chase + null-backend advance. */
    tport = CreateMsgPort();
    if (tport)
        treq = (struct timerequest *)CreateIORequest(tport, sizeof(struct timerequest));
    if (treq && OpenDevice((STRPTR)"timer.device", UNIT_MICROHZ,
        (struct IORequest *)treq, 0) == 0)
        timer_ok = 1;
    if (timer_ok) {
        treq->tr_node.io_Command = TR_ADDREQUEST;
        treq->tr_time.tv_secs = 0;
        treq->tr_time.tv_micro = 100000;
        SendIO((struct IORequest *)treq);
        timer_armed = 1;
    }

    /* Canonical union loop (m60): input, sync, meters, wait. */
    for (;;) {
        struct RILiveMeters m;
        uint64_t cursor = 0u;
        int have_cursor = 0;
        ret = (LONG)DoMethod(app, MUIM_Application_NewInput, &sigs);
        if (ret == (LONG)MUIV_Application_ReturnID_Quit)
            break;
        if (timer_armed && CheckIO((struct IORequest *)treq)) {
            WaitIO((struct IORequest *)treq);
            treq->tr_node.io_Command = TR_ADDREQUEST;
            treq->tr_time.tv_secs = 0;
            treq->tr_time.tv_micro = 100000;
            SendIO((struct IORequest *)treq);
            if (!s_live && s_tr_state != RI_TR_STOPPED)
                ri_live_render(&s_sess, s_fl, s_fr, RIAPP_FRAMES);
        }
        if (ri_live_meters_read(&s_sess, &m) == 0) {
            cursor = m.cursor_ticks;
            have_cursor = 1;
        }
        sync_transport();
        for (i = C_P0; i <= C_P3; i++)
            sync_pat(i, have_cursor ? cursor : 0u);
        sync_303();
        sync_values();
        meter_round(s_live ? s_lv.mix_freq : 48000u);
        sigs |= SIGBREAKF_CTRL_C | (timer_armed ? 1UL << tport->mp_SigBit : 0UL);
        sigs = Wait(sigs);
        if (sigs & SIGBREAKF_CTRL_C)
            break;
    }
    if (timer_armed) {
        if (!CheckIO((struct IORequest *)treq))
            AbortIO((struct IORequest *)treq);
        WaitIO((struct IORequest *)treq);
    }
    if (timer_ok)
        CloseDevice((struct IORequest *)treq);
    if (treq)
        DeleteIORequest((struct IORequest *)treq);
    if (tport)
        DeleteMsgPort(tport);
    if (s_live) {
        au_live_close(&s_lv);
        if (DOSBase)
            rlog("RIAPP closed: buffers=%lu xruns=%lu render_max=%lu us render_total=%lu ms period=%lu us\n",
                s_lv.buffers, s_lv.xruns, s_lv.render_us_max, s_lv.render_us_sum_ms, s_lv.period_us);
    }
    SetAttrs(win, MUIA_Window_Open, FALSE, TAG_DONE);
    MUI_DisposeObject(app);
    ri_rsection_dispose_class();
    return 0;
}
