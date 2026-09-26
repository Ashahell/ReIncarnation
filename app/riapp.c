/* app/riapp.c — RIAPP live application shell (G9.4).
 * AROS-only. Supersedes the bare app/main.c window (main.c stays until
 * the full RISECT panel integration lands; this shell proves the live
 * path end to end).
 *
 * Layout: one Intuition window (transport + demo status + meter line).
 * The full panel reuses the RISECT keys/live layouts — that wiring lands
 * after the Dell first-sound proof; this shell already drives the same
 * session, control plane and meter snapshot the panel will use.
 * Usage: RIAPP [frames] (device buffer, default 1024; 64..4096).
 * Controls: Space = Play, S = Stop (E1 p. 145 C4 law via ri_live_stop),
 * C / V = 303 cutoff down / up, P = 303 pan centre/left/right, L / K = 303 strip level down / up,
 * M = log meters (RAM:RIAPP.LOG; no sound change), R = 5 s soak (null backend only), Q or CloseWindow =
 * quit. With AHI the render task owns the session: the GUI only posts
 * transport requests (au_live_request) and knob keys (control plane). Every knob key sends its lane key
 * through the control plane (one path, G9.1); pattern edits stay in the
 * pure edit functions + snapshot request (next slice).
 * Meters: live snapshot (section peaks, FX peaks, comp GR, position) —
 * closes G6b on the app side; the stand-in clock is gone here.
 * Startup: built-in demo song (the G9.2 t81 fixture); AHI missing ->
 * null backend + RI_AUDIO_NULL_MSG; unbound 909 pack -> 909 silence +
 * notice (§17, never silent).
 */
#ifndef __AROS__
#error "app/riapp.c is AROS-only"
#endif

#include <exec/types.h>
#include <exec/ports.h>
#include <utility/tagitem.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
#include "engine/live.h"
#include "engine/seq/ctlplane.h"
#include "engine/seq/autolane.h"
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"
#include "audio_io/audio_ahi_live.h"

extern struct DosLibrary *DOSBase;

#define RIAPP_FRAMES 64u /* null-backend render chunk */
#define RIAPP_DEV_FRAMES 1024u /* default device buffer (first sound; M1.1 accepted 64) */
/* Demo mix (owner, Dell 2026-09-26: "the 808 volume is a bit low", then
 * "808 should still be louder"): the 303 strip starts at 72 (-9.9 dB,
 * P-17) and the 808 downbeats are accented, so the 808 sits ~1.5 dB above
 * the 303 (host-measured RMS: 808 -19.8 dBFS; 303A at 90 was -17.3). The
 * 808/303 voice calibration itself is unchanged (engine goldens). */
#define RIAPP_303_LEVEL 72u
#define RIAPP_SOAK_BUFS 3750u /* 5 s at 64 frames/48 kHz, null backend */

static struct RIPatternBank s_ba, s_bb, s_b808, s_b909;
static struct RISongTrack s_tr;
static struct RIEvent s_scratch[512];
static struct RIControlPlane s_ctl;
static float s_fl[RIAPP_FRAMES], s_fr[RIAPP_FRAMES];
static struct RILiveSession s_sess;
static struct AuLive s_lv;

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

