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
 * evidence file, never silent-by-design): transport shuffle/loop/
 * rewind/FF/song-mode, record lamp, pattern shuffle switches, mixer
 * on/off switches (the tempo knob drives the session since 2026-09-29),
 * 303 programming buttons (pending state: they shape the
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
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/muimaster.h>
#include <proto/timer.h>
#include <proto/utility.h>
#include <proto/asl.h>
#include <libraries/asl.h>
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
#include "gui/zoomfit.h"
#include "gui/skin_aros.h"
#include "gui/panelui.h"
#include "gui/panelctl.h"
#include "gui/midimap.h"
#include "gui/miditrans.h"
#include "midi_io/midi_bridge.h"
#include "midi_io/midi_chan.h"
#include "midi_io/midi_levi.h"
#include "midi_io/midi_out.h"
#include "midi_io/midi_mmc.h"
#include "midi_io/midi_follow.h"
#include "platform/pal/ri_pal_midi.h"
#include "gui/panelgeo.h"
#include "gui/livestate.h"
#include "gui/widgets/rsection.h"
#include "gui/draw/art.h"
#include "audio_io/audio_ahi_live.h"
#include "app/core/riapp_core.h"
#include "platform/aros/pack_909.h"
#include "project/rbng.h"
#include "project/playlist.h"
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
    C_FX0, C_FX1, C_FX2, C_FX3, C_MXA, C_MXB, C_MX9, C_MXL, C_MST, C_N
};
static const ULONG c_sections[C_N] = {
    RI_SEC_TRANSPORT,
    RI_SEC_PAT_SYNTH1, RI_SEC_PAT_SYNTH2, RI_SEC_PAT_808, RI_SEC_PAT_909, RI_SEC_PAT_LEVI,
    RI_SEC_SYNTH1, RI_SEC_SYNTH2, RI_SEC_808, RI_SEC_909, RI_SEC_LEVI, RI_SEC_MIX_808,
    RI_SEC_PCF, RI_SEC_DELAY, RI_SEC_DIST, RI_SEC_COMP,
    RI_SEC_MIX_SYNTH1, RI_SEC_MIX_SYNTH2, RI_SEC_MIX_909, RI_SEC_MIX_LEVI, RI_SEC_MASTER
};
/* Pattern instance (bank + track slot) behind each PAT/voice canvas. */
static const uint32_t c_pat_instance[C_N] = {
    0u, 0u, 1u, 2u, 3u, 4u, 0u, 1u, 2u, 3u, 4u, 2u, 0u, 0u, 0u, 0u, 0u, 1u, 3u, 4u, 0u
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
/* MIDI input (M2): the 15 value canvases feed the push shadow; the
 * bridge queues arrive here, intents are counted for M3 to consume. */
static const int s_val_canvas[15] = {
    C_303A, C_303B, C_808, C_909, C_LEVI, C_MIX, C_FX0, C_FX1, C_FX2, C_FX3, C_MXA, C_MXB, C_MX9, C_MXL, C_MST
};
static struct RIMidiIn s_midi;
static struct RIMidiTrans s_mtrans;
static struct RIMidiSettings s_mset;
static struct RISectUI *s_midi_uis[15];
static uint8_t s_midi_sec[15];
static uint8_t s_midi_sh[15u * 256u];
static struct RIMidiMsg s_midi_msgs[32];
static struct RIMidiIntent s_midi_its[16];
static uint32_t s_midi_intents[5];
static ULONG s_midi_lms, s_midi_lmu;
static ULONG s_efreq;   /* EClock rate, read once; see lat_efreq() */
static Object *s_canvas[C_N];
static int s_zoom[C_N]; /* content zoom per canvas (transport: compact) */
static int s_skin_zoom; /* content zoom mirrored into the skin registry */
/* S7 installed mods (Classic always entry 0, no files) + loader sync. */
static char s_moddir[96];
static char s_modnames[15][64];
static const char *s_modptrs[16];
static uint32_t s_nmods;
static struct RISkinAssign s_skin_shadow; /* repaint only on real change */
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
/* Rail as a page group, one page per tab (owner 2026-10-05): each page holds
 * its own copies of the power buttons, so a tab switch flips a page instead
 * of MUIA_ShowMe on buttons, which relayouted the whole window (5-37 ms). */
static Object *s_railpages;
static Object *s_railbtn[RI_TAB_COUNT][5];
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
#define RIAPP_ID_ZOOM0 1030u /* + zoom: 0 = 1x, 1 = 1.5x, 2 = 2x, 3 = Fit */
#define RIAPP_ID_SONG0 1040u /* + Songs menu: 0 load song, 1 load playlist, 2 next, 3 previous, 4 pattern mode */
/* Window chrome estimate for Fit (S5, fail-safe generous: borders +
 * title + menu strip + slack; overestimating can only pick smaller). */
#define RIAPP_CHROME_W 32
#define RIAPP_CHROME_H 72
static Object *s_root;          /* window root group (zoom InitChange) */
static Object *s_zoomitems[4];  /* View menu items (zoom checkmarks) */
static int s_zoom_mode = RI_ZOOMFIT_FIT; /* -1 Fit, else content zoom */
static Object *s_songitems[5]; /* Songs menu items (NULL when the menu failed) */

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
static int s_mst_shown[2]; /* MASTER strip L/R meter shadows */

/* Demo song, bank table, transport and meters live in app/core (T8).
 * Status lines also go to the TEMP log, opened, appended and closed per
 * line so it can be read while RIAPP runs (the Run redirect stays empty).
 * Formatted with vsnprintf (T6: no RawDoFmt packing) and sunk preformatted.
 *
 * TEMP is the first mounted USB/stick volume when there is one, RAM: when
 * there is not (platform/aros/fs_aros.c). RIAPP.LOG is the evidence every
 * on-target run is judged from, and on RAM: it died with the first reboot:
 * the 2026-10-02 session's 346925 B of log was lost to /tmp on the host
 * side and the guest copy went with the next reboot, so the figures had to
 * be reconstructed from a transcript. The log belongs where a reboot cannot
 * reach it. RIAPP_LOG=<vol> overrides, as RIAPP_EVLOG does. */
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
    uint32_t k = 0u;
    s_evvol[0] = 0;
    if (!DOSBase)
        return;
    {
        char v[16];
        LONG r = GetVar((STRPTR)"RIAPP_EVLOG", (STRPTR)v, (LONG)sizeof v - 1u, 0L);
        if (r > 0) {
            while (v[k] && k < sizeof(s_evvol) - 1u) {
                s_evvol[k] = v[k];
                k++;
            }
            s_evvol[k] = 0;
            return;
        }
    }
    /* The volume list lives in platform/ now (ri_pal_sticky_vol): it is
     * the same list that decides where RIAPP.LOG goes, and two copies of
     * a list that must agree is a list that will not. */
    if (ri_pal_sticky_vol(s_evvol, sizeof(s_evvol)) != 0
        && ri_pal_path(RI_PATH_TEMP, s_evvol, sizeof(s_evvol)) != 0)
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
    char buf[256]; /* TAB carries fn/fb/fr/fl/mui since B0: 192 truncated it */
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

/* Where RIAPP.LOG lands. RIAPP_LOG=<vol> pins it (as RIAPP_EVLOG pins the
 * ev-log); otherwise RI_PATH_TEMP, which prefers a mounted stick over the
 * reboot-wiped RAM:. Resolved per line because a stick can be pulled
 * mid-run, and a log that follows the volume out is better than one that
 * keeps writing to a handle that has stopped working. */
static int rlog_base(char *out, uint32_t cap) {
    if (!DOSBase)
        return 1;
    {
        char v[16];
        LONG r = GetVar((STRPTR)"RIAPP_LOG", (STRPTR)v, (LONG)sizeof v - 1u, 0L);
        if (r > 0) {
            uint32_t k = 0u;
            while (v[k] && k + 1u < cap) {
                out[k] = v[k];
                k++;
            }
            out[k] = 0;
            return 0;
        }
    }
    return ri_pal_path(RI_PATH_TEMP, out, cap);
}

/* Roll the log once per process when it has grown past the cap.
 *
 * WHY (owner 2026-10-04). RIAPP.LOG is append-only and lives on the Dell's
 * stick, so it only ever grows: it had reached 575791 B. That is a defect of
 * ours with two costs. Every evidence pull has to move the whole file, because
 * the bulk-get protocol has no ranged read -- and the Dell's e1000.device 1.1
 * is the driver whose Tx-interrupt/FreeMem race wedges the guest under exactly
 * that (llm-wiki 2026-09-25, proven and reproduced). So an unbounded log is a
 * standing contribution to losing the lane.
 *
 * WHY ROTATE RATHER THAN TRUNCATE. The append-only behaviour is deliberate and
 * useful: every lane record reads several runs out of one file, and instances
 * are told apart by a changed log line. So the previous session is KEPT as
 * RIAPP.LOG.1 and the current one starts clean. That bounds the footprint at two
 * generations while preserving exactly what cross-run analysis needs -- the
 * run before this one.
 *
 * Roll happens BEFORE the first append of the process, so the size test costs
 * one open and the steady-state cost is one flag check.
 */
#define RIAPP_LOG_ROLL_BYTES 262144u   /* 256 KB; two generations ~= 512 KB worst case */
#define RIAPP_LOG_ROLL_MAX   4194304u  /* stop counting past 4 MB: the decision is already made */
static int s_log_rolled;
static char s_roll_note[200];      /* the roll decision, written by rlog */

static void log_roll_if_big(const char *dir, const char *leaf) {
    ULONG size = 0;
    int have = 0;
    char fn[160], prev[176];
    struct Process *me = 0;
    APTR oldwin = 0;
    char note[200];
    if (s_log_rolled)
        return;
    s_log_rolled = 1;
    if (ri_pal_path_join(fn, sizeof fn, dir, leaf) != 0)
        return;
    /* SIZE BY COUNTING THE BYTES, ONCE PER SESSION (2026-10-04). Two obvious
     * ways to ask "how big is this file" are both silently wrong on this stick:
     * Seek(f, 0, OFFSET_END) returns 0 for a 600 KB file, and Lock() straight
     * onto the FILE fails while the log is being appended. The 4-arg Examine
     * that would settle it is not in the v1 SDK. So the file is READ once and
     * its length counted: no dependence on Seek, Lock or Examine semantics,
     * which is the point -- a cap built on any of the three reads exactly like
     * a cap that works when it silently does nothing.
     *
     * Bounded on purpose. Above RIAPP_LOG_ROLL_MAX the answer does not change
     * the decision (roll either way), so the read is skipped rather than
     * growing with the file. 4 MB at a USB stick's read rate is well under a
     * second, and it happens exactly once per process. */
    {
        static UBYTE buf[4096];
        BPTR rf = Open((CONST_STRPTR)fn, MODE_OLDFILE);
        if (rf) {
            for (;;) {
                LONG got = Read(rf, buf, (ULONG)sizeof buf);
                if (got <= 0)
                    break;
                size += (ULONG)got;
                if (size > RIAPP_LOG_ROLL_MAX)
                    break;              /* far past the cap; stop counting */
            }
            Close(rf);
            have = 1;
        }
    }
    if (!have) {
        snprintf(note, sizeof note, "RIAPP log: session start, size UNKNOWN -> keeping\n");
    } else if (size < RIAPP_LOG_ROLL_BYTES) {
        snprintf(note, sizeof note, "RIAPP log: session start, %lu B of %lu -> keeping\n",
            (unsigned long)size, (unsigned long)RIAPP_LOG_ROLL_BYTES);
    } else {
        snprintf(note, sizeof note, "RIAPP log: session start, %lu B of %lu -> ROLLING to RIAPP.LOG.1\n",
            (unsigned long)size, (unsigned long)RIAPP_LOG_ROLL_BYTES);
        snprintf(prev, sizeof prev, "%s.1", fn);
        DeleteFile((CONST_STRPTR)prev);
        if (!Rename((CONST_STRPTR)fn, (CONST_STRPTR)prev)) {
            snprintf(note, sizeof note,
                "RIAPP log: %lu B but RENAME FAILED -> keeping, never lose a log\n",
                (unsigned long)size);
        } else {
            strncat(note, "RIAPP log: previous session kept as RIAPP.LOG.1\n",
                sizeof note - strlen(note) - 1u);
        }
    }
    if (me)
        me->pr_WindowPtr = oldwin;
    /* Always say what was decided: a silent optimisation that quietly never
     * fires is indistinguishable from one that works, and this one did. */
    snprintf(s_roll_note, sizeof s_roll_note, "%s", note);  /* rlog writes it */
}

/* RIAPP.LOG stays open for the whole run (owner Dell 2026-10-05). The old
 * open-append-close per line lost every line written while any other
 * process had the file open: on the stick's FAT handler a MODE_READWRITE
 * open is refused while a reader holds the file (a `Type` or an evidence
 * pull), while a reader can still open a file a writer already holds, and
 * every write during that read lands. So: open once and Flush each line;
 * lines that cannot be written yet wait in a bounded buffer and go out on
 * the next successful open; a failed write (stick pulled) closes the
 * handle so the next line reopens, wherever RIAPP_LOG points by then. */
static BPTR s_logfh;
static char s_logfn[96];
static char s_logpend[16384];
static ULONG s_logpend_n, s_logpend_lost;

static void rlog_close(void) {
    if (s_logfh) {
        Close(s_logfh);
        s_logfh = 0;
    }
}

static void rlog_hold(const char *buf) {
    ULONG n = (ULONG)strlen(buf);
    if (s_logpend_n + n >= sizeof s_logpend) {
        s_logpend_lost++;
        return;
    }
    memcpy(s_logpend + s_logpend_n, buf, n);
    s_logpend_n += n;
}

static void rlog(const char *fmt, ...) {
    char buf[512], base[48], fn[96];
    va_list ap;
    if (!DOSBase)
        return;
    va_start(ap, fmt);
    ri_log_format(buf, sizeof buf, fmt, ap);
    va_end(ap);
    ri_pal_log_sink(buf);
    if (rlog_base(base, sizeof base) != 0)
        return;
    if (ri_pal_path_join(fn, sizeof fn, base, "RIAPP.LOG") != 0)
        return;
    log_roll_if_big(base, "RIAPP.LOG");
    if (s_roll_note[0]) {
        rlog_hold(s_roll_note);         /* goes out first, with the log's own path */
        s_roll_note[0] = 0;
    }
    if (s_logfh && strcmp(fn, s_logfn) != 0)
        rlog_close();                   /* RIAPP_LOG moved the log */
    if (!s_logfh) {
        s_logfh = Open((STRPTR)fn, MODE_READWRITE);
        if (!s_logfh) {
            rlog_hold(buf);             /* busy (a reader holds it): keep */
            return;
        }
        strncpy(s_logfn, fn, sizeof s_logfn - 1u);
        s_logfn[sizeof s_logfn - 1u] = '\0';
        Seek(s_logfh, 0, OFFSET_END);
        if (s_logpend_n) {
            Write(s_logfh, s_logpend, (LONG)s_logpend_n);
            s_logpend_n = 0u;
        }
        if (s_logpend_lost) {
            char note[96];
            snprintf(note, sizeof note, "RIAPP log: %lu lines lost while the log was busy\n",
                (unsigned long)s_logpend_lost);
            FPuts(s_logfh, (STRPTR)note);
            s_logpend_lost = 0u;
        }
    }
    if (FPuts(s_logfh, (STRPTR)buf) != 0 || !Flush(s_logfh)) {
        rlog_close();                   /* stick pulled: reopen next line */
        rlog_hold(buf);
    }
}

/* Transport state edge -> the render task (or the null session). */
/* CAPTURE=<file>: record the live output (what AHI plays) from the next
 * Play to the next Stop/pause, then write a 16-bit stereo WAV at the device
 * rate (owner 2026-10-01: pull a WAV of a song off the Dell). The render
 * task copies each half into the GUI-owned buffer (live driver W capture);
 * all file IO stays on the GUI task. One capture per run. */
#define RIAPP_CAP_SECONDS 600u
static char s_cap_path[256];
static int s_cap_state;            /* 0 off, 1 armed, 2 recording, 3 written */
static WORD *s_cap_mem;

static void cap_put32(UBYTE *p, ULONG v) {
    p[0] = (UBYTE)v;
    p[1] = (UBYTE)(v >> 8);
    p[2] = (UBYTE)(v >> 16);
    p[3] = (UBYTE)(v >> 24);
}

static void capture_arm(const char *path) {
    ULONG max;
    if (!s_live || !path || !path[0] || s_cap_state)
        return;
    max = RIAPP_CAP_SECONDS * s_lv.mix_freq;
    s_cap_mem = (WORD *)AllocVec(max * 4u, MEMF_ANY | MEMF_CLEAR);
    if (!s_cap_mem) {
        rlog("RIAPP capture %s: no memory for %lu s\n", path, (unsigned long)RIAPP_CAP_SECONDS);
        return;
    }
    strncpy(s_cap_path, path, sizeof s_cap_path - 1u);
    s_cap_path[sizeof s_cap_path - 1u] = '\0';
    s_lv.cap_buf = s_cap_mem;
    s_lv.cap_max = max;
    s_cap_state = 1;
    rlog("RIAPP capture %s: armed (%lu s max)\n", s_cap_path, (unsigned long)RIAPP_CAP_SECONDS);
}

static void capture_start(void) {
    if (s_cap_state != 1)
        return;
    ri_atomic_store_rel(&s_lv.drv.cap_pos, 0u);
    ri_atomic_store_rel(&s_lv.drv.cap_on, 1u);
    s_cap_state = 2;
    rlog("RIAPP capture %s: recording\n", s_cap_path);
}

static void capture_finish(const char *why) {
    UBYTE h[44];
    ULONG frames, bytes, rate;
    BPTR fh;
    LONG ok;
    if (s_cap_state != 2)
        return;
    ri_atomic_store_rel(&s_lv.drv.cap_on, 0u);
    Delay(2);                                         /* > one device period */
    frames = ri_atomic_load_acq(&s_lv.drv.cap_pos);
    s_cap_state = 3;
    rate = s_lv.mix_freq;
    bytes = frames * 4u;
    memcpy(h, "RIFF", 4);
    cap_put32(h + 4, 36u + bytes);
    memcpy(h + 8, "WAVEfmt ", 8);
    cap_put32(h + 16, 16u);
    h[20] = 1; h[21] = 0;                             /* PCM */
    h[22] = 2; h[23] = 0;                             /* stereo */
    cap_put32(h + 24, rate);
    cap_put32(h + 28, rate * 4u);
    h[32] = 4; h[33] = 0;                             /* block align */
    h[34] = 16; h[35] = 0;                            /* bits */
    memcpy(h + 36, "data", 4);
    cap_put32(h + 40, bytes);
    fh = Open((CONST_STRPTR)s_cap_path, MODE_NEWFILE);
    ok = fh && Write(fh, h, 44) == 44 && Write(fh, s_cap_mem, (LONG)bytes) == (LONG)bytes;
    if (fh)
        Close(fh);
    rlog("RIAPP capture %s: %s, %lu frames (%lu s) at %lu Hz%s\n", s_cap_path, why,
        (unsigned long)frames, (unsigned long)(rate ? frames / rate : 0u), (unsigned long)rate,
        ok ? "" : " - WRITE FAILED");
}

/* M5h: MMC out (R3's "and add sending"). Separate E0 from clock out on
 * purpose -- driving another machine's transport is a bigger consequence
 * than sending it a clock, so the safe choice (clock only) has to be
 * expressible. Declared up here because sync_transport() (below) and
 * midi_drain() both read them, and both come before the midi_io block. */
static struct RIMidiMmcOut s_mmcout;
static uint32_t s_clk_sent_last;  /* sender counters, for the R4 lamp */
static int s_mmc_echo;            /* 1 while applying an INBOUND MMC intent */

static void sync_transport(void) {
    int st = s_ui[C_TR]->u.tr.tr.state;
    /* Owner 2026-09-29: the tempo knob drives the session (was
     * display-only); the live render adopts bpm into the engine.
     * M3: while MIDI clock is followed the session tracks the measured
     * tempo (float, set by the MIDI drain after this), so the integer
     * knob value must not overwrite it here. */
    {
        int tempo = ri_sui_value(s_ui[C_TR], RI_STR_TEMPO);
        if (!s_ui[C_TR]->u.tr.tempo_lock && tempo >= 20 && tempo <= 500 &&
            (float)tempo != s_core.session.bpm)
            ri_live_set_bpm(&s_core.session, (float)tempo);
    }
    if (st == s_tr_state)
        return;
    s_tr_state = st;
    /* MMC out (M5h). Driven from the ONE place the transport changes, so
     * there is no second path that can move it and forget to announce it.
     * `echo` is 0 when the change came FROM an inbound MMC: echoing a
     * command back to the master that just sent it is a feedback loop,
     * and a locate would bounce between the two machines. */
    if (midi_mmc_out_enabled(&s_mmcout)) {
        uint8_t f[10];
        uint32_t fn = 0u;
        if (st == RI_TR_PLAYING || st == RI_TR_RECORD)
            fn = midi_mmc_out_play(&s_mmcout, f, sizeof f);
        else if (st == RI_TR_STOPPED)
            fn = midi_mmc_out_stop(&s_mmcout, f, sizeof f);
        if (fn && !s_mmc_echo) {
            if (ri_pal_midi_send(s_mset.cluster, f, fn) == 0)
                evlog("MMC", "tx=%u bytes=%lu", (ULONG)f[4], (ULONG)fn);
            else
                evlog("MMC", "tx=%u refused", (ULONG)f[4]);
        }
    }
    if (st == RI_TR_PLAYING || st == RI_TR_RECORD) {
        /* RECORD plays: the record lane lands in Step 4. */
        if (s_live)
            au_live_request(&s_lv, AU_LIVE_CMD_PLAY);
        else
            ri_core_play(&s_core);
        capture_start();
        rlog("RIAPP play\n");
        evlog("TR", "PLAY");
    } else {
        if (s_live)
            au_live_request(&s_lv, AU_LIVE_CMD_STOP);
        else
            ri_core_stop(&s_core);
        rlog("RIAPP stop\n");
        capture_finish("stopped");
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
        /* Song mode (songs & playlists 2026-09-30): the song track owns
         * the selections; the panel only shows the chosen slot. */
        if (!s_ui[C_TR]->u.tr.song_mode)
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
    if (!s_ui[C_TR]->u.tr.song_mode)
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

/* MIDI input drain (M2): bridge queues into midimap (G7 behaviour,
 * LEDs included), changed values push through the bridge, follower
 * intents are logged and counted for the M3 transport application.
 * Routing depends only on the panel focus — never on the visible tab —
 * so the Levi tab cannot swallow remote notes for foci 1-4. */
static uint32_t s_midi_ignored;
static uint64_t s_midi_last_f8;
static int s_midi_was_locked;
static uint32_t s_midi_fb_n;   /* follow lines since the last drift print */
static uint32_t s_midi_f8n;    /* F8 clocks the app has counted */
static struct RIMidiChan s_chan;   /* M4: device instances and their channels */
static struct RIMidiOut s_mout;    /* M5: clock-out producer (render writes it) */
/* M5h: MMC out (R3's "and add sending"). Separate E0 from clock out on
 * purpose -- driving another machine's transport is a bigger consequence
 * than sending it a clock, so the safe choice (clock only) has to be
 * expressible. */
static uint32_t s_midi_levi, s_midi_levi_note, s_midi_levi_perf;
/* R4's clock-out lamp, driven from the SENDER's own counters rather than
 * from the setting. A lamp wired to "is it enabled" stays green through
 * exactly the failure it exists to show: requested on, sender task
 * refused, producer failed closed, no byte on the wire. */
static void clkout_lamp(void) {
    uint32_t sent = ri_pal_midi_sent();
    int flowing = (sent != s_clk_sent_last) ? 1 : 0;
    s_clk_sent_last = sent;
    ri_str_clkout_set(&s_ui[C_TR]->u.tr, flowing, sent);
}

static void midi_drain(void) {
    struct RIMidiBridge *b = ri_pal_midi_bridge();
    uint32_t n, k, i, sends = 0u, nf8 = 0u;
    uint32_t ch0;
    ULONG ns, nu, dms;
    uint32_t locked;
    uint64_t eng_cursor;
    float bpm, tempo;
    clkout_lamp();
    if (!b)
        return;
    ch0 = s_panel.changes;
    n = midi_bridge_read_ch(b, s_midi_msgs, 32u);
    for (i = 0u; i < n; i++) {
        int tr0 = s_ui[C_TR]->u.tr.tr.state;
        struct RIMidiLeviAction la;
        /* Two devices, two maps, one message each: the ReBirth remote
         * (G7, Appendix C) and the Leviasynth (M4, its own chart) each
         * decide for themselves whether this message is theirs. Neither
         * can reach the other's controls (t183). */
        if (midi_levi_message(&s_chan, s_midi_msgs[i].b[0], s_midi_msgs[i].b[1],
                s_midi_msgs[i].b[2], &la)) {
            s_midi_levi++;
            if (la.kind == RI_LEVI_ACT_PARAM) {
                ri_ctl_send(&s_core.ctl, la.key, (uint8_t)la.val);
                evlog("LEVI", "param %04x=%u", la.key, (unsigned)la.val);
            } else if (la.kind == RI_LEVI_ACT_NOTE) {
                ri_ctl_send2(&s_core.ctl, la.key, (uint8_t)la.val, la.hi);
                s_midi_levi_note++;
                evlog("LEVI", "note %u %s vel %u", (unsigned)la.note,
                    la.on ? "on" : "off", (unsigned)la.val);
            } else {
                /* Performance signals ride the spare byte too: a bend
                 * needs 14 bits, a poly-aftertouch needs its note. */
                ri_ctl_send2(&s_core.ctl, la.key, (uint8_t)la.val, la.hi);
                s_midi_levi_perf++;
                evlog("LEVI", "perf %u val %u hi %u", (unsigned)la.perf,
                    (unsigned)la.val, (unsigned)la.hi);
            }
        }
        ri_midi_msg(&s_midi, &s_panel, s_midi_msgs[i].b[0], s_midi_msgs[i].b[1],
            s_midi_msgs[i].b[2]);
        /* Transport edges are momentary commands: a Play followed by Stop
         * in the same drained batch must still reach the engine as two
         * transitions, rather than collapsing to the batch-final state. */
        if (s_ui[C_TR]->u.tr.tr.state != tr0)
            sync_transport();
        if (s_midi_msgs[i].b[0] == 0xF8u) {
            nf8++;
            s_midi_f8n++;
            s_midi_last_f8 = s_midi_msgs[i].t_us;
        }
    }
    if (n)
        sends = ri_panel_midi_push(s_midi_uis, s_midi_sec, 15u, &s_core.ctl, s_midi_sh);
    if (sends)
        evlog("MIDI", "push=%u", sends);
    if (s_midi.ignored != s_midi_ignored) {
        evlog("MIDI", "ignored=%u", s_midi.ignored);
        s_midi_ignored = s_midi.ignored;
    }
    /* The applier's cursor IS the engine's tick cursor (session ppq):
     * the panel position is a display projection, so writing it never
     * moved the engine — a Continue after a seek played from wherever the
     * engine had stopped, and the servo's reference was a per-frame
     * projection at (until the meters fix) a fixed 120 BPM rate. Read by
     * the render task; a torn read is a one-block phase error the servo
     * removes. */
    eng_cursor = s_core.session.cursor_ticks;
    k = midi_bridge_read_in(b, s_midi_its, 16u);
    for (i = 0u; i < k; i++) {
        uint32_t kind = s_midi_its[i].it.kind;
        if (kind < 5u)
            s_midi_intents[kind]++;
        evlog("MIDI", "intent=%u seek=%lu", kind, (ULONG)s_midi_its[i].it.seek_16ths);
        /* This intent came FROM the wire, so any MMC out it causes must
         * not be echoed back to the master that sent it. Cleared after
         * the apply below, which is what actually moves the transport. */
        s_mmc_echo = 1;
        midi_trans_apply(&s_mtrans, &s_midi_its[i].it, s_ui[C_TR], &eng_cursor);
        s_mmc_echo = 0;
        /* START and SEEK wrote the take's start point into eng_cursor and
         * only pressed Play on the panel. The engine has to be standing
         * there: ri_live_locate hands the move to the render task, which
         * owns both cursors (a store here is discarded while playing, and
         * pressing Play on an already-playing panel is a no-op — the M3
         * lane proof read tick 12095 for a Start that meant tick 0). */
        if (kind == RI_FOLLOW_PLAY_START || kind == RI_FOLLOW_SEEK) {
            ri_live_locate(&s_core.session, eng_cursor);
            s_ui[C_TR]->u.tr.cursor = eng_cursor;  /* display until it plays */
            evlog("MIDI", "locate=%lu", (ULONG)eng_cursor);
        }
    }
    /* Tempo follows the measured clock while locked (M3); the panel
     * tempo shows measured and its knob + TAP stay read-only. */
    locked = midi_follow_locked(&b->follow);
    bpm = midi_follow_bpm(&b->follow);
    eng_cursor = s_core.session.cursor_ticks;
    tempo = midi_trans_tempo(&s_mtrans, s_ui[C_TR], nf8, locked, bpm,
        eng_cursor);
    if (tempo > 0.0f) {
        ri_live_set_bpm(&s_core.session, tempo);
        /* The drift trace (E0 cadence, 32 blocks): the phase error in
         * ticks is the whole point of the servo and nothing else in the
         * log shows it. Wall clock from CurrentTime — the ev-log's
         * counter column is audio buffers, which is not a clock. */
        if (!s_midi_was_locked || ++s_midi_fb_n >= 32u) {
            s_midi_fb_n = 0u;
            CurrentTime(&ns, &nu);
            /* The drift trace (E0 cadence, 32 blocks). err is the phase
             * error in ticks and is the whole point of the servo; f8 is
             * what the app counted against the bridge's wire-side f8w,
             * and the three drops say who lost anything. */
            evlog("MIDI", "follow=%ubpm sess=%u eng=%lu smpl=%lu exp=%ld "
                "err=%ld f8=%lu/%lu drop=%lu,%lu,%lu xr=%lu t=%lu.%03lu",
                (uint32_t)(tempo + 0.5f),
                (uint32_t)(s_core.session.bpm * 10.0f), (ULONG)eng_cursor,
                (ULONG)s_core.session.meters.samples,
                (LONG)s_mtrans.expected,
                (LONG)(s_mtrans.expected - (int64_t)eng_cursor),
                (ULONG)s_midi_f8n, (ULONG)midi_follow_clocks(&b->follow),
                (ULONG)b->ch_dropped, (ULONG)b->in_dropped, (ULONG)b->camd_dropped,
                (ULONG)s_core.session.xruns,
                (ULONG)ns, (ULONG)(nu / 1000u));
        }
    }
    /* Dropout: silence past the R1 law ends the take here (the follower
     * poll cannot run app-side — the bridge task owns the follower).
     * Self-latching: it only fires while the panel still plays. */
    if (midi_trans_tempo_locked(&s_mtrans) &&
        s_ui[C_TR]->u.tr.tr.state != RI_TR_STOPPED && bpm > 0.0f) {
        uint64_t tick_us = 60000000u / ((uint64_t)(bpm * 24.0f) + 1u);
        uint64_t limit = 96u * tick_us;
        struct EClockVal tk;
        uint64_t now_us = 0u;
        if (s_efreq) {
            ReadEClock(&tk);
            now_us = ((uint64_t)tk.ev_hi << 32) | tk.ev_lo;
            now_us = now_us / s_efreq * 1000000u + (now_us % s_efreq) * 1000000u / s_efreq;
        }
        if (limit < 2000000u)
            limit = 2000000u;
        if (now_us > s_midi_last_f8 && now_us - s_midi_last_f8 > limit) {
            struct RIFollowIntent stop;
            stop.kind = RI_FOLLOW_STOP;
            stop.pad[0] = stop.pad[1] = stop.pad[2] = 0u;
            stop.seek_16ths = 0u;
            midi_trans_apply(&s_mtrans, &stop, s_ui[C_TR], &s_ui[C_TR]->u.tr.cursor);
            evlog("MIDI", "dropout");
        }
    }
    s_midi_was_locked = midi_trans_tempo_locked(&s_mtrans);
    CurrentTime(&ns, &nu);
    dms = s_midi_lms ? (ns - s_midi_lms) * 1000u + nu / 1000u - s_midi_lmu / 1000u : 0u;
    s_midi_lms = ns;
    s_midi_lmu = nu;
    ri_midi_elapse(&s_midi, &s_panel, dms);
    if (s_panel.changes != ch0) {
        for (i = 0u; i < 15u; i++)
            MUI_Redraw(s_canvas[i], MADF_DRAWOBJECT);
    }
}

/* Sounding value controls (mouse incl. drags and arrow repeats report the
 * hit control): exactly one control-plane message when the lane key is
 * nonzero (the bridge owns that law, t83). Transport and PAT canvases
 * travel their state paths above. */
static void sync_values(void) {
    int i;
    for (i = 0; i < 15; i++) {
        int c = s_val_canvas[i];
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
            struct RISectUI *u = s_ui[c];
            uint16_t okey = 0u;
            int oval = 0;
            if (ri_sui_ctl_key(u, s_dg[c]->last_hit & 0xFFu, &okey, &oval)) {
                /* Levi per-oscillator param (fidelity P2): explicit key. */
                ri_panel_ctl_send_key(&s_core.ctl, okey, oval);
                evlog("CTL", "%04x=%d", okey, oval);
            } else {
                /* Levi page encoders send their target (fidelity P1). */
                uint16_t reg = (uint16_t)((uint16_t)c_sections[c] << 8) |
                    (uint16_t)(ri_sui_ctl_idx(u, s_dg[c]->last_hit & 0xFFu) & 0xFFu);
                int val = ri_sui_value(u, reg & 0xFFu);
                ri_panel_ctl_send(&s_core.ctl, reg, val);
                evlog("CTL", "%04x=%d", reg, val);
            }
        }
    }
}

/* ---- Songs & playlists (owner 2026-09-30) -------------------------------
 * Load an RBNG song (or a playlist of them) into the core, show it on the
 * panels, play it in Song mode and move to the next playlist entry when a
 * song has played through its last sounding bar. The song track owns the
 * pattern selections in Song mode (sync_pat stops capturing). */
static struct RISong s_song;
static struct RBAutoEv s_song_ev[RI_CORE_AUTO_CAP];
static uint32_t s_song_tk[RI_CORE_AUTO_CAP];
static uint16_t s_song_ct[RI_CORE_AUTO_CAP];
static uint8_t s_song_vl[RI_CORE_AUTO_CAP];
static struct RIPlaylist s_pl;
static int s_pl_on;
static uint32_t s_pl_cur;
static int s_song_on;             /* a song is loaded (Song mode) */
/* Set while the automatic demo-song probe loads. Suppresses the modal
 * failure requester for that load only -- see song_fail. A user-initiated load
 * from the Songs menu still gets the requester. */
static int s_song_probe;

/* Stop the transport and wait until the render task has applied it: the
 * load below rewrites banks/track/automation it reads while playing. */
static void song_transport_stop(void) {
    struct RISectUI *t = s_ui[C_TR];
    int n;
    if (t->u.tr.tr.state != RI_TR_STOPPED) {
        ri_str_press(&t->u.tr, RI_STR_STOP);
        sync_transport();
    }
    for (n = 0; n < 50 && s_core.session.tr.state != RI_TR_STOPPED; n++)
        Delay(1);
}

/* Tick-0 automation onto the panels (knobs show the song's sound). */
static void song_ui_apply(void) {
    uint32_t i, k, c;
    struct RISectLevi *lv = &s_ui[C_LEVI]->u.slevi;
    for (i = 0u; i < s_song.natrk && s_song.atrk[i].tick == 0u; i++) {
        uint16_t key = s_song.atrk[i].ctl;
        uint8_t val = s_song.atrk[i].val;
        uint32_t blk = key & 0xFF00u, lo = key & 0xFFu;
        if (blk == 0x0F00u) {                        /* Levi oscillator params */
            lv->opv[(lo >> 5) & 7u][lo & 31u] = val;
            if ((lo & 31u) == RI_LEVI_OP_MODE)
                lv->opmode[(lo >> 5) & 7u] = val;
            continue;
        }
        if (blk == 0x1000u) {                        /* ENV 1-5 / LFO 1-5 */
            if (lo < 0xA0u && (lo >> 5) < RI_LEVI_NMENV)
                lv->mev[lo >> 5][lo & 31u] = val;
            else if (lo >= 0xA0u && lo < 0xF0u)
                lv->lfv[(lo - 0xA0u) >> 4][lo & 15u] = val;
            continue;
        }
        if (blk == 0x1100u && lo < 0x80u) {          /* matrix route fields */
            lv->mxv[lo >> 2][lo & 3u] = val;
            continue;
        }
        if (blk == 0x1200u) {                        /* macro route fields */
            lv->mrv[lo >> 5][(lo >> 2) & 7u][lo & 3u] = val;
            continue;
        }
        if (blk == 0x1300u || blk == 0x1400u) { /* LFO step editor (P8e) */
            uint32_t l = blk == 0x1300u ? (lo >> 2) & 3u : 4u, f = lo & 3u;
            if (f == RI_LEVI_LS_STEP)
                lv->lfsc[l] = val;
            else if (f == RI_LEVI_LS_VALUE)
                lv->lfsv[l] = val;
            else if (f == RI_LEVI_LS_RAMP)
                lv->lfsr[l] = val;
            continue;
        }
        for (k = 0u; k < ri_ctlreg_count(); k++) {
            const struct RICtlDef *d = ri_ctlreg_at(k);
            if (!d || !d->automatable || ri_ctlreg_auto_id(d) != key)
                continue;
            for (c = 0u; c < C_N; c++)
                if (s_ui[c] && c_sections[c] == d->section) {
                    ri_sui_set(s_ui[c], d->reg_id & 0xFFu, val);
                    break;
                }
            break;
        }
    }
    for (c = 0u; c < C_N; c++)
        if (s_canvas[c])
            ri_rsection_refresh(s_canvas[c]);
}

/* Pattern panels: lengths from the banks, selection = the bar-0 slot,
 * voice canvases refreshed through sync_pat's selection branch. */
static void song_ui_sync(void) {
    int c;
    for (c = C_P0; c <= C_P4; c++) {
        struct RISectUI *u = s_ui[c];
        uint32_t inst = c_pat_instance[c], k;
        const struct RIPatternBank *b = ri_core_bank_ro(&s_core, inst);
        uint8_t sel = ri_track_selected(&s_core.track, 0u, inst);
        for (k = 0u; k < 32u; k++) {
            uint8_t len = b->pat[k].length ? b->pat[k].length : 16u;
            u->u.pat.length[k] = len;
            s_pat_len[c][k] = len;
        }
        u->u.pat.off = 0u;
        s_pat_off[c] = 0u;
        u->u.pat.bank = (uint8_t)(sel / 8u);
        u->u.pat.pattern = (uint8_t)(sel % 8u);
        s_pat_bank[c] = 0xFFu;                        /* force the refresh branch */
        sync_pat(c, 0u);
        ri_rsection_refresh(s_canvas[c]);
    }
}

/* Tell the user why a song or playlist did not load (owner Dell
 * 2026-10-01: a silent failure reads as "nothing happens").
 *
 * QUIET MODE, and it is not optional for the automatic probe (2026-10-04).
 * EasyRequestArgs is MODAL: it blocks main() until a human clicks OK. The
 * demo-song lookup calls song_load_path up to three times, so a miss stacked
 * three requesters on the Dell and froze startup with the log cut mid-line --
 * which reads exactly like a hang, and it did: a stalled run was misdiagnosed
 * as an infinite loop in a directory walk before the requesters were spotted.
 *
 * The demo block's own contract already said "a miss is NOT an error and must
 * not raise a requester". This enforces it. In quiet mode the log line below
 * IS the response, and deliberately so: it is reachable over atcpbin with no
 * human at the keyboard, which a modal box on an unattended Dell is not. */
static void song_fail(const char *what, const char *path, const char *why) {
    struct EasyStruct es;
    IPTR args[3];
    rlog("RIAPP %s %s: %s%s\n", what, path, why,
        s_song_probe ? " (probe; requester suppressed)" : "");
    if (s_song_probe)
        return;
    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (CONST_STRPTR)"RIAPP";
    es.es_TextFormat = (CONST_STRPTR)"Cannot load %s\n%s\n%s";
    es.es_GadgetFormat = (CONST_STRPTR)"OK";
    args[0] = (IPTR)what;
    args[1] = (IPTR)path;
    args[2] = (IPTR)why;
    EasyRequestArgs(NULL, &es, NULL, (RAWARG)args);
}

/* Say so when the sound card could not be opened (owner 2026-10-02). The null
 * backend keeps the app usable for offline render, but a launch that lost the
 * audio path used to be completely silent: window, panels, playlist and
 * transport all came up, with no sound and no requester -- strictly worse than
 * a requester, which at least reports that something happened. On the Dell the
 * cause was a second RIAPP holding ahi.device, so AHI_AllocAudioA failed. Only
 * the genuinely absent-device case stays quiet, because there the null backend
 * is the documented fallback. Policy and evidence: ri_core_audio_failure_is_loud
 * (app/core/riapp_core.h) and tests/unit/t158_audio_failure_loud.c.
 *
 * The step number goes to the log, not the requester: this file already casts
 * err to IPTR for rlog's %ld, and a vararg format is not worth the risk in a
 * user-facing box when the number is already in the log. */
static void audio_fail(long err) {
    struct EasyStruct es;
    rlog("RIAPP audio: sound card unusable, continuing without sound [err %ld]\n",
        (IPTR)err, 0, 0, 0, 0);
    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (CONST_STRPTR)"RIAPP";
    es.es_TextFormat = (CONST_STRPTR)"Cannot start audio"
        "\n\nThe sound card could not be opened."
        "\nAnother program may already be using it."
        "\n\nRIAPP will run without sound (offline render only)."
        "\nClose the other program and start RIAPP again.";
    es.es_GadgetFormat = (CONST_STRPTR)"OK";
    EasyRequestArgs(NULL, &es, NULL, NULL);
}

/* The loaded song's name lives in the window title (owner Dell 2026-10-05):
 * the transport plate has no strip that fits a line of text at every zoom. */
static Object *s_win;
static char s_wintitle[96];

static void win_title_song(const char *leaf) {
    if (leaf && leaf[0])
        snprintf(s_wintitle, sizeof s_wintitle, "RIAPP live panel - %s", leaf);
    else
        snprintf(s_wintitle, sizeof s_wintitle, "RIAPP live panel");
    if (s_win)
        SetAttrs(s_win, MUIA_Window_Title, (IPTR)s_wintitle, TAG_DONE);
}

static int song_load_path(const char *path) {
    static char err[160];
    struct RICoreSong cs;
    struct RISectUI *t = s_ui[C_TR];
    uint32_t i, k;
    song_transport_stop();
    rbng_song_init(&s_song);
    s_song.atrk = s_song_ev;
    s_song.atrk_cap = RI_CORE_AUTO_CAP;
    if (rbng_read_song(path, &s_song, err, sizeof err) != 0) {
        song_fail("song", path, err);
        evlog("SONG", "fail %s", path);
        return 2;
    }
    memset(&cs, 0, sizeof cs);
    for (k = 0u; k < s_song.nbanks; k++)
        if (s_song.bank[k].instance < RI_SONGTRACK_INSTANCES)
            cs.bank[s_song.bank[k].instance] = &s_song.bank[k];
    cs.track = &s_song.track;
    cs.bpm = (float)s_song.tempo;
    cs.ppq = s_song.ppq;
    for (i = 0u; i < s_song.natrk; i++) {
        s_song_tk[i] = s_song.atrk[i].tick;
        s_song_ct[i] = s_song.atrk[i].ctl;
        s_song_vl[i] = s_song.atrk[i].val;
    }
    cs.auto_tick = s_song_tk;
    cs.auto_ctl = s_song_ct;
    cs.auto_val = s_song_vl;
    cs.nauto = s_song.natrk;
    if (ri_core_load_song(&s_core, &cs) != 0)
        rlog("RIAPP song %s: automation refused (%ld events)\n", path, (long)cs.nauto);
    s_core.session.cursor_ticks = 0u;                 /* from the top */
    s_core.session.tr.clicks = 0u;
    /* Drum-tail A0: one song, one ring record. The render loop reloads the
     * engine per buffer and must never reset the ring itself (Dell
     * 2026-10-07: per-buffer reset held n at 1 for a whole song) — so the
     * reset lives here, on the once-per-song path every load converges on
     * (explicit, demo probe, playlist advance). s_core.session.eng is the
     * object the live driver renders (drv.session aliases it). */
    ri_engine_drum_reset(&s_core.session.eng);
    ri_live_set_sections(&s_core.session, ri_live_sections(&s_core.session) | s_core.song_sections);
    if (!t->u.tr.song_mode)
        ri_str_press(&t->u.tr, RI_STR_MODE);
    ri_str_set_value(&t->u.tr, RI_STR_TEMPO, (int)s_song.tempo);
    t->u.tr.cursor = 0u;
    s_song_on = 1;
    song_ui_sync();
    song_ui_apply();
    /* Show it. The log line has said this all along, but the user had no way
     * to see which song was playing without pulling the log off the guest by
     * hand (owner 2026-10-03). The leaf name is what identifies a song; the
     * directory is noise at this size. */
    {
        const char *leaf = path, *q;
        for (q = path; *q; q++)
            if (*q == ':' || *q == '/')
                leaf = q + 1;
        ri_art_tr_set_song(leaf);
        win_title_song(ri_art_tr_song());
    }
    rlog("RIAPP song %s: %ld bars at %ld BPM\n", path, (long)s_core.song_bars, (long)s_song.tempo);
    evlog("SONG", "load %s bars=%lu bpm=%lu", path, (unsigned long)s_core.song_bars, (unsigned long)s_song.tempo);
    ri_str_press(&t->u.tr, RI_STR_PLAY);
    sync_transport();
    return 0;
}

static int playlist_load_path(const char *path) {
    static char text[16384], err[160], dir[RI_PLAYLIST_PATH];
    BPTR fh = Open((CONST_STRPTR)path, MODE_OLDFILE);
    LONG n;
    if (!fh) {
        song_fail("playlist", path, "cannot open");
        return 2;
    }
    n = Read(fh, text, (LONG)sizeof text - 1);
    Close(fh);
    text[n > 0 ? n : 0] = '\0';
    ri_playlist_dirname(path, dir, sizeof dir);
    if (ri_playlist_parse(text, dir, &s_pl, err, sizeof err) != 0) {
        song_fail("playlist", path, err);
        return 2;
    }
    s_pl_on = 1;
    s_pl_cur = 0u;
    rlog("RIAPP playlist %s: %ld songs\n", path, (long)s_pl.n);
    return song_load_path(s_pl.e[0].path);
}

static void playlist_step(int dir) {
    uint32_t k;
    if (!s_pl_on)
        return;
    for (k = 0u; k < s_pl.n; k++) {                   /* skip entries that fail to load */
        s_pl_cur = dir > 0 ? ri_playlist_next(&s_pl, s_pl_cur) : ri_playlist_prev(&s_pl, s_pl_cur);
        if (song_load_path(s_pl.e[s_pl_cur].path) == 0)
            return;
    }
}

/* ASL file requester; 0 ok with path filled. Every outcome is logged. */
static int song_pick(const char *title, const char *pattern, char *path, ULONG cap) {
    struct FileRequester *fr;
    int ok = 0;
    if (!AslBase) {
        song_fail("file", title, "asl.library did not open");
        return 2;
    }
    fr = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
        ASLFR_TitleText, (IPTR)title,
        ASLFR_InitialPattern, (IPTR)pattern,
        ASLFR_DoPatterns, TRUE,
        TAG_DONE);
    if (!fr) {
        song_fail("file", title, "no file requester");
        return 2;
    }
    if (!AslRequest(fr, 0))
        rlog("RIAPP %s: cancelled\n", title);
    else if (!fr->fr_File || !fr->fr_File[0])
        rlog("RIAPP %s: no file chosen (drawer %s)\n", title,
            fr->fr_Drawer ? (const char *)fr->fr_Drawer : "-");
    else {
        char leaf[128];
        uint32_t n = 0u;
        /* ASL returns the file gadget's text, which pads a partial name out to
         * the pattern with DOTS -- "zombie-nation.rbng" followed by ~150 of
         * them (observed 2026-10-04, Dell). That padding is what lands in
         * fr_File, so it must be stripped or the path cannot open. Trimming
         * trailing dots is the whole fix; anything else would mangle a real
         * name. Bounded copy because fr_File is ASL-owned memory. */
        while (fr->fr_File[n] && n + 1u < sizeof leaf) {
            leaf[n] = fr->fr_File[n];
            n++;
        }
        leaf[n] = 0;
        while (n > 0u && leaf[n - 1u] == '.')
            leaf[--n] = 0;
        strncpy(path, fr->fr_Drawer ? (const char *)fr->fr_Drawer : "", cap - 1u);
        path[cap - 1u] = '\0';
        ok = (n > 0u && AddPart((STRPTR)path, (CONST_STRPTR)leaf, cap)) ? 1 : 0;
        rlog("RIAPP %s: picked %s%s\n", title, path, ok ? "" : " (no usable file)");
    }
    FreeAslRequest(fr);
    return ok ? 0 : 2;
}

static void song_menu(ULONG id) {
    static char path[256];
    rlog("RIAPP songs menu %lu\n", (unsigned long)id);
    switch (id) {
    case 0u:
        if (song_pick("Load song", "#?.rbng", path, sizeof path) == 0) {
            s_pl_on = 0;
            song_load_path(path);
        }
        break;
    case 1u:
        if (song_pick("Load playlist", "#?.rbpl", path, sizeof path) == 0)
            playlist_load_path(path);
        break;
    case 2u:
        playlist_step(1);
        break;
    case 3u:
        playlist_step(-1);
        break;
    default:                                          /* back to Pattern mode */
        s_song_on = 0;
        s_pl_on = 0;
        if (s_ui[C_TR]->u.tr.song_mode)
            ri_str_press(&s_ui[C_TR]->u.tr, RI_STR_MODE);
        break;
    }
}

/* End of song: one bar of tail after the last sounding bar, then the next
 * playlist entry (wrapping), or stop for a single song. */
static void song_poll_end(int have_cursor, uint64_t cursor) {
    if (!s_song_on || !have_cursor || !s_core.song_bars || s_core.session.tr.state == RI_TR_STOPPED)
        return;
    if (cursor / RIAPP_TICKS_BAR < (uint64_t)s_core.song_bars + 1u)
        return;
    if (s_pl_on && s_pl.n > 1u)
        playlist_step(1);
    else if (s_pl_on)
        song_load_path(s_pl.e[s_pl_cur].path);        /* one-song playlist: again */
    else
        song_transport_stop();
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
        /* S3: meters repaint their own box (MIX(sec, RI_SMIX_METER)). */
        {
            const struct RIGeoSection *g = ri_geo_section(strip_sec[k]);
            int x0, y0, x1, y1;
            /* Zoom must be the CANVAS's zoom, not a literal 0 (2026-10-04). A
             * bbox computed at another zoom is a rectangle in a different
             * coordinate space, so it is clamped into something that looks
             * valid and repaints the wrong art. */
            if (g && ri_geo_bbox(g, (uint16_t)(((uint32_t)strip_sec[k] << 8) | RI_SMIX_METER),
                s_zoom[c_mix_canvas[k]], &x0, &y0, &x1, &y1) == 0)
                ri_rsection_refresh_box(s_canvas[c_mix_canvas[k]], x0, y0, x1, y1);
            else
                ri_rsection_refresh(s_canvas[c_mix_canvas[k]]);
        }
    }
    /* S4 MASTER L/R meters from the post-master snapshot taps. */
    {
        struct RISectUI *u = s_ui[C_MST];
        int ch;
        for (ch = 0; ch < 2; ch++) {
            int lvl = 0;
            const struct RIGeoSection *g;
            int x0, y0, x1, y1;
            if (ri_live_meters_read(&s_core.session, &mm) == 0)
                lvl = ri_live_meter_level(mm.master_peak[ch]);
            if (lvl == s_mst_shown[ch] || !u || !u->u.mix.board)
                continue;
            s_mst_shown[ch] = lvl;
            ri_smix_meter_set(u->u.mix.board, RI_SEC_MASTER, (uint32_t)ch, lvl);
            g = ri_geo_section(RI_SEC_MASTER);
            if (g && ri_geo_bbox(g, (uint16_t)(((uint32_t)RI_SEC_MASTER << 8) | (1u + (uint32_t)ch)),
                s_zoom[C_MST], &x0, &y0, &x1, &y1) == 0)
                ri_rsection_refresh_box(s_canvas[C_MST], x0, y0, x1, y1);
            else
                ri_rsection_refresh(s_canvas[C_MST]);
        }
    }
    playing = (s_tr_state != RI_TR_STOPPED);
    {
        /* G6b: the live feed says WHY it changed, and each canvas is
         * repainted only if its art can show that reason
         * (gui/panelui.h: ri_panel_live_stale). The blanket full
         * refresh this replaces cost 18 full repaints a second while
         * playing; with the governor's pri -1 yield (app/core/
         * live_driver.c) each one pre-empted the audio task for a
         * buffer or two, which is the Dell xruns (owner 2026-10-01:
         * 923 xruns in the tab-switch window against 630 repaints). */
        uint32_t live = ri_panel_live(&s_panel, playing, sixteenths);
        static int8_t s_chase_last[C_N];
        static int s_chase_init;
        int c;
        if (!s_chase_init) {
            for (c = 0; c < C_N; c++)
                s_chase_last[c] = -1;
            s_chase_init = 1;
        }
        for (k = 0; k < C_N; k++) {
            uint32_t sec = c_sections[k];
            int stale = ri_panel_live_stale(live, sec);
            if (stale == RI_STALE_NONE)
                continue;              /* no art here reads live state */
            if (stale == RI_STALE_ALL) {
                /* a held delete-tap edited pattern data: repaint the row
                 * it edited (and leave the chase shadow, which the full
                 * paint has just made true again). */
                ri_rsection_refresh(s_canvas[k]);
                continue;
            }
            if (stale == RI_STALE_BAR) {
                /* song mode: the Song Position display followed the song */
                const struct RIGeoSection *g = ri_geo_section(sec);
                int x0, y0, x1, y1;
                /* s_zoom[C_TR], NOT a literal 0 (2026-10-04). The transport
                 * canvas is unconditionally RI_GEO_ZOOM_COMPACT, so asking for
                 * zoom 0 asked for a rectangle in the wrong coordinate space:
                 *
                 *   asked (zoom 0)      : 540,52..622,92
                 *   real  (compact = 3) : 404,38..467,70
                 *   DISJOINT
                 *
                 * So the RI_STALE_BAR path was repainting a region that does not
                 * contain the Song Position display, clamped into something that
                 * looked valid and was therefore attributed to box_bar and read
                 * as innocent. Verified disjoint on the host, not reasoned about.
                 * This is a visible-behaviour bug (the bar digits were not being
                 * repainted by this path), filed separately from the stall. */
                if (g && ri_geo_bbox(g,
                    (uint16_t)(((uint32_t)RI_SEC_TRANSPORT << 8) | RI_STR_BAR),
                    s_zoom[C_TR],
                    &x0, &y0, &x1, &y1) == 0)
                    ri_rsection_refresh_box_why(s_canvas[k], x0, y0, x1, y1,
                        RI_RSEC_BOX_BAR);
                else
                    ri_rsection_refresh(s_canvas[k]);
                continue;
            }
            {
                /* RI_STALE_STEPS: a drum row, old lamp and new (the
                 * chase lamp lives inside its step key, S3). */
                uint32_t base = (sec == RI_SEC_808) ? RI_S808_STEP0 : RI_S909_STEP0;
                int f = ri_panel_focus_of(sec);
                int st = (f >= 0 && f < (int)RI_FOCUS_COUNT) ? s_panel.playhead[f] : -1;
                const struct RIGeoSection *g = ri_geo_section(sec);
                if (st < 0 || st > 15 || !g) {
                    ri_rsection_refresh(s_canvas[k]);
                    s_chase_last[k] = -1;
                    continue;
                }
                if (st == s_chase_last[k])
                    continue; /* lamp already where it belongs */
                {
                    /* ONE INVALIDATION FOR BOTH LAMPS, NOT TWO (2026-10-04).
                     *
                     * The old lamp and the new lamp are separate boxes, and this
                     * used to refresh them separately. That cost twice what it
                     * needed to because **MUI_Redraw is SYNCHRONOUS on AROS Zune**
                     * -- workbench/libs/muimaster/mui_redraw.c calls
                     * `DoMethod(obj, MUIM_Draw, 0)` inline, with no deferral -- so
                     * each call was a COMPLETE draw cycle, not a queued one.
                     *
                     * Measured on the Dell (2026-10-05): a quiet box repaint is
                     * ~234 us, of which ~122 us is the fixed per-partial cost that
                     * no phase timer covers. Two of them is ~714 us per step
                     * change; the drum chase moves constantly.
                     *
                     * The union is equivalent because build_dl and replay_dl_dmg
                     * are both clip-aware: one invalidation over the union paints
                     * both controls identically, and nothing between them is
                     * stale because everything inside the union is repainted.
                     * t167 asserts exactly that against the two-separate-boxes
                     * result.
                     */
                    int steps[2] = { s_chase_last[k], st }, s, ok = 1;
                    int ux0 = 0, uy0 = 0, ux1 = -1, uy1 = -1;
                    for (s = 0; s < 2; s++) {
                        int x0, y0, x1, y1;
                        if (steps[s] < 0 || steps[s] > 15)
                            continue;
                        if (ri_geo_bbox(g, (uint16_t)(base + (uint32_t)steps[s]), s_zoom[k],
                            &x0, &y0, &x1, &y1) == 0) {
                            if (ux1 < ux0) {  /* first box seeds the union */
                                ux0 = x0; uy0 = y0; ux1 = x1; uy1 = y1;
                            } else {
                                if (x0 < ux0) ux0 = x0;
                                if (y0 < uy0) uy0 = y0;
                                if (x1 > ux1) ux1 = x1;
                                if (y1 > uy1) uy1 = y1;
                            }
                        } else
                            ok = 0;
                    }
                    if (ok && ux1 >= ux0 && uy1 >= uy0)
                        ri_rsection_refresh_box_why(s_canvas[k], ux0, uy0, ux1, uy1,
                            RI_RSEC_BOX_STEPS);
                    else
                        ri_rsection_refresh(s_canvas[k]);
                    s_chase_last[k] = (int8_t)st;
                }
            }
        }
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
        Object *prow;
        if (rows[r].pat_sec == RI_TAB_PAT_NONE) {
            /* Levi voice-only row (owner 2026-09-29): the real
             * instrument has no pattern section; the bank engine
             * (C_P4/sync_pat) keeps running on slot 0 underneath. */
            prow = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE,
                MUIA_Group_Spacing, 2,
                Child, (IPTR)canvas_for_section(rows[r].voice_sec), TAG_DONE);
            if (!prow || !canvas_for_section(rows[r].voice_sec))
                return 0;
        } else {
            prow = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE,
                MUIA_Group_Spacing, 2,
                Child, (IPTR)canvas_for_section(rows[r].pat_sec),
                Child, (IPTR)canvas_for_section(rows[r].voice_sec), TAG_DONE);
            if (!prow || !canvas_for_section(rows[r].pat_sec) ||
                !canvas_for_section(rows[r].voice_sec))
                return 0;
        }
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

