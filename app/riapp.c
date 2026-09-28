/* app/riapp.c — RIAPP live application with the ReBirth panel (G9b Step 2).
 * AROS-only. Supersedes the bare app/main.c window (kept) and the Step-1
 * text window: one MUI window with Transport framed above a slim device
 * rail above a Register with the Synths/Drums/Mix/FX tabs (owner
 * 2026-09-27; stock Register.mui, device rows follow the visible set
 * through the t98 model), driving the same live session / control plane
 * / meter snapshot the text shell proved on the Dell.
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
 * next stepped note, which does reach the engine), skins (Classic
 * procedural), capture keys (recording UX is Step 5).
 * Startup: built-in demo song (the G9.2 t81 fixture); AHI missing ->
 * null backend + RI_AUDIO_NULL_MSG (panel chases on the null render);
 * 909 pack: classic-01 binds at startup (unbound voices render silence + notice, §17).
 */
#ifndef __AROS__
#error "app/riapp.c is AROS-only"
#endif

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <libraries/mui.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include <proto/muimaster.h>
#include <proto/timer.h>
#include <proto/utility.h>
#include <utility/tagitem.h>
#include <clib/alib_protos.h>
#include "engine/live.h"
#include "engine/seq/ctlplane.h"
#include "engine/seq/autolane.h"
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"
#include "engine/seq/transport.h"
#include "gui/ctlreg.h"
#include "gui/sectui.h"
#include "gui/sect303.h"
#include "gui/sect808.h"
#include "gui/sect909.h"
#include "gui/sectfx.h"
#include "gui/sectpat.h"
#include "gui/secttr.h"
#include "gui/sectmix.h"
#include "gui/panels.h"
#include "gui/visdev.h"
#include "gui/tabpages.h"
#include "gui/panelui.h"
#include "gui/panelctl.h"
#include "gui/panelgeo.h"
#include "gui/livestate.h"
#include "gui/widgets/rsection.h"
#include "gui/draw/art.h"
#include "audio_io/audio_ahi_live.h"
#include "app/core/riapp_core.h"
#include "platform/aros/pack_909.h"
#include "platform/pal/ri_pal_fs.h"
#include "platform/pal/ri_pal_log.h"

/* Build identity for device attribution (never ambiguous binaries again):
 * the v11 build script passes -DRIAPP_BUILD_HASH="<short-sha>"; every
 * other build reports "?". Logged in the ev-log RUN line. */
#ifndef RIAPP_BUILD_HASH
#define RIAPP_BUILD_HASH "?"
#endif

extern struct DosLibrary *DOSBase;

#define RIAPP_FRAMES 64u /* null-backend render chunk */
#define RIAPP_DEV_FRAMES 256u /* default device buffer (owner-approved 2026-09-26) */
/* Demo mix (owner, Dell 2026-09-26): the 303 strip starts at 72 (-9.9 dB,
 * P-17) and the 808 downbeats are accented. Set on the board at startup
 * and sent through the bridge like any other fader (one path). */
#define RIAPP_303_LEVEL 72u
#define RIAPP_PPQ 96u
#define RIAPP_TICKS_BAR (4u * RIAPP_PPQ)

/* Panel canvases: transport (frame, always visible) + 5 pattern sections
 * + 5 voice canvases + mixer board + 4 FX units, grouped into the
 * Register tabs Synths/Drums/Mix/FX (owner 2026-09-27; Levi 2026-09-28).
 * Levi has no mixer strip yet (MIX_LEVI is the next slice) and no
 * keyboard-focus slot (s_panel stays 4-focus; mouse path is direct). */
enum {
    C_TR, C_P0, C_P1, C_P2, C_P3, C_P4, C_303A, C_303B, C_808, C_909, C_LEVI, C_MIX,
    C_FX0, C_FX1, C_FX2, C_FX3, C_MXA, C_MXB, C_MX9, C_MXL, C_N
};
static const ULONG c_sections[C_N] = {
    RI_SEC_TRANSPORT,
    RI_SEC_PAT_SYNTH1, RI_SEC_PAT_SYNTH2, RI_SEC_PAT_808, RI_SEC_PAT_909, RI_SEC_PAT_LEVI,
    RI_SEC_SYNTH1, RI_SEC_SYNTH2, RI_SEC_808, RI_SEC_909, RI_SEC_LEVI, RI_SEC_MIX_808,
    RI_SEC_PCF, RI_SEC_DELAY, RI_SEC_DIST, RI_SEC_COMP,
    RI_SEC_MIX_SYNTH1, RI_SEC_MIX_SYNTH2, RI_SEC_MIX_909, RI_SEC_MIX_LEVI
};
/* Pattern instance (bank + track slot) behind each PAT/voice canvas. */
static const uint32_t c_pat_instance[C_N] = {
    0u, 0u, 1u, 2u, 3u, 4u, 0u, 1u, 2u, 3u, 4u, 2u, 0u, 0u, 0u, 0u, 0u, 1u, 3u, 4u
};
/* Mix tab (owner 2026-09-28): one strip per device, device order; C_MIX
 * (the 808 strip) owns the shared board, the others bind to it. */
static const int c_mix_canvas[5] = { C_MXA, C_MXB, C_MIX, C_MX9, C_MXL };
/* Voice canvas per classic device (tab rows follow this through t98). */
static const int c_voice_canvas[5] = { C_303A, C_303B, C_808, C_909, C_LEVI };

static struct RIAppCore s_core; /* portable session wiring (T8) */
static float s_fl[RIAPP_FRAMES], s_fr[RIAPP_FRAMES];
static struct AuLive s_lv;
static int s_live; /* AHI backend up (render task owns the session) */

static struct RIPanelUI s_panel;
static Object *s_canvas[C_N];
static struct RISectUI *s_ui[C_N];
static const struct RSectionDiag *s_dg[C_N];

/* Device visibility (owner 2026-09-27): the rail toggles rows. Since
 * the activation slice the same bit means ACTIVE: ShowMe the row AND
 * flip the engine sections bit (snapshot-free single-word swap, read
 * once per render block). Mixer strips stay (mute there); sync shadows
 * follow every canvas regardless of ShowMe. */
static struct RIVisSet s_vis;
static Object *s_devrow[5];
static Object *s_devbtn[5];
static Object *s_devled[5];
static Object *s_mixslot[5]; /* Mix tab strip per device (follows s_vis) */
static Object *s_pages;      /* page group: rail follows its active page */
static Object *s_tabs[5];    /* hardware tab keys (S1, one per tab) */
/* Rack furniture (owner 2026-09-28, second pass): the Mix/FX bays and the
 * device rail are brushed dark metal (RBay, a Group that paints its own
 * background for itself and every child without one), the bays sit
 * between steel rack rails with screws, modules meet in dark seams, and
 * the devices switch with power buttons whose glyph is the LED (RArt, a
 * self-drawing Area). All art is gui/draw (ri_art_bay/rack_rail/seam/
 * power, host-checked in t93) replayed through the section canvas colour
 * path (ri_rsection_replay: exact RGB on truecolor screens). The rail
 * LEDs of 2026-09-27 (RLed, after the Bitmap.mui saga — llm-wiki
 * 2026-09-27-rail-led-bitmap-saga.md) live on as the power glyph. */
#define MUIA_RArt_Kind (TAG_USER | 0x5241524Bu)  /* RART_* (init) */
#define MUIA_RArt_On (TAG_USER | 0x52494C45u)    /* BOOL: power lit */
#define MUIA_RArt_Label (TAG_USER | 0x5241524Cu) /* STRPTR, kept by caller */
#define MUIA_RArt_Active (TAG_USER | 0x52415442u) /* BOOL: tab latched */
#define RART_RAIL 0
#define RART_SEAM_L 1
#define RART_SEAM_R 2
#define RART_POWER 3
#define RART_TAB 4
#define RIAPP_BAY_SPEC "2:r1A1A1A1A,1B1B1B1B,1E1E1E1E" /* fallback without RBay */
static struct MUI_CustomClass *s_art_mcc;
static struct MUI_CustomClass *s_bay_mcc;
static struct ri_dcmd s_art_back[16384];
static char s_art_spool[512];
static char s_devlbl[5][16];
static char s_tablbl[5][16];
#define RIAPP_ID_DEV0 1001u /* + device: Devices-tab toggle buttons */
#define RIAPP_ID_TAB0 1020u /* + tab: hardware tab keys */

