/*
 * app/sectproof.c — section canvas on-device proof (§12.10 G4).
 *
 * AROS-ONLY. usage: RISECT [303|808|909|mix|fx|tr|keys|live|remote] [demo] [mod=<name>]
 * One window with the RSection canvas for the chosen section at 1x plus a
 * readout row, so state can be verified by number as well as by ui_capture.
 * mod=<name> loads SYS:Classes/ReIncarnation/Mods/<name>/ (G8.1 skins);
 * Ctrl+M cycles the installed mods live; selection events append to
 * RAM:RISECT.MOD. A missing mod falls back to Classic with a note.
 * "demo" drives the same behaviour calls a click makes (gui/sectui.h):
 *   303 — manual p. 42 "programming from scratch in Pitch Mode" + knobs;
 *   808 — BD four-on-the-floor, CH offbeats, AC on 5 and 13, LT->LC switch,
 *         CH left selected (steps show the CH row), BD Level/Tone moved;
 *   909 — BD low/high alternating on 1,5,9,13, SD flam on 5 and 13, CH
 *         selected with low hits on the off-beats, BD Tune / Flam moved;
 *   mix — the four section mixers + Master side by side on ONE board:
 *         808 muted, PCF on Synth 1 then stolen by the 808 (p. 62), Dist
 *         on the 909, Comp on the Master, faders/Pan/Delay moved, meters
 *         fed fixed test levels;
 *   fx  — PCF, Delay, Dist, Comp side by side: all on, PCF pattern 12 by
 *         twelve arrow-up clicks, BP, sliders moved; Delay steps 3 -> 6 by
 *         arrows, triplets, Pan/F.Back moved; Dist Amount/Shape; Comp
 *         Ratio/Threshold, input meters and level reduction fed;
 *   tr  — Transport (compact 0.75x) over the four Pattern sections: Song
 *         mode, tempo 120 -> 128 by arrows, Record (plays), FF x2 + Bar
 *         arrow -> bar 22, loop 17 + 8 on, Shuffle 40, Sync/MIDI lit;
 *         Synth 1 C3, Synth 2 A1 length 12, 808 bank B armed only (no
 *         pattern lit) + shuffle, 909 section off;
 *   keys — G5 proof: Transport, the four Pattern sections and Synth 1 on
 *         ONE front panel (gui/panelui.h): focus bar, click-to-focus and
 *         the Appendix E keyboard, driven by real key events (no demo:
 *         keys arrive through the QEMU monitor's sendkey). Every panel
 *         change appends the readout line to RAM:RISECT.LOG;
 *   live — G6a proof: Transport, 808 + mixer, 909 + mixer, the four
 *         Pattern sections on one panel; running light, taps at the
 *         playhead, held delete, Song-mode bar follow, meters. CLOCK IS A
 *         STAND-IN: the sample position comes from Intuition CurrentTime()
 *         (wall time), not the render task — riqemu1 has no audio device
 *         and binding the render task is G6b. Meter source is a stand-in
 *         too: a hit at the playhead = 0 dBFS, 20 dB/s decay (P-16);
 *   remote — G7 proof: the live panel plus a CAMD receiver on cluster
 *         "ri.remote", channel 1: Standard Mapping (gui/midimap.h) drives
 *         the panel; MIDISEND plays a message script into the same
 *         cluster, i.e. the real camd.library path without hardware.
 * Exit: close gadget or Ctrl-C. Return codes: 0 ok, 5+ build failures.
 * Must NEVER enter the host build (audit gates app/).
 */

#ifndef __AROS__
#error "app/sectproof.c is AROS-only: Zune proof window, never in the host build"
#endif

#include <exec/types.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <string.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include "gui/ctlreg.h"
#include "gui/sectui.h"
#include "gui/panelgeo.h"
#include "gui/livestate.h"
#include "gui/midimap.h"
#include <midi/camd.h>
#include <proto/camd.h>
#include <dos/exall.h>
#include "gui/widgets/rsection.h"
#include "gui/skin.h"
#include "gui/skin_aros.h"

static char s_readout[160];
static Object *s_mix[10];
static int s_nall;
static struct RIMidiIn s_midi;
static ULONG s_camd_got, s_camd_last;   /* raw CAMD diagnostics: messages taken, last mm_Msg */
struct Library *CamdBase;

/* G8.1 mod selection: installed list scanned from the Mods dir, the loaded
 * skin, and the name the panel currently asks for (Ctrl+M cycles it). */
static struct RISkin s_skin;
static char s_loaded[64];
static const char *s_installed[17];
static char s_installed_buf[16][64];
static int s_installed_n;
static char s_mod_arg[64];
static int s_mod_pending; /* mod= arg seen but not applied yet */

/* One MOD-log line per selection event (evidence for the G8.1 proof). */
static void mod_log(const char *line) {
    BPTR f = Open((CONST_STRPTR)"RAM:RISECT.MOD", MODE_NEWFILE);
    if (f) {
        FPuts(f, (CONST_STRPTR)line);
        FPuts(f, (CONST_STRPTR)"\n");
        Close(f);
    }
}