/* Tab-switch probe counters: rack-art draws, bay background requests and
 * the commands they built vs kept after the clip. */
static ULONG s_cnt_art, s_cnt_bay, s_cnt_bay_built, s_cnt_bay_kept;

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
        s_cnt_art++;
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
    struct BitMap *bm;      /* the plate, rendered once per size (see below) */
    LONG bw, bh;
};

/* The bay plate is ~one RECT per pixel row plus streaks, and Zune asks for
 * the background once per child box: a MIX page switch made 59 requests,
 * rebuilt the plate 59 times (30137 commands) and replayed 12377 RectFills
 * through the layer (owner Dell 2026-10-05). The plate depends only on the
 * bay's size, so it is rendered once into a friend bitmap and every request
 * is one blit of its box. */
static int rbay_plate(Object *obj, struct RBayData *d) {
    LONG w = _width(obj), h = _height(obj);
    struct RastPort brp;
    struct ri_dlist dl;
    if (w <= 0 || h <= 0)
        return 0;
    if (d->bm && d->bw == w && d->bh == h)
        return 1;
    if (d->bm)
        FreeBitMap(d->bm);
    d->bm = AllocBitMap((ULONG)w, (ULONG)h, GetBitMapAttr(_rp(obj)->BitMap, BMA_DEPTH),
        BMF_MINPLANES, _rp(obj)->BitMap);
    if (!d->bm)
        return 0;
    d->bw = w;
    d->bh = h;
    InitRastPort(&brp);
    brp.BitMap = d->bm;
    ri_dlist_init(&dl, s_art_back, 16384u, s_art_spool, sizeof s_art_spool);
    ri_art_bay(&dl, 0, 0, (int)w - 1, (int)h - 1);
    s_cnt_bay_built += dl.n;
    ri_rsection_replay(&brp, &dl);
    return 1;
}