/* Sync shadows (state-compare: the panel is the truth, the session follows). */
static int s_tr_state;
static uint8_t s_pat_bank[C_N], s_pat_pat[C_N], s_pat_off[C_N], s_pat_shuf[C_N];
static uint8_t s_pat_len[C_N][32];
static struct RI303Row s_303_row[2][16];
static uint8_t s_303_slot[2];
static struct RIPattern s_drum_pat[2]; /* 808/909 canvas-pattern shadow */
static uint8_t s_drum_slot[2];
static struct RIPattern s_levi_pat; /* Levi voice-canvas shadow (bank 4 slot) */
static uint8_t s_levi_slot;
static IPTR s_changes[C_N];
static int s_meter_shown[5];

/* Demo song, bank table, transport and meters live in app/core (T8).
 * Status lines also go to the TEMP log, opened, appended and closed per
 * line so it can be read while RIAPP runs (the Run redirect stays empty).
 * Formatted with vsnprintf (T6: no RawDoFmt packing) and sunk preformatted. */
/* Event log (owner, 2026-09-27): every user-driven event (control sends,
 * transport edges, pattern/step edits) goes to RIAPP-EV.LOG, fresh per run
 * (MODE_NEWFILE at startup), on a USB stick when one is present, RAM:
 * otherwise. Open once, Flush per line (a wedge keeps all but the last
 * line); best effort after that (a pulled stick just stops landing). */
static BPTR s_evfh;
static ULONG s_evseq;
static char s_evvol[48]; /* USB volume or the TEMP base (never a literal) */

static void evlog_vol(void) {
    /* Spike hunt 2026-09-27: RIAPP_EVLOG=RAM: forces the ev-log off the
     * USB stick (the stick must stay in: the Dell runs system parts
     * off it, and shells won't open without it). Default unchanged. */
    static const char *const vols[] = { "Vk4aros:", "USB0:", "USB1:", "UMSD0:", "UMSD1:", "USBDISK0:" };
    uint32_t i;
    s_evvol[0] = 0;
    if (!DOSBase)
        return;
    {
        char v[16];
        LONG r = GetVar((STRPTR)"RIAPP_EVLOG", (STRPTR)v, (LONG)sizeof v - 1u, 0L);
        if (r > 0) {
            uint32_t k = 0u;
            while (v[k] && k < sizeof(s_evvol) - 1u) {
                s_evvol[k] = v[k];
                k++;
            }
            s_evvol[k] = 0;
            return;
        }
    }
    for (i = 0u; i < sizeof(vols) / sizeof(vols[0]); i++) {
        BPTR lock = Lock((CONST_STRPTR)vols[i], ACCESS_READ);
        if (lock) {
            UnLock(lock);
            {
                uint32_t k = 0u;
                while (vols[i][k] && k < sizeof(s_evvol) - 1u) {
                    s_evvol[k] = vols[i][k];
                    k++;
                }
                s_evvol[k] = 0;
            }
            return;
        }
    }
    /* No stick: TEMP base through PAL (T6 — no literals outside platform/). */
    if (ri_pal_path(RI_PATH_TEMP, s_evvol, sizeof(s_evvol)) != 0)
        s_evvol[0] = 0;
}

static void evlog_open(void) {
    char fn[64];
    uint32_t k = 0u, j = 0u;
    static const char leaf[] = "RIAPP-EV.LOG";
    const char *base;
    if (!DOSBase || s_evfh)
        return;
    evlog_vol();
    if (!s_evvol[0])
        return; /* nowhere to log: evlog() stays a no-op */
    base = s_evvol;
    while (base[k] && k < sizeof(fn) - 1u) {
        fn[k] = base[k];
        k++;
    }
    while (leaf[j] && k < sizeof(fn) - 1u)
        fn[k++] = leaf[j++];
    fn[k] = 0;
    s_evfh = Open((STRPTR)fn, MODE_NEWFILE);
    s_evseq = 0u;
}

static void evlog_putu(char *buf, uint32_t *n, ULONG v) {
    char t[10];
    int i = 0;
    if (v == 0u)
        t[i++] = '0';
    else
        while (v > 0u && i < 10) {
            t[i++] = (char)('0' + v % 10u);
            v /= 10u;
        }
    while (i > 0 && *n < 190u)
        buf[(*n)++] = t[--i];
}

static void evlog(const char *kind, const char *fmt, ...) {
    char buf[192];
    va_list ap;
    ULONG bufs;
    uint32_t n = 0u, k;
    if (!DOSBase || !s_evfh || !kind || !fmt)
        return;
    bufs = s_live ? ri_atomic_load_acq(&s_lv.drv.buffers) : 0u;
    buf[n++] = 'e';
    buf[n++] = 'v';
    buf[n++] = ' ';
    evlog_putu(buf, &n, ++s_evseq);
    buf[n++] = ' ';
    evlog_putu(buf, &n, bufs);
    buf[n++] = ' ';
    for (k = 0u; kind[k] && n < 60u; k++)
        buf[n++] = kind[k];
    buf[n++] = ' ';
    buf[n] = 0;
    va_start(ap, fmt);
    ri_log_format(buf + n, sizeof(buf) - n, fmt, ap);
    va_end(ap);
    {
        uint32_t m = 0u;
        while (buf[m] && m < sizeof(buf) - 1u)
            m++;
        if (m + 1u < sizeof(buf)) {
            buf[m++] = '\n';
            buf[m] = 0;
        }
    }
    FPuts(s_evfh, (STRPTR)buf);
    Flush(s_evfh);
}

static void rlog(const char *fmt, ...) {
    char buf[512], base[48], fn[96];
    va_list ap;
    BPTR f;
    if (!DOSBase)
        return;
    va_start(ap, fmt);
    ri_log_format(buf, sizeof buf, fmt, ap);
    va_end(ap);
    ri_pal_log_sink(buf);
    if (ri_pal_path(RI_PATH_TEMP, base, sizeof base) != 0)
        return;
    if (ri_pal_path_join(fn, sizeof fn, base, "RIAPP.LOG") != 0)
        return;
    f = Open((STRPTR)fn, MODE_READWRITE);
    if (!f)
        return;
    Seek(f, 0, OFFSET_END);
    FPuts(f, (STRPTR)buf);
    Close(f);
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
            ri_core_play(&s_core);
        rlog("RIAPP play\n");
        evlog("TR", "PLAY");
    } else {
        if (s_live)
            au_live_request(&s_lv, AU_LIVE_CMD_STOP);
        else
            ri_core_stop(&s_core);
        rlog("RIAPP stop\n");
        evlog("TR", "STOP");
    }
}

/* One PAT canvas -> banks/track. Selection captures at the snapshot bar
 * (changeover at the pattern end); length writes the bank slot; off parks
 * the bank length at 0 (silent) and restores the canvas length on on. */