static void skin_apply(const char *name) {
    char dir[192], note[160];
    int i, n;
    dir[0] = '\0';
    i = 0;
    {
        static const char pre[] = "SYS:Classes/ReIncarnation/Mods/";
        while (pre[i] && i < 160) {
            dir[i] = pre[i];
            i++;
        }
    }
    n = 0;
    while (name[n] && i < 190) {
        dir[i++] = name[n++];
    }
    dir[i] = '\0';
    ri_skin_aros_free(&s_skin);
    memset(&s_skin, 0, sizeof s_skin);
    s_loaded[0] = '\0';
    ri_skin_aros_set_active(0);
    if (!strcmp(name, "Classic") || name[0] == '\0') {
        for (i = 0; name[i] && i < 63; i++)
            s_loaded[i] = name[i];
        s_loaded[i] = '\0';
        mod_log("mod=Classic: procedural (no files)");
        return;
    }
    n = ri_skin_aros_load(dir, &s_skin);
    if (n < 0) {
        /* §17 row 3: warn, default mod, never substitute silently. */
        int rc = n;
        i = 0;
        {
            static const char pre[] = "mod='";
            while (pre[i] && i < 150) {
                note[i] = pre[i];
                i++;
            }
        }
        n = 0;
        while (name[n] && i < 120) {
            note[i++] = name[n++];
        }
        {
            static const char post[] = "' not found (rc=";
            int k = 0;
            while (post[k] && i < 150) {
                note[i++] = post[k++];
            }
            if (i < 156) {
                note[i++] = (char)('0' - rc);
            }
            {
                static const char post2[] = ") — Classic, song dirty";
                k = 0;
                while (post2[k] && i < 158) {
                    note[i++] = post2[k++];
                }
            }
        }
        note[i] = '\0';
        mod_log(note);
        for (i = 0; name[i] && i < 63; i++)
            s_loaded[i] = name[i];
        s_loaded[i] = '\0';
        return;
    }
    ri_skin_aros_zoom(&s_skin, 0);
    ri_skin_aros_set_active(&s_skin);
    for (i = 0; name[i] && i < 63; i++)
        s_loaded[i] = name[i];
    s_loaded[i] = '\0';
    {
        static const char pre[] = "mod='";
        i = 0;
        while (pre[i] && i < 150) {
            note[i] = pre[i];
            i++;
        }
        n = 0;
        while (name[n] && i < 120) {
            note[i++] = name[n++];
        }
        {
            static const char post[] = "' active";
            int k = 0;
            while (post[k] && i < 158) {
                note[i++] = post[k++];
            }
        }
        note[i] = '\0';
        mod_log(note);
    }
}

/* Scan the Mods dir for installed skins (Classic always first). */
static void skin_scan(void) {
    BPTR lock;
    struct ExAllControl *eac;
    static ULONG s_exbuf[1024]; /* 4 KB ExAll buffer, ULONG-aligned */
    int n = 1, more = 1;
    s_installed[0] = "Classic";
    lock = Lock((CONST_STRPTR)"SYS:Classes/ReIncarnation/Mods/", ACCESS_READ);
    if (!lock) {
        s_installed[1] = 0;
        return;
    }
    eac = (struct ExAllControl *)AllocDosObject(DOS_EXALLCONTROL, 0);
    if (!eac) {
        UnLock(lock);
        s_installed[1] = 0;
        return;
    }
    eac->eac_LastKey = 0;
    while (more && n < 16) {
        LONG ok = ExAll(lock, (struct ExAllData *)s_exbuf, sizeof s_exbuf,
                        ED_NAME, eac);
        if (ok) {
            more = 0;
        } else if (IoErr() != ERROR_NO_MORE_ENTRIES) {
            break;
        } else {
            more = 0;
        }
        if (eac->eac_Entries > 0) {
            UBYTE *p = (UBYTE *)s_exbuf;
            LONG k;
            for (k = 0; k < (LONG)eac->eac_Entries && n < 16; k++) {
                struct ExAllData *ead = (struct ExAllData *)p;
                int j = 0;
                if (ead->ed_Type <= 0) {
                    p += ead->ed_Size;
                    continue;
                }
                while (ead->ed_Name[j] && j < 63) {
                    s_installed_buf[n][j] = ead->ed_Name[j];
                    j++;
                }
                s_installed_buf[n][j] = '\0';
                if (strcmp(s_installed_buf[n], "Classic") != 0) {
                    s_installed[n] = s_installed_buf[n];
                    n++;
                }
                p += ead->ed_Size;
            }
        }
    }
    FreeDosObject(DOS_EXALLCONTROL, eac);
    UnLock(lock);
    s_installed[n] = 0;
}
static struct RIPanelUI s_panel;

static void put_num(char **p, long v) {
    char t[12];
    int n = 0;
    if (v < 0) {
        *(*p)++ = '-';
        v = -v;
    }
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 11);
    while (n)
        *(*p)++ = t[--n];
}

static struct RIMixBoard *mix_board_of(Object *canvas) {
    struct RISectUI *u = 0;
    GetAttr(MUIA_RSection_State, canvas, (IPTR *)&u);
    return u ? u->u.mix.board : 0;
}

static void put_str(char **p, const char *s) {
    while (*s)
        *(*p)++ = *s++;
}

