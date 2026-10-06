/* tools/songplay — render an RBNG song (or a playlist entry) through the
 * app core, headless (songs & playlists, owner 2026-09-30). The same path
 * the live app plays: ri_core_load_song -> live session (player + song
 * track + automation lane) -> engine, in 256-frame device buffers.
 *
 * usage:
 *   songplay IN.rbng OUT.wav [--bars N] [--pack P]  render N bars (default:
 *                                            the song length) + a 2 s tail;
 *                                            P = the 909 pack (default
 *                                            reference/packs/classic-01/pack.rbnm)
 *   songplay --playlist IN.rbpl [--list]     list the entries (and check
 *                                            that each song loads)
 * Output: 48 kHz 16-bit stereo WAV. Prints length, peak, RMS, clipped
 * samples and the peak of each section.
 * exit: 0 ok, 2 usage / IO / codec error.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "project/rbng.h"
#include "project/playlist.h"
#include "project/rbnm.h"
#include "engine/dsp/rb909.h"
#include "app/core/riapp_core.h"

#define SR 48000u
#define CHUNK 256u
#define MAXAUTO 65536u

static struct RISong song;
static struct RBAutoEv ev[MAXAUTO];
static uint32_t tk[MAXAUTO];
static uint16_t ct[MAXAUTO];
static uint8_t vl[MAXAUTO];
static struct RIAppCore core;

static int load(const char *path, struct RICoreSong *cs, char *err, uint32_t cap) {
    uint32_t i, k;
    rbng_song_init(&song);
    song.atrk = ev;
    song.atrk_cap = MAXAUTO;
    if (rbng_read_song(path, &song, err, cap) != 0)
        return 2;
    memset(cs, 0, sizeof *cs);
    for (k = 0u; k < song.nbanks; k++)
        if (song.bank[k].instance < RI_SONGTRACK_INSTANCES)
            cs->bank[song.bank[k].instance] = &song.bank[k];
    cs->track = &song.track;
    cs->bpm = (float)song.tempo;
    cs->ppq = song.ppq;
    for (i = 0u; i < song.natrk; i++) {
        tk[i] = song.atrk[i].tick;
        ct[i] = song.atrk[i].ctl;
        vl[i] = song.atrk[i].val;
    }
    cs->auto_tick = tk;
    cs->auto_ctl = ct;
    cs->auto_val = vl;
    cs->nauto = song.natrk;
    return 0;
}

/* The classic-01 909 pack, bound like the app does on AROS
 * (platform/aros/pack_909.c); layer buffers live for the process. */
static int bind_909(struct RIEngine *eng, const char *pack) {
    static struct RBNMLayerInfo info[32];
    static char err[160];
    int32_t nl, v, bound = 0;
    nl = rbnm_pack_layers(pack, info, 32u, err, sizeof err);
    if (nl < 0) {
        printf("songplay: 909 pack %s: %s (909 silent)\n", pack, err);
        return 0;
    }
    for (v = 0; v < (int32_t)RI_909_NVOICES; v++) {
        struct RISampleLayer lay[3];
        uint32_t n = 0u, rate0 = 0u;
        int32_t k;
        for (k = 0; k < nl && n < 3u; k++) {
            uint32_t rate = 0u;
            int32_t got;
            float *b;
            if (info[k].voice != (uint32_t)v || info[k].frames > 88200u)
                continue;
            if (!(b = (float *)malloc(info[k].frames * sizeof(float))))
                break;
            got = rbnm_load_smpl(pack, info[k].id, b, info[k].frames, &rate, err, sizeof err);
            if (got < 0 || !rate || (n && rate != rate0)) {
                free(b);
                break;
            }
            rate0 = rate;
            lay[n].data = b;
            lay[n].frames = (uint32_t)got;
            lay[n].rate = rate;
            lay[n].lo = info[k].lo;
            lay[n].hi = info[k].hi;
            n++;
        }
        if (n && ri_engine_909_bind(eng, (uint32_t)v, lay, n) == 0)
            bound++;
    }
    return bound;
}

static void wr32(FILE *f, uint32_t v) {
    fputc((int)(v & 255u), f);
    fputc((int)((v >> 8) & 255u), f);
    fputc((int)((v >> 16) & 255u), f);
    fputc((int)(v >> 24), f);
}

static void wr16(FILE *f, uint32_t v) {
    fputc((int)(v & 255u), f);
    fputc((int)((v >> 8) & 255u), f);
}

static int playlist(const char *path) {
    static char text[65536], err[256], dir[256], name[64];
    static struct RIPlaylist pl;
    struct RICoreSong cs;
    FILE *f = fopen(path, "rb");
    size_t n;
    uint32_t i, bad = 0u;
    if (!f) {
        printf("songplay: cannot open %s\n", path);
        return 2;
    }
    n = fread(text, 1, sizeof text - 1u, f);
    fclose(f);
    text[n] = '\0';
    ri_playlist_dirname(path, dir, sizeof dir);
    if (ri_playlist_parse(text, dir, &pl, err, sizeof err) != 0) {
        printf("songplay: %s: %s\n", path, err);
        return 2;
    }
    printf("playlist '%s': %u songs\n", pl.title, (unsigned)pl.n);
    for (i = 0u; i < pl.n; i++) {
        ri_playlist_name(&pl, i, name, sizeof name);
        if (load(pl.e[i].path, &cs, err, sizeof err) != 0) {
            printf("  %2u  %-28s %s  (MISSING/INVALID: %s)\n", (unsigned)i + 1u, name, pl.e[i].path, err);
            bad++;
            continue;
        }
        ri_core_init(&core, 96u, (float)SR, cs.bpm, 0x1Fu);
        ri_core_load_song(&core, &cs);
        printf("  %2u  %-28s %s  (%u bars, %u BPM)\n", (unsigned)i + 1u, name, pl.e[i].path,
            (unsigned)core.song_bars, (unsigned)song.tempo);
    }
    return bad ? 2 : 0;
}