static void sync_pat(int c, uint64_t cursor_ticks) {
    struct RISectUI *u = s_ui[c];
    uint32_t inst = c_pat_instance[c];
    struct RIPatternBank *b = ri_core_bank(&s_core, inst);
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
        ri_core_capture_sel(&s_core, bar, inst, (uint8_t)sel);
        evlog("PAT", "c=%d inst=%d sel=%d", c, inst, sel);
        if (c == C_P0 || c == C_P1) {
            /* 303A/B show the selected slot: refresh steps from the bank. */
            uint32_t v = (c == C_P0) ? 0u : 1u;
            struct RISectUI *uv = s_ui[c_voice_canvas[v]];
            uint32_t k;
            s_303_slot[v] = (uint8_t)sel;
            for (k = 0u; k < 16u; k++) {
                s_303_row[v][k] = b->pat[sel].row.r303[k];
                uv->u.s303.pat.row.r303[k] = b->pat[sel].row.r303[k];
            }
            ri_rsection_refresh(s_canvas[c_voice_canvas[v]]);
        } else if (c == C_P2 || c == C_P3) {
            /* 808/909 show the selected slot: refresh the canvas pattern. */
            uint32_t v = (c == C_P2) ? 0u : 1u;
            struct RISectUI *uv = s_ui[c_voice_canvas[2u + v]];
            struct RIPattern *dp = (v == 0u) ? &uv->u.s808.pat : &uv->u.s909.pat;
            *dp = b->pat[sel];
            s_drum_pat[v] = b->pat[sel];
            s_drum_slot[v] = (uint8_t)sel;
            ri_rsection_refresh(s_canvas[c_voice_canvas[2u + v]]);
        } else if (c == C_P4) {
            /* Levi shows the selected slot: refresh the chord canvas. */
            struct RISectUI *uv = s_ui[C_LEVI];
            uv->u.slevi.pat = b->pat[sel];
            s_levi_pat = b->pat[sel];
            s_levi_slot = (uint8_t)sel;
            ri_rsection_refresh(s_canvas[C_LEVI]);
        }
    }
    /* Sticky live selection (E1 pattern mode): re-assert the panel selection
     * at the snapshot bar every round (no-op when stored, silent). */
    ri_core_capture_sel(&s_core, bar, inst, (uint8_t)sel);
    if (u->u.pat.length[sel] != s_pat_len[c][sel]) {
        s_pat_len[c][sel] = u->u.pat.length[sel];
        evlog("PATLEN", "c=%d sel=%d len=%d", c, sel, u->u.pat.length[sel]);
        if (!u->u.pat.off)
            ri_pattern_set_length(&b->pat[sel], u->u.pat.length[sel]);
    }
    if (u->u.pat.off != s_pat_off[c]) {
        s_pat_off[c] = u->u.pat.off;
        evlog("PATOFF", "c=%d off=%d", c, u->u.pat.off ? 1 : 0);
        if (u->u.pat.off)
            b->pat[sel].length = 0u; /* single-byte park: player reads 0 as silent */
        else
            ri_pattern_set_length(&b->pat[sel], u->u.pat.length[sel]);
    }
    s_pat_shuf[c] = u->u.pat.shuffle; /* placeholder: shuffle flags are OPEN */
}

/* 303 steps -> the GUI-side bank slot (pure edit functions, one path). */
static void sync_303v(uint32_t v) {
    int vc = c_voice_canvas[v];
    struct RISectUI *u = s_ui[vc];
    struct RIPatternBank *b = ri_core_bank(&s_core, v);
    uint32_t k;
    for (k = 0u; k < 16u; k++) {
        if (u->u.s303.pat.row.r303[k].key != s_303_row[v][k].key ||
            u->u.s303.pat.row.r303[k].flags != s_303_row[v][k].flags) {
            s_303_row[v][k] = u->u.s303.pat.row.r303[k];
            ri_p303_set(&b->pat[s_303_slot[v]], k, s_303_row[v][k].key,
                s_303_row[v][k].flags);
            evlog("STEP", "v=%d slot=%d step=%d key=%d flags=%d", v,
                s_303_slot[v], k, s_303_row[v][k].key, s_303_row[v][k].flags);
        }
    }
}

/* Drum steps -> the GUI-side bank slot (11 lanes + AC row, one path).
 * v = 0 (808, bank 2) / 1 (909, bank 3). */
static void sync_drumv(uint32_t v) {
    int vc = c_voice_canvas[2u + v];
    uint32_t inst = 2u + v;
    struct RISectUI *u = s_ui[vc];
    struct RIPattern *cp = (v == 0u) ? &u->u.s808.pat : &u->u.s909.pat;
    struct RIPattern *bp = &ri_core_bank(&s_core, inst)->pat[s_drum_slot[v]];
    uint32_t step, lane;
    for (step = 0u; step < 16u; step++) {
        for (lane = 0u; lane < 11u; lane++) {
            uint32_t cs = ri_pdrum_get(cp, step, lane);
            if (cs != ri_pdrum_get(&s_drum_pat[v], step, lane)) {
                /* Lane bits only: flags (AC) have their own branch below. */
                s_drum_pat[v].row.drum[step].on = cp->row.drum[step].on;
                s_drum_pat[v].row.drum[step].high = cp->row.drum[step].high;
                s_drum_pat[v].row.drum[step].flam = cp->row.drum[step].flam;
                ri_pdrum_set(bp, step, lane, cs);
                evlog("DSTEP", "v=%d slot=%d step=%d lane=%d st=%d", v,
                    s_drum_slot[v], step, lane, cs);
            }
        }
        {
            int cs = (cp->row.drum[step].flags & RI_DRUM_AC) ? 1 : 0;
            int os = (s_drum_pat[v].row.drum[step].flags & RI_DRUM_AC) ? 1 : 0;
            if (cs != os) {
                s_drum_pat[v].row.drum[step].flags = cp->row.drum[step].flags;
                ri_pdrum_set_ac(bp, step, cs);
                evlog("DSTEPAC", "v=%d slot=%d step=%d ac=%d", v,
                    s_drum_slot[v], step, cs);
            }
        }
    }
}

/* Levi chord steps -> the GUI-side bank-4 slot (16 steps x 6 lanes).
 * On/off + pitch both travel: a canvas edit and the bank slot agree
 * step for step, and the shadow makes the sync edge-triggered. */
static void sync_leviv(void) {
    struct RISectUI *u = s_ui[C_LEVI];
    struct RIPattern *cp = &u->u.slevi.pat;
    struct RIPattern *bp = &ri_core_bank(&s_core, 4u)->pat[s_levi_slot];
    uint32_t step, lane;
    for (step = 0u; step < 16u; step++) {
        for (lane = 0u; lane < RI_LEVI_LANES; lane++) {
            int con = ri_levi_on(cp, step, lane);
            int son = ri_levi_on(&s_levi_pat, step, lane);
            uint32_t cn = ri_levi_get(cp, step, lane);
            uint32_t sn = ri_levi_get(&s_levi_pat, step, lane);
            if (con != son || (con && cn != sn)) {
                s_levi_pat.row.levi[step].on = cp->row.levi[step].on;
                s_levi_pat.row.levi[step].note[lane] = cp->row.levi[step].note[lane];
                ri_levi_set(bp, step, lane, (uint8_t)cn, con);
                evlog("LSTEP", "slot=%d step=%d lane=%d on=%d note=%d",
                    s_levi_slot, step, lane, con, cn);
            }
        }
    }
}

/* Sounding value controls (mouse incl. drags and arrow repeats report the
 * hit control): exactly one control-plane message when the lane key is
 * nonzero (the bridge owns that law, t83). Transport and PAT canvases
 * travel their state paths above. */
static void sync_values(void) {
    static const int val_canvas[14] = {
        C_303A, C_303B, C_808, C_909, C_LEVI, C_MIX, C_FX0, C_FX1, C_FX2, C_FX3, C_MXA, C_MXB, C_MX9, C_MXL
    };
    int i;
    for (i = 0; i < 14; i++) {
        int c = val_canvas[i];
        IPTR ch = 0;
        GetAttr(MUIA_RSection_Changes, s_canvas[c], &ch);
        if (ch == s_changes[c])
            continue;
        s_changes[c] = ch;
        if (s_dg[c] && s_dg[c]->last_hit != 0xFFFFu) {
            /* Re-tag to the canvas's own section: the 303B canvas shares
             * the SYNTH1 geometry table, so raw hit ids arrive as 303A.
             * (All other value canvases own their tables; no-op for them.
             * Owner 2026-09-27: 303B knobs drove 303A params.) */
            uint16_t reg = (uint16_t)((uint16_t)c_sections[c] << 8) |
                (uint16_t)(s_dg[c]->last_hit & 0xFFu);
            struct RISectUI *u = s_ui[c];
            int val = ri_sui_value(u, reg & 0xFFu);
            ri_panel_ctl_send(&s_core.ctl, reg, val);
            evlog("CTL", "%04x=%d", reg, val);
        }
    }
}

/* Meters + position from the published snapshot only (G6b). Levels feed
 * the five mixer strips; the playhead chases the transport cursor. */