static void format_readout(const struct RISectUI *ui, const struct RSectionDiag *dg, long changes) {
    char *p = s_readout;
    if (s_panel.tr == ui && s_panel.drum[0]) {    /* live mode */
        const struct RIPattern *p8 = &s_panel.drum[0]->u.s808.pat, *p9 = &s_panel.drum[1]->u.s909.pat;
        unsigned int k;
        put_str(&p, "FOCUS ");
        put_num(&p, s_panel.focus);
        put_str(&p, " PH ");
        put_num(&p, s_panel.playhead[2]);
        *p++ = '/';
        put_num(&p, s_panel.playhead[3]);
        put_str(&p, " BAR ");
        put_num(&p, ri_sui_value(ui, RI_STR_BAR));
        put_str(&p, " BPM ");
        put_num(&p, ri_sui_value(ui, RI_STR_TEMPO));
        put_str(&p, " ST ");
        put_num(&p, ui->u.tr.tr.state);
        put_str(&p, " BD8 ");
        for (k = 0; k < 16; k++)
            *p++ = ri_pdrum_get(p8, k, RI_L808_BD) ? 'x' : '.';
        put_str(&p, " CH9 ");
        for (k = 0; k < 16; k++)
            *p++ = ri_pdrum_get(p9, k, RI_L909_CH) ? 'x' : '.';
        put_str(&p, " N ");
        put_num(&p, (long)s_panel.changes);
        put_str(&p, " RAW ");
        put_num(&p, s_panel.last_raw);
        put_str(&p, " MIDI ");
        put_num(&p, (long)s_midi.messages);
        *p++ = '/';
        put_num(&p, (long)s_midi.ignored);
        put_str(&p, " LED ");
        put_num(&p, ri_sui_value(ui, RI_STR_MIDI));
        put_num(&p, ri_sui_value(ui, RI_STR_SYNC));
        put_str(&p, " CAMD ");
        put_num(&p, (long)s_camd_got);
        *p++ = '/';
        put_num(&p, (long)(s_camd_last >> 24));
        *p++ = '.';
        put_num(&p, (long)(s_camd_last & 0xFFu));
        put_str(&p, " SEL8 ");
        put_num(&p, ri_sui_value(s_panel.drum[0], RI_S808_SELECT));
        put_str(&p, " P909 ");
        put_num(&p, ri_spat_selected(&s_panel.pat[3]->u.pat));
        *p = 0;
        (void)dg;
        (void)changes;
        return;
    }
    if (s_panel.tr == ui && s_panel.synth[0]) {   /* keys mode */
        unsigned int k;
        put_str(&p, "FOCUS ");
        put_num(&p, s_panel.focus);
        put_str(&p, " OPT ");
        *p++ = s_panel.opts.select_patterns ? 'P' : '-';
        *p++ = s_panel.opts.program_synth ? 'S' : '-';
        put_str(&p, " PAT");
        for (k = 0; k < 4; k++) {
            *p++ = ' ';
            *p++ = (char)('A' + ri_spat_selected(&s_panel.pat[k]->u.pat) / 8);
            *p++ = (char)('1' + ri_spat_selected(&s_panel.pat[k]->u.pat) % 8);
        }
        put_str(&p, " STEP ");
        put_num(&p, ri_sui_display(s_panel.synth[0], RI_S303_DISPLAY));
        put_str(&p, " BPM ");
        put_num(&p, ri_sui_value(ui, RI_STR_TEMPO));
        put_str(&p, " ST ");
        put_num(&p, ui->u.tr.tr.state);
        put_str(&p, " LAST ");
        put_num(&p, s_panel.last.kind);
        *p++ = '/';
        put_num(&p, s_panel.last.arg);
        put_str(&p, " N ");
        put_num(&p, (long)s_panel.changes);
        put_str(&p, " RAW ");
        put_num(&p, s_panel.last_raw);
        *p++ = '/';
        put_num(&p, s_panel.last_qual);
        put_str(&p, " CLK ");   /* mouse buttons seen by the transport canvas */
        put_num(&p, dg ? dg->buttons : -1);
        *p = 0;
        (void)changes;
        return;
    }
    if (ui->section == RI_SEC_TRANSPORT) {
        unsigned int k;
        put_str(&p, "SONG ");
        put_num(&p, ri_sui_value(ui, RI_STR_MODE));
        put_str(&p, " BPM ");
        put_num(&p, ri_sui_value(ui, RI_STR_TEMPO));
        put_str(&p, " ST ");
        put_num(&p, ui->u.tr.tr.state);
        put_str(&p, " BAR ");
        put_num(&p, ri_sui_value(ui, RI_STR_BAR));
        put_str(&p, " LOOP ");
        put_num(&p, ri_sui_value(ui, RI_STR_LOOP_START));
        *p++ = '+';
        put_num(&p, ri_sui_value(ui, RI_STR_LOOP_LEN));
        put_str(&p, " PAT");
        for (k = 0; k < 4; k++) {
            const struct RISectUI *f = 0;
            GetAttr(MUIA_RSection_State, s_mix[k + 1], (IPTR *)&f);
            *p++ = ' ';
            *p++ = f->u.pat.off ? '-' : (char)('A' + ri_spat_selected(&f->u.pat) / 8);
            *p++ = (char)('1' + ri_spat_selected(&f->u.pat) % 8);
            *p++ = '/';
            put_num(&p, ri_sui_value(f, RI_SPAT_LENGTH));
        }
    } else if (ui->section >= RI_SEC_PCF && ui->section <= RI_SEC_COMP) {
        unsigned int k;
        for (k = 0; k < 4; k++) {
            const struct RISectUI *f = 0;
            GetAttr(MUIA_RSection_State, s_mix[k], (IPTR *)&f);
            put_str(&p, k == 0 ? "PCF " : k == 1 ? " DLY " : k == 2 ? " DST " : " CMP ");
            put_num(&p, f->u.fx.val[0]);
            *p++ = ':';
            put_num(&p, f->u.fx.val[2]);
            *p++ = '/';
            put_num(&p, f->u.fx.val[3]);
        }
    } else if (ri_smix_strip(ui->section) >= 0) {
        const struct RIMixBoard *b = ui->u.mix.board;
        unsigned int st;
        put_str(&p, "ON ");
        for (st = 0; st < 4; st++)
            *p++ = b->val[st][RI_SMIX_ONOFF] ? '1' : '0';
        put_str(&p, " DIST ");
        put_num(&p, ri_route_owner(&b->route, RI_ROUTE_DIST));
        put_str(&p, " PCF ");
        put_num(&p, ri_route_owner(&b->route, RI_ROUTE_PCF));
        put_str(&p, " COMP ");
        put_num(&p, ri_route_owner(&b->route, RI_ROUTE_COMP));
        put_str(&p, " LVL ");
        for (st = 0; st < 4; st++) {
            put_num(&p, b->val[st][RI_SMIX_LEVEL]);
            *p++ = '/';
        }
        put_num(&p, b->val[4][RI_SMST_LEVEL]);
    } else if (ui->section == RI_SEC_909) {
        const struct RISect909 *s = &ui->u.s909;
        unsigned int st;
        put_str(&p, "SEL ");
        put_num(&p, s->val[RI_S909_SELECT]);
        put_str(&p, " ROW ");
        for (st = 0; st < 16; st++)
            *p++ = ".LHF"[ri_sui_led(ui, RI_S909_STEP0 + st, 0) & 3];
        put_str(&p, " FLAM ");
        put_num(&p, s->val[RI_S909_FLAM]);
    } else if (ui->section == RI_SEC_808) {
        const struct RISect808 *s = &ui->u.s808;
        unsigned int st;
        put_str(&p, "SEL ");
        put_num(&p, s->val[RI_S808_SELECT]);
        put_str(&p, " ROW ");
        for (st = 0; st < 16; st++)
            *p++ = ri_sui_led(ui, RI_S808_STEP0 + st, 0) ? 'X' : '.';
        put_str(&p, " BD ");
        put_num(&p, s->pat.row.drum[0].on);
        put_str(&p, " LC ");
        put_num(&p, s->val[9]);
    } else {
        const struct RISect303 *s = &ui->u.s303;
        const struct RI303Row *r = &s->pat.row.r303[s->edit_step % 16u];
        put_str(&p, "STEP ");
        put_num(&p, ri_s303_display(s));
        put_str(&p, " KEY ");
        put_num(&p, r->key);
        put_str(&p, " FL ");
        put_num(&p, r->flags);
        put_str(&p, " PM ");
        put_num(&p, s->pitch_mode);
        put_str(&p, " CUT ");
        put_num(&p, s->val[2]);
    }
    put_str(&p, " CHG ");
    put_num(&p, changes);
    if (dg) {
        put_str(&p, " EV ");
        put_num(&p, dg->events);
        put_str(&p, "/");
        put_num(&p, dg->buttons);
    }
    *p = 0;
}