int main(int argc, char **argv) {
    static char err[256];
    static float L[CHUNK], R[CHUNK];
    struct RICoreSong cs;
    uint32_t bars = 0u, i;
    const char *pack = "reference/packs/classic-01/pack.rbnm";
    uint64_t frames, done = 0u, clip = 0u;
    /* P0 oracle: FNV-1a 64 over the raw float L/R bit patterns. */
    uint64_t fhash = 1469598103934665603ULL;
    int do_hash = 0;
    const char *f32path = 0;
    double sum = 0.0, peak = 0.0;
    float secpk[RI_ROUTE_NSECTIONS];
    FILE *f;
    if (argc >= 3 && !strcmp(argv[1], "--playlist"))
        return playlist(argv[2]);
    if (argc < 3) {
        printf("usage: songplay IN.rbng OUT.wav [--bars N] [--pack 909PACK] | songplay --playlist IN.rbpl\n");
        return 2;
    }
    for (i = 3u; i + 1u < (uint32_t)argc; i++)
        if (!strcmp(argv[i], "--bars"))
            bars = (uint32_t)atoi(argv[i + 1u]);
        else if (!strcmp(argv[i], "--pack"))
            pack = argv[i + 1u];
    for (i = 3u; i < (uint32_t)argc; i++)
        if (!strcmp(argv[i], "--hash"))
            do_hash = 1;
        else if (!strcmp(argv[i], "--f32") && i + 1u < (uint32_t)argc)
            f32path = argv[i + 1u];
    if (load(argv[1], &cs, err, sizeof err) != 0) {
        printf("songplay: %s: %s\n", argv[1], err);
        return 2;
    }
    ri_core_init(&core, 96u, (float)SR, cs.bpm, 0x1Fu);
    printf("songplay: 909 pack: %d voices bound\n", bind_909(&core.session.eng, pack));
    if (ri_core_load_song(&core, &cs) != 0) {
        printf("songplay: automation refused (%u events)\n", (unsigned)cs.nauto);
        return 2;
    }
    if (!bars)
        bars = core.song_bars;
    if (!bars) {
        printf("songplay: the song plays nothing\n");
        return 2;
    }
    frames = (uint64_t)((double)bars * 4.0 * 60.0 / (double)cs.bpm * SR) + 2u * SR;
    if (!(f = fopen(argv[2], "wb"))) {
        printf("songplay: cannot write %s\n", argv[2]);
        return 2;
    }
    fwrite("RIFF", 1, 4, f);
    wr32(f, 36u + (uint32_t)frames * 4u);
    fwrite("WAVEfmt ", 1, 8, f);
    wr32(f, 16u);
    wr16(f, 1u);
    wr16(f, 2u);
    wr32(f, SR);
    wr32(f, SR * 4u);
    wr16(f, 4u);
    wr16(f, 16u);
    fwrite("data", 1, 4, f);
    wr32(f, (uint32_t)frames * 4u);
    for (i = 0u; i < RI_ROUTE_NSECTIONS; i++)
        secpk[i] = 0.0f;
    ri_core_play(&core);
    FILE *ff = 0;
    if (f32path) {
        ff = fopen(f32path, "wb");
        if (!ff) {
            printf("songplay: cannot write %s\n", f32path);
            return 2;
        }
    }
    while (done < frames) {
        uint32_t want = frames - done > CHUNK ? CHUNK : (uint32_t)(frames - done), k;
        const struct RILiveMeters *m;
        ri_live_render(&core.session, L, R, want);
        m = ri_live_meters(&core.session);
        for (k = 0u; k < RI_ROUTE_NSECTIONS; k++)
            if (m->sec_peak[k] > secpk[k])
                secpk[k] = m->sec_peak[k];
        for (k = 0u; k < want; k++) {
            float s[2];
            uint32_t c;
            s[0] = L[k];
            s[1] = R[k];
            if (ff)
                fwrite(s, sizeof(float), 2u, ff);
            if (do_hash) {
                uint32_t u;
                memcpy(&u, &s[0], 4);
                fhash ^= (uint64_t)u;
                fhash *= 1099511628211ULL;
                memcpy(&u, &s[1], 4);
                fhash ^= (uint64_t)u;
                fhash *= 1099511628211ULL;
            }
            for (c = 0u; c < 2u; c++) {
                double a = fabs((double)s[c]);
                long q;
                if (!(a < 1e9))
                    a = 1e9;
                sum += a * a;
                if (a > peak)
                    peak = a;
                if (a >= 1.0)
                    clip++;
                q = (long)lrintf(s[c] * 32767.0f);
                q = q > 32767 ? 32767 : q < -32768 ? -32768 : q;
                wr16(f, (uint32_t)(q & 0xFFFF));
            }
        }
        done += want;
    }
    fclose(f);
    if (ff)
        fclose(ff);
    printf("songplay: %s -> %s: %u bars at %.0f BPM, %.1f s, peak %.3f, rms %.4f, clipped %llu, xruns %u\n",
        argv[1], argv[2], (unsigned)bars, (double)cs.bpm, (double)frames / SR, peak,
        sqrt(sum / (double)(frames * 2u)), (unsigned long long)clip, (unsigned)core.session.xruns);
    printf("songplay: section peaks:");
    for (i = 0u; i < RI_ROUTE_NSECTIONS; i++)
        printf(" %.3f", (double)secpk[i]);
    printf("\n");
    if (do_hash)
        printf("songplay: f32 fnv1a64 %016llx\n", (unsigned long long)fhash);
    return 0;
}