static void meter_round(ULONG mix_freq) {
    static const ULONG strip_sec[5] = {
        RI_SEC_MIX_SYNTH1, RI_SEC_MIX_SYNTH2, RI_SEC_MIX_808, RI_SEC_MIX_909, RI_SEC_MIX_LEVI
    };
    struct RILiveMeters mm;
    int lvl[5];
    uint64_t sixteenths;
    int playing, k;
    if (!ri_core_meters(&s_core, &lvl[0], &lvl[2], &sixteenths, mix_freq ? mix_freq : 48000u,
        s_tr_state != RI_TR_STOPPED))
        return;
    lvl[1] = lvl[3] = 0;
    if (ri_live_meters_read(&s_core.session, &mm) == 0) {
        lvl[1] = ri_live_meter_level(mm.sec_peak[1]);
        lvl[3] = ri_live_meter_level(mm.sec_peak[3]);
        lvl[4] = ri_live_meter_level(mm.sec_peak[4]);
    }
    for (k = 0; k < 5; k++) {
        struct RISectUI *u = s_ui[c_mix_canvas[k]];
        if (lvl[k] == s_meter_shown[k] || !u || !u->u.mix.board)
            continue;
        s_meter_shown[k] = lvl[k];
        ri_smix_meter_set(u->u.mix.board, strip_sec[k], 0, lvl[k]);
        ri_rsection_refresh(s_canvas[c_mix_canvas[k]]);
    }
    playing = (s_tr_state != RI_TR_STOPPED);
    if (ri_panel_live(&s_panel, playing, sixteenths)) {
        for (k = 0; k < C_N; k++)
            ri_rsection_refresh(s_canvas[k]);
    }
}

/* Canvas object for a section id (tab rows follow the t98 model). */
static Object *canvas_for_section(ULONG sec) {
    int i;
    for (i = 0; i < C_N; i++)
        if (c_sections[i] == sec)
            return s_canvas[i];
    return 0;
}

/* One device tab page: a row (pattern + voice canvas) per visible device.
 * Row objects (by device) go through devs/objs for ShowMe toggling. */
static Object *tab_device_page(uint32_t group, struct RIVisSet *vis,
    uint32_t *devs, Object **objs, uint32_t cap, uint32_t *n_out) {
    struct RITabDev rows[5];
    uint32_t n, r;
    Object *page;
    if (!vis)
        return 0;
    n = ri_tab_devices(group, vis, rows, 5u);
    page = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Spacing, 2, TAG_DONE);
    if (!page)
        return 0;
    if (n_out)
        *n_out = 0u;
    for (r = 0u; r < n; r++) {
        Object *prow = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE,
            MUIA_Group_Spacing, 2,
            Child, (IPTR)canvas_for_section(rows[r].pat_sec),
            Child, (IPTR)canvas_for_section(rows[r].voice_sec), TAG_DONE);
        if (!prow || !canvas_for_section(rows[r].pat_sec) ||
            !canvas_for_section(rows[r].voice_sec))
            return 0;
        DoMethod(page, OM_ADDMEMBER, (IPTR)prow);
        if (devs && objs && n_out && *n_out < cap) {
            devs[*n_out] = rows[r].device;
            objs[*n_out] = prow;
            (*n_out)++;
        }
    }
    return (n > 0u) ? page : 0;
}

/* Keep only the parts of rects and axis lines inside the clip box (the
 * bay emits nothing else); a background request repaints just its box. */
static void art_clip(struct ri_dlist *dl, int x0, int y0, int x1, int y1) {
    uint32_t i, n = 0u;
    for (i = 0u; i < dl->n; i++) {
        struct ri_dcmd c = dl->cmd[i];
        int a0, b0, a1, b1;
        if (c.op != RI_D_RECT && !(c.op == RI_D_LINE && (c.x0 == c.x1 || c.y0 == c.y1)))
            continue;
        a0 = c.x0 < c.x1 ? c.x0 : c.x1;
        a1 = c.x0 < c.x1 ? c.x1 : c.x0;
        b0 = c.y0 < c.y1 ? c.y0 : c.y1;
        b1 = c.y0 < c.y1 ? c.y1 : c.y0;
        if (a0 < x0) a0 = x0;
        if (b0 < y0) b0 = y0;
        if (a1 > x1) a1 = x1;
        if (b1 > y1) b1 = y1;
        if (a0 > a1 || b0 > b1)
            continue;
        c.op = RI_D_RECT;
        c.x0 = (int16_t)a0;
        c.y0 = (int16_t)b0;
        c.x1 = (int16_t)a1;
        c.y1 = (int16_t)b1;
        dl->cmd[n++] = c;
    }
    dl->n = n;
}

struct RArtData {
    LONG kind;
    BOOL on;
    BOOL active;
    const char *label;
};

BOOPSI_DISPATCHER_PROTO(IPTR, rart_dispatcher, Class *, Object *, Msg);