static void demo(Object *canvas, struct RISectUI *ui) {
    unsigned int i;
    if (ui->section == RI_SEC_TRANSPORT) {
        struct RISectUI *p[4];
        ri_sui_press(ui, RI_STR_MODE);
        for (i = 0; i < 8; i++)
            ri_sui_step(ui, RI_STR_TEMPO, 1);
        ri_sui_press(ui, RI_STR_RECORD);
        ri_sui_press(ui, RI_STR_FF);
        ri_sui_press(ui, RI_STR_FF);
        ri_sui_step(ui, RI_STR_BAR, 1);
        ri_sui_press(ui, RI_STR_LOOP);
        ri_sui_set(ui, RI_STR_LOOP_START, 17);
        ri_sui_set(ui, RI_STR_LOOP_LEN, 8);
        ri_sui_set(ui, RI_STR_SHUFFLE, 40);
        ri_str_indicator_set(&ui->u.tr, RI_STR_SYNC, 1);
        ri_str_indicator_set(&ui->u.tr, RI_STR_MIDI, 1);
        for (i = 0; i < 4; i++)
            GetAttr(MUIA_RSection_State, s_mix[i + 1], (IPTR *)&p[i]);
        ri_sui_set(p[0], RI_SPAT_BANK, 2);
        ri_sui_set(p[0], RI_SPAT_PATTERN, 2);
        for (i = 0; i < 4; i++)
            ri_sui_step(p[1], RI_SPAT_LENGTH, -1);
        ri_sui_set(p[2], RI_SPAT_BANK, 1);
        ri_sui_press(p[2], RI_SPAT_SHUFFLE);
        ri_sui_press(p[3], RI_SPAT_OFF);
        for (i = 0; i < 5; i++)
            ri_rsection_refresh(s_mix[i]);
        (void)canvas;
        return;
    }
    if (ui->section >= RI_SEC_PCF && ui->section <= RI_SEC_COMP) {
        struct RISectUI *f[4];
        for (i = 0; i < 4; i++) {
            GetAttr(MUIA_RSection_State, s_mix[i], (IPTR *)&f[i]);
            ri_sui_press(f[i], RI_SFX_ONOFF);
            ri_sfx_meter_set(&f[i]->u.fx, RI_SFX_METER, 70 + 20 * (int)i);
        }
        for (i = 0; i < 12; i++)
            ri_sui_step(f[0], RI_SFX_PCF_PATTERN, 1);
        ri_sui_press(f[0], RI_SFX_PCF_MODE);
        ri_sui_set(f[0], 4, 90);
        ri_sui_set(f[0], 5, 40);
        ri_sui_set(f[0], 6, 110);
        ri_sui_set(f[0], 7, 20);
        for (i = 0; i < 3; i++)
            ri_sui_step(f[1], RI_SFX_DLY_STEPS, 1);
        ri_sui_press(f[1], RI_SFX_DLY_TRIPLET);
        ri_sui_set(f[1], 4, 20);
        ri_sui_set(f[1], 5, 90);
        ri_sui_set(f[2], 2, 110);
        ri_sui_set(f[2], 3, 30);
        ri_sui_set(f[3], 2, 100);
        ri_sui_set(f[3], 3, 40);
        ri_sfx_meter_set(&f[3]->u.fx, RI_SFX_COMP_GR, 70);
        for (i = 0; i < 4; i++)
            ri_rsection_refresh(s_mix[i]);
        (void)canvas;
        return;
    }
    if (ri_smix_strip(ui->section) >= 0) {
        struct RIMixBoard *b = ui->u.mix.board;
        static const unsigned char lvl[4] = { 110, 90, 100, 120 }, pan[4] = { 30, 98, 64, 64 };
        static const unsigned char dly[4] = { 80, 0, 40, 0 }, mtr[4] = { 100, 60, 0, 127 };
        (void)canvas;
        ri_smix_press(b, RI_SEC_MIX_808, RI_SMIX_ONOFF);          /* mute 808 */
        ri_smix_press(b, RI_SEC_MIX_SYNTH1, RI_SMIX_PCF);
        ri_smix_press(b, RI_SEC_MIX_808, RI_SMIX_PCF);            /* steals it */
        ri_smix_press(b, RI_SEC_MIX_909, RI_SMIX_DIST);
        ri_smix_press(b, RI_SEC_MASTER, RI_SMST_COMP);
        for (i = 0; i < 4; i++) {
            ri_smix_set_value(b, RI_SEC_MIX_SYNTH1 + i, RI_SMIX_LEVEL, lvl[i]);
            ri_smix_set_value(b, RI_SEC_MIX_SYNTH1 + i, RI_SMIX_PAN, pan[i]);
            ri_smix_set_value(b, RI_SEC_MIX_SYNTH1 + i, RI_SMIX_DELAY, dly[i]);
            ri_smix_meter_set(b, RI_SEC_MIX_SYNTH1 + i, 0, mtr[i]);
        }
        ri_smix_set_value(b, RI_SEC_MASTER, RI_SMST_LEVEL, 90);
        ri_smix_meter_set(b, RI_SEC_MASTER, 0, 96);
        ri_smix_meter_set(b, RI_SEC_MASTER, 1, 127);
        for (i = 0; i < 5; i++)
            ri_rsection_refresh(s_mix[i]);
        return;
    }
    if (ui->section == RI_SEC_909) {
        for (i = 0; i < 16; i += 4) {
            ri_sui_press(ui, RI_S909_STEP0 + i);          /* BD low */
            if (i == 4 || i == 12)
                ri_sui_press(ui, RI_S909_STEP0 + i);      /* -> high */
        }
        ri_sui_set(ui, RI_S909_SELECT, 2);                /* SD */
        ri_sui_press(ui, RI_S909_FLAMBTN);
        ri_sui_press(ui, RI_S909_STEP0 + 4);
        ri_sui_press(ui, RI_S909_STEP0 + 12);
        ri_sui_press(ui, RI_S909_FLAMBTN);
        ri_sui_set(ui, RI_S909_SELECT, 8);                /* CH */
        for (i = 2; i < 16; i += 4)
            ri_sui_press(ui, RI_S909_STEP0 + i);
        ri_sui_set(ui, 2, 90);                            /* BD Tune */
        ri_sui_set(ui, RI_S909_FLAM, 30);
        ri_rsection_refresh(canvas);
        return;
    }
    if (ui->section == RI_SEC_808) {
        for (i = 0; i < 16; i += 4)
            ri_sui_press(ui, RI_S808_STEP0 + i);           /* BD */
        ri_sui_set(ui, RI_S808_SELECT, 11);                 /* CH */
        for (i = 2; i < 16; i += 4)
            ri_sui_press(ui, RI_S808_STEP0 + i);
        ri_sui_set(ui, RI_S808_SELECT, 0);                  /* AC */
        ri_sui_press(ui, RI_S808_STEP0 + 4);
        ri_sui_press(ui, RI_S808_STEP0 + 12);
        ri_sui_press(ui, 9);                                /* LT -> LC */
        ri_sui_set(ui, 1, 120);                             /* BD Level */
        ri_sui_set(ui, 2, 20);                              /* BD Tone */
        ri_sui_set(ui, RI_S808_SELECT, 11);                 /* show the CH row */
    } else {
        static const unsigned char seq[] = {
            RI_S303_PITCHMODE, RI_S303_ACCENT, RI_S303_KEY0 + 7, RI_S303_SLIDE, RI_S303_NOTEPAUSE,
            RI_S303_KEY0 + 12, RI_S303_UP, RI_S303_NOTEPAUSE, RI_S303_KEY0 + 4
        };
        for (i = 0; i < sizeof(seq); i++)
            ri_sui_press(ui, seq[i]);
        ri_sui_set(ui, 2, 30);
        ri_sui_set(ui, 3, 110);
        ri_sui_press(ui, RI_S303_WAVE);
    }
    ri_rsection_refresh(canvas);
}