static void rbay_plate_free(struct RBayData *d) {
    if (d->bm)
        FreeBitMap(d->bm);
    d->bm = NULL;
    d->bw = d->bh = 0;
}

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
    case MUIM_Cleanup:                  /* the window's bitmap may change */
        rbay_plate_free((struct RBayData *)INST_DATA(cl, obj));
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_DrawBackground: {
        /* One blit of the requested box out of the cached plate (grain
         * keyed to the bay's own box, as before). */
        struct MUIP_DrawBackground *m = (struct MUIP_DrawBackground *)msg;
        struct ri_dlist dl;
        d = (struct RBayData *)INST_DATA(cl, obj);
        if (!d->shown || m->width <= 0 || m->height <= 0)
            return FALSE;
        s_cnt_bay++;
        if (rbay_plate(obj, d)) {
            LONG sx = m->left - _left(obj), sy = m->top - _top(obj);
            LONG bw = m->width, bh = m->height;
            if (sx < 0) { bw += sx; sx = 0; }
            if (sy < 0) { bh += sy; sy = 0; }
            if (sx + bw > d->bw) bw = d->bw - sx;
            if (sy + bh > d->bh) bh = d->bh - sy;
            if (bw > 0 && bh > 0)
                BltBitMapRastPort(d->bm, sx, sy, _rp(obj), _left(obj) + sx, _top(obj) + sy,
                    bw, bh, 0xC0);
            return TRUE;
        }
        /* No bitmap (memory): the old per-request build, clipped. */
        ri_dlist_init(&dl, s_art_back, 16384u, s_art_spool, sizeof s_art_spool);
        ri_art_bay(&dl, _left(obj), _top(obj), _right(obj), _bottom(obj));
        s_cnt_bay_built += dl.n;
        art_clip(&dl, (int)m->left, (int)m->top, (int)(m->left + m->width - 1),
            (int)(m->top + m->height - 1));
        s_cnt_bay_kept += dl.n;
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
 * module's slot for ShowMe; gap_after inserts a 6 px double-seam gap
 * after that module index (S4: the MASTER strip stands apart). */
static Object *rack_page_gap(Object *const *mods, uint32_t n, Object **slots, uint32_t gap_after) {
    Object *row, *col, *l, *r, *sp[4], *gap = 0;
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
        if (i == gap_after) {
            gap = (Object *)MUI_NewObject(MUIC_Rectangle, MUIA_FixWidth, 6, TAG_DONE);
            if (!gap)
                return 0;
            DoMethod(row, OM_ADDMEMBER, (IPTR)gap);
        }
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
#define rack_page(mods, n, slots) rack_page_gap(mods, n, slots, 99u)

/* Mirror each device's active bit into its power glyph (the class redraws
 * itself when the state changes). */
static void rail_leds_show(void) {
    uint32_t d, g;
    if (!s_art_mcc)
        return;
    for (g = 0u; g < RI_TAB_COUNT; g++)
        for (d = 0u; d < 5u; d++)
            if (s_railbtn[g][d])
                SetAttrs(s_railbtn[g][d], MUIA_RArt_On,
                    (IPTR)(ri_vis_get(&s_vis, d) > 0 ? TRUE : FALSE), TAG_DONE);
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
static int rail_shows(uint32_t page, uint32_t d) {
    return (page == RI_TAB_MIX || page == RI_TAB_FX) ? 1 : dev_tab(d) == page;
}

static void rail_for_tab(void) {
    IPTR page = 0, now = 0;
    if (!s_pages || !s_railpages)
        return;
    GetAttr(MUIA_Group_ActivePage, s_pages, &page);
    GetAttr(MUIA_Group_ActivePage, s_railpages, &now);
    if (now != page)
        SetAttrs(s_railpages, MUIA_Group_ActivePage, page, TAG_DONE);
}

/* Device rail (owner 2026-09-27, power buttons 2026-09-28): a brushed
 * strip above the Register with one power button per device; the glyph
 * is the device's LED. One rail page per tab holds that tab's buttons
 * (2026-10-05), switched with the main pages by rail_for_tab. Labels live
 * in s_devlbl (kept, static). Without the classes the chips fall back to
 * plain buttons. */
static Object *tab_rail(void) {
    Object *pages, *rail, *fill;
    struct TagItem tags[6];
    uint32_t d, g;
    tags[0].ti_Tag = MUIA_Group_Horiz;   tags[0].ti_Data = TRUE;
    tags[1].ti_Tag = MUIA_Group_Spacing; tags[1].ti_Data = 6;
    tags[2].ti_Tag = MUIA_InnerLeft;     tags[2].ti_Data = 8;
    tags[3].ti_Tag = MUIA_InnerTop;      tags[3].ti_Data = 3;
    tags[4].ti_Tag = MUIA_InnerBottom;   tags[4].ti_Data = 3;
    tags[5].ti_Tag = TAG_DONE;           tags[5].ti_Data = 0;
    pages = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_PageMode, TRUE, TAG_DONE);
    if (!pages)
        return 0;
    for (d = 0u; d < 5u; d++) {
        const struct RIPanelDesc *pd = ri_panel_get(d);
        snprintf(s_devlbl[d], sizeof s_devlbl[d], "%s", (pd && pd->name) ? pd->name : "?");
    }
    for (g = 0u; g < RI_TAB_COUNT; g++) {
        rail = bay_group(tags);
        if (!rail)
            return 0;
        for (d = 0u; d < 5u; d++) {
            Object *btn;
            if (!rail_shows(g, d))
                continue;
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
            s_railbtn[g][d] = btn;
            if (!s_devbtn[d]) {
                s_devbtn[d] = btn;
                s_devled[d] = s_art_mcc ? btn : 0;
            }
            DoMethod(rail, OM_ADDMEMBER, (IPTR)btn);
        }
        fill = (Object *)MUI_NewObject(MUIC_Rectangle, TAG_DONE);
        if (fill)
            DoMethod(rail, OM_ADDMEMBER, (IPTR)fill);
        DoMethod(pages, OM_ADDMEMBER, (IPTR)rail);
    }
    s_railpages = pages;
    return pages;
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
/* ═══ USER-FACING LATENCY (owner 2026-10-05) ═══════════════════════════════
 *
 * The box-repaint path is closed, and the owner redirected the work to what a
 * person actually feels: a click or knob drag that takes long to appear, slow tab
 * switches, and a loop tick that arrives late because the GUI was starved. Three
 * numbers, and -- the rule this lane most needs -- EACH ONE CARRIES A SELF-CHECK,
 * because six instruments this session reported a property the code did not have.
 *
 * The self-checks are of the same kind as the gap counter's partition invariant:
 * they can FAIL, they are printed next to the number they protect, and each one
 * is a statement about arithmetic rather than about intent.
 *
 *   ticks  vs elapsed   ticks x 100 ms must account for the span, within one
 *                       tick. A late tick therefore cannot hide inside a count.
 *   hist   sums to n    the four buckets must partition the observations. A
 *                       dropped or double-counted sample fails here.
 *   cycle  sums to total the five per-tab times must not exceed the measured
 *                       cycle, and a cycle is only reported when all five are
 *                       present, so a partial cycle is never averaged in.
 *
 * Clock: EClock, the same PIT source the draw timing uses, and gated on
 * RIAPP_DIAG like it -- these are diagnostics and must not cost a release build
 * anything. With the switch off every field reads 0 and `latdiag=0` says so.
 */
typedef struct {
    ULONG ticks, late_n, late_max, late_sum, hist[4], span_lo;
    uint64_t span_ec, late_ec, early_ec;  /* exact EClock sums for the identity */
    int tick_seeded;
    ULONG lat_n, lat_max, lat_sum, lat_hist[4];
    struct { uint32_t lo, hi; } lat_in_at;   /* input stamp */
    int lat_have;
    ULONG cyc_n, cyc_total, cyc_max, cyc_tab[RI_TAB_COUNT], cyc_fill;
    ULONG cyc_acc[RI_TAB_COUNT];   /* summed over cycles; mean at report */
    int armed;
} RILatency;
static RILatency s_lat;

/* EClock rate for the latency arithmetic, read ONCE on first use.
 *
 * ReadEClock returns the RATE in its return value and the CURRENT value in the
 * struct, so this is both the rate and the probe that tells us timer.device is
 * there at all. `TimerBase` is a weak extern owned by the audio side, so this
 * does not add a dependency or a startup ordering requirement. */
static ULONG lat_efreq(void) {
    struct EClockVal t;
    if (!s_efreq && TimerBase)
        s_efreq = ReadEClock(&t);
    return s_efreq;
}

/* Bucket edges in microseconds: <1 ms, <5, <20, >=20. Chosen at the scale a
 * person would notice rather than at round numbers. */
static void lat_bucket(ULONG *h, ULONG us) {
    if (us < 1000UL) h[0]++;
    else if (us < 5000UL) h[1]++;
    else if (us < 20000UL) h[2]++;
    else h[3]++;
}

static int lat_on(void) {
    return rsection_diag_enabled();
}

static void lat_report(void) {
    ULONG i, hsum = 0u, lsum = 0u, span_us, expect_us, drift;
    if (!lat_on())
        return;
    for (i = 0u; i < 4u; i++) { hsum += s_lat.hist[i]; lsum += s_lat.lat_hist[i]; }
    span_us = s_lat.span_lo;                    /* accumulated clock since arm */
    expect_us = s_lat.ticks * 100000UL;         /* one period per tick */
    /* Self-check 1: the tick count must account for the elapsed span, to within
     * one period. Printed as `drift`, which is the honest residual. */
    drift = span_us > expect_us ? span_us - expect_us : expect_us - span_us;
    rlog("RIAPP lat: ticks=%lu late=%lu late_max=%lu us hist=%lu/%lu/%lu/%lu"
        " hs=%lu | input=%lu in_max=%lu us in_avg=%lu us ihist=%lu/%lu/%lu/%lu"
        " is=%lu | cyc=%lu cyc_avg=%lu us cyc_max=%lu us per_tab=%lu/%lu/%lu/%lu/%lu"
        " sumtab=%lu | span=%lu us expect=%lu us drift=%lu us latesum=%lu us SELFCHK=%s\n",
        (unsigned long)s_lat.ticks, (unsigned long)s_lat.late_n,
        (unsigned long)s_lat.late_max,
        (unsigned long)s_lat.hist[0], (unsigned long)s_lat.hist[1],
        (unsigned long)s_lat.hist[2], (unsigned long)s_lat.hist[3],
        (unsigned long)hsum,
        (unsigned long)s_lat.lat_n, (unsigned long)s_lat.lat_max,
        (unsigned long)(s_lat.lat_n ? s_lat.lat_sum / s_lat.lat_n : 0u),
        (unsigned long)s_lat.lat_hist[0], (unsigned long)s_lat.lat_hist[1],
        (unsigned long)s_lat.lat_hist[2], (unsigned long)s_lat.lat_hist[3],
        (unsigned long)lsum,
        (unsigned long)s_lat.cyc_n,
        (unsigned long)(s_lat.cyc_n ? s_lat.cyc_total / s_lat.cyc_n : 0u),
        (unsigned long)s_lat.cyc_max,
        (unsigned long)(s_lat.cyc_n ? s_lat.cyc_acc[0] / s_lat.cyc_n : 0u),
        (unsigned long)(s_lat.cyc_n ? s_lat.cyc_acc[1] / s_lat.cyc_n : 0u),
        (unsigned long)(s_lat.cyc_n ? s_lat.cyc_acc[2] / s_lat.cyc_n : 0u),
        (unsigned long)(s_lat.cyc_n ? s_lat.cyc_acc[3] / s_lat.cyc_n : 0u),
        (unsigned long)(s_lat.cyc_n ? s_lat.cyc_acc[4] / s_lat.cyc_n : 0u),
        (unsigned long)(s_lat.cyc_n ? (s_lat.cyc_acc[0] + s_lat.cyc_acc[1] +
            s_lat.cyc_acc[2] + s_lat.cyc_acc[3] + s_lat.cyc_acc[4]) / s_lat.cyc_n : 0u),
        (unsigned long)span_us, (unsigned long)expect_us, (unsigned long)drift,
        (unsigned long)s_lat.late_sum,
        /* Self-check 3, inline so it cannot be separated from the numbers. */
        /* span - ticks*period is, by construction, the SUM of every tick's
         * lateness. So the check is an EQUALITY, not a threshold: `drift` must
         * equal `late_sum`. Printing a drift and calling the line ok without
         * comparing the two is how 2.8e9 us shipped as SELFCHK=ok. */
        (hsum == s_lat.ticks && lsum == s_lat.lat_n &&
         s_lat.span_ec + s_lat.early_ec ==
             (uint64_t)s_lat.ticks * ((uint64_t)s_efreq / 10ULL) + s_lat.late_ec) ? "ok" : "VIOLATED");
    s_lat.ticks = s_lat.late_n = s_lat.late_max = s_lat.late_sum = 0u;
    for (i = 0u; i < 4u; i++) { s_lat.hist[i] = 0u; s_lat.lat_hist[i] = 0u; }
    s_lat.lat_n = s_lat.lat_max = s_lat.lat_sum = 0u;
    s_lat.cyc_n = s_lat.cyc_total = s_lat.cyc_max = 0u;
    s_lat.cyc_fill = 0u;
    for (i = 0u; i < RI_TAB_COUNT; i++) s_lat.cyc_acc[i] = 0u;
    s_lat.span_lo = 0u;
    s_lat.span_ec = s_lat.late_ec = s_lat.early_ec = 0u;
}

static void tab_switch(uint32_t g) {
    uint32_t k;
    if (g >= RI_TAB_COUNT || !s_pages)
        return;
    ULONG x0 = s_live ? ri_atomic_load_acq(&s_lv.drv.xruns) : 0u, us = 0u, efreq = 0u;
    ULONG c_sec = ri_rsection_draw_calls(), c_art = s_cnt_art, c_bay = s_cnt_bay;
    ULONG c_built = s_cnt_bay_built, c_kept = s_cnt_bay_kept;
    /* Full-draw split baseline (tab-switch B0): per-canvas df_* sums at
     * entry; the same sums after the rail give this switch's fb/fr/fl/fn
     * as deltas. Integer adds only — no clock reads of its own, so this
     * costs a release build nothing measurable. */
    ULONG c_fb = 0u, c_fr = 0u, c_fl = 0u;
    LONG c_fn = 0;
    struct EClockVal e0, e1, e2, e3;
    ULONG pg_us = 0u, tb_us = 0u, rl_us = 0u;
    for (k = 0u; k < (uint32_t)C_N; k++)
        if (s_dg[k]) {
            c_fb += s_dg[k]->df_build_sum;
            c_fr += s_dg[k]->df_replay_sum;
            c_fl += s_dg[k]->df_blit_sum;
            c_fn += s_dg[k]->df_n;
        }
    if (TimerBase)
        efreq = ReadEClock(&e0);
    SetAttrs(s_pages, MUIA_Group_ActivePage, (IPTR)g, TAG_DONE);
    if (efreq)
        ReadEClock(&e1);
    for (k = 0u; k < RI_TAB_COUNT; k++)
        if (s_tabs[k])
            SetAttrs(s_tabs[k], MUIA_RArt_Active, (IPTR)(k == g ? TRUE : FALSE), TAG_DONE);
    if (efreq)
        ReadEClock(&e2);
    rail_for_tab();
    if (efreq) {                      /* tab-stall probe (owner Dell 2026-10-01) */
        uint64_t a, b, c, dd;
        ReadEClock(&e3);
        a = ((uint64_t)e0.ev_hi << 32) | e0.ev_lo;
        b = ((uint64_t)e1.ev_hi << 32) | e1.ev_lo;
        c = ((uint64_t)e2.ev_hi << 32) | e2.ev_lo;
        dd = ((uint64_t)e3.ev_hi << 32) | e3.ev_lo;
        us = (ULONG)((dd - a) * 1000000ULL / efreq);
        pg_us = (ULONG)((b - a) * 1000000ULL / efreq);
        tb_us = (ULONG)((c - b) * 1000000ULL / efreq);
        rl_us = (ULONG)((dd - c) * 1000000ULL / efreq);
    }
    /* Phases partition the switch: page + tabs + rail == us (to rounding).
     * Draw counts say who painted: canvases (sec), rack art (art), bay
     * background requests (bay) and the commands those built vs kept.
     * Full-draw split (tab-switch B0): fn full draws during the switch cost
     * fb (display-list build) + fr (replay into the canvas bitmap) + fl
     * (blit to the window); mui = page_us - (fb + fr + fl) is MUI's own page
     * handling, layout, backfill and anything outside our draw. mui must
     * never read negative: the spans are disjoint by construction (the draws
     * run inline inside the SetAttrs above), so a negative mui means the
     * instrument overlaps and every number from it is suspect. With
     * RIAPP_DIAG off the df_* sums stay zero and fn/fb/fr/fl read 0 while
     * mui == page_us — "not measured", not "free". */
    {
        ULONG e_fb = 0u, e_fr = 0u, e_fl = 0u;
        LONG e_fn = 0;
        LONG mui;
        for (k = 0u; k < (uint32_t)C_N; k++)
            if (s_dg[k]) {
                e_fb += s_dg[k]->df_build_sum;
                e_fr += s_dg[k]->df_replay_sum;
                e_fl += s_dg[k]->df_blit_sum;
                e_fn += s_dg[k]->df_n;
            }
        mui = (LONG)pg_us - (LONG)(e_fb - c_fb + e_fr - c_fr + e_fl - c_fl);
        evlog("TAB", "page=%d us=%lu page_us=%lu tabs_us=%lu rail_us=%lu sec=%lu art=%lu bay=%lu built=%lu kept=%lu fn=%ld fb=%lu fr=%lu fl=%lu mui=%ld xruns+%lu",
            g, (unsigned long)us, (unsigned long)pg_us, (unsigned long)tb_us, (unsigned long)rl_us,
            (unsigned long)(ri_rsection_draw_calls() - c_sec), (unsigned long)(s_cnt_art - c_art),
            (unsigned long)(s_cnt_bay - c_bay), (unsigned long)(s_cnt_bay_built - c_built),
            (unsigned long)(s_cnt_bay_kept - c_kept),
            (long)(e_fn - c_fn), (unsigned long)(e_fb - c_fb),
            (unsigned long)(e_fr - c_fr), (unsigned long)(e_fl - c_fl), (long)mui,
            (unsigned long)((s_live ? ri_atomic_load_acq(&s_lv.drv.xruns) : 0u) - x0));
    }
    /* Latency cycle (owner 2026-10-05): one bucket per tab slot, and a cycle is
     * only counted when all five slots are filled, so a partial cycle is never
     * averaged in as if it were a whole one. That is the self-check on this
     * number -- not a comment, a condition. */
    if (lat_on()) {
        /* cyc_fill is a bit mask of the tabs visited (review 2026-10-05): a
         * count of switches let five switches between two tabs close a
         * "cycle" whose other three slots were stale from an older one. */
        s_lat.cyc_tab[g] = us;
        s_lat.cyc_fill |= 1UL << g;
        if (s_lat.cyc_fill == (1UL << RI_TAB_COUNT) - 1UL) {
            ULONG t = 0u, k2;
            for (k2 = 0u; k2 < RI_TAB_COUNT; k2++)
                t += s_lat.cyc_tab[k2];
            s_lat.cyc_n++;
            s_lat.cyc_total += t;
            if (t > s_lat.cyc_max)
                s_lat.cyc_max = t;
            {   /* per-tab ACCUMULATORS, reset with everything else.
                 *
                 * This was a self-check gap found by the first run: per_tab
                 * held the LAST cycle's values and was printed next to
                 * `cyc=0`, so a window containing no complete cycle showed
                 * five non-zero numbers as though they were current. The
                 * `SELFCHK=ok` did not catch it, because the buckets and the
                 * tick arithmetic were all internally consistent -- the lie
                 * was in a field nothing was checking. So the field now
                 * accumulates over every cycle and is reported as a mean
                 * that is only defined when cyc_n > 0. **A self-check covers
                 * the quantities it sums; it does not cover the ones it does
                 * not.** */
                ULONG k3;
                for (k3 = 0u; k3 < RI_TAB_COUNT; k3++)
                    s_lat.cyc_acc[k3] += s_lat.cyc_tab[k3];
            }
            s_lat.cyc_fill = 0u;
            for (k2 = 0u; k2 < RI_TAB_COUNT; k2++)
                s_lat.cyc_tab[k2] = 0u;
        }
    }
}

/* S5 zoom choice: persisted mode (Fit default) in ENVARC: prefs. */
static int zoom_persist_read(void) {
    char base[64], path[80], buf[8];
    uint32_t got = 0u;
    if (!DOSBase)
        return RI_ZOOMFIT_FIT;
    if (ri_pal_path(RI_PATH_PREFS, base, sizeof base) != 0)
        return RI_ZOOMFIT_FIT;
    if (ri_pal_path_join(path, sizeof path, base, "zoom") != 0)
        return RI_ZOOMFIT_FIT;
    if (ri_pal_read_file(path, buf, sizeof buf - 1u, &got) != 0 || got == 0u)
        return RI_ZOOMFIT_FIT;
    if (got > sizeof buf - 1u)
        got = (uint32_t)sizeof buf - 1u;
    buf[got] = 0;
    return ri_zoom_parse(buf, got);
}

static void zoom_persist_write(int mode) {
    char base[64], path[80], buf[8];
    int n;
    if (!DOSBase)
        return;
    n = ri_zoom_format(mode, buf, sizeof buf);
    if (n <= 0)
        return;
    if (ri_pal_path(RI_PATH_PREFS, base, sizeof base) != 0)
        return;
    if (ri_pal_path_join(path, sizeof path, base, "zoom") != 0)
        return;
    if (ri_pal_write_file(path, buf, (uint32_t)n) != 0) {
        /* Prefs dir may not exist yet (fresh install): create and retry. */
        (void)CreateDir((STRPTR)base);
        if (ri_pal_write_file(path, buf, (uint32_t)n) != 0)
            rlog("RIAPP zoom: cannot persist choice\n", 0, 0, 0, 0, 0);
    }
}

/* Frontmost public screen size; 0 on failure (caller fails closed). */
static int zoom_screen_size(int *w, int *h) {
    struct Screen *sc;
    if (!w || !h)
        return 2;
    *w = 0;
    *h = 0;
    sc = LockPubScreen(NULL);
    if (!sc)
        return 2;
    *w = (int)sc->Width;
    *h = (int)sc->Height;
    UnlockPubScreen(NULL, sc);
    return 0;
}

/* Apply a zoom mode (-1 Fit, else 0..2): relayout the window around the
 * new canvas minima, latch the menu checkmarks, persist the choice. */
/* Apply a zoom mode (-1 Fit, else 0..2), clamped to what fits the
 * screen (owner breakage 2026-09-29: an overflowing zoom drops
 * rail/strip/pages): relayout the window around the new canvas minima,
 * latch the menu checkmarks, persist the requested choice. */
static void app_set_zoom(int mode) {
    int z, i, k;
    int sw = 0, sh = 0;
    if (mode != RI_ZOOMFIT_FIT && (mode < 0 || mode > 2))
        return;
    if (zoom_screen_size(&sw, &sh) != 0 || sw <= 0 || sh <= 0) {
        sw = 0;
        sh = 0;
    }
    z = ri_zoom_clamp(sw, sh, RIAPP_CHROME_W, RIAPP_CHROME_H, mode);
    if (DOSBase && z != (mode == RI_ZOOMFIT_FIT ? z : mode))
        rlog("RIAPP zoom: want %d clamped to %d on %dx%d\n", mode, z, sw, sh, 0);
    if (!s_root)
        return;
    DoMethod(s_root, MUIM_Group_InitChange);
    for (i = 0; i < C_N; i++) {
        /* Levi (dense hardware panel) takes a zoom step when it fits. */
        LONG cz = (LONG)(i == C_TR ? RI_GEO_ZOOM_COMPACT
            : i == C_LEVI ? ri_zoom_levi(z, sw, sh, RIAPP_CHROME_W, RIAPP_CHROME_H) : z);
        s_zoom[i] = (int)cz;
        SetAttrs(s_canvas[i], MUIA_RSection_Zoom, (IPTR)cz, TAG_DONE);
    }
    DoMethod(s_root, MUIM_Group_ExitChange);
    s_zoom_mode = mode;
    if (z != s_skin_zoom) {
        /* Skin zoom caches follow the canvas zoom (S7 registry). The
         * zoom attribute already freed every canvas bitmap, so the
         * relayout repaints with the new caches. */
        s_skin_zoom = z;
        if (DOSBase)
            evlog("SKIN", "sync=%d zoom=%d",
                ri_skin_aros_sync(&s_panel.skin_assign, s_moddir, s_skin_zoom),
                s_skin_zoom);
    }
    for (k = 0; k < 4; k++)
        if (s_zoomitems[k])
            SetAttrs(s_zoomitems[k], MUIA_Menuitem_Checked,
                (IPTR)(LONG)(k == (mode == RI_ZOOMFIT_FIT ? 3 : mode) ? TRUE : FALSE),
                TAG_DONE);
    zoom_persist_write(mode);
    evlog("ZOOM", "mode=%d zoom=%d", mode, z);
}

/* S7 installed-mod scan (Classic always entry 0): PAL dir walk,
 * bounded, skips a Classic directory like sectproof. */
static int skin_scan_cb(void *u, const char *name) {
    uint32_t *n = (uint32_t *)u, k;
    if (!u || !name || !name[0] || *n >= 15u)
        return 0;
    if (!strcmp(name, "Classic"))
        return 0;
    for (k = 0u; k < 63u && name[k]; k++)
        s_modnames[*n][k] = name[k];
    s_modnames[*n][k] = '\0';
    s_modptrs[1u + *n] = s_modnames[*n];
    (*n)++;
    return 0;
}

/* Mirror the panel assignment into the loader (S7 registry). */
static void skin_sync_current(void) {
    int n;
    if (!DOSBase)
        return;
    n = ri_skin_aros_sync(&s_panel.skin_assign, s_moddir, s_skin_zoom);
    evlog("SKIN", "sync=%d zoom=%d current=%s", n, s_skin_zoom,
        s_panel.skin_current);
    rlog("RIAPP skin: sync=%d zoom=%d current=%s\n", n, s_skin_zoom,
        s_panel.skin_current, 0, 0);
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

/* NOAUDIO: do not touch ahi.device at all (owner 2026-10-02).
 *
 * This exists for the riqemu1 lane, where `ahi.device ReadConfig` executes a
 * `movaps` against a source address that is not 16-byte aligned and raises
 * #GP inside the guest. That fault happens during OpenDevice, i.e. inside the
 * attempt itself, so no error code comes back to be reported: AROS raises its
 * own "Software Failure!" requester and RIAPP additionally raises its own
 * audio_fail() box, which is two requesters for one fault and leaves the log
 * unreadable behind the modal one.
 *
 * The reason this is a switch and not a fix: the null backend renders and
 * accounts per stage exactly as the device path does -- the dstg block is
 * present on runs whose audio line reads "AHI unavailable [err 4]" -- so every
 * render-cost measurement still works with the audio device skipped. Only the
 * sound is gone, and on a guest whose AHI cannot be opened there was never any
 * to lose. On a healthy guest the argument is absent and nothing changes.
 *
 * The earlier version of this was written and reverted unverified, because the
 * requester it failed to clear was the guest's and the log behind the modal box
 * could not be read. It is verified now precisely because the skip is logged
 * before the panel comes up, so the line is readable with no requester up. */
static int riapp_arg_noaudio(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++)
        if (argv[i] && !strcmp(argv[i], "NOAUDIO"))
            return 1;
    return 0;
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

/* Chunk drain for the Leviasynth startup adoption (W2, bug 1): move the
 * burst through the live session the same way the tail drain below does
 * (no-op once the render task owns the session). */
static uint32_t burst_drain(void *ctx) {
    (void)ctx;
    if (!s_live)
        ri_live_render(&s_core.session, s_fl, s_fr, RIAPP_FRAMES);
    return 1u;
}

/* MIDI settings from per-machine ENVARC: (M2, E0 pending owner decision
 * 3). Returns 1 when the variable exists (parsed into *v). */
static int midi_getnum(const char *name, long *v) {
    char vb[16];
    LONG r, k = 0;
    long sign = 1L, acc = 0L;
    int any = 0;
    if (!DOSBase)
        return 0;
    r = GetVar((STRPTR)name, (STRPTR)vb, (LONG)sizeof vb - 1u, 0L);
    if (r <= 0)
        return 0;
    vb[sizeof vb - 1u] = 0;
    while (vb[k] == ' ' || vb[k] == '\t')
        k++;
    if (vb[k] == '-') {
        sign = -1L;
        k++;
    } else if (vb[k] == '+') {
        k++;
    }
    while (vb[k] >= '0' && vb[k] <= '9') {
        acc = acc * 10L + (long)(vb[k] - '0');
        any = 1;
        k++;
    }
    if (!any)
        return 0;
    *v = sign * acc;
    return 1;
}

/* MIDI input setup (M2): ENVARC: settings, midimap + sync state, value
 * shadow, bridge receiver. Logs the resolved setup; a missing cluster
 * only logs (the bridge idles until gear appears). */
static void midi_setup(void) {
    char cb[64];
    LONG r;
    long v;
    int i;
    midi_settings_defaults(&s_mset);
    if (DOSBase) {
        r = GetVar((STRPTR)"RIAPP_MIDI_IN", (STRPTR)cb, (LONG)sizeof cb - 1u, 0L);
        if (r > 0) {
            uint32_t k = 0u;
            cb[sizeof cb - 1u] = 0;
            while (cb[k] && k < sizeof s_mset.cluster - 1u) {
                s_mset.cluster[k] = cb[k];
                k++;
            }
            s_mset.cluster[k] = 0;
        }
        if (midi_getnum("RIAPP_MIDI_CH", &v))
            midi_settings_set(&s_mset, RI_MIDI_SET_CHANNEL, v);
        if (midi_getnum("RIAPP_MIDI_SYNC", &v))
            midi_settings_set(&s_mset, RI_MIDI_SET_SYNC, v);
        if (midi_getnum("RIAPP_MIDI_LEVI", &v))
            midi_settings_set(&s_mset, RI_MIDI_SET_LEVI_CH, v);
        if (midi_getnum("RIAPP_MIDI_CLKOUT", &v))
            midi_settings_set(&s_mset, RI_MIDI_SET_CLK_OUT, v);
        if (midi_getnum("RIAPP_MIDI_LATMS", &v))
            midi_settings_set(&s_mset, RI_MIDI_SET_LAT_MS, v);
        if (midi_getnum("RIAPP_MIDI_MMCOUT", &v))
            midi_settings_set(&s_mset, RI_MIDI_SET_MMC_OUT, v);
    }
    rlog("RIAPP midi in=%s ch=%u sync=%u levi=%u clkout=%u lat=%d\n", s_mset.cluster,
        s_mset.channel, s_mset.sync, s_mset.levi_ch, s_mset.clk_out, s_mset.lat_ms);
    ri_midi_init(&s_midi, (uint8_t)(s_mset.channel - 1u));
    /* M4 channel table (E0, ledgered): the remote on its channel, the
     * Leviasynth on RIAPP_MIDI_LEVI_CH (default 2). */
    midi_chan_defaults(&s_chan);
    midi_chan_bind(&s_chan, RI_MCHAN_REMOTE, s_mset.channel);
    midi_chan_bind(&s_chan, RI_MCHAN_LEVI, s_mset.levi_ch);
    evlog("LEVI", "chan remote=%lu levi=%lu devices=%lu",
        (ULONG)s_mset.channel, (ULONG)s_mset.levi_ch, (ULONG)midi_chan_n(&s_chan));
    /* M5 clock out (E0, off by default). The render fills this ring and
     * the AROS sender task carries it to camd; the app itself sends
     * nothing. `lat_ms` is the output latency the clock LEADS by, so a
     * master receives the tick no later than the sound it describes. */
    midi_out_init(&s_mout, s_lv.mix_freq ? (uint32_t)s_lv.mix_freq : 48000u,
        (uint32_t)((s_lv.drv.session ? (double)s_lv.drv.session->bpm : 120.0) * 1000.0),
        (uint32_t)((uint64_t)s_mset.lat_ms *
            (s_lv.mix_freq ? s_lv.mix_freq : 48000u) / 1000u));
    if (s_mset.clk_out) {
        midi_out_enable(&s_mout, 1);
        if (ri_pal_midi_send_start(s_mset.cluster, &s_mout) != 0) {
            /* Fail closed: a sender we could not start means clock out is
             * off, not a half-working clock. */
            midi_out_enable(&s_mout, 0);
            rlog("RIAPP clock out refused (no sender task)\n");
        }
    }
    evlog("CLK", "out=%u enabled=%u lat_ms=%u",
        (ULONG)s_mset.clk_out, (ULONG)midi_out_pending(&s_mout),
        (ULONG)s_mset.lat_ms);
    /* MMC out (R3 sending). Off by default like everything else here. */
    midi_mmc_out_init(&s_mmcout, s_mset.mmc_out);
    evlog("MMC", "out=%u", (ULONG)midi_mmc_out_enabled(&s_mmcout));
    midi_trans_init(&s_mtrans);
    midi_trans_set_source(&s_mtrans, s_mset.sync);
    midi_trans_set_lat(&s_mtrans, s_mset.lat_ms);
    for (i = 0; i < 15; i++) {
        s_midi_uis[i] = s_ui[s_val_canvas[i]];
        s_midi_sec[i] = (uint8_t)c_sections[s_val_canvas[i]];
    }
    ri_panel_midi_shadow_init(s_midi_uis, s_midi_sec, 15u, s_midi_sh);
    if (ri_pal_midi_open_in(s_mset.cluster, 0, 0) != 0)
        rlog("RIAPP midi: no input on %s\n", s_mset.cluster);
}

static int riapp_main(int argc, char **argv) {
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
    int noaudio = riapp_arg_noaudio(argc, argv);

    ri_core_demo(&s_core);
    /* BUILD HASH AND DIAG MODE, STAMPED INTO THE MAIN LOG (owner 2026-10-05).
     *
     * The ev log already carried `build=`, but RIAPP.LOG -- the file every
     * on-target number is judged from -- did not, and this session logged three
     * stale-binary readings that a hash would have ended immediately ("the
     * optimisation does nothing", "it does not reproduce", a stale
     * ctlreg_index failure). "stale binary" is only a usable explanation if the
     * log says WHICH binary, so both line sets now carry it.
     *
     * `diag=` rides along because with the per-phase timing OFF by default the
     * gap column reads 0, and 0 must not be readable as a measured zero. */
    if (DOSBase)
        rlog("RIAPP LOG build=%s diag=%lu\n", RIAPP_BUILD_HASH,
            (unsigned long)rsection_diag_enabled());
    evlog_open();
    if (rack_classes_make() != 0 && DOSBase)
        rlog("RIAPP rack classes unavailable (plain buttons, flat bay)\n", 0, 0, 0, 0, 0);
    evlog("RUN", "frames=%lu vol=%s build=%s", frames, s_evvol, RIAPP_BUILD_HASH);
    if (noaudio) {
        /* Logged here, before the panel, so this line is readable with no
         * requester up -- which is what makes the skip verifiable at all. */
        if (DOSBase)
            rlog("audio: skipped by NOAUDIO - null backend active (offline render only)\n",
                0, 0, 0, 0, 0);
        evlog("AUDIO", "skipped NOAUDIO");
        s_live = 0;
        rc = -1;
    } else {
        rc = au_live_open(&s_lv, frames, 48000u);
        if (rc == 0) {
            s_live = 1;
            rate = (float)s_lv.mix_freq; /* E0 (G9.0): the session runs at the device rate */
        }
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
    for (i = 1; i < argc; i++)                        /* before the task takes the buffer */
        if (argv[i] && !strncmp(argv[i], "CAPTURE=", 8))
            capture_arm(argv[i] + 8);
    /* M5: attach the clock-out producer to the driver AFTER the session
     * exists, so the render feeds the ring that the sender task drains.
     * The driver only ever fills the ring; the sending is the task's. */
    if (s_live) {
        s_lv.drv.clk_out = s_mset.clk_out ? &s_mout : 0;
        if (s_lv.drv.clk_out && s_lv.drv.session)
            midi_out_set_bpm(&s_mout,
                (uint32_t)((double)s_lv.drv.session->bpm * 1000.0),
                s_lv.drv.session->sample_cursor);
    }
    if (s_live && au_live_run(&s_lv, &s_core.session) != 0) {
        au_live_close(&s_lv);
        s_live = 0;
    }
    if (DOSBase && !noaudio) {
        if (s_live)
            rlog("audio: AHI low-level mode=0x%08lx mix=%lu Hz buffer=%lu frames period=%lu us\n",
                s_lv.mode_id, s_lv.mix_freq, s_lv.frames, s_lv.period_us, 0);
        else {
            rlog("audio: AHI unavailable - null backend active (offline render only) [err %ld]\n",
                (IPTR)s_lv.err, 0, 0, 0, 0);
            /* No AHI hardware is a documented quiet fallback; a device we
             * reached and then lost is a failure the user must be told about
             * (t158). */
            if (ri_core_audio_failure_is_loud((long)s_lv.err)) {
                /* Name the usual cause in the LOG, not only in the box. The
                 * box is modal and blocks startup until a human clicks it,
                 * while this line is readable over atcpbin with nobody at the
                 * keyboard -- which is the whole difference between diagnosing
                 * this in seconds and losing a run to it (owner, 2026-10-04:
                 * "the soundcard could not be opened ... check if a version of
                 * RIAPP is running before you try to run a new version"). */
                rlog("RIAPP audio: another RIAPP instance is the usual cause;"
                    " check `status` for a running RIAPP and close it before"
                    " starting another (err %ld)\n", (IPTR)s_lv.err, 0, 0, 0, 0);
                audio_fail((long)s_lv.err);
            }
        }
    }

    /* Panel: content canvases at the persisted/Fit zoom, transport compact.
     * Fit (default) measures the frontmost public screen; a bad read or a
     * stored explicit zoom skips the measure. */
    ri_panel_init(&s_panel);
    s_modptrs[0] = "Classic";
    s_nmods = 0u;
    s_moddir[0] = '\0';
    if (DOSBase && ri_pal_path(RI_PATH_MODS, s_moddir, sizeof s_moddir) == 0)
        ri_pal_list_dirs(s_moddir, skin_scan_cb, &s_nmods);
    evlog("SKIN", "installed=%u dir=%s", 1u + s_nmods, s_moddir);
    rlog("RIAPP skin: installed=%u dir=%s\n", 1u + s_nmods, s_moddir, 0, 0, 0);
    ri_panel_skins(&s_panel, s_modptrs, 1u + s_nmods, "Classic");
    s_zoom_mode = zoom_persist_read();
    {
        /* Clamped like the runtime path: a persisted overflow zoom (e.g.
         * 1.5x stored before the guard) opens fitting, never broken. */
        int sw = 0, sh = 0, z;
        if (zoom_screen_size(&sw, &sh) != 0 || sw <= 0 || sh <= 0) {
            sw = 0;
            sh = 0;
        }
        z = ri_zoom_clamp(sw, sh, RIAPP_CHROME_W, RIAPP_CHROME_H, s_zoom_mode);
        if (DOSBase)
            rlog("RIAPP zoom: mode=%d zoom=%d screen=%dx%d\n", s_zoom_mode, z, sw, sh,
                0);
        for (i = 0; i < C_N; i++)
            s_zoom[i] = (i == C_TR) ? RI_GEO_ZOOM_COMPACT
                : (i == C_LEVI) ? ri_zoom_levi(z, sw, sh, RIAPP_CHROME_W, RIAPP_CHROME_H) : z;
        if (DOSBase)
            rlog("RIAPP zoom: levi=%d\n", s_zoom[C_LEVI], 0, 0, 0, 0);
        s_skin_zoom = z;
        s_skin_shadow = s_panel.skin_assign; /* registry starts uniform */
        skin_sync_current();
    }
    for (i = 0; i < C_N; i++) {
        LONG zoom = (LONG)s_zoom[i];
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
        else if (i == C_MST)
            s_panel.mix[4] = u;
        else if (i >= C_FX0 && i <= C_FX3)
            s_panel.fx[i - C_FX0] = u;
        SetAttrs(s_canvas[i], MUIA_RSection_Panel, (IPTR)&s_panel,
            MUIA_RSection_KeyOwner, i == C_TR, TAG_DONE);
    }
    for (i = 0; i < 5; i++)                 /* one mixer board, five strips */
        if (c_mix_canvas[i] != C_MIX)
            ri_sui_bind_board(s_ui[c_mix_canvas[i]], s_ui[C_MIX]->u.mix.board);
    ri_sui_bind_board(s_ui[C_MST], s_ui[C_MIX]->u.mix.board); /* MASTER shares it */
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
    {   /* MASTER song-data fader adopts the panel (registry def 100):
         * the engine default is unity, so without this the fader would
         * show 100 while the mix plays at 127. */
        struct RISectUI *mu = s_ui[C_MIX];
        ri_panel_ctl_send(&s_core.ctl, (uint16_t)((uint16_t)RI_SEC_MASTER << 8),
            ri_smix_value(mu->u.mix.board, (uint32_t)RI_SEC_MASTER, 0u));
    }
    {   /* Leviasynth section adopts the panel (W2, bug 1): every Levi
         * value through the bridge, chunked with drains so the 256-entry
         * plane never overflows. Same mapping live knob turns use. */
        ri_panel_levi_adopt(&s_ui[C_LEVI]->u.slevi, &s_core.ctl, 0, burst_drain);
    }
    midi_setup(); /* MIDI input (M2): ENVARC: settings, bridge receiver */
    s_tr_state = s_ui[C_TR]->u.tr.tr.state;
    for (i = 0; i < C_N; i++) {
        IPTR ch = 0;
        GetAttr(MUIA_RSection_Changes, s_canvas[i], &ch);
        s_changes[i] = ch;
    }
    s_meter_shown[0] = s_meter_shown[1] = s_meter_shown[2] = s_meter_shown[3] = s_meter_shown[4] = -1;
    s_mst_shown[0] = s_mst_shown[1] = -1;
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
            Object *mx[6];
            Object *mslots[6];
            for (r = 0u; r < 5u; r++)
                mx[r] = s_canvas[c_mix_canvas[r]];
            mx[5] = s_canvas[C_MST];
            mix_page = rack_page_gap(mx, 6u, mslots, 4u);
            for (r = 0u; r < 5u; r++)
                s_mixslot[r] = mslots[r];
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
    s_root = row;
    {
        /* S5 View menu: explicit zooms plus Fit (menu bar works by mouse;
         * keys stay scarce, t70). Checkmarks mirror s_zoom_mode. Nested
         * MUIA_Family_Child creation (the Zune test.c pattern):
         * OM_ADDMEMBER after the fact leaves the strip empty (Dell
         * 2026-09-29: no menu bar rendered). A failed menu falls back
         * to no menustrip (logged) while the panel still opens. */
        static const char *const zt[4] = { "Zoom 1x", "Zoom 1.5x", "Zoom 2x", "Zoom Fit" };
        /* Songs menu (songs & playlists 2026-09-30). */
        static const char *const st[5] = { "Load Song...", "Load Playlist...", "Next Song", "Previous Song",
            "Pattern Mode" };
        Object *menu = 0, *menustrip = 0, *smenu = 0;
        LONG checked = (LONG)(s_zoom_mode == RI_ZOOMFIT_FIT ? 3 : s_zoom_mode);
        int k, ok = 1;
        for (k = 0; k < 4; k++) {
            s_zoomitems[k] = (Object *)MUI_NewObject(MUIC_Menuitem,
                MUIA_Menuitem_Title, (IPTR)zt[k],
                MUIA_Menuitem_Checkit, TRUE,
                MUIA_Menuitem_Checked, (IPTR)(LONG)(k == checked ? TRUE : FALSE),
                TAG_DONE);
            if (!s_zoomitems[k])
                ok = 0;
        }
        if (ok) {
            menu = (Object *)MUI_NewObject(MUIC_Menu,
                MUIA_Menu_Title, (IPTR)"View",
                MUIA_Family_Child, (IPTR)s_zoomitems[0],
                MUIA_Family_Child, (IPTR)s_zoomitems[1],
                MUIA_Family_Child, (IPTR)s_zoomitems[2],
                MUIA_Family_Child, (IPTR)s_zoomitems[3],
                TAG_DONE);
            if (!menu)
                ok = 0;
        }
        if (ok) {
            for (k = 0; k < 5; k++)
                s_songitems[k] = (Object *)MUI_NewObject(MUIC_Menuitem,
                    MUIA_Menuitem_Title, (IPTR)st[k],
                    TAG_DONE);
            smenu = (Object *)MUI_NewObject(MUIC_Menu,
                MUIA_Menu_Title, (IPTR)"Songs",
                MUIA_Family_Child, (IPTR)s_songitems[0],
                MUIA_Family_Child, (IPTR)s_songitems[1],
                MUIA_Family_Child, (IPTR)s_songitems[2],
                MUIA_Family_Child, (IPTR)s_songitems[3],
                MUIA_Family_Child, (IPTR)s_songitems[4],
                TAG_DONE);
            if (!smenu && DOSBase)
                rlog("RIAPP songs: no Songs menu (SONG=/PLAYLIST= still work)\n");
        }
        if (ok) {
            menustrip = smenu ? (Object *)MUI_NewObject(MUIC_Menustrip,
                MUIA_Family_Child, (IPTR)smenu,
                MUIA_Family_Child, (IPTR)menu,
                TAG_DONE) : (Object *)MUI_NewObject(MUIC_Menustrip,
                MUIA_Family_Child, (IPTR)menu,
                TAG_DONE);
            if (!menustrip)
                ok = 0;
        }
        if (!ok) {
            for (k = 0; k < 4; k++)
                s_zoomitems[k] = 0;
            if (DOSBase)
                rlog("RIAPP zoom: no menu strip (panel still opens)\n", 0, 0, 0, 0, 0);
        } else if (DOSBase) {
            rlog("RIAPP zoom: View menu built (1x/1.5x/2x/Fit)\n", 0, 0, 0, 0, 0);
        }
        win = (Object *)MUI_NewObject(MUIC_Window,
            MUIA_Window_Title, (IPTR)"RIAPP live panel",
            MUIA_Window_LeftEdge, 0,
            MUIA_Window_TopEdge, 0,
            MUIA_Window_CloseGadget, TRUE,
            MUIA_Window_DepthGadget, TRUE,
            MUIA_Window_DragBar, TRUE,
            MUIA_Window_Menustrip, (IPTR)menustrip,
            MUIA_Window_RootObject, (IPTR)row,
            TAG_DONE);
    }
    s_win = win;
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
    for (i = 0; i < (int)(RI_TAB_COUNT * 5u); i++)
        if (s_railbtn[i / 5][i % 5])
            DoMethod(s_railbtn[i / 5][i % 5], MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 3,
                MUIM_Application_ReturnID, RIAPP_ID_DEV0 + (ULONG)(i % 5));
    for (i = 0; i < (int)RI_TAB_COUNT; i++)
        DoMethod(s_tabs[i], MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 3,
            MUIM_Application_ReturnID, RIAPP_ID_TAB0 + (ULONG)i);
    for (i = 0; i < 4; i++)
        if (s_zoomitems[i])
            DoMethod(s_zoomitems[i], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
                (IPTR)app, 3, MUIM_Application_ReturnID, RIAPP_ID_ZOOM0 + (ULONG)i);
    for (i = 0; i < 5; i++)
        if (s_songitems[i])
            DoMethod(s_songitems[i], MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
                (IPTR)app, 3, MUIM_Application_ReturnID, RIAPP_ID_SONG0 + (ULONG)i);
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

    /* Songs & playlists from the command line: SONG=<file> PLAYLIST=<file>
     * (any argument position; the first numeric argument stays the buffer
     * size). */
    for (i = 1; i < argc; i++) {
        if (argv[i] && !strncmp(argv[i], "PLAYLIST=", 9))
            playlist_load_path(argv[i] + 9);
        else if (argv[i] && !strncmp(argv[i], "SONG=", 5))
            song_load_path(argv[i] + 5);
    }

    /* Default demo song (owner 2026-10-03: "we want Zombie Nation as the
     * default demo song"). Tried ONLY when the command line did not already
     * choose a song -- an explicit SONG=/PLAYLIST= always wins, because an
     * argument is a decision and this is a default.
     *
     * Zombie Nation is the packaged arrangement in songs/local/zombie-nation/,
     * deployed to SYS:Classes/ReIncarnation/Songs/ -- which is where
     * RI_PATH_SONGS already points, so this adds no new location. The
     * RIAPP_DEMO_SONG override names a different file, and RIAPP_DEMO=0 turns
     * the attempt off for a session that wants the bare pattern-mode demo.
     *
     * A miss is NOT an error and must not raise a requester: on a guest with
     * no song library deployed (riqemu1 has none) this path simply does not
     * fire, and the built-in demo pattern carries on as before. Saying so in
     * the log is the whole response. */
    {
        static char demo[RI_PLAYLIST_PATH];
        const char *want = "zombie-nation.rbng";
        LONG v[1];
        int try_demo = 1;
        if (GetVar((STRPTR)"RIAPP_DEMO", (STRPTR)v, (LONG)sizeof v, 0L) > 0 && v[0] == 0L)
            try_demo = 0;
        v[0] = 0;
        if (try_demo && GetVar((STRPTR)"RIAPP_DEMO_SONG", (STRPTR)demo,
                (LONG)sizeof demo - 1L, 0L) > 0)
            want = demo;
        if (try_demo && !s_song_on) {
            if (ri_pal_path(RI_PATH_SONGS, demo, sizeof demo) == 0) {
                /* The library root is not always the song's own directory: the
                 * shipped layout is <root>/<song-dir>/<song>.rbng, so a flat
                 * join of root+leaf misses on the Dell even with a correct root
                 * -- "open failed" on a song that was present two levels down
                 * (2026-10-04).
                 *
                 * The relative shapes are TRIED, not DISCOVERED. An earlier
                 * version walked the root with ri_pal_list_dirs. That walk was
                 * NOT the stall that cost this lane a cycle: the real blocker was
                 * EasyRequestArgs being MODAL (see song_fail), and a stalled run
                 * was misdiagnosed as an infinite loop before anyone noticed the
                 * requesters. The enumeration still came out, because a startup
                 * that can block or spin before the app can play is not worth a
                 * directory walk, and three bounded shapes cannot do either. */
                /* probe is deliberately larger than root: it holds root + a fixed middle
                 * segment + the leaf, and truncating a path silently is how you
                 * get "open failed" on a file that exists. */
                char root[RI_PLAYLIST_PATH], probe[RI_PLAYLIST_PATH * 4], stem[RI_PLAYLIST_PATH];
                const char *ext;
                uint32_t k;
                int loaded = 0;
                snprintf(root, sizeof root, "%s", demo);
                snprintf(stem, sizeof stem, "%s", want);
                ext = strrchr(stem, '.');
                if (ext)
                    stem[ext - stem] = 0; /* "zombie-nation.rbng" -> "zombie-nation" */
                /* Quiet for the whole probe: every one of these misses is
                 * expected on some lane, and three modal boxes before the app
                 * can play is the failure mode, not the fix. */
                s_song_probe = 1;
                for (k = 0u; !loaded && k < 3u; k++) {
                    if (k == 0u)
                        snprintf(probe, sizeof probe, "%s%s", root, want);
                    else if (k == 1u)
                        snprintf(probe, sizeof probe, "%slocal/%s", root, want);
                    else if (k == 2u)
                        snprintf(probe, sizeof probe, "%slocal/%s/%s", root, stem, want);
                    loaded = (song_load_path(probe) == 0);
                }
                s_song_probe = 0;
                /* Every shape tried, in order, so a miss is diagnosable remotely
                 * over atcpbin instead of by clicking a requester on an
                 * unattended Dell (owner, 2026-10-04). */
                if (!loaded)
                    rlog("RIAPP demo song: no layout matched under %s"
                        " (tried %s%s | %slocal/%s | %slocal/%s/%s)\n",
                        root, root, want, root, want, root, stem, want);
                if (!loaded)
                    rlog("RIAPP demo song %s not found under %s; built-in demo only\n",
                        want, root, 0, 0, 0);
            } else {
                rlog("RIAPP demo song: no songs volume\n", 0, 0, 0, 0, 0);
            }
        }
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
        /* INPUT -> REPAINTED (owner 2026-10-05). NewInput returns when a
         * message is available and Zune dispatches MUIM_Draw INLINE inside the
         * redraw that follows (mui_redraw.c: DoMethod(obj, MUIM_Draw, 0)), so
         * the span from here to the end of the loop body is input-to-painted.
         * The histogram plus the max is the number; the buckets summing to the
         * observation count is its self-check. Gated on RIAPP_DIAG so a release
         * build pays nothing for it. */
        /* Only wakes caused by input count (review 2026-10-05): sampling
         * every pass made the 10 Hz timer iterations most of the "input"
         * population, and the heartbeat's file writes its maximum. */
        if (lat_on() && lat_efreq() &&
            (sigs & ~(SIGBREAKF_CTRL_C | (timer_armed ? 1UL << tport->mp_SigBit : 0UL)))) {
            struct EClockVal i0;
            ReadEClock(&i0);
            s_lat.lat_in_at.lo = i0.ev_lo;
            s_lat.lat_in_at.hi = i0.ev_hi;
            s_lat.lat_have = 1;
        }
        ret = (LONG)DoMethod(app, MUIM_Application_NewInput, &sigs);
        if (ret == (LONG)MUIV_Application_ReturnID_Quit)
            break;
        if (ret >= (LONG)RIAPP_ID_DEV0 && ret < (LONG)(RIAPP_ID_DEV0 + 5u))
            dev_visibility_toggle((uint32_t)ret - RIAPP_ID_DEV0);
        if (ret >= (LONG)RIAPP_ID_TAB0 && ret < (LONG)(RIAPP_ID_TAB0 + RI_TAB_COUNT))
            tab_switch((uint32_t)ret - RIAPP_ID_TAB0);
        if (ret >= (LONG)RIAPP_ID_SONG0 && ret < (LONG)(RIAPP_ID_SONG0 + 5u))
            song_menu((ULONG)ret - RIAPP_ID_SONG0);
        if (ret >= (LONG)RIAPP_ID_ZOOM0 && ret < (LONG)(RIAPP_ID_ZOOM0 + 4u)) {
            int m = (int)ret - (int)RIAPP_ID_ZOOM0;
            app_set_zoom(m >= 3 ? RI_ZOOMFIT_FIT : m);
        }
        if (timer_armed && CheckIO((struct IORequest *)treq)) {
            WaitIO((struct IORequest *)treq);
            /* TICK LATENESS (owner 2026-10-05). Lateness is measured against the
             * schedule, not against the previous tick: accumulate one period per
             * tick and compare with the clock. A tick that arrives late because
             * the loop was busy is exactly the "the GUI was starved" signal the
             * owner asked for, and it is invisible to a loop that only counts. */
            if (lat_on() && lat_efreq()) {
                struct EClockVal tk;
                static struct EClockVal s_last;
                uint64_t a, b, d;
                ReadEClock(&tk);
                b = ((uint64_t)tk.ev_hi << 32) | tk.ev_lo;
                /* SEED ON THE FIRST TICK. Without this the first interval is
                 * measured from a zero stamp, i.e. "since boot" -- which is what
                 * produced late_max=2805650769 us and drift=2806003567 on the first
                 * real run. Both fields were absurd, printed next to SELFCHK=ok,
                 * because nothing was checking them. */
                if (!s_lat.tick_seeded) {
                    s_lat.tick_seeded = 1;
                    s_last = tk;
                    goto tick_armed;
                }
                a = ((uint64_t)s_last.ev_hi << 32) | s_last.ev_lo;
                s_lat.ticks++;
                s_lat.span_lo += (ULONG)((b - a) * 1000000ULL / s_efreq);
                s_last = tk;
                d = b - a;
                {   /* one period in EClock units, from the rate ReadEClock gave */
                    uint64_t per = (uint64_t)s_efreq / 10ULL;   /* 100 ms */
                    ULONG late = d > per ? (ULONG)((d - per) * 1000000ULL / s_efreq) : 0u;
                    /* Exact sums in EClock units (review 2026-10-05): the us
                     * figures are rounded per tick and an early tick (d < per)
                     * adds 0 to late but subtracts from span, so a us equality
                     * can only hold by luck. In EClock units the identity
                     * span == ticks*per + late - early is exact. */
                    s_lat.span_ec += d;
                    if (d > per)
                        s_lat.late_ec += d - per;
                    else
                        s_lat.early_ec += per - d;
                    s_lat.late_sum += late;      /* EVERY late tick, not only the notable ones */
                    if (late > 500UL) {          /* 0.5 ms: below this is scheduling jitter */
                        s_lat.late_n++;
                        if (late > s_lat.late_max)
                            s_lat.late_max = late;
                    }
                    lat_bucket(s_lat.hist, late);
                }
            }
            tick_armed:
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
        if (s_cap_state == 2 && !ri_atomic_load_acq(&s_lv.drv.cap_on))
            capture_finish("buffer full");
        song_poll_end(have_cursor, cursor);
        for (i = C_P0; i <= C_P4; i++)
            sync_pat(i, have_cursor ? cursor : 0u);
        sync_303v(0u);
        sync_303v(1u);
        sync_drumv(0u);
        sync_drumv(1u);
        sync_leviv();
        sync_values();
        midi_drain(); /* bridge MIDI into midimap + the bridge (M2) */
        /* Owner 2026-09-29: Ctrl+1..4 tab keys arrive via the panel. */
        if (s_panel.tab_req >= 0 && s_panel.tab_req < (int8_t)RI_TAB_COUNT) {
            uint32_t g = (uint32_t)s_panel.tab_req;
            s_panel.tab_req = -1;
            tab_switch(g);
        }
        /* S7: Ctrl+M reassigns section skins in the panel; repaint only
         * on a real assignment change (memcmp is 1.3 KB, trivial). */
        if (memcmp(&s_panel.skin_assign, &s_skin_shadow,
            sizeof s_skin_shadow) != 0) {
            s_skin_shadow = s_panel.skin_assign;
            skin_sync_current();
            for (i = 0; i < C_N; i++)
                if (s_canvas[i])
                    MUI_Redraw(s_canvas[i], MADF_DRAWOBJECT);
        }
        meter_round(s_live ? s_lv.mix_freq : 48000u);
        /* Wedge diagnostic (2026-09-27 Dell freeze under interaction):
         * ~30 s heartbeat while live (10 Hz timer ticks the loop).
         * Last line dates the wedge; buffers/xruns say whether the
         * render task was still producing. snd/pend are diagnostic-only
         * unsynchronized byte reads (same basis as the meter snapshot):
         * they show WHAT the engine plays after pattern selection.
         * Remove after.
         * S3 draw timing joins the window (live or not): full vs partial
         * redraw us, max + mean, so a knob-drag session reports both. */
        /* The latency report rides the same 30 s heartbeat, so it lands in the
         * same log set and the same rotation window as everything else. */
        if (++hb >= 300u) {
            lat_report();
            s_lat.lat_have = 0;    /* this pass writes the log: not input latency */
            ULONG dfmax = 0u, dpmax = 0u, dfsum = 0u, dpsum = 0u, dfn = 0u, dpn = 0u, blmax = 0u, alln = 0u;
            ULONG dpwmax[4] = { 0u, 0u, 0u, 0u }, dpwsum[4] = { 0u, 0u, 0u, 0u }, dpwn[4] = { 0u, 0u, 0u, 0u };
            ULONG dpbmax = 0u, dpbsum = 0u;
            /* boxrep = repeats / new / longest-run, summed over canvases. The
             * ratio is the whole question: a burst dominated by repeats is a
             * cache waiting to happen, one dominated by new work is real cost. */
            ULONG dpr_rep = 0u, dpr_new = 0u, dpr_run = 0u;
            /* Partial-path split: replay vs blit, which were never timed
             * separately and whose absence is what let a 90 %-not-build stall
             * be filed as a build problem (2026-10-04). */
            ULONG rp_max = 0u, rp_sum = 0u, bl_max = 0u, bl_sum = 0u;
            /* gap = wall time attributable to no phase; bl_min = the blit's
             * lower bound. gap_max large with everything else normal means the
             * draw was INTERRUPTED; bl_max >> bl_min is the amplitude. */
            ULONG gp_max = 0u, gp_sum = 0u, bl_min = 0u;
            /* Build cost attributed to the section that owned most of it. */
            ULONG dpbsecsum = 0u, dpb_sec = 0u, dpb_seci = 0u;
            int w;
            hb = 0u;
            for (i = 0; i < C_N; i++) {
                struct RSectionDiag *dg = (struct RSectionDiag *)s_dg[i];
                if (!dg)
                    continue;
                if (dg->df_max > dfmax)
                    dfmax = dg->df_max;
                if (dg->dp_max > dpmax)
                    dpmax = dg->dp_max;
                dfsum += dg->df_sum;
                dpsum += dg->dp_sum;
                dfn += (ULONG)(dg->df_n > 0 ? dg->df_n : 0);
                dpn += (ULONG)(dg->dp_n > 0 ? dg->dp_n : 0);
                if (dg->blit_max > blmax)
                    blmax = dg->blit_max;
                alln += dg->alloc_n;
                if (dg->dp_build_max > dpbmax)
                    dpbmax = dg->dp_build_max;
                /* Which SECTION the build cost belonged to (2026-10-05). Argmax
                 * of the per-window build SUM, not of the max: one canvas is one
                 * section, so the sums are already partitioned by section and the
                 * largest is the target. dpb_seci records whether any canvas
                 * reported at all, so an all-zero window reads "none" rather than
                 * "section 0" -- a floor reading is indistinguishable from one
                 * that never ran, which this lane has been bitten by twice. */
                if (dg->dp_build_sum > dpbsecsum) {
                    dpbsecsum = dg->dp_build_sum;
                    dpb_sec = (ULONG)dg->dp_sec;
                    dpb_seci = 1u;
                }
                dpbsum += dg->dp_build_sum;
                if (dg->dp_gap_max > gp_max)
                    gp_max = dg->dp_gap_max;
                gp_sum += dg->dp_gap_sum;
                if (dg->dp_n > 0 &&
                    (bl_min == 0u || dg->dp_blit_min < bl_min))
                    bl_min = dg->dp_blit_min;
                rp_max = dg->dp_replay_max > rp_max ? dg->dp_replay_max : rp_max;
                rp_sum += dg->dp_replay_sum;
                bl_max = dg->dp_blit_max > bl_max ? dg->dp_blit_max : bl_max;
                bl_sum += dg->dp_blit_sum;
                dpr_rep += dg->dpr_rep;
                dpr_new += dg->dpr_new;
                if (dg->dpr_run_max > dpr_run)
                    dpr_run = dg->dpr_run_max;
                dg->dp_build_max = dg->dp_build_sum = 0u;
                for (w = 0; w < 4; w++) {
                    if (dg->dpw_max[w] > dpwmax[w])
                        dpwmax[w] = dg->dpw_max[w];
                    dpwsum[w] += dg->dpw_sum[w];
                    dpwn[w] += (ULONG)(dg->dpw_n[w] > 0 ? dg->dpw_n[w] : 0L);
                    dg->dpw_max[w] = dg->dpw_sum[w] = 0u;
                    dg->dpw_n[w] = 0L;
                }
                /* Redundant-invalidation counters are per-window like the rest
                 * of this block, BUT dpr_run_now is deliberately NOT reset: it
                 * is the length of the CURRENT run, and zeroing it would make
                 * every window report a run of 1 and hide a burst that spans
                 * windows -- which is what a 1.36 s stall does. dpr_run_max
                 * keeps the high-water mark across the whole session, so the
                 * longest single run survives being read. */
                dg->dp_replay_max = dg->dp_replay_sum = 0u;
                dg->dp_blit_max = dg->dp_blit_sum = 0u;
                dg->dp_gap_max = dg->dp_gap_sum = 0u;
                /* dp_blit_min is NOT reset: a minimum that is re-zeroed every
                 * window is not a minimum, it is the same average in disguise. */
                dg->dpr_rep = 0u;
                dg->dpr_new = 0u;
                /* dpr_run_now IS reset with its siblings (corrected 2026-10-04).
                 * Leaving it running was defended as "so a burst spanning
                 * windows stays visible", but dpr_run_max already does that and
                 * is never reset. Worse, dpr_rep/dpr_new are zeroed here, so
                 * the first invalidation of each window was classified as a
                 * REPEAT whenever it matched the previous window's last -- a
                 * spurious dpr_rep per canvas per window, landing in the quiet
                 * windows, biasing the ratio toward "cache it". */
                dg->dpr_run_now = 0u;
                dg->blit_max = 0u;
                dg->alloc_n = 0u;
                dg->df_max = dg->df_sum = 0u;
                dg->df_n = 0;
                dg->dp_max = dg->dp_sum = 0u;
                dg->dp_n = 0;
            }
            /* box_* splits the partials by RI_RSEC_BOX_*: steps (drum lamps), bar (Song
             * Position), other. An expensive partial now names its caller. */
            rlog("RIAPP draw: full_max=%lu us full_avg=%lu us n=%lu part_max=%lu us part_avg=%lu us n=%lu blit_max=%lu us allocs=%lu box_steps=%lu/%lu/%lu box_bar=%lu/%lu/%lu box_other=%lu/%lu/%lu box_none=%lu/%lu/%lu build_avg=%lu us build_max=%lu us boxrep=%lu/%lu/%lu rpl_max=%lu us rpl_avg=%lu us blt_max=%lu us blt_avg=%lu us blt_min=%lu us gap_max=%lu us gap_avg=%lu us bsec=%lu bsecsum=%lu us diag=%lu\n",
                dfmax, dfn ? dfsum / dfn : 0u, dfn, dpmax, dpn ? dpsum / dpn : 0u, dpn, blmax, alln,
                dpwn[RI_RSEC_BOX_STEPS] ? dpwsum[RI_RSEC_BOX_STEPS] / dpwn[RI_RSEC_BOX_STEPS] : 0u,
                dpwmax[RI_RSEC_BOX_STEPS], dpwn[RI_RSEC_BOX_STEPS],
                dpwn[RI_RSEC_BOX_BAR] ? dpwsum[RI_RSEC_BOX_BAR] / dpwn[RI_RSEC_BOX_BAR] : 0u,
                dpwmax[RI_RSEC_BOX_BAR], dpwn[RI_RSEC_BOX_BAR],
                dpwn[RI_RSEC_BOX_OTHER] ? dpwsum[RI_RSEC_BOX_OTHER] / dpwn[RI_RSEC_BOX_OTHER] : 0u,
                dpwmax[RI_RSEC_BOX_OTHER], dpwn[RI_RSEC_BOX_OTHER],
                dpwn[RI_RSEC_BOX_NONE] ? dpwsum[RI_RSEC_BOX_NONE] / dpwn[RI_RSEC_BOX_NONE] : 0u,
                dpwmax[RI_RSEC_BOX_NONE], dpwn[RI_RSEC_BOX_NONE],
                dpn ? dpbsum / dpn : 0u, dpbmax,
                dpr_rep, dpr_new, dpr_run,
                rp_max, dpn ? rp_sum / dpn : 0u, bl_max, dpn ? bl_sum / dpn : 0u,
                bl_min, gp_max, dpn ? gp_sum / dpn : 0u,
                /* bsec prints 255 when no canvas reported any build at all, so
                 * "nothing built" cannot be read as "section 0 built". */
                dpb_seci ? dpb_sec : 255ul, dpbsecsum,
                /* diag=0 means the per-phase timing is OFF by default (owner
                 * 2026-10-05), so gap_avg 0 there means NOT MEASURED, not
                 * zero. The partition invariant is likewise only meaningful
                 * when this reads 1. */
                (ULONG)rsection_diag_enabled());
            /* Render-stage breakdown (Dell 2026-10-02): avg and max per stage,
             * with the playing and stopped paths counted separately, so an idle
             * average can never be quoted as a playing cost again. Sticky
             * cumulative, so the close line carries the whole run. Diagnostic-
             * only unsynchronized reads, same basis as load_pm. */
            if (s_live && s_lv.drv.session) {
                const struct RILiveStages *g = ri_live_stages(s_lv.drv.session);
                unsigned q;
                rlog("RIAPP stg: playing=%lu stopped=%lu stopped_avg=%lu us\n",
                    (unsigned long)g->playing_buffers, (unsigned long)g->stopped_buffers,
                    (unsigned long)(g->n[RI_LIVE_ST_STOPPED]
                        ? g->sum_us[RI_LIVE_ST_STOPPED] / g->n[RI_LIVE_ST_STOPPED] : 0u));
                for (q = 0; q < RI_LIVE_ST_COUNT; q++)
                    rlog("RIAPP stg[%d]: avg=%lu us max=%lu us n=%lu\n", q,
                        (unsigned long)(g->n[q] ? g->sum_us[q] / g->n[q] : 0u),
                        (unsigned long)g->max_us[q], (unsigned long)g->n[q]);
            }
            /* DSP sub-stages, per BLOCK (the engine's block loop is the unit
             * here, so n is blocks and not buffers). A section stage reports
             * n=0 when its section is disabled, which is what distinguishes
             * "cheap" from "never ran". Compact rather than one line each:
             * this log is already long, and a table that has to be scrolled
             * apart from its maxima is a table nobody reads. */
            if (s_live && s_lv.drv.session) {
                const struct RIEngineStages *h =
                    ri_engine_stages(&s_lv.drv.session->eng);
                static const char *nm[RI_ENGINE_ST_COUNT] = {
                    "zero", "delay", "comp", "master", "meter", "limit",
                    "303a", "303b", "808", "909", "levi",
                    "lev-arp", "lev-seq", "lev-voice", "lev-mix",
                    "lev-tempo", "lev-probe", "lev-probe2", "block" };
                unsigned q;
                rlog("RIAPP dstg n=%lu\n", (unsigned long)h->n[RI_ENGINE_ST_TOTAL]);
                for (q = 0; q < RI_ENGINE_ST_COUNT; q++)
                    rlog("RIAPP dstg %-6s avg=%lu us max=%lu us\n", nm[q],
                        (unsigned long)(h->n[q] ? h->sum_us[q] / h->n[q] : 0u),
                        (unsigned long)h->max_us[q]);
                /* Work counters from inside levi_voice_render_sum_stereo, for
                 * THIS block (the engine resets them per block). Counts, not
                 * times: a clock read costs 4-6 us and that function averages
                 * ~3 us per sample, so timing its regions would cost more than
                 * the code being measured. lfo_iters is the suspect to watch --
                 * it is RI_LEVI_NLFO * RI_LEVI_NVOICES per trigged LFO.
                 *
                 * vus is the exception: the engine's own clock for THIS block's
                 * whole call (RI_ESTAGE_E_STORE, same pair, no extra reads). It
                 * rides along here because its only use is the pairing with
                 * voice_active -- across blocks, cost = fixed + per-active-voice
                 * x n. That regression is what gets inside a function whose
                 * regions interleave inside the sample loop. */
                {
                    const struct RILeviSet *L = &s_lv.drv.session->eng.slevi;
                    rlog("RIAPP vcount: samples=%lu lfo_samples=%lu lfo_iters=%lu"
                        " voice_calls=%lu voice_active=%lu fx_samples=%lu vus=%lu\n",
                        (unsigned long)L->vc_samples,
                        (unsigned long)L->vc_lfo_samples,
                        (unsigned long)L->vc_lfo_iters,
                        (unsigned long)L->vc_voice_calls,
                        (unsigned long)L->vc_voice_active,
                        (unsigned long)L->vc_fx_samples,
                        (unsigned long)L->vc_voice_us);
                }
                /* Drum-tail A0 paired samples: (stage us, active voice-samples)
                 * per sampled block for the 808 and 909. Dumped at close so
                 * nothing per-block goes to the log during the run; gated on
                 * RIAPP_DIAG like the other per-phase timing, so a release
                 * run pays no log volume. Fit us = a + b x active per song
                 * in the evidence; the ring stops at 512 samples (~175 s). */
                if (rsection_diag_enabled()) {
                    const struct RIEngine *de = &s_lv.drv.session->eng;
                    ULONG di;
                    rlog("RIAPP drumdiag n=%lu stride=%u\n", (unsigned long)de->drum_n,
                        (unsigned)RI_DRUMDIAG_STRIDE, 0, 0, 0);
                    for (di = 0u; di < de->drum_n; di++)
                        rlog("RIAPP drumdiag i=%lu us808=%lu a808=%lu us909=%lu a909=%lu\n",
                            (unsigned long)di,
                            (unsigned long)de->drum_ring[di].us808,
                            (unsigned long)de->drum_ring[di].a808,
                            (unsigned long)de->drum_ring[di].us909,
                            (unsigned long)de->drum_ring[di].a909);
                }
            }
            if (s_live)
            /* wake_max/wake_n/prio separate "late" (the render task was not
             * scheduled) from "slow" (the render took too long) — render_max
             * alone cannot. arm_us is the governor's own accumulation: with
             * overloads=0 it distinguishes "never over budget" from "over
             * budget and reset many times". */
            {   /* wall clock on every heartbeat (2026-10-05): a gap between
                 * lines must be visible as a gap in time, not inferred */
                struct DateStamp ds;
                DateStamp(&ds);
                rlog("RIAPP hb: t=%02ld:%02ld:%02ld\n", (long)(ds.ds_Minute / 60),
                    (long)(ds.ds_Minute % 60), (long)(ds.ds_Tick / TICKS_PER_SECOND));
            }
            rlog("RIAPP hb: buffers=%lu xruns=%lu render_max=%lu us wake_max=%lu us wake_n=%lu prio=%ld arm_us=%llu load=%lu/1000 overloads=%lu snd=%u/%u/%u/%u pend=%u/%u/%u/%u\n",
                ri_atomic_load_acq(&s_lv.drv.buffers),
                ri_atomic_load_acq(&s_lv.drv.xruns),
                ri_atomic_load_acq(&s_lv.drv.render_us_max),
                ri_atomic_load_acq(&s_lv.drv.wake_us_max),
                ri_atomic_load_acq(&s_lv.drv.wake_n),
                (LONG)(int32_t)ri_atomic_load_acq(&s_lv.drv.prio_now),
                (unsigned long long)s_lv.drv.over_run_us, /* diagnostic-only unsynchronized read */
                (ULONG)s_lv.drv.load_pm, /* diagnostic-only unsynchronized read */
                ri_atomic_load_acq(&s_lv.drv.overloads),
                s_core.session.player.sounding_slot[0], s_core.session.player.sounding_slot[1],
                s_core.session.player.sounding_slot[2], s_core.session.player.sounding_slot[3],
                s_core.session.player.pending_slot[0], s_core.session.player.pending_slot[1],
                s_core.session.player.pending_slot[2], s_core.session.player.pending_slot[3]);
        }
        /* Close the input sample: everything the loop body did between the
         * stamp and here, including any inline redraw, is the user's latency. */
        if (lat_on() && s_lat.lat_have && lat_efreq()) {
            struct EClockVal i1;
            uint64_t a = ((uint64_t)s_lat.lat_in_at.hi << 32) | s_lat.lat_in_at.lo;
            uint64_t b;
            ReadEClock(&i1);
            b = ((uint64_t)i1.ev_hi << 32) | i1.ev_lo;
            if (b > a) {
                ULONG us = (ULONG)((b - a) * 1000000ULL / s_efreq);
                s_lat.lat_n++;
                s_lat.lat_sum += us;
                if (us > s_lat.lat_max)
                    s_lat.lat_max = us;
                lat_bucket(s_lat.lat_hist, us);
            }
            s_lat.lat_have = 0;
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
    capture_finish("quit");
    ri_pal_midi_close(); /* bridge task teardown (M2) */
    if (s_live) {
        au_live_close(&s_lv);
        if (s_cap_mem) {
            FreeVec(s_cap_mem);
            s_cap_mem = NULL;
        }
        if (DOSBase)
            rlog("RIAPP closed: buffers=%lu xruns=%lu render_max=%lu us render_total=%lu ms period=%lu us wake_max=%lu us wake_total=%lu ms wake_n=%lu stg_total_avg=%lu us stg_dsp_avg=%lu us stg_evt_avg=%lu us stg_playing=%lu stg_stopped=%lu\n",
                ri_atomic_load_acq(&s_lv.drv.buffers), ri_atomic_load_acq(&s_lv.drv.xruns), ri_atomic_load_acq(&s_lv.drv.render_us_max), ri_atomic_load_acq(&s_lv.drv.render_us_sum_ms), s_lv.period_us,
                ri_atomic_load_acq(&s_lv.drv.wake_us_max), ri_atomic_load_acq(&s_lv.drv.wake_us_sum_ms), ri_atomic_load_acq(&s_lv.drv.wake_n),
                (unsigned long)(s_lv.drv.session && ri_live_stages(s_lv.drv.session)->n[RI_LIVE_ST_TOTAL]
                    ? ri_live_stages(s_lv.drv.session)->sum_us[RI_LIVE_ST_TOTAL] / ri_live_stages(s_lv.drv.session)->n[RI_LIVE_ST_TOTAL] : 0u),
                (unsigned long)(s_lv.drv.session && ri_live_stages(s_lv.drv.session)->n[RI_LIVE_ST_DSP]
                    ? ri_live_stages(s_lv.drv.session)->sum_us[RI_LIVE_ST_DSP] / ri_live_stages(s_lv.drv.session)->n[RI_LIVE_ST_DSP] : 0u),
                (unsigned long)(s_lv.drv.session && ri_live_stages(s_lv.drv.session)->n[RI_LIVE_ST_EVENTS]
                    ? ri_live_stages(s_lv.drv.session)->sum_us[RI_LIVE_ST_EVENTS] / ri_live_stages(s_lv.drv.session)->n[RI_LIVE_ST_EVENTS] : 0u),
                (unsigned long)(s_lv.drv.session ? ri_live_stages(s_lv.drv.session)->playing_buffers : 0u),
                (unsigned long)(s_lv.drv.session ? ri_live_stages(s_lv.drv.session)->stopped_buffers : 0u));
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

/* Every return path of riapp_main leaves through here, so the long-lived
 * log handle is always closed (no atexit in the v11 link). */
int main(int argc, char **argv) {
    int rc = riapp_main(argc, argv);
    rlog_close();
    return rc;
}