static void riapp_print_meters(const struct RILiveSession *s, int live) {
    const struct RILiveMeters *m = ri_live_meters(s);
    if (!m || !DOSBase)
        return;
    /* With AHI the render task writes these words; a torn read only
     * misprints one status line (display-only, never fed back). */
    rlog("RIAPP pos tick=%lu xruns=%lu bufs=%lu 303A=%ld%% 808=%ld%%\n",
        (IPTR)(ULONG)m->cursor_ticks, (IPTR)(live ? s_lv.xruns : (ULONG)m->xruns),
        (IPTR)(live ? s_lv.buffers : 0UL),
        (IPTR)(LONG)(m->sec_peak[0] * 100.0f), (IPTR)(LONG)(m->sec_peak[2] * 100.0f));
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
    struct Window *win = 0;
    struct IntuiMessage *msg;
    struct MsgPort *port;
    static TEXT title[] = "RIAPP live (Space=Play S=Stop C/V cutoff L/K level P pan M meters Q quit)";
    static struct TagItem wi_tags[8];
    const struct RIPatternBank *b4[4];
    ULONG frames = riapp_arg_frames(argc, argv);
    float rate = 48000.0f;
    int done = 0, live = 0, rc;
    uint8_t cutoff = 64u, level = RIAPP_303_LEVEL, pan = 64u;

    riapp_demo_song();
    b4[0] = &s_ba;
    b4[1] = &s_bb;
    b4[2] = &s_b808;
    b4[3] = &s_b909;
    ri_ctl_init(&s_ctl);
    rc = au_live_open(&s_lv, frames, 48000u);
    if (rc == 0) {
        live = 1;
        rate = (float)s_lv.mix_freq; /* E0 (G9.0): the session runs at the device rate */
    }
    ri_live_init(&s_sess, 96u, rate, 120.0f, RI_ENGINE_S303A | RI_ENGINE_S808, s_scratch, 512u);
    ri_live_set_banks(&s_sess, b4, &s_tr, 0);
    ri_live_set_ctl(&s_sess, &s_ctl);
    ri_engine_set_level(&s_sess.eng, 0u, RIAPP_303_LEVEL); /* before the task owns it */
    if (live && au_live_run(&s_lv, &s_sess) != 0) {
        au_live_close(&s_lv);
        live = 0;
    }
    if (DOSBase) {
        if (live)
            rlog("audio: AHI low-level mode=0x%08lx mix=%lu Hz buffer=%lu frames\n",
                s_lv.mode_id, s_lv.mix_freq, s_lv.frames, 0, 0);
        else
            rlog("audio: AHI unavailable - null backend active (offline render only) [err %ld]\n",
                (IPTR)s_lv.err, 0, 0, 0, 0);
        rlog("RIAPP 909 pack: unbound - 909 renders silence (load a pack for drums)\n", 0, 0, 0, 0, 0);
    }

    wi_tags[0].ti_Tag = WA_Title;
    wi_tags[0].ti_Data = (IPTR)title;
    wi_tags[1].ti_Tag = WA_Width;
    wi_tags[1].ti_Data = 640;
    wi_tags[2].ti_Tag = WA_Height;
    wi_tags[2].ti_Data = 120;
    wi_tags[3].ti_Tag = WA_CloseGadget;
    wi_tags[3].ti_Data = TRUE;
    wi_tags[4].ti_Tag = WA_DragBar;
    wi_tags[4].ti_Data = TRUE;
    wi_tags[5].ti_Tag = WA_DepthGadget;
    wi_tags[5].ti_Data = TRUE;
    wi_tags[6].ti_Tag = WA_IDCMP;
    wi_tags[6].ti_Data = IDCMP_CLOSEWINDOW | IDCMP_VANILLAKEY;
    wi_tags[7].ti_Tag = TAG_DONE;
    wi_tags[7].ti_Data = 0;
    win = (struct Window *)OpenWindowTagList(NULL, wi_tags);
    if (!win) {
        if (live)
            au_live_close(&s_lv);
        return 10;
    }
    port = win->UserPort;
    if (DOSBase)
        rlog("RIAPP ready: Space plays the demo\n", 0, 0, 0, 0, 0);
    while (!done) {
        Wait(1UL << port->mp_SigBit);
        while ((msg = (struct IntuiMessage *)GetMsg(port)) != NULL) {
            ULONG cls = msg->Class;
            UWORD code = msg->Code;
            ReplyMsg((struct Message *)msg);
            if (cls == IDCMP_CLOSEWINDOW) {
                done = 1;
                break;
            }
            if (cls != IDCMP_VANILLAKEY)
                continue;
            switch (code) {
            case ' ':
                if (live)
                    au_live_request(&s_lv, AU_LIVE_CMD_PLAY);
                else {
                    ri_live_play(&s_sess);
                    ri_live_render(&s_sess, s_fl, s_fr, RIAPP_FRAMES);
                }
                if (DOSBase)
                    rlog("RIAPP play\n", 0, 0, 0, 0, 0);
                break;
            case 's': case 'S':
                if (live)
                    au_live_request(&s_lv, AU_LIVE_CMD_STOP);
                else
                    ri_live_stop(&s_sess);
                if (DOSBase)
                    rlog("RIAPP stop\n", 0, 0, 0, 0, 0);
                break;
            case 'c': case 'C': case 'v': case 'V':
                cutoff = (code == 'c' || code == 'C') ? (cutoff >= 16u ? cutoff - 16u : 0u)
                                                      : (cutoff <= 111u ? cutoff + 16u : 127u);
                ri_ctl_send(&s_ctl, RI_CTL_303A_CUTOFF, cutoff);
                if (!live)
                    ri_live_render(&s_sess, s_fl, s_fr, RIAPP_FRAMES);
                if (DOSBase)
                    rlog("RIAPP 303 cutoff -> %lu\n", cutoff, 0, 0, 0, 0);
                break;
            case 'l': case 'L': case 'k': case 'K':
                level = (code == 'l' || code == 'L') ? (level >= 16u ? level - 16u : 0u)
                                                     : (level <= 111u ? level + 16u : 127u);
                ri_ctl_send(&s_ctl, RI_AUTO_ID_MIX(0u, RI_AUTO_MIX_LEVEL), level);
                if (!live)
                    ri_live_render(&s_sess, s_fl, s_fr, RIAPP_FRAMES);
                if (DOSBase)
                    rlog("RIAPP 303A level -> %lu\n", level, 0, 0, 0, 0);
                break;
            case 'p': case 'P': /* centre -> hard left -> hard right -> centre */
                pan = pan == 64u ? 0u : pan == 0u ? 127u : 64u;
                ri_ctl_send(&s_ctl, RI_AUTO_ID_MIX(0u, RI_AUTO_MIX_PAN), pan);
                if (!live)
                    ri_live_render(&s_sess, s_fl, s_fr, RIAPP_FRAMES);
                if (DOSBase)
                    rlog("RIAPP 303A pan -> %lu (0 left, 64 centre, 127 right)\n", pan, 0, 0, 0, 0);
                break;
            case 'm': case 'M':
                riapp_print_meters(&s_sess, live);
                break;
            case 'r': case 'R':
                if (!live) {
                    ULONG b;
                    if (DOSBase)
                        rlog("RIAPP null soak 5 s...\n", 0, 0, 0, 0, 0);
                    for (b = 0u; b < RIAPP_SOAK_BUFS; b++)
                        ri_live_render(&s_sess, s_fl, s_fr, RIAPP_FRAMES);
                    riapp_print_meters(&s_sess, live);
                }
                break;
            case 'q': case 'Q':
                done = 1;
                break;
            default:
                break;
            }
            if (done)
                break;
        }
    }
    if (live) {
        au_live_close(&s_lv);
        if (DOSBase)
            rlog("RIAPP closed: buffers=%lu xruns=%lu\n", s_lv.buffers, s_lv.xruns, 0, 0, 0);
    }
    CloseWindow(win);
    return 0;
}