BOOPSI_DISPATCHER(IPTR, rart_dispatcher, cl, obj, msg) {
    struct RArtData *d;
    switch (msg->MethodID) {
    case OM_NEW: {
        struct TagItem *tl = ((struct opSet *)msg)->ops_AttrList;
        Object *o = (Object *)DoSuperMethodA(cl, obj, msg);
        if (!o)
            return (IPTR)NULL;
        d = (struct RArtData *)INST_DATA(cl, o);
        d->kind = (LONG)GetTagData(MUIA_RArt_Kind, RART_RAIL, tl);
        d->on = (BOOL)GetTagData(MUIA_RArt_On, TRUE, tl);
        d->active = (BOOL)GetTagData(MUIA_RArt_Active, FALSE, tl);
        d->label = (const char *)GetTagData(MUIA_RArt_Label, 0, tl);
        return (IPTR)o;
    }
    case OM_SET: {
        struct TagItem *tl = ((struct opSet *)msg)->ops_AttrList;
        struct TagItem *t = FindTagItem(MUIA_RArt_On, tl);
        struct TagItem *ta = FindTagItem(MUIA_RArt_Active, tl);
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        d = (struct RArtData *)INST_DATA(cl, obj);
        if (t && (BOOL)(t->ti_Data != 0) != d->on) {
            d->on = (BOOL)(t->ti_Data != 0);
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        } else if (ta && (BOOL)(ta->ti_Data != 0) != d->active) {
            d->active = (BOOL)(ta->ti_Data != 0);
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        } else if (d->kind == RART_POWER && FindTagItem(MUIA_Selected, tl))
            MUI_Redraw(obj, MADF_DRAWOBJECT); /* cap sinks while held */
        else if (d->kind == RART_TAB && FindTagItem(MUIA_Selected, tl))
            MUI_Redraw(obj, MADF_DRAWOBJECT); /* key sinks while held */
        return rc;
    }
    case MUIM_AskMinMax: {
        struct MUIP_AskMinMax *m = (struct MUIP_AskMinMax *)msg;
        struct MUI_MinMax *mm = m->MinMaxInfo;
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        LONG w = RI_ART_SEAM_W;
        d = (struct RArtData *)INST_DATA(cl, obj);
        if (d->kind == RART_POWER) {
            struct RastPort trp;
            LONG tw = 0;
            InitRastPort(&trp);
            SetFont(&trp, _font(obj));
            if (d->label)
                tw = (LONG)TextLength(&trp, (STRPTR)d->label, (ULONG)strlen(d->label));
            mm->MinWidth += RI_ART_POWER_H + 6 + tw + 10;
            mm->MinHeight += RI_ART_POWER_H;
            mm->DefWidth = mm->MaxWidth = mm->MinWidth;
            mm->DefHeight = mm->MaxHeight = mm->MinHeight;
            return rc;
        }
        if (d->kind == RART_TAB) {
            struct RastPort trp;
            LONG tw = 0;
            InitRastPort(&trp);
            SetFont(&trp, _font(obj));
            if (d->label)
                tw = (LONG)TextLength(&trp, (STRPTR)d->label, (ULONG)strlen(d->label));
            mm->MinWidth += tw + 2 * 14;
            mm->MinHeight += RI_ART_TAB_H;
            mm->DefWidth = mm->MaxWidth = mm->MinWidth;
            mm->DefHeight = mm->MaxHeight = mm->MinHeight;
            return rc;
        }
        if (d->kind == RART_RAIL)
            w = RI_ART_RAIL_W;
        mm->MinWidth += w;
        mm->DefWidth = mm->MaxWidth = mm->MinWidth;
        mm->MinHeight += 1;
        mm->DefHeight += 1;
        mm->MaxHeight = MUI_MAXMAX;
        return rc;
    }
    case MUIM_Draw: {
        struct MUIP_Draw *m = (struct MUIP_Draw *)msg;
        struct ri_dlist dl;
        int x0, y0, x1, y1;
        DoSuperMethodA(cl, obj, msg); /* Area fills the parent (bay) background */
        if (!(m->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE)))
            return 0;
        d = (struct RArtData *)INST_DATA(cl, obj);
        x0 = _mleft(obj);
        y0 = _mtop(obj);
        x1 = _mright(obj);
        y1 = _mbottom(obj);
        ri_dlist_init(&dl, s_art_back, 16384u, s_art_spool, sizeof s_art_spool);
        if (d->kind == RART_POWER) {
            IPTR sel = 0;
            GetAttr(MUIA_Selected, obj, &sel);
            SetFont(_rp(obj), _font(obj));
            ri_art_power(&dl, x0, y0, x1, y1, d->label, d->on ? 1 : 0, sel ? 1 : 0);
        } else if (d->kind == RART_TAB) {
            IPTR sel = 0;
            GetAttr(MUIA_Selected, obj, &sel);
            SetFont(_rp(obj), _font(obj));
            ri_art_tab(&dl, x0, y0, x1, y1, d->label, d->active ? 1 : 0, sel ? 1 : 0);
        } else if (d->kind == RART_RAIL)
            ri_art_rack_rail(&dl, x0, y0, x1, y1);
        else
            ri_art_seam(&dl, x0, y0, x1, y1, d->kind == RART_SEAM_R);
        ri_rsection_replay(_rp(obj), &dl);
        return 0;
    }
    default:
        return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

struct RBayData {
    BOOL shown;
};

BOOPSI_DISPATCHER_PROTO(IPTR, rbay_dispatcher, Class *, Object *, Msg);

BOOPSI_DISPATCHER(IPTR, rbay_dispatcher, cl, obj, msg) {
    struct RBayData *d;
    switch (msg->MethodID) {
    case MUIM_Show: {
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        ((struct RBayData *)INST_DATA(cl, obj))->shown = TRUE;
        return rc;
    }
    case MUIM_Hide:
        ((struct RBayData *)INST_DATA(cl, obj))->shown = FALSE;
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_DrawBackground: {
        /* The whole plate is built from the bay's own box (grain keyed to
         * it) and clipped to the requested box. */
        struct MUIP_DrawBackground *m = (struct MUIP_DrawBackground *)msg;
        struct ri_dlist dl;
        d = (struct RBayData *)INST_DATA(cl, obj);
        if (!d->shown || m->width <= 0 || m->height <= 0)
            return FALSE;
        ri_dlist_init(&dl, s_art_back, 16384u, s_art_spool, sizeof s_art_spool);
        ri_art_bay(&dl, _left(obj), _top(obj), _right(obj), _bottom(obj));
        art_clip(&dl, (int)m->left, (int)m->top, (int)(m->left + m->width - 1),
            (int)(m->top + m->height - 1));
        ri_rsection_replay(_rp(obj), &dl);
        return TRUE;
    }
    default:
        return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

/* 0 ok, 2 classes unavailable (plain buttons and a flat bay instead). */
static int rack_classes_make(void) {
    if (!s_art_mcc)
        s_art_mcc = MUI_CreateCustomClass(NULL, (ClassID)MUIC_Area, NULL, sizeof(struct RArtData),
            (APTR)rart_dispatcher);
    if (!s_bay_mcc)
        s_bay_mcc = MUI_CreateCustomClass(NULL, (ClassID)MUIC_Group, NULL, sizeof(struct RBayData),
            (APTR)rbay_dispatcher);
    return (s_art_mcc && s_bay_mcc) ? 0 : 2;
}

/* After the application object is disposed (no instances left). */
static void rack_classes_drop(void) {
    if (s_art_mcc) {
        MUI_DeleteCustomClass(s_art_mcc);
        s_art_mcc = 0;
    }
    if (s_bay_mcc) {
        MUI_DeleteCustomClass(s_bay_mcc);
        s_bay_mcc = 0;
    }
}

static Object *rart(LONG kind) {
    if (s_art_mcc)
        return (Object *)NewObject(s_art_mcc->mcc_Class, NULL, MUIA_RArt_Kind, kind, TAG_DONE);
    return (Object *)MUI_NewObject(MUIC_Rectangle, MUIA_FixWidth,
        (LONG)(kind == RART_RAIL ? RI_ART_RAIL_W : RI_ART_SEAM_W), TAG_DONE);
}

/* A brushed bay group; tags are the group's (TAG_MORE list). */
static Object *bay_group(struct TagItem *tags) {
    if (s_bay_mcc)
        return (Object *)NewObject(s_bay_mcc->mcc_Class, NULL, TAG_MORE, (IPTR)tags);
    return (Object *)MUI_NewObject(MUIC_Group, MUIA_Background, (IPTR)RIAPP_BAY_SPEC,
        TAG_MORE, (IPTR)tags);
}

/* One racked module: seams at both sides (two neighbours' seams meet as a
 * dark groove), top-aligned in its column (the bay shows below). */
static Object *rack_slot(Object *mod) {
    Object *l = rart(RART_SEAM_L), *r = rart(RART_SEAM_R), *fill, *unit;
    fill = (Object *)MUI_NewObject(MUIC_Rectangle, TAG_DONE);
    unit = (l && r && fill) ? (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE,
        MUIA_Group_Spacing, 0, Child, (IPTR)l, Child, (IPTR)mod, Child, (IPTR)r, TAG_DONE) : 0;
    return unit ? (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Spacing, 0,
        Child, (IPTR)unit, Child, (IPTR)fill, TAG_DONE) : 0;
}

/* Rack bay page: rail | bay | modules (touching, tops aligned, centred)
 * | bay | rail, all on the brushed bay. slots[] (optional) gets each
 * module's slot for ShowMe. */
static Object *rack_page(Object *const *mods, uint32_t n, Object **slots) {
    Object *row, *col, *l, *r, *sp[4];
    struct TagItem tags[8];
    uint32_t i;
    /* Weight 1 against the spacers' 100: the row stays at the tallest
     * module, so the bay splits evenly above and below. */
    row = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE,
        MUIA_Group_Spacing, 0, MUIA_Weight, 1, TAG_DONE);
    if (!row)
        return 0;
    for (i = 0u; i < n; i++) {
        Object *slot = rack_slot(mods[i]);
        if (!slot)
            return 0;
        if (slots)
            slots[i] = slot;
        DoMethod(row, OM_ADDMEMBER, (IPTR)slot);
    }
    for (i = 0u; i < 4u; i++)
        sp[i] = (Object *)MUI_NewObject(MUIC_Rectangle, TAG_DONE);
    col = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Spacing, 0,
        Child, (IPTR)sp[0], Child, (IPTR)row, Child, (IPTR)sp[1], TAG_DONE);
    l = rart(RART_RAIL);
    r = rart(RART_RAIL);
    if (!col || !l || !r || !sp[2] || !sp[3])
        return 0;
    tags[0].ti_Tag = MUIA_Group_Horiz;   tags[0].ti_Data = TRUE;
    tags[1].ti_Tag = MUIA_Group_Spacing; tags[1].ti_Data = 0;
    tags[2].ti_Tag = MUIA_InnerLeft;     tags[2].ti_Data = 0;
    tags[3].ti_Tag = MUIA_InnerRight;    tags[3].ti_Data = 0;
    tags[4].ti_Tag = MUIA_InnerTop;      tags[4].ti_Data = 0;
    tags[5].ti_Tag = MUIA_InnerBottom;   tags[5].ti_Data = 0;
    tags[6].ti_Tag = TAG_DONE;           tags[6].ti_Data = 0;
    {
        Object *page = bay_group(tags);
        if (!page)
            return 0;
        DoMethod(page, OM_ADDMEMBER, (IPTR)l);
        DoMethod(page, OM_ADDMEMBER, (IPTR)sp[2]);
        DoMethod(page, OM_ADDMEMBER, (IPTR)col);
        DoMethod(page, OM_ADDMEMBER, (IPTR)sp[3]);
        DoMethod(page, OM_ADDMEMBER, (IPTR)r);
        return page;
    }
}

/* Mirror each device's active bit into its power glyph (the class redraws
 * itself when the state changes). */
static void rail_leds_show(void) {
    uint32_t d;
    for (d = 0u; d < 5u; d++)
        if (s_devled[d])
            SetAttrs(s_devled[d], MUIA_RArt_On, (IPTR)(ri_vis_get(&s_vis, d) > 0 ? TRUE : FALSE), TAG_DONE);
}

/* Tab a device's rows live on (t98 model, all devices shown), or
 * RI_TAB_COUNT when none. */
static uint32_t dev_tab(uint32_t dev) {
    struct RIVisSet all;
    struct RITabDev t[5];
    uint32_t g, i, n;
    ri_vis_init(&all);
    for (g = 0u; g < RI_TAB_COUNT; g++) {
        n = ri_tab_devices(g, &all, t, 5u);
        for (i = 0u; i < n; i++)
            if (t[i].device == dev)
                return g;
    }
    return RI_TAB_COUNT;
}

/* Rail follows the page group (owner 2026-09-28, S1: custom tabs replace
 * the Register): Synths shows the synth power buttons, Drums the drum
 * machines, Levi its own chip; Mix and FX serve every device, so they
 * show all five. Mouse-only tab keys (ledger S1: F1-F4/Ctrl+1..4 need an
 * owner key decision, #2). */
static void rail_for_tab(void) {
    static int shown[5] = { -1, -1, -1, -1, -1 };
    IPTR page = 0;
    uint32_t d;
    if (!s_pages)
        return;
    GetAttr(MUIA_Group_ActivePage, s_pages, &page);
    for (d = 0u; d < 5u; d++) {
        int show = (page == RI_TAB_MIX || page == RI_TAB_FX) ? 1 : dev_tab(d) == (uint32_t)page;
        if (s_devbtn[d] && show != shown[d]) {
            SetAttrs(s_devbtn[d], MUIA_ShowMe, show ? TRUE : FALSE, TAG_DONE);
            shown[d] = show;
        }
    }
}

/* Device rail (owner 2026-09-27, power buttons 2026-09-28): a brushed
 * strip above the Register with one power button per device; the glyph
 * is the device's LED. Labels live in s_devlbl (kept, static). Without
 * the classes the chips fall back to plain buttons. */
static Object *tab_rail(void) {
    Object *rail, *fill;
    struct TagItem tags[6];
    uint32_t d;
    tags[0].ti_Tag = MUIA_Group_Horiz;   tags[0].ti_Data = TRUE;
    tags[1].ti_Tag = MUIA_Group_Spacing; tags[1].ti_Data = 6;
    tags[2].ti_Tag = MUIA_InnerLeft;     tags[2].ti_Data = 8;
    tags[3].ti_Tag = MUIA_InnerTop;      tags[3].ti_Data = 3;
    tags[4].ti_Tag = MUIA_InnerBottom;   tags[4].ti_Data = 3;
    tags[5].ti_Tag = TAG_DONE;           tags[5].ti_Data = 0;
    rail = bay_group(tags);
    if (!rail)
        return 0;
    for (d = 0u; d < 5u; d++) {
        const struct RIPanelDesc *pd = ri_panel_get(d);
        const char *nm = (pd && pd->name) ? pd->name : "?";
        Object *btn;
        snprintf(s_devlbl[d], sizeof s_devlbl[d], "%s", nm);
        if (s_art_mcc)
            btn = (Object *)NewObject(s_art_mcc->mcc_Class, NULL,
                MUIA_RArt_Kind, RART_POWER,
                MUIA_RArt_Label, (IPTR)s_devlbl[d],
                MUIA_RArt_On, (IPTR)(ri_vis_get(&s_vis, d) > 0 ? TRUE : FALSE),
                MUIA_InputMode, MUIV_InputMode_RelVerify,
                MUIA_ShowSelState, FALSE,
                MUIA_FillArea, TRUE,
                TAG_DONE);
        else
            btn = (Object *)MUI_MakeObject(MUIO_Button, (IPTR)s_devlbl[d]);
        if (!btn)
            return 0;
        s_devbtn[d] = btn;
        s_devled[d] = s_art_mcc ? btn : 0;
        DoMethod(rail, OM_ADDMEMBER, (IPTR)btn);
    }
    fill = (Object *)MUI_NewObject(MUIC_Rectangle, TAG_DONE);
    if (fill)
        DoMethod(rail, OM_ADDMEMBER, (IPTR)fill);
    return rail;
}

/* Hardware tab strip (S1): an RBay HGroup of RART_TAB keys, one per tab,
 * labels from ri_tab_title. Without the class, plain buttons. */
static Object *tab_strip(void) {
    Object *strip, *fill;
    struct TagItem tags[6];
    uint32_t g;
    tags[0].ti_Tag = MUIA_Group_Horiz;   tags[0].ti_Data = TRUE;
    tags[1].ti_Tag = MUIA_Group_Spacing; tags[1].ti_Data = 4;
    tags[2].ti_Tag = MUIA_InnerLeft;     tags[2].ti_Data = 8;
    tags[3].ti_Tag = MUIA_InnerTop;      tags[3].ti_Data = 3;
    tags[4].ti_Tag = MUIA_InnerBottom;   tags[4].ti_Data = 3;
    tags[5].ti_Tag = TAG_DONE;           tags[5].ti_Data = 0;
    strip = bay_group(tags);
    if (!strip)
        return 0;
    for (g = 0u; g < RI_TAB_COUNT; g++) {
        const char *t = ri_tab_title(g);
        Object *btn;
        if (!t)
            return 0;
        snprintf(s_tablbl[g], sizeof s_tablbl[g], "%s", t);
        if (s_art_mcc)
            btn = (Object *)NewObject(s_art_mcc->mcc_Class, NULL,
                MUIA_RArt_Kind, RART_TAB,
                MUIA_RArt_Label, (IPTR)s_tablbl[g],
                MUIA_RArt_Active, (IPTR)(g == 0u ? TRUE : FALSE),
                MUIA_InputMode, MUIV_InputMode_RelVerify,
                MUIA_ShowSelState, FALSE,
                MUIA_FillArea, TRUE,
                TAG_DONE);
        else
            btn = (Object *)MUI_MakeObject(MUIO_Button, (IPTR)s_tablbl[g]);
        if (!btn)
            return 0;
        s_tabs[g] = btn;
        DoMethod(strip, OM_ADDMEMBER, (IPTR)btn);
    }
    fill = (Object *)MUI_NewObject(MUIC_Rectangle, TAG_DONE);
    if (fill)
        DoMethod(strip, OM_ADDMEMBER, (IPTR)fill);
    return strip;
}

/* Switch the page group to g: set ActivePage, latch the tab keys, rail. */
static void tab_switch(uint32_t g) {
    uint32_t k;
    if (g >= RI_TAB_COUNT || !s_pages)
        return;
    SetAttrs(s_pages, MUIA_Group_ActivePage, (IPTR)g, TAG_DONE);
    for (k = 0u; k < RI_TAB_COUNT; k++)
        if (s_tabs[k])
            SetAttrs(s_tabs[k], MUIA_RArt_Active, (IPTR)(k == g ? TRUE : FALSE), TAG_DONE);
    rail_for_tab();
    evlog("TAB", "page=%d", g);
}

/* Rail toggle: flip the visible bit, ShowMe the row, mirror the LED —
 * and flip the engine bit with it (ACTIVE, rack requirement
 * 2026-09-24). */
static void dev_visibility_toggle(uint32_t dev) {
    int show;
    uint32_t d, mask = 0u;
    if (dev >= 5u || !s_devbtn[dev])
        return;
    show = !ri_vis_get(&s_vis, dev);
    ri_vis_set(&s_vis, dev, show);
    rail_leds_show();
    if (s_devrow[dev])
        SetAttrs(s_devrow[dev], MUIA_ShowMe, show ? TRUE : FALSE, TAG_DONE);
    if (s_mixslot[dev])     /* Mix tab: a strip per active device */
        SetAttrs(s_mixslot[dev], MUIA_ShowMe, show ? TRUE : FALSE, TAG_DONE);
    /* Bit law (t100): device d owns bit d; dev 4 is RI_ENGINE_SLEVI. */
    for (d = 0u; d < 5u; d++)
        if (ri_vis_get(&s_vis, d) > 0)
            mask |= (uint32_t)RI_ENGINE_S303A << d;
    ri_live_set_sections(&s_core.session, mask);
    evlog("VIS", "dev=%d show=%d mask=%02x", dev, show ? 1 : 0, mask);
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
    Object *app, *win, *row;
    LONG ret;
    ULONG sigs = 0;
    ULONG frames = riapp_arg_frames(argc, argv);
    float rate = 48000.0f;
    int i, rc;
    ULONG hb = 0u; /* wedge-diagnostic heartbeat counter (see loop) */
    struct MsgPort *tport = 0;
    struct timerequest *treq = 0;
    int timer_ok = 0, timer_armed = 0;
    static const char *installed[1] = { "Classic" };

    ri_core_demo(&s_core);
    evlog_open();
    if (rack_classes_make() != 0 && DOSBase)
        rlog("RIAPP rack classes unavailable (plain buttons, flat bay)\n", 0, 0, 0, 0, 0);
    evlog("RUN", "frames=%lu vol=%s build=%s", frames, s_evvol, RIAPP_BUILD_HASH);
    rc = au_live_open(&s_lv, frames, 48000u);
    if (rc == 0) {
        s_live = 1;
        rate = (float)s_lv.mix_freq; /* E0 (G9.0): the session runs at the device rate */
    }
    ri_core_init(&s_core, RIAPP_PPQ, rate, 140.0f,
        RI_ENGINE_S303A | RI_ENGINE_S303B | RI_ENGINE_S808 | RI_ENGINE_S909 | RI_ENGINE_SLEVI);
    { /* 909 sample pack (owner 2026-09-27): bind idle, before the task runs. */
        char err[128];
        int bound;
        err[0] = 0;
        bound = ri_pack_909_bind(&s_core.session.eng, err, sizeof err);
        if (DOSBase) {
            if (bound > 0)
                rlog("RIAPP 909 pack: %d voices bound\n", bound, 0, 0, 0, 0);
            else
                rlog("RIAPP 909 pack missing: %s (909 renders silence)\n", err[0] ? err : "no pack", 0, 0, 0, 0);
        }
    }
    if (s_live && au_live_run(&s_lv, &s_core.session) != 0) {
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
            if (s_evfh) {
                Close(s_evfh);
                s_evfh = 0;
            }
            return 5;
        }
        GetAttr(MUIA_RSection_State, s_canvas[i], (IPTR *)&u);
        s_ui[i] = u;
        GetAttr(MUIA_RSection_Diag, s_canvas[i], (IPTR *)&s_dg[i]);
        if (i == C_TR)
            s_panel.tr = u;
        else if (i >= C_P0 && i <= C_P3)
            s_panel.pat[i - C_P0] = u;
        /* C_P4/C_LEVI stay out of s_panel (4-focus keyboard model; the
         * MIDI slice owns focus 5; mouse + rail paths use s_ui direct). */
        else if (i == C_303A)
            s_panel.synth[0] = u;
        else if (i == C_303B)
            s_panel.synth[1] = u;
        else if (i == C_808)
            s_panel.drum[0] = u;
        else if (i == C_909)
            s_panel.drum[1] = u;
        else if (i == C_MIX)
            s_panel.mix[2] = u;
        else if (i == C_MXA || i == C_MXB)
            s_panel.mix[i - C_MXA] = u;
        else if (i == C_MX9)
            s_panel.mix[3] = u;
        else if (i == C_MXL)
            s_panel.mix[5] = u;
        else if (i >= C_FX0 && i <= C_FX3)
            s_panel.fx[i - C_FX0] = u;
        SetAttrs(s_canvas[i], MUIA_RSection_Panel, (IPTR)&s_panel,
            MUIA_RSection_KeyOwner, i == C_TR, TAG_DONE);
    }
    for (i = 0; i < 5; i++)                 /* one mixer board, five strips */
        if (c_mix_canvas[i] != C_MIX)
            ri_sui_bind_board(s_ui[c_mix_canvas[i]], s_ui[C_MIX]->u.mix.board);
    /* The panel shows the demo: 303A/B steps mirror bank slots 0;
     * 808/909 canvases mirror their bank slot 0 (empty until programmed). */
    for (i = 0; i < 16; i++) {
        s_ui[C_303A]->u.s303.pat.row.r303[i] = s_core.banks[0].pat[0].row.r303[i];
        s_303_row[0][i] = s_core.banks[0].pat[0].row.r303[i];
        s_ui[C_303B]->u.s303.pat.row.r303[i] = s_core.banks[1].pat[0].row.r303[i];
        s_303_row[1][i] = s_core.banks[1].pat[0].row.r303[i];
    }
    s_303_slot[0] = s_303_slot[1] = 0u;
    s_ui[C_808]->u.s808.pat = s_core.banks[2].pat[0];
    s_drum_pat[0] = s_core.banks[2].pat[0];
    s_ui[C_909]->u.s909.pat = s_core.banks[3].pat[0];
    s_drum_pat[1] = s_core.banks[3].pat[0];
    s_drum_slot[0] = s_drum_slot[1] = 0u;
    /* Levi mirrors bank 4 slot 0 (silent until programmed, 303B precedent). */
    s_ui[C_LEVI]->u.slevi.pat = s_core.banks[4].pat[0];
    s_levi_pat = s_core.banks[4].pat[0];
    s_levi_slot = 0u;
    for (i = C_P0; i <= C_P4; i++) {
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
     * strip 0). Non-automatable indices are filtered by the bridge. */
    for (i = 0; i <= 6; i++) {
        ri_panel_ctl_send(&s_core.ctl, (uint16_t)c_sections[C_303A] << 8 | (uint16_t)i,
            ri_sui_value(s_ui[C_303A], (uint32_t)i));
        ri_panel_ctl_send(&s_core.ctl, (uint16_t)c_sections[C_303B] << 8 | (uint16_t)i,
            ri_sui_value(s_ui[C_303B], (uint32_t)i));
    }
    for (i = 0; i < (int)RI_S808_NCTL; i++)
        ri_panel_ctl_send(&s_core.ctl, (uint16_t)c_sections[C_808] << 8 | (uint16_t)i,
            ri_sui_value(s_ui[C_808], (uint32_t)i));
    for (i = 0; i < (int)RI_S909_NCTL; i++)
        ri_panel_ctl_send(&s_core.ctl, (uint16_t)c_sections[C_909] << 8 | (uint16_t)i,
            ri_sui_value(s_ui[C_909], (uint32_t)i));
    for (i = C_FX0; i <= C_FX3; i++) {
        int k;
        for (k = 0; k < (int)RI_SFX_NCTL; k++)
            ri_panel_ctl_send(&s_core.ctl, (uint16_t)c_sections[i] << 8 | (uint16_t)k,
                ri_sui_value(s_ui[i], (uint32_t)k));
    }
    for (i = 0; i < 4; i++) {
        int k;
        struct RISectUI *mu = s_ui[C_MIX];
        for (k = 2; k <= 7; k++)
            ri_panel_ctl_send(&s_core.ctl,
                (uint16_t)(RI_SEC_MIX_SYNTH1 + i) << 8 | (uint16_t)k,
                ri_smix_value(mu->u.mix.board, (uint32_t)(RI_SEC_MIX_SYNTH1 + i),
                    (uint32_t)k));
    }
    {   /* Levi strip defaults ride the same burst (section id is not
         * contiguous: MIX_SYNTH1+i would land on MASTER). */
        int k;
        struct RISectUI *mu = s_ui[C_MIX];
        for (k = 2; k <= 7; k++)
            ri_panel_ctl_send(&s_core.ctl,
                (uint16_t)RI_SEC_MIX_LEVI << 8 | (uint16_t)k,
                ri_smix_value(mu->u.mix.board, (uint32_t)RI_SEC_MIX_LEVI,
                    (uint32_t)k));
    }
    s_tr_state = s_ui[C_TR]->u.tr.tr.state;
    for (i = 0; i < C_N; i++) {
        IPTR ch = 0;
        GetAttr(MUIA_RSection_Changes, s_canvas[i], &ch);
        s_changes[i] = ch;
    }
    s_meter_shown[0] = s_meter_shown[1] = s_meter_shown[2] = s_meter_shown[3] = s_meter_shown[4] = -1;
    if (!s_live)
        ri_live_render(&s_core.session, s_fl, s_fr, RIAPP_FRAMES); /* drain the burst */

    {
        /* Tabbed panel (owner 2026-09-27, S1 hardware tabs 2026-09-28):
         * transport racked above, then the device rail (always visible),
         * then the hardware tab strip, then the page group; device rows
         * follow the visible set through the t98 model. The window root
         * is a brushed bay, so no stock MUI grey remains. */
        Object *synth_page, *drums_page, *levi_page, *mix_page, *fx_page, *rail, *strip, *pages, *trslot;
        struct TagItem roottags[6];
        uint32_t r, nrows;
        uint32_t rowdev[3];
        Object *rowobj[3];
        ri_vis_init(&s_vis);
        for (r = 0u; r < 5u; r++) {
            s_devrow[r] = 0;
            s_tabs[r] = 0;
        }
        synth_page = tab_device_page(RI_TAB_SYNTH, &s_vis, rowdev, rowobj, 3u, &nrows);
        for (r = 0u; r < nrows; r++)
            s_devrow[rowdev[r]] = rowobj[r];
        drums_page = tab_device_page(RI_TAB_DRUMS, &s_vis, rowdev, rowobj, 2u, &nrows);
        for (r = 0u; r < nrows; r++)
            s_devrow[rowdev[r]] = rowobj[r];
        levi_page = tab_device_page(RI_TAB_LEVI, &s_vis, rowdev, rowobj, 1u, &nrows);
        for (r = 0u; r < nrows; r++)
            s_devrow[rowdev[r]] = rowobj[r];
        {
            Object *mx[5];
            for (r = 0u; r < 5u; r++)
                mx[r] = s_canvas[c_mix_canvas[r]];
            mix_page = rack_page(mx, 5u, s_mixslot);
        }
        fx_page = rack_page(&s_canvas[C_FX0], 4u, 0);
        rail = tab_rail();
        strip = tab_strip();
        pages = (synth_page && drums_page && levi_page && mix_page && fx_page) ?
            (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_PageMode, TRUE,
                Child, (IPTR)synth_page, Child, (IPTR)drums_page,
                Child, (IPTR)levi_page,
                Child, (IPTR)mix_page, Child, (IPTR)fx_page, TAG_DONE) : 0;
        trslot = rack_slot(s_canvas[C_TR]);
        s_pages = pages;
        roottags[0].ti_Tag = MUIA_Group_Spacing; roottags[0].ti_Data = 4;
        roottags[1].ti_Tag = MUIA_InnerLeft;     roottags[1].ti_Data = 6;
        roottags[2].ti_Tag = MUIA_InnerRight;    roottags[2].ti_Data = 6;
        roottags[3].ti_Tag = MUIA_InnerTop;      roottags[3].ti_Data = 6;
        roottags[4].ti_Tag = MUIA_InnerBottom;   roottags[4].ti_Data = 6;
        roottags[5].ti_Tag = TAG_DONE;           roottags[5].ti_Data = 0;
        row = (pages && rail && strip && trslot) ? bay_group(roottags) : 0;
        if (row) {
            DoMethod(row, OM_ADDMEMBER, (IPTR)trslot);
            DoMethod(row, OM_ADDMEMBER, (IPTR)rail);
            DoMethod(row, OM_ADDMEMBER, (IPTR)strip);
            DoMethod(row, OM_ADDMEMBER, (IPTR)pages);
        }
    }
    if (!row) {
        if (s_live)
            au_live_close(&s_lv);
        if (s_evfh) {
            Close(s_evfh);
            s_evfh = 0;
        }
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
    for (i = 0; i < 5; i++)
        DoMethod(s_devbtn[i], MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 3,
            MUIM_Application_ReturnID, RIAPP_ID_DEV0 + (ULONG)i);
    for (i = 0; i < (int)RI_TAB_COUNT; i++)
        DoMethod(s_tabs[i], MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 3,
            MUIM_Application_ReturnID, RIAPP_ID_TAB0 + (ULONG)i);
    rail_for_tab();
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    rail_leds_show();
    {
        IPTR open = 0, tabs = 0;
        GetAttr(MUIA_Window_Open, win, &open);
        for (i = 0; i < (int)RI_TAB_COUNT; i++)
            tabs += s_tabs[i] ? 1 : 0;
        rlog("RIAPP panel: tabbed Synths/Drums/Levi/Mix/FX + transport, rack bay (open=%ld rack=%ld tabs=%ld)\n",
            (long)open, (long)(s_art_mcc && s_bay_mcc), (long)tabs, 0, 0);
    }

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
        if (ret >= (LONG)RIAPP_ID_DEV0 && ret < (LONG)(RIAPP_ID_DEV0 + 5u))
            dev_visibility_toggle((uint32_t)ret - RIAPP_ID_DEV0);
        if (ret >= (LONG)RIAPP_ID_TAB0 && ret < (LONG)(RIAPP_ID_TAB0 + RI_TAB_COUNT))
            tab_switch((uint32_t)ret - RIAPP_ID_TAB0);
        if (timer_armed && CheckIO((struct IORequest *)treq)) {
            WaitIO((struct IORequest *)treq);
            treq->tr_node.io_Command = TR_ADDREQUEST;
            treq->tr_time.tv_secs = 0;
            treq->tr_time.tv_micro = 100000;
            SendIO((struct IORequest *)treq);
            if (!s_live && s_tr_state != RI_TR_STOPPED)
                ri_live_render(&s_core.session, s_fl, s_fr, RIAPP_FRAMES);
        }
        if (ri_live_meters_read(&s_core.session, &m) == 0) {
            cursor = m.cursor_ticks;
            have_cursor = 1;
        }
        sync_transport();
        for (i = C_P0; i <= C_P4; i++)
            sync_pat(i, have_cursor ? cursor : 0u);
        sync_303v(0u);
        sync_303v(1u);
        sync_drumv(0u);
        sync_drumv(1u);
        sync_leviv();
        sync_values();
        meter_round(s_live ? s_lv.mix_freq : 48000u);
        /* Wedge diagnostic (2026-09-27 Dell freeze under interaction):
         * ~30 s heartbeat while live (10 Hz timer ticks the loop).
         * Last line dates the wedge; buffers/xruns say whether the
         * render task was still producing. snd/pend are diagnostic-only
         * unsynchronized byte reads (same basis as the meter snapshot):
         * they show WHAT the engine plays after pattern selection.
         * Remove after. */
        if (s_live && ++hb >= 300u) {
            hb = 0u;
            rlog("RIAPP hb: buffers=%lu xruns=%lu render_max=%lu us snd=%u/%u/%u/%u pend=%u/%u/%u/%u\n",
                ri_atomic_load_acq(&s_lv.drv.buffers),
                ri_atomic_load_acq(&s_lv.drv.xruns),
                ri_atomic_load_acq(&s_lv.drv.render_us_max),
                s_core.session.player.sounding_slot[0], s_core.session.player.sounding_slot[1],
                s_core.session.player.sounding_slot[2], s_core.session.player.sounding_slot[3],
                s_core.session.player.pending_slot[0], s_core.session.player.pending_slot[1],
                s_core.session.player.pending_slot[2], s_core.session.player.pending_slot[3]);
        }
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
                ri_atomic_load_acq(&s_lv.drv.buffers), ri_atomic_load_acq(&s_lv.drv.xruns), ri_atomic_load_acq(&s_lv.drv.render_us_max), ri_atomic_load_acq(&s_lv.drv.render_us_sum_ms), s_lv.period_us);
    }
    SetAttrs(win, MUIA_Window_Open, FALSE, TAG_DONE);
    MUI_DisposeObject(app);
    rack_classes_drop();
    ri_rsection_dispose_class();
    if (s_evfh) {
        Close(s_evfh);
        s_evfh = 0;
    }
    return 0;
}
