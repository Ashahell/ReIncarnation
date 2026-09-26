/* app/riapp.c — RIAPP live application shell (G9.4).
 * AROS-only. Supersedes the bare app/main.c window (main.c stays until
 * the full RISECT panel integration lands; this shell proves the live
 * path end to end).
 *
 * Layout: one Intuition window (transport + demo status + meter line).
 * The full panel reuses the RISECT keys/live layouts — that wiring lands
 * after the Dell first-sound proof; this shell already drives the same
 * session, control plane and meter snapshot the panel will use.
 * Controls: Space = Play, S = Stop (E1 p. 145 C4 law via ri_live_stop),
 * C = 303 cutoff demo move, P = pan, L = level, R = run 5 s null-backend
 * soak, Q or CloseWindow = quit. Every knob key sends its lane key
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

#define RIAPP_FRAMES 64u
#define RIAPP_SOAK_BUFS 3750u /* 5 s at 64 frames/48 kHz, null backend */

static struct RIPatternBank s_ba, s_bb, s_b808, s_b909;
static struct RISongTrack s_tr;
static struct RIEvent s_scratch[512];
static struct RIControlPlane s_ctl;
static float s_fl[RIAPP_FRAMES], s_fr[RIAPP_FRAMES];

static void riapp_demo_song(void) {
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
    for (i = 0u; i < 16u; i++)
        ri_p303_set(&s_ba.pat[0], i, 6u,
            (i == 0u) ? 0u : (uint8_t)RI_STEP_REST);
    ri_track_init(&s_tr);
}

static void riapp_print_meters(const struct RILiveSession *s) {
    const struct RILiveMeters *m = ri_live_meters(s);
    if (!m || !DOSBase)
        return;
    Printf((STRPTR)"RIAPP pos tick=%lu samp=%lu xruns=%lu 303A=%.3f fx0=%.3f gr=%.1f\n",
        (ULONG)m->cursor_ticks, (ULONG)m->samples, m->xruns,
        (double)m->sec_peak[0], (double)m->fx_peak[0], (double)m->comp_gr);
}

int main(void) {
    struct Window *win = 0;
    struct IntuiMessage *msg;
    struct MsgPort *port;
    static TEXT title[] = "RIAPP live (G9 shell: Space=Play S=Stop R=soak Q=quit)";
    static struct TagItem wi_tags[8];
    struct RILiveSession sess;
    const struct RIPatternBank *b4[4];
    struct AuLive lv;
    static float lv_l[RIAPP_FRAMES], lv_r[RIAPP_FRAMES];
    static WORD lv_s[RIAPP_FRAMES * 2u];
    int done = 0;
    int has_ahi = 0;

    riapp_demo_song();
    b4[0] = &s_ba;
    b4[1] = &s_bb;
    b4[2] = &s_b808;
    b4[3] = &s_b909;
    ri_ctl_init(&s_ctl);
    ri_live_init(&sess, 96u, 48000.0f, 120.0f, RI_ENGINE_S303A, s_scratch,
        512u);
    ri_live_set_banks(&sess, b4, &s_tr, 0);
    ri_live_set_ctl(&sess, &s_ctl);

    lv.session = &sess;
    lv.f32_l = lv_l;
    lv.f32_r = lv_r;
    lv.s16 = lv_s;
    lv.frames = RIAPP_FRAMES;

    /* AHI probe: OpenDevice here would wedge on failure paths; the lane
     * measurement owns the real negotiate. No device -> null backend. */
    if (!has_ahi && DOSBase)
        Printf((STRPTR)"audio: AHI unavailable - null backend active (offline render only)\n");
    if (DOSBase)
        Printf((STRPTR)"RIAPP 909 pack: unbound - 909 renders silence (load a pack for drums)\n");

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
    if (!win)
        return 10;
    port = win->UserPort;
    if (DOSBase)
        Printf((STRPTR)"RIAPP ready: Space plays the demo, C/P/L move knobs\n");
    while (!done) {
        ULONG sigs = Wait((1UL << port->mp_SigBit));
        (void)sigs;
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
            if (code == ' ' && DOSBase) {
                ri_live_play(&sess);
                au_live_start(&lv);
                ri_live_render(&sess, s_fl, s_fr, RIAPP_FRAMES);
                Printf((STRPTR)"RIAPP play\n");
                riapp_print_meters(&sess);
            } else if ((code == 's' || code == 'S')) {
                ri_live_stop(&sess);
                au_live_stop(&lv);
                if (DOSBase)
                    Printf((STRPTR)"RIAPP stop\n");
                riapp_print_meters(&sess);
            } else if ((code == 'c' || code == 'C')) {
                ri_ctl_send(&s_ctl, RI_CTL_303A_CUTOFF, 20u);
                ri_live_render(&sess, s_fl, s_fr, RIAPP_FRAMES);
                if (DOSBase)
                    Printf((STRPTR)"RIAPP 303 cutoff -> 20 (heard in one buffer)\n");
                riapp_print_meters(&sess);
            } else if ((code == 'p' || code == 'P')) {
                ri_ctl_send(&s_ctl, RI_AUTO_ID_MIX(0u, RI_AUTO_MIX_PAN),
                    96u);
                ri_live_render(&sess, s_fl, s_fr, RIAPP_FRAMES);
                if (DOSBase)
                    Printf((STRPTR)"RIAPP 303A pan -> 96\n");
                riapp_print_meters(&sess);
            } else if ((code == 'l' || code == 'L')) {
                ri_ctl_send(&s_ctl, RI_AUTO_ID_MIX(0u, RI_AUTO_MIX_LEVEL),
                    90u);
                ri_live_render(&sess, s_fl, s_fr, RIAPP_FRAMES);
                if (DOSBase)
                    Printf((STRPTR)"RIAPP 303A level -> 90\n");
                riapp_print_meters(&sess);
            } else if ((code == 'r' || code == 'R')) {
                ULONG b;
                if (DOSBase)
                    Printf((STRPTR)"RIAPP null soak 5 s...\n");
                for (b = 0u; b < RIAPP_SOAK_BUFS; b++)
                    ri_live_render(&sess, s_fl, s_fr, RIAPP_FRAMES);
                if (DOSBase)
                    Printf((STRPTR)"RIAPP soak done\n");
                riapp_print_meters(&sess);
            } else if ((code == 'q' || code == 'Q')) {
                done = 1;
                break;
            }
        }
    }
    au_live_stop(&lv);
    CloseWindow(win);
    return 0;
}
