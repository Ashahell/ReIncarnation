/* t140_songs — songs & playlists (owner 2026-09-30).
 * Laws: the RBS song script compiles every statement (303 notes fold into
 * range with accent/slide, drum lanes and accents, Levi chords, the song
 * track, automation by key and by registry name; later AUTO wins, sorted)
 * and names the failing line; RBNG carries Levi pattern banks (v1.5) and
 * a Levi song with automation reads back (the minor is the highest one a
 * feature needs); playlists resolve host and AROS paths, name entries and
 * wrap; the app core loads a song (length, devices, tempo) and plays it
 * from bar 0 with every selection on its own bar, also when block edges
 * meet bar lines, and again after stop -> play.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "project/songscript.h"
#include "project/playlist.h"
#include "project/rbng.h"
#include "app/core/riapp_core.h"

static struct RISong S, R;
static struct RBAutoEv EV[64], EVR[64];
static struct RIAppCore C;
static float L[96000u * 5u], Rr[96000u * 5u];

static int parse(const char *text, char *err) {
    rbng_song_init(&S);
    S.atrk = EV;
    S.atrk_cap = 64u;
    return ri_rbs_parse(text, &S, err, 128u);
}

static float bar_energy(const float *x, uint32_t bar, uint32_t barlen) {
    uint32_t i;
    float e = 0.0f;
    for (i = 0u; i < barlen; i++)
        e += x[bar * barlen + i] * x[bar * barlen + i];
    return e;
}

int main(void) {
    char err[128];
    uint32_t i, k;

    /* ---- Script: every statement. ---- */
    RI_ASSERT(parse("RBS 1\n"
        "TEMPO 120\nPPQ 96\nCPRG test credit\n"
        "P303 0 1 46a - 58s 22 -\n"
        "PDRUM 3 2 BD x...X...x...f...\nPACC 3 2 x.......x.......\n"
        "PDRUM 2 1 CB ..x.\n"
        "PLEVI 4 16 0 58,65,70\nPLEVI 4 16 0 60\n"
        "TRACK 0 3 1 0 1 2 4\nTRACK 2 2 - 0 - - -\n"
        "AUTO 10 0x0300 5\nSET 0 303a.Cutoff 40\nAUTO 10 0x0300 7\n# comment\n", err) == 0, "parse: %s", err);
    RI_ASSERT(S.tempo == 120u && S.ppq == 96u && !strcmp(S.cprg, "test credit"), "header");
    RI_ASSERT(S.bank[0].pat[1].length == 5u, "303 length = tokens");
    RI_ASSERT(ri_p303_note(&S.bank[0].pat[1].row.r303[0]) == 46u &&
        (S.bank[0].pat[1].row.r303[0].flags & RI_STEP_ACCENT), "303 note + accent");
    RI_ASSERT(S.bank[0].pat[1].row.r303[1].flags & RI_STEP_REST, "303 rest");
    RI_ASSERT(ri_p303_note(&S.bank[0].pat[1].row.r303[2]) == 58u &&
        (S.bank[0].pat[1].row.r303[2].flags & RI_STEP_SLIDE), "303 up octave + slide");
    RI_ASSERT(ri_p303_note(&S.bank[0].pat[1].row.r303[3]) == 34u, "22 folds up an octave to 34");
    RI_ASSERT(S.bank[0].pat[0].row.r303[0].flags & RI_STEP_REST, "unnamed 303 slots are silent");
    RI_ASSERT(ri_pdrum_get(&S.bank[3].pat[2], 0u, RI_L909_BD) == RI_HIT_LOW &&
        ri_pdrum_get(&S.bank[3].pat[2], 4u, RI_L909_BD) == RI_HIT_HIGH &&
        ri_pdrum_get(&S.bank[3].pat[2], 12u, RI_L909_BD) == RI_HIT_FLAM &&
        (S.bank[3].pat[2].row.drum[8].flags & RI_DRUM_AC), "909 lanes + accent");
    RI_ASSERT(S.bank[2].pat[1].length == 4u && ri_pdrum_get(&S.bank[2].pat[1], 2u, RI_L808_CB) == RI_HIT_LOW,
        "808 lane, length from the row");
    RI_ASSERT(ri_levi_get(&S.bank[4].pat[4], 0u, 0u) == 60u && !ri_levi_on(&S.bank[4].pat[4], 0u, 1u),
        "a later PLEVI replaces the step");
    RI_ASSERT(S.track.slot[0][0] == 1u && S.track.slot[3][4] == 4u && S.track.slot[2][1] == 0u &&
        S.track.slot[2][0] == 1u && S.track.slot[4][0] == 0u, "track ranges, '-' keeps");
    RI_ASSERT(S.natrk == 2u && S.atrk[0].tick == 0u && S.atrk[0].ctl == 0x0300u && S.atrk[0].val == 40u &&
        S.atrk[1].tick == 10u && S.atrk[1].val == 7u, "SET by name, sorted, later AUTO wins");
    RI_ASSERT(parse("RBS 1\nTEMPO 20\n", err) == 2 && !strncmp(err, "line 2", 6), "range error names the line: %s", err);
    RI_ASSERT(parse("TEMPO 120\n", err) == 2 && !strncmp(err, "line 1", 6), "header required");
    RI_ASSERT(parse("RBS 1\nSET 0 303a.Nope 1\n", err) == 2, "unknown control refused");
    RI_ASSERT(parse("RBS 1\nPDRUM 3 1 ZZ x...\n", err) == 2, "unknown lane refused");
    RI_ASSERT(parse("RBS 1\nPLEVI 1 16 0 1,2,3,4,5,6,7\n", err) == 2, "7 Levi lanes refused");
    RI_ASSERT(parse("RBS 1\nBOGUS\n", err) == 2 && strstr(err, "unknown statement"), "unknown statement");
    RI_ASSERT(parse("RBS 1\nPACC 3 1 x.q.\n", err) == 2, "bad accent char refused");
    RI_ASSERT(parse("RBS 1\nSET 0 levi.Morph/Drive 10\nSET 0 levi.D_Type 9\n", err) == 0 && S.natrk == 2u &&
        S.atrk[0].ctl == 0x0E38u && S.atrk[1].ctl == 0x0E39u, "names with '/' and blanks resolve");

    /* ---- RBNG: Levi banks (v1.5) and Levi + automation read back. ---- */
    RI_ASSERT(parse("RBS 1\nPLEVI 3 8 2 50,57\nTRACK 0 0 0 0 0 0 3\nAUTO 0 0x0E00 70\n", err) == 0, "levi song");
    RI_ASSERT(rbng_write_song("/tmp/ri/run/t140.rbng", &S, err, sizeof err) == 0, "write: %s", err);
    rbng_song_init(&R);
    R.atrk = EVR;
    R.atrk_cap = 64u;
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t140.rbng", &R, err, sizeof err) == 0, "read back: %s", err);
    for (k = 0u; k < R.nbanks && R.bank[k].instance != 4u; k++)
        ;
    RI_ASSERT(k < R.nbanks && R.bank[k].kind == RI_PATTERN_KIND_LEVI &&
        !memcmp(&R.bank[k].pat[3], &S.bank[4].pat[3], sizeof S.bank[4].pat[3]), "Levi bank round-trips");
    RI_ASSERT(R.natrk == 1u && R.track.slot[0][4] == 3u, "automation + Levi track round-trip");
    {
        FILE *f = fopen("/tmp/ri/run/t140.rbng", "rb");
        unsigned char h[24];
        RI_ASSERT(f && fread(h, 1, 24, f) == 24u, "header bytes");
        if (f)
            fclose(f);
        RI_ASSERT(!memcmp(h + 12, "VERS", 4) && h[20] == 0u && h[21] == 1u && h[22] == 0u && h[23] == 5u,
            "VERS 1.5 (%u.%u)", h[20] << 8 | h[21], h[22] << 8 | h[23]);
    }
    /* A Levi track with automation and no Levi bank: v1.4 (it used to
     * write 1.2 next to a 5-wide STRK and fail to read back). */
    rbng_song_init(&S);
    S.atrk = EV;
    S.atrk_cap = 64u;
    S.nsteps = 1u;
    S.steps[0].note = 36u;
    S.steps[0].flags = RI_RBNG_REST;
    S.track.slot[0][4] = 1u;
    S.atrk[0].tick = 0u;
    S.atrk[0].ctl = 0x0300u;
    S.atrk[0].val = 70u;
    S.natrk = 1u;
    RI_ASSERT(rbng_write_song("/tmp/ri/run/t140b.rbng", &S, err, sizeof err) == 0, "levi track + automation, no bank");
    rbng_song_init(&R);
    R.atrk = EVR;
    R.atrk_cap = 64u;
    RI_ASSERT(rbng_read_song("/tmp/ri/run/t140b.rbng", &R, err, sizeof err) == 0, "minor 4 file reads: %s", err);

    /* ---- Playlists. ---- */
    {
        static struct RIPlaylist pl;
        char dir[64], name[64], j[64];
        ri_playlist_dirname("songs/local/demos.rbpl", dir, sizeof dir);
        RI_ASSERT(!strcmp(dir, "songs/local"), "host dirname %s", dir);
        ri_playlist_dirname("Vk4aros:songs/demos.rbpl", dir, sizeof dir);
        RI_ASSERT(!strcmp(dir, "Vk4aros:songs"), "aros dirname %s", dir);
        ri_playlist_dirname("Work:demos.rbpl", dir, sizeof dir);
        RI_ASSERT(!strcmp(dir, "Work:"), "volume dirname %s", dir);
        RI_ASSERT(ri_playlist_join("Work:", "a.rbng", j, sizeof j) == 0 && !strcmp(j, "Work:a.rbng"), "volume join");
        RI_ASSERT(ri_playlist_join("x/y", "/abs.rbng", j, sizeof j) == 0 && !strcmp(j, "/abs.rbng"), "absolute stays");
        RI_ASSERT(ri_playlist_parse("# demo\nRBPL 1\nTITLE  Demo songs \n\nzombie.rbng | Zombie Nation\n"
            "../demo/classic.rbng\nWork:x/y.rbng\n", "songs/local", &pl, err, sizeof err) == 0, "playlist: %s", err);
        RI_ASSERT(pl.n == 3u && !strcmp(pl.title, "Demo songs"), "entries + title");
        /* Entry 1 was asserted here as "songs/local/../demo/classic.rbng" until
         * 2026-10-02: the test pinned the un-folded join, which is why a playlist
         * entry that could not be opened on AROS passed every host gate. The join is
         * canonicalised now (t157) and this is the resolved path. */
        RI_ASSERT(!strcmp(pl.e[0].path, "songs/local/zombie.rbng") && !strcmp(pl.e[1].path,
            "songs/demo/classic.rbng") && !strcmp(pl.e[2].path, "Work:x/y.rbng"), "paths resolve");
        ri_playlist_name(&pl, 0u, name, sizeof name);
        RI_ASSERT(!strcmp(name, "Zombie Nation"), "title name");
        ri_playlist_name(&pl, 1u, name, sizeof name);
        RI_ASSERT(!strcmp(name, "classic"), "file name without extension: %s", name);
        RI_ASSERT(ri_playlist_next(&pl, 2u) == 0u && ri_playlist_prev(&pl, 0u) == 2u && ri_playlist_next(&pl, 0u) == 1u,
            "wrap");
        RI_ASSERT(ri_playlist_parse("RBPL 1\n", "", &pl, err, sizeof err) == 2, "no songs refused");
        RI_ASSERT(ri_playlist_parse("RBPL 2\nx\n", "", &pl, err, sizeof err) == 2 && !strncmp(err, "line 1", 6),
            "header checked");
    }

    /* ---- App core: load and play on the bar. 120 BPM at 256-frame buffers:
     * every bar line meets a block edge (the case that used to lag). ---- */
    {
        static const char *SONG = "RBS 1\nTEMPO 120\n"
            "P303 0 1 46 46 46 46 46 46 46 46 46 46 46 46 46 46 46 46\n"
            "TRACK 0 0 1 0 0 0 0\nTRACK 2 2 1 0 0 0 0\nSET 0 303a.Cutoff 100\n";
        struct RICoreSong cs;
        uint32_t tk[64], barlen = 96000u, done = 0u, pass;
        uint16_t ct[64];
        uint8_t vl[64];
        RI_ASSERT(parse(SONG, err) == 0, "core song: %s", err);
        memset(&cs, 0, sizeof cs);
        for (k = 0u; k < 5u; k++)
            cs.bank[k] = &S.bank[k];
        cs.track = &S.track;
        cs.bpm = 120.0f;
        cs.ppq = 96u;
        for (i = 0u; i < S.natrk; i++) {
            tk[i] = S.atrk[i].tick;
            ct[i] = S.atrk[i].ctl;
            vl[i] = S.atrk[i].val;
        }
        cs.auto_tick = tk;
        cs.auto_ctl = ct;
        cs.auto_val = vl;
        cs.nauto = S.natrk;
        ri_core_init(&C, 96u, 48000.0f, 140.0f, 0x1Fu);
        RI_ASSERT(ri_core_load_song(&C, &cs) == 0, "load");
        RI_ASSERT(C.song_bars == 3u && C.song_sections == 0x01u && C.song_bpm == 120.0f, "length %u, devices %x",
            C.song_bars, C.song_sections);
        RI_ASSERT(ri_core_pattern_silent(&C.banks[0], 0u) && !ri_core_pattern_silent(&C.banks[0], 1u), "silence law");
        for (pass = 0u; pass < 2u; pass++) {             /* again after stop -> play */
            done = 0u;
            ri_core_play(&C);
            while (done < barlen * 4u) {
                uint32_t got = ri_live_render(&C.session, L + done, Rr + done, 256u);
                if (!got)
                    break;
                done += got;
            }
            ri_core_stop(&C);                             /* first Stop pauses ... */
            ri_core_stop(&C);                             /* ... the second rewinds (E1 p. 145) */
            RI_ASSERT(done == barlen * 4u && C.session.cursor_ticks == 0u, "rendered, rewound");
            RI_ASSERT(L[0] != 0.0f || L[1] != 0.0f || L[64] != 0.0f, "pass %u: sound from the first sample", pass);
            RI_ASSERT(bar_energy(L, 0u, barlen) > 1.0f && bar_energy(L, 2u, barlen) > 1.0f, "bars 0 and 2 play");
            RI_ASSERT(bar_energy(L, 1u, barlen / 2u + barlen / 2u) < 0.01f * bar_energy(L, 0u, barlen) ||
                bar_energy(L + barlen + barlen / 4u, 0u, barlen * 3u / 4u) < 0.01f * bar_energy(L, 0u, barlen),
                "pass %u: bar 1 selects the silent slot (tail aside)", pass);
            RI_ASSERT(bar_energy(L + barlen * 3u + barlen / 4u, 0u, barlen * 3u / 4u) < 0.01f * bar_energy(L, 0u, barlen),
                "pass %u: bar 3 silent", pass);
        }
    }
    /* ---- Pause mid-bar and resume = one uninterrupted run; a loop wraps
     * back to its start and keeps playing (the sample cursor follows the
     * tick cursor). ---- */
    {
        static float M[96000u * 4u], MR[96000u * 4u];
        uint32_t barlen = 96000u, done = 0u, cut = 96000u + 48128u;   /* 1.5 bars, block aligned */
        struct RILoop lp;
        const struct RIPatternBank *b5[RI_SONGTRACK_INSTANCES];
        ri_core_stop(&C);
        ri_core_stop(&C);
        ri_core_play(&C);
        while (done < barlen * 3u)
            done += ri_live_render(&C.session, M + done, MR + done, 256u);
        ri_core_stop(&C);
        ri_core_stop(&C);
        ri_core_play(&C);
        done = 0u;
        while (done < cut)
            done += ri_live_render(&C.session, L + done, Rr + done, 256u);
        ri_core_stop(&C);                                            /* pause */
        ri_core_play(&C);                                            /* resume */
        while (done < barlen * 3u)
            done += ri_live_render(&C.session, L + done, Rr + done, 256u);
        /* bar 2 still starts on its own downbeat after the mid-bar pause */
        RI_ASSERT(bar_energy(L, 2u, barlen) > 0.99f * bar_energy(M, 2u, barlen) &&
            bar_energy(L, 2u, barlen) < 1.01f * bar_energy(M, 2u, barlen) &&
            bar_energy(L + barlen + barlen / 2u + barlen / 4u, 0u, barlen / 4u) < 0.01f * bar_energy(M, 2u, barlen),
            "pause + resume keeps the bar grid");
        ri_core_stop(&C);
        ri_core_stop(&C);
        for (k = 0u; k < RI_SONGTRACK_INSTANCES; k++)
            b5[k] = &C.banks[k];
        lp.on = 1u;
        lp.start_bar = 0u;
        lp.len_bars = 2u;
        /* stop mid-bar, rewind, play: bar 0 again from its first step */
        ri_core_play(&C);
        done = 0u;
        while (done < barlen + barlen / 4u)
            done += ri_live_render(&C.session, L + done, Rr + done, 256u);
        ri_core_stop(&C);
        ri_core_stop(&C);
        ri_core_play(&C);
        done = 0u;
        while (done < barlen * 2u)
            done += ri_live_render(&C.session, L + done, Rr + done, 256u);
        RI_ASSERT(bar_energy(L, 0u, barlen) > 0.99f * bar_energy(M, 0u, barlen) &&
            bar_energy(L + barlen + barlen / 4u, 0u, barlen * 3u / 4u) < 0.01f * bar_energy(M, 0u, barlen),
            "restart after a mid-bar rewind plays bar 0 from step 1");
        ri_core_stop(&C);
        ri_core_stop(&C);
        ri_live_set_banks(&C.session, b5, &C.track, &lp);
        ri_core_play(&C);
        done = 0u;
        while (done < barlen * 4u)
            done += ri_live_render(&C.session, L + done, Rr + done, 256u);
        RI_ASSERT(bar_energy(L, 2u, barlen) > 1.0f && bar_energy(L, 2u, barlen) > 0.99f * bar_energy(L, 0u, barlen) &&
            bar_energy(L + barlen * 3u + barlen / 4u, 0u, barlen * 3u / 4u) < 0.01f * bar_energy(L, 0u, barlen),
            "loop wraps to bar 0 and keeps playing (%f %f)", (double)bar_energy(L, 0u, barlen),
            (double)bar_energy(L, 2u, barlen));
    }
    RI_RESULT("songs");
}