int main(int argc, char **argv) {
    Object *app, *win, *canvas, *readout;
    ULONG sigs = 0, section = RI_SEC_SYNTH1;
    LONG ret;
    struct RISectUI *ui = 0;
    const struct RSectionDiag *dg = 0;
    int i, do_demo = 0, mix = 0, fx = 0, trp = 0, keys = 0, live = 0, remote = 0;
    struct MidiNode *mnode = 0;
    BYTE msig = -1;
    ULONG lms = 0, lmu = 0;
    ULONG t0s = 0, t0u = 0;
    int meter_lvl[2] = { 0, 0 }, last_ph[2] = { -1, -1 };
    ULONG seen = 0;
    UBYTE seen_st = 0;
    UWORD seen_raw = 0;
    BPTR trace = 0;
    Object *row = 0;

    for (i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "mod=", 4)) {
            int k = 0;
            while (argv[i][4 + k] && k < 63) {
                s_mod_arg[k] = argv[i][4 + k];
                k++;
            }
            s_mod_arg[k] = '\0';
            s_mod_pending = 1;
        } else if (argv[i][0] == '8')
            section = RI_SEC_808;
        else if (argv[i][0] == '9')
            section = RI_SEC_909;
        else if (argv[i][0] == 'm')
            mix = 1;
        else if (argv[i][0] == 'f')
            fx = 1;
        else if (argv[i][0] == 't')
            trp = 1;
        else if (argv[i][0] == 'k')
            keys = 1;
        else if (argv[i][0] == 'l')
            live = 1;
        else if (argv[i][0] == 'r')
            live = remote = 1;
        else if (argv[i][0] == 'd')
            do_demo = 1;
    }
    skin_scan();
    for (s_installed_n = 0; s_installed[s_installed_n]; s_installed_n++)
        ;
    if (s_mod_arg[0]) {   /* mod= applies in every mode (single sections have no panel) */
        skin_apply(s_mod_arg);
        s_mod_pending = 0;
    }
    if (live) {                        /* G6a: live panel, stand-in clock */
        static const ULONG sec[9] = { RI_SEC_TRANSPORT, RI_SEC_808, RI_SEC_MIX_808, RI_SEC_909, RI_SEC_MIX_909,
            RI_SEC_PAT_SYNTH1, RI_SEC_PAT_SYNTH2, RI_SEC_PAT_808, RI_SEC_PAT_909 };
        struct RISectUI *u = 0, *mix8 = 0;
        Object *r8, *r9, *pats;
        ri_panel_init(&s_panel);
        ri_panel_skins(&s_panel, s_installed, (uint32_t)s_installed_n,
                           s_mod_arg[0] ? s_mod_arg : "Classic");
        skin_apply(s_panel.skin_current);
        for (i = 0; i < 9; i++) {
            s_mix[i] = (Object *)ri_rsection_create(sec[i], 0);
            if (!s_mix[i])
                return 5;
            GetAttr(MUIA_RSection_State, s_mix[i], (IPTR *)&u);
            if (i == 0)
                s_panel.tr = u;
            else if (i == 1 || i == 3)
                s_panel.drum[i == 3] = u;
            else if (i == 2)
                s_panel.mix[2] = mix8 = u;
            else if (i == 4) {
                ri_sui_bind_board(u, mix8->u.mix.board);
                s_panel.mix[3] = u;
            }
            else
                s_panel.pat[i - 5] = u;
            SetAttrs(s_mix[i], MUIA_RSection_Panel, (IPTR)&s_panel, MUIA_RSection_KeyOwner, i == 0, TAG_DONE);
        }
        s_nall = 9;
        ui = s_panel.tr;
        canvas = s_mix[0];
        r8 = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 2,
            Child, (IPTR)s_mix[1], Child, (IPTR)s_mix[2], Child, (IPTR)MUI_NewObject(MUIC_Rectangle, TAG_DONE), TAG_DONE);
        r9 = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 2,
            Child, (IPTR)s_mix[3], Child, (IPTR)s_mix[4], Child, (IPTR)MUI_NewObject(MUIC_Rectangle, TAG_DONE), TAG_DONE);
        pats = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 2,
            Child, (IPTR)s_mix[5], Child, (IPTR)s_mix[6], Child, (IPTR)s_mix[7], Child, (IPTR)s_mix[8],
            Child, (IPTR)MUI_NewObject(MUIC_Rectangle, TAG_DONE), TAG_DONE);
        row = (r8 && r9 && pats) ? (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Spacing, 2,
            Child, (IPTR)s_mix[0], Child, (IPTR)r8, Child, (IPTR)r9, Child, (IPTR)pats, TAG_DONE) : 0;
        if (!row)
            return 5;
        section = RI_SEC_TRANSPORT;
        mix = 5;
        keys = 1;                      /* same key/trace/refresh path as keys mode */
        trace = Open((CONST_STRPTR)"RAM:RISECT.LOG", MODE_NEWFILE);
        ri_midi_init(&s_midi, 0);
        if (remote) {                  /* CAMD receiver: cluster "ri.remote", channel 1 */
            CamdBase = OpenLibrary((CONST_STRPTR)"camd.library", 0);
            msig = AllocSignal(-1);
            if (CamdBase && msig >= 0)
                mnode = CreateMidi(MIDI_Name, (IPTR)"RISECT", MIDI_RecvSignal, (IPTR)msig, MIDI_MsgQueue, 512, TAG_END);
            if (!mnode || !AddMidiLink(mnode, MLTYPE_Receiver, MLINK_Location, (IPTR)"ri.remote", TAG_END))
                return 9;
        }
    } else if (keys) {                 /* G5: one front panel across six canvases */
        Object *pats;
        struct RISectUI *u = 0;
        ri_panel_init(&s_panel);
        ri_panel_skins(&s_panel, s_installed, (uint32_t)s_installed_n,
                           s_mod_arg[0] ? s_mod_arg : "Classic");
        skin_apply(s_panel.skin_current);
        s_mix[0] = (Object *)ri_rsection_create(RI_SEC_TRANSPORT, 0);
        for (i = 0; i < 4; i++)
            s_mix[i + 1] = (Object *)ri_rsection_create(RI_SEC_PAT_SYNTH1 + (ULONG)i, 0);
        s_mix[5] = (Object *)ri_rsection_create(RI_SEC_SYNTH1, 0);
        for (i = 0; i < 6; i++) {
            if (!s_mix[i])
                return 5;
            GetAttr(MUIA_RSection_State, s_mix[i], (IPTR *)&u);
            if (i == 0)
                s_panel.tr = u;
            else if (i < 5)
                s_panel.pat[i - 1] = u;
            else
                s_panel.synth[0] = u;
            SetAttrs(s_mix[i], MUIA_RSection_Panel, (IPTR)&s_panel, MUIA_RSection_KeyOwner, i == 0, TAG_DONE);
        }
        ui = s_panel.tr;
        canvas = s_mix[0];
        /* fixed-size canvases in a wider column: rectangles fill the slack
         * so no stale pixels show beside them */
        pats = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 2,
            Child, (IPTR)MUI_NewObject(MUIC_Rectangle, TAG_DONE),
            Child, (IPTR)s_mix[1], Child, (IPTR)s_mix[2], Child, (IPTR)s_mix[3], Child, (IPTR)s_mix[4],
            Child, (IPTR)MUI_NewObject(MUIC_Rectangle, TAG_DONE), TAG_DONE);
        row = pats ? (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Spacing, 2,
            Child, (IPTR)s_mix[0], Child, (IPTR)pats, Child, (IPTR)s_mix[5], TAG_DONE) : 0;
        if (!row)
            return 5;
        section = RI_SEC_TRANSPORT;
        mix = 4;
        s_nall = 6;
        trace = Open((CONST_STRPTR)"RAM:RISECT.LOG", MODE_NEWFILE);
    } else if (trp) {                  /* transport (compact) over the four pattern sections */
        Object *pats;
        s_mix[0] = (Object *)ri_rsection_create(RI_SEC_TRANSPORT, RI_GEO_ZOOM_COMPACT);
        for (i = 0; i < 4; i++)
            s_mix[i + 1] = (Object *)ri_rsection_create(RI_SEC_PAT_SYNTH1 + (ULONG)i, 0);
        for (i = 0; i < 5; i++)
            if (!s_mix[i])
                return 5;
        GetAttr(MUIA_RSection_State, s_mix[0], (IPTR *)&ui);
        canvas = s_mix[0];
        pats = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 2,
            Child, (IPTR)s_mix[1], Child, (IPTR)s_mix[2], Child, (IPTR)s_mix[3], Child, (IPTR)s_mix[4], TAG_DONE);
        row = pats ? (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Spacing, 2,
            Child, (IPTR)s_mix[0], Child, (IPTR)pats, TAG_DONE) : 0;
        if (!row)
            return 5;
        section = RI_SEC_TRANSPORT;
        mix = 3;
    } else if (fx) {                   /* the four effect units */
        for (i = 0; i < 4; i++) {
            s_mix[i] = (Object *)ri_rsection_create(RI_SEC_PCF + (ULONG)i, 0);
            if (!s_mix[i])
                return 5;
        }
        GetAttr(MUIA_RSection_State, s_mix[0], (IPTR *)&ui);
        canvas = s_mix[0];
        row = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 2,
            Child, (IPTR)s_mix[0], Child, (IPTR)s_mix[1], Child, (IPTR)s_mix[2], Child, (IPTR)s_mix[3], TAG_DONE);
        if (!row)
            return 5;
        section = RI_SEC_PCF;
        mix = 2;
    } else if (mix) {                  /* four mixers + master, one shared board */
        struct RISectUI *u = 0;
        for (i = 0; i < 5; i++) {
            s_mix[i] = (Object *)ri_rsection_create(i < 4 ? RI_SEC_MIX_SYNTH1 + (ULONG)i : RI_SEC_MASTER, 0);
            if (!s_mix[i])
                return 5;
            GetAttr(MUIA_RSection_State, s_mix[i], (IPTR *)&u);
            if (i == 0)
                ui = u;
            else
                ri_sui_bind_board(u, ui->u.mix.board);
        }
        canvas = s_mix[0];
        row = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 2,
            Child, (IPTR)s_mix[0], Child, (IPTR)s_mix[1], Child, (IPTR)s_mix[2], Child, (IPTR)s_mix[3],
            Child, (IPTR)s_mix[4], TAG_DONE);
        if (!row)
            return 5;
        section = RI_SEC_MIX_SYNTH1;
    } else {
        canvas = (Object *)ri_rsection_create(section, 0);
        if (!canvas)
            return 5;
        row = canvas;
    }
    if (!mix)
        GetAttr(MUIA_RSection_State, canvas, (IPTR *)&ui);
    GetAttr(MUIA_RSection_Diag, canvas, (IPTR *)&dg);
    format_readout(ui, dg, 0);
    readout = (Object *)MUI_NewObject(MUIC_Text, MUIA_Text_Contents, (IPTR)s_readout, TAG_DONE);
    if (!readout)
        return 6;
    win = (Object *)MUI_NewObject(MUIC_Window,
        MUIA_Window_Title, mix == 5 ? "RI-LIVE" : mix == 4 ? "RI-KEYS" : mix == 3 ? "RI-TRANSPORT" : mix == 2 ? "RI-FX" : mix ? "RI-MIX" : section == RI_SEC_808 ? "RI-808" : section == RI_SEC_909 ? "RI-909" : "RI-303",
        MUIA_Window_LeftEdge, 0,
        MUIA_Window_TopEdge, 0,
        MUIA_Window_CloseGadget, TRUE,
        MUIA_Window_DepthGadget, TRUE,
        MUIA_Window_DragBar, TRUE,
        MUIA_Window_RootObject, (IPTR)MUI_NewObject(MUIC_Group,
            MUIA_Group_Spacing, 2,
            Child, (IPTR)row,
            Child, (IPTR)readout,
            TAG_DONE),
        TAG_DONE);
    if (!win)
        return 7;
    app = (Object *)MUI_NewObject(MUIC_Application,
        MUIA_Application_Title, "RI-SECT",
        MUIA_Application_Base, "RISECT",
        SubWindow, (IPTR)win,
        TAG_DONE);
    if (!app)
        return 8;
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2,
        MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    if (do_demo) {
        demo(canvas, ui);
        format_readout(ui, dg, 0);
        SetAttrs(readout, MUIA_Text_Contents, (IPTR)s_readout, TAG_DONE);
    }
    /* canonical union loop (m60) */
    for (;;) {
        IPTR ch = 0; /* GetAttr stores a full IPTR (never a LONG: stack smash, 2026-09-25) */
        ret = (LONG)DoMethod(app, MUIM_Application_NewInput, &sigs);
        if (ret == (LONG)MUIV_Application_ReturnID_Quit)
            break;
        GetAttr(MUIA_RSection_Changes, canvas, &ch);
        if (mnode) {                   /* drain CAMD: Standard Mapping onto the panel */
            MidiMsg mm;
            ULONG ns, nu, dms;
            while (GetMidi(mnode, &mm)) {
                s_camd_got++;
                s_camd_last = mm.mm_Msg;
                ri_midi_msg(&s_midi, &s_panel, mm.mm_Status, mm.mm_Data1, mm.mm_Data2);
            }
            CurrentTime(&ns, &nu);
            dms = lms ? (ns - lms) * 1000u + nu / 1000u - lmu / 1000u : 0u;
            lms = ns;
            lmu = nu;
            ri_midi_elapse(&s_midi, &s_panel, dms);
        }
        if (live) {   /* STAND-IN clock: wall time -> samples (G6b: the render task) */
            ULONG ns, nu;
            int playing = ui->u.tr.tr.state != RI_TR_STOPPED, k;
            uint64_t samples = 0;
            CurrentTime(&ns, &nu);
            if (playing && !s_panel.playing) {
                t0s = ns;
                t0u = nu;
            }
            if (playing)
                samples = ((uint64_t)(ns - t0s) * 1000000u + nu - t0u) * 48u / 1000u;
            ri_panel_live(&s_panel, playing, ri_live_16ths(samples, (uint32_t)ri_sui_value(ui, RI_STR_TEMPO), 48000u));
            for (k = 0; k < 2; k++) {           /* stand-in meters: hit -> 0 dBFS, then -20 dB/s */
                const struct RIPattern *pp = k ? &s_panel.drum[1]->u.s909.pat : &s_panel.drum[0]->u.s808.pat;
                int ph = s_panel.playhead[2 + k], hit = 0;
                uint32_t ln;
                if (ph >= 0 && ph != last_ph[k])
                    for (ln = 0; ln < 11u; ln++)
                        hit |= ri_pdrum_get(pp, (uint32_t)ph, ln) != RI_HIT_OFF;
                if (ph != last_ph[k])
                    meter_lvl[k] = hit ? ri_live_meter_level(1.0f) : meter_lvl[k] - 9;   /* ~9 levels per 16th */
                if (meter_lvl[k] < 0 || !playing)
                    meter_lvl[k] = 0;
                last_ph[k] = ph;
                ri_smix_meter_set(mix_board_of(s_mix[2 + 2 * k]), RI_SEC_MIX_808 + (uint32_t)k, 0, meter_lvl[k]);
            }
        }
        if (keys && (s_panel.changes != seen || ui->u.tr.tr.state != seen_st || s_panel.last_raw != seen_raw)) {
            seen_st = ui->u.tr.tr.state;
            seen_raw = s_panel.last_raw;   /* focus/keys touch several canvases */
            seen = s_panel.changes;
            for (i = 0; i < s_nall; i++)
                ri_rsection_refresh(s_mix[i]);
            if (trace) {                          /* one line per change: key-proof evidence */
                format_readout(ui, dg, 0);
                FPuts(trace, (CONST_STRPTR)s_readout);
                FPuts(trace, (CONST_STRPTR)"\n");
                Flush(trace);
            }
        }
        format_readout(ui, dg, (long)ch);
        SetAttrs(readout, MUIA_Text_Contents, (IPTR)s_readout, TAG_DONE);
        if (keys && strcmp(s_panel.skin_current, s_loaded) != 0) {  /* Ctrl+M cycled */
            int k;                                                     /* (panel modes only: */
            skin_apply(s_panel.skin_current);                          /* single sections have */
            for (k = 0; k < s_nall; k++)                               /* no panel to cycle) */
                ri_rsection_refresh(s_mix[k]);
        }
        sigs |= SIGBREAKF_CTRL_C | (msig >= 0 ? 1UL << msig : 0UL);
        sigs = Wait(sigs);
        if (sigs & SIGBREAKF_CTRL_C)
            break;
    }
    if (trace)
        Close(trace);
    if (mnode)
        DeleteMidi(mnode);
    if (msig >= 0)
        FreeSignal(msig);
    if (CamdBase)
        CloseLibrary(CamdBase);
    SetAttrs(win, MUIA_Window_Open, FALSE, TAG_DONE);
    MUI_DisposeObject(app);
    ri_rsection_dispose_class();
    return 0;
}
