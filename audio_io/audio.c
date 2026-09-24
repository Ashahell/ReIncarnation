/* audio.c — W1 backend graph API + render task (Task 6, gate G6).
 * Spec §4 (no alloc/lock/IO on the render path), §4.2 (low-level AHI path
 * is the declared direction; ahi.device stays the fallback), §5 (ONE
 * renderer: file export and live drain share au_render_frames, sinks only
 * differ in chunk size), §17 #5 (xruns latched, clock never rewinds).
 *
 * Realtime split: au_render_frames() performs NO allocation, NO file/IO
 * calls, NO unbounded loops (event cursor only advances) — it is the code
 * the future AHI render task will call. AuRenderToFile() and the null
 * drain are NON-realtime file sinks (fopen/fwrite allowed there only).
 *
 * Task 6 scope: one 303 section, one pattern (first-light). Registration
 * allows up to AU_MAX_SOURCES/AU_MAX_BUSES; the render core builds its
 * event list from the lowest CONNECTED source (multi-device fan-out and
 * the mixer land in later tasks). M1.1 MEASURED 2026-09-22: latency
 * reports RI_DEVICE_FRAMES = 64 (low-level ladder floor, Dell + ABIv1);
 * the AROS low-level driver hookup remains a later task — all targets
 * boot the null backend in this task.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "audio_io/audio.h"
#include "engine/engine.h"
#include "engine/seq/clock.h"
#include "engine/seq/sched.h"
#include "engine/dsp/rb303.h"

#define AU_MAX_OBJECTS 4u
#define AU_MAX_SOURCES 4u
#define AU_MAX_BUSES 4u
#define AU_MAX_STEPS 64u
#define AU_TAIL_SMP 48000u /* 1 s release tail after the last event */
#define AU_NAME_LEN 32u

struct AUSource {
    int used;
    struct RIStep steps[AU_MAX_STEPS];
    uint32_t nsteps;
    int tempo;
};

struct AUBus {
    int used;
    char name[AU_NAME_LEN];
};

struct AudioObject {
    int used;
    int started;
    const char *backend; /* "null" (Task 6, all targets) */
    uint32_t xruns;      /* latched underrun count (spec §17 #5) */
    struct AUSource src[AU_MAX_SOURCES];
    struct AUBus bus[AU_MAX_BUSES];
    int connected[AU_MAX_SOURCES]; /* bus index (0-based) or -1 */
    /* Render cursor: the shared engine core (§12.3, spec §5). The event
     * list + voices + cursor that used to live here now live in the
     * engine; this object keeps the graph (sources/buses/connections).
     * ev[] stages emission (per-object: the pool holds several); the
     * engine borrows it, cursor/evpos live in the engine only. */
    struct RIEngine eng;
    struct RIEvent ev[RI_SCHED_MAX_EVENTS];
};

static struct AudioObject s_pool[AU_MAX_OBJECTS];

/* ---- small deterministic helpers (same conventions as tools/render.c) ---- */

static void write_u16(unsigned char *p, uint16_t v) {
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
}

static void write_u16be(unsigned char *p, uint16_t v) {
    p[0] = (unsigned char)((v >> 8) & 0xffu);
    p[1] = (unsigned char)(v & 0xffu);
}

static void write_u32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
}

static void write_u32be(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)((v >> 24) & 0xffu);
    p[1] = (unsigned char)((v >> 16) & 0xffu);
    p[2] = (unsigned char)((v >> 8) & 0xffu);
    p[3] = (unsigned char)(v & 0xffu);
}

static float auf_clampf(float x) {
    if (x > 1.0f)
        return 1.0f;
    if (x < -1.0f)
        return -1.0f;
    return x;
}

/* Deterministic float -> int16 (round-half-away, no libm). */
static int16_t auf_f32_to_s16(float x) {
    float c = auf_clampf(x) * 32767.0f;
    if (c >= 0.0f)
        return (int16_t)(c + 0.5f);
    return (int16_t)(c - 0.5f);
}

/* Parse one "k=v" token. Returns 0 ok, 1 no match, 2 bad value. */
static int auf_take_int(const char *tok, const char *key, int *val) {
    size_t kl = strlen(key);
    long acc = 0;
    int neg = 0;
    const char *p;
    if (strncmp(tok, key, kl) != 0 || tok[kl] != '=')
        return 1;
    p = tok + kl + 1;
    if (*p == '-') {
        neg = 1;
        p++;
    }
    if (*p < '0' || *p > '9')
        return 2;
    while (*p >= '0' && *p <= '9') {
        acc = acc * 10 + (*p - '0');
        p++;
    }
    if (*p != '\0')
        return 2;
    *val = neg ? -(int)acc : (int)acc;
    return 0;
}

/* Parse the Task-4 song scaffold (same grammar as tools/render.c).
 * Returns 0 ok (fills steps/tempo), else nonzero. */
static int auf_parse_song(const char *path, struct RIStep *steps, uint32_t *nsteps, int *tempo) {
    char line[256];
    FILE *f = fopen(path, "r");
    *nsteps = 0;
    *tempo = 0;
    if (!f)
        return 2;
    while (fgets(line, sizeof line, f)) {
        char *tok;
        size_t ln = strlen(line);
        while (ln > 0 && (line[ln - 1] == '\n' || line[ln - 1] == '\r'))
            line[--ln] = '\0';
        if (ln == 0 || line[0] == '#')
            continue;
        if (strncmp(line, "tempo=", 6) == 0) {
            int v;
            if (auf_take_int(line, "tempo", &v) || v < 30 || v > 300) {
                fclose(f);
                return 2;
            }
            *tempo = v;
            continue;
        }
        if (strncmp(line, "step ", 5) == 0) {
            int note = -1, slide = 0, accent = 0, rest = 0;
            int bad = 0;
            if (*nsteps >= AU_MAX_STEPS) {
                fclose(f);
                return 2;
            }
            tok = line + 5;
            for (;;) {
                char *sp, *word;
                int v, r;
                while (*tok == ' ' || *tok == '\t')
                    tok++;
                if (*tok == '\0')
                    break;
                word = tok;
                sp = tok;
                while (*sp != ' ' && *sp != '\t' && *sp != '\0')
                    sp++;
                if (*sp != '\0') {
                    *sp = '\0';
                    tok = sp + 1;
                } else {
                    tok = sp;
                }
                if ((r = auf_take_int(word, "note", &v)) == 0) {
                    if (v < 0 || v > 127)
                        bad = 1;
                    note = v;
                } else if (r == 2) {
                    bad = 1;
                } else if ((r = auf_take_int(word, "slide", &v)) == 0) {
                    if (v != 0 && v != 1)
                        bad = 1;
                    slide = v;
                } else if (r == 2) {
                    bad = 1;
                } else if ((r = auf_take_int(word, "accent", &v)) == 0) {
                    if (v != 0 && v != 1)
                        bad = 1;
                    accent = v;
                } else if (r == 2) {
                    bad = 1;
                } else if ((r = auf_take_int(word, "rest", &v)) == 0) {
                    if (v != 0 && v != 1)
                        bad = 1;
                    rest = v;
                } else {
                    bad = 1;
                }
            }
            if (bad || (rest == 0 && note < 0) || (rest == 1 && note >= 0)) {
                fclose(f);
                return 2;
            }
            steps[*nsteps].note = (uint8_t)(note < 0 ? 0 : note);
            steps[*nsteps].flags = (uint8_t)((rest ? RI_STEP_REST : 0u) |
                (slide ? RI_STEP_SLIDE : 0u) | (accent ? RI_STEP_ACCENT : 0u));
            (*nsteps)++;
            continue;
        }
        fclose(f);
        return 2;
    }
    fclose(f);
    if (*tempo == 0 || *nsteps == 0)
        return 2;
    return 0;
}

/* First-light knob defaults (Task 4 report: cutoff 80 / reso 40 / envmod 64
 * / decay 64 / accent 96 / saw / vol 127). */
/* First-light knob defaults live in the engine core (ri_engine_defaults);
 * this TU keeps no voice-default or event-routing copy (one source, §12.3:
 * ri_engine_apply_event replaces auf_apply_event for all three paths). */

/* Lowest connected source index, or -1 when the graph cannot render. */
static int auf_render_source(struct AudioObject *ao) {
    uint32_t i;
    for (i = 0; i < AU_MAX_SOURCES; i++) {
        if (ao->src[i].used && ao->connected[i] >= 0)
            return (int)i;
    }
    return -1;
}

/* ---- brief-exact API ---- */

struct AudioObject *AuCreateObject(struct Library *AudioBase, struct TagItem *tags) {
    uint32_t i, k;
    (void)AudioBase;
    (void)tags; /* reserved for the AROS AHI base + create tags */
    for (i = 0; i < AU_MAX_OBJECTS; i++) {
        if (!s_pool[i].used) {
            s_pool[i].used = 1;
            s_pool[i].started = 0;
            s_pool[i].backend = "null";
            s_pool[i].xruns = 0;
            ri_engine_init(&s_pool[i].eng);
            for (k = 0; k < AU_MAX_SOURCES; k++) {
                s_pool[i].src[k].used = 0;
                s_pool[i].src[k].nsteps = 0;
                s_pool[i].src[k].tempo = 0;
                s_pool[i].connected[k] = -1;
            }
            for (k = 0; k < AU_MAX_BUSES; k++) {
                s_pool[i].bus[k].used = 0;
                s_pool[i].bus[k].name[0] = '\0';
            }
            return &s_pool[i];
        }
    }
    return NULL;
}

uint32_t AuAddSource(struct AudioObject *ao, struct TagItem *tags) {
    const char *song_path = NULL;
    uint32_t i;
    if (!ao)
        return 0;
    if (tags) {
        for (i = 0; tags[i].ti_Tag != 0; i++) {
            if (tags[i].ti_Tag == AUTA_SongPath)
                song_path = (const char *)tags[i].ti_Data;
        }
    }
    for (i = 0; i < AU_MAX_SOURCES; i++) {
        if (!ao->src[i].used) {
            ao->src[i].used = 1;
            ao->src[i].nsteps = 0;
            ao->src[i].tempo = 0;
            if (song_path) {
                if (auf_parse_song(song_path, ao->src[i].steps,
                        &ao->src[i].nsteps, &ao->src[i].tempo) != 0) {
                    ao->src[i].used = 0;
                    return 0;
                }
            }
            return i + 1u; /* 1-based id; 0 = error */
        }
    }
    return 0;
}

uint32_t AuAddBus(struct AudioObject *ao, const char *name, struct TagItem *tags) {
    uint32_t i;
    (void)tags; /* reserved */
    if (!ao || !name)
        return 0;
    for (i = 0; i < AU_MAX_BUSES; i++) {
        if (!ao->bus[i].used) {
            size_t k;
            ao->bus[i].used = 1;
            for (k = 0; k + 1 < AU_NAME_LEN && name[k]; k++)
                ao->bus[i].name[k] = name[k];
            ao->bus[i].name[k] = '\0';
            return i + 1u; /* 1-based id; 0 = error */
        }
    }
    return 0;
}

int AuConnect(struct AudioObject *ao, uint32_t src, uint32_t bus) {
    if (!ao || src < 1u || src > AU_MAX_SOURCES || bus < 1u || bus > AU_MAX_BUSES)
        return 2;
    if (!ao->src[src - 1u].used || !ao->bus[bus - 1u].used)
        return 2;
    ao->connected[src - 1u] = (int)(bus - 1u);
    return 0;
}

int AuStart(struct AudioObject *ao) {
    if (!ao)
        return 2;
    /* Task 6: null backend on all targets (M1.1 numbers pending). The AROS
     * low-level hookup (AllocAudioA + PlayerFunc Signal, §4.2) lands once
     * the reference box is measured. */
    ao->backend = "null";
    ao->started = 1;
    printf("%s\n", RI_AUDIO_NULL_MSG);
    au_rewind(ao);
    return 0;
}

void AuStop(struct AudioObject *ao) {
    if (ao)
        ao->started = 0;
}

uint32_t AuQueryAttr(struct AudioObject *ao, uint32_t attr) {
    if (!ao)
        return 0;
    switch (attr) {
    case AUQA_LatencyFrames:
        return RI_DEVICE_FRAMES; /* M1.1-measured floor (64) */
    case AUQA_XRUN_COUNT:
        return ao->xruns;
    default:
        return 0;
    }
}

/* §12.3: the shared engine core is wired (spec §5). au_render_frames is a
 * thin mono sink over ri_engine_render_mono now: file export and the live
 * device drain run the same core with different chunkings (64-frame engine
 * blocks vs RI_DEVICE_FRAMES device chunks). The old RI_LIVE_FULL_GRAPH_*
 * marker is retired by this wiring; full-graph sections (808/909/FX/mixer)
 * enable inside the engine as their slices land. */
 /* The ONE renderer (realtime-safe: no alloc, no IO, bounded loops).
 * Renders up to n samples from the cursor, applying walker events at exact
 * sample positions. Chunk-size agnostic: splitting n into a+b renders
 * sample-identical output (voice state is purely sequential). Returns the
 * frames actually rendered (0 at end of song). */
uint32_t au_render_frames(struct AudioObject *ao, float *out, uint32_t n) {
    if (!ao || !out)
        return 0;
    return ri_engine_render_mono(&ao->eng, out, n, (float)RI_AUDIO_SR);
}

/* Reset the voice + event cursor from the lowest connected source. */
void au_rewind(struct AudioObject *ao) {
    static struct RISegment segs[1];
    struct RITempoMap map;
    uint32_t nev = 0;
    uint64_t total = 0;
    int si;
    if (!ao)
        return;
    ri_engine_init(&ao->eng);
    ri_engine_defaults(&ao->eng);
    si = auf_render_source(ao);
    if (si < 0 || ao->src[(uint32_t)si].nsteps == 0) {
        ri_engine_load(&ao->eng, 0, 0, 0, RI_ENGINE_S303A);
        return;
    }
    segs[0].start_tick = 0;
    segs[0].ns_per_quarter = 60000000000ULL / (uint64_t)ao->src[(uint32_t)si].tempo;
    map.segs = segs;
    map.n = 1;
    map.ppq = 96;
    map.sr = RI_AUDIO_SR;
    nev = ri_sched_emit_sorted(&map, 0, 96, ao->src[(uint32_t)si].steps,
        ao->src[(uint32_t)si].nsteps, 0, ao->ev, RI_SCHED_MAX_EVENTS);
    if (nev != 0)
        total = ao->ev[nev - 1u].sample + AU_TAIL_SMP;
    ri_engine_load(&ao->eng, ao->ev, nev, total, RI_ENGINE_S303A);
}

/* Deterministic float -> int24 (round-half-away, no libm). */
int32_t auf_f32_to_s24(float x) {
    float c = auf_clampf(x) * 8388607.0f;
    if (c >= 0.0f)
        return (int32_t)(c + 0.5f);
    return (int32_t)(c - 0.5f);
}

/* 80-bit extended sample rates (ffmpeg-verified byte patterns;
 * only the two engine rates exist — anything else fails closed). */
static void auf_aiff_ext(uint8_t out[10], uint32_t sr) {
    static const uint8_t e44100[10] =
        { 0x40, 0x0e, 0xac, 0x44, 0, 0, 0, 0, 0, 0 };
    static const uint8_t e48000[10] =
        { 0x40, 0x0e, 0xbb, 0x80, 0, 0, 0, 0, 0, 0 };
    const uint8_t *src = (sr == 44100u) ? e44100 : e48000;
    uint32_t i;
    for (i = 0; i < 10; i++)
        out[i] = src[i];
}

/* Plain-AIFF 54-byte header builder (FORM + COMM + SSND head). */
int auf_aiff_header(unsigned char hdr[54], uint32_t frames, uint8_t depth,
    uint32_t sr) {
    uint32_t bps, bytes, i;
    uint8_t ext[10];
    if (depth != 16u && depth != 24u)
        return 2;
    if (sr != 44100u && sr != 48000u)
        return 2;
    bps = depth / 8u;
    bytes = frames * bps;
    for (i = 0; i < 54; i++)
        hdr[i] = 0;
    memcpy(hdr, "FORM", 4);
    /* FORM size = 4 (AIFF) + COMM chunk (8 + 18) + SSND chunk
     * (8 + 8 + data). */
    write_u32be(hdr + 4, 4u + 26u + 16u + bytes);
    memcpy(hdr + 8, "AIFF", 4);
    memcpy(hdr + 12, "COMM", 4);
    write_u32be(hdr + 16, 18u);
    write_u16be(hdr + 20, 1u); /* mono */
    write_u32be(hdr + 22, frames);
    write_u16be(hdr + 26, (uint16_t)depth);
    auf_aiff_ext(ext, sr);
    for (i = 0; i < 10; i++)
        hdr[28 + i] = ext[i];
    memcpy(hdr + 38, "SSND", 4);
    write_u32be(hdr + 42, 8u + bytes);
    /* SSND offset (46) + blocksize (50) stay zeroed. */
    return 0;
}

/* Canonical 44-byte PCM header builder (shared by both file sinks). */
int auf_wav_header(unsigned char hdr[44], uint32_t total, uint8_t depth,
    uint32_t sr) {
    uint32_t bps, bytes;
    if (depth != 16u && depth != 24u)
        return 2;
    if (sr == 0u)
        return 2;
    bps = depth / 8u;
    bytes = total * bps;
    memset(hdr, 0, 44);
    memcpy(hdr, "RIFF", 4);
    write_u32(hdr + 4, 36u + bytes);
    memcpy(hdr + 8, "WAVEfmt ", 8);
    write_u32(hdr + 16, 16u);
    write_u16(hdr + 20, 1u);
    write_u16(hdr + 22, 1u);
    write_u32(hdr + 24, sr);
    write_u32(hdr + 28, sr * bps);
    write_u16(hdr + 32, (uint16_t)bps);
    write_u16(hdr + 34, depth);
    memcpy(hdr + 36, "data", 4);
    write_u32(hdr + 40, bytes);
    return 0;
}



/* Shared file-sink writer: rewind, pump the core in caller-chosen chunks,
 * convert, stream to disk (no multi-megabyte buffer — §17 #6 memory
 * pressure). AuRenderToFile uses RI_AUDIO_BLOCK engine blocks; the null
 * (live-stub) drain uses RI_DEVICE_FRAMES device chunks. Same core. */
int au_render_song_to_wav(struct AudioObject *ao, const char *path,
    uint32_t chunk, uint64_t cap_total) {
    return au_render_song_to_wav_depth(ao, path, chunk, cap_total, 16u);
}

int au_render_song_to_wav_depth(struct AudioObject *ao, const char *path,
    uint32_t chunk, uint64_t cap_total, uint8_t depth) {
    static float fbuf[RI_DEVICE_FRAMES];
    FILE *f;
    uint64_t left, total;
    unsigned char hdr[44];
    if (!ao || !path || chunk == 0 || chunk > RI_DEVICE_FRAMES)
        return 2;
    if (depth != 16u && depth != 24u)
        return 2;
    if (auf_render_source(ao) < 0)
        return 2;
    au_rewind(ao);
    if (ao->eng.nev == 0)
        return 2;
    total = ao->eng.total;
    if (cap_total > 0)
        total = cap_total; /* preview cap (may shorten OR pad — same core) */
    if (total > 0xffffffffu)
        return 2;
    f = fopen(path, "wb");
    if (!f)
        return 2;
    if (auf_wav_header(hdr, (uint32_t)total, depth, RI_AUDIO_SR) != 0) {
        fclose(f);
        return 2;
    }
    if (fwrite(hdr, 1, 44, f) != 44) {
        fclose(f);
        return 2;
    }
    left = total;
    while (left > 0) {
        uint32_t want = left > chunk ? chunk : (uint32_t)left;
        uint32_t got, k;
        got = au_render_frames(ao, fbuf, want);
        if (got == 0) {
            /* Song ended early: pad silence (clock never rewinds, §17 #5).
             * No cursor bookkeeping: the engine already sits at its end. */
            for (k = 0; k < want; k++)
                fbuf[k] = 0.0f;
            got = want;
        }
        for (k = 0; k < got; k++) {
            if (depth == 24u) {
                int32_t s = auf_f32_to_s24(fbuf[k]);
                unsigned char b[3];
                b[0] = (unsigned char)(s & 0xff);
                b[1] = (unsigned char)((s >> 8) & 0xff);
                b[2] = (unsigned char)((s >> 16) & 0xff);
                if (fwrite(b, 1, 3, f) != 3) {
                    fclose(f);
                    return 2;
                }
            } else {
                int16_t s = auf_f32_to_s16(fbuf[k]);
                unsigned char b[2];
                b[0] = (unsigned char)(s & 0xff);
                b[1] = (unsigned char)((s >> 8) & 0xff);
                if (fwrite(b, 1, 2, f) != 2) {
                    fclose(f);
                    return 2;
                }
            }
        }
        left -= got;
    }
    fclose(f);
    return 0;
}

int AuRenderToFileDepth(struct AudioObject *ao, const char *path,
    uint32_t ms, uint8_t depth) {
    uint64_t cap = 0;
    if (!ao || !path || !ao->started)
        return 2;
    if (depth != 16u && depth != 24u)
        return 2;
    if (ms > 0) {
        cap = (uint64_t)ms * (RI_AUDIO_SR / 1000u);
        if (cap == 0)
            return 2;
    }
    return au_render_song_to_wav_depth(ao, path, RI_AUDIO_BLOCK, cap, depth);
}

/* Plain-AIFF sink: same core/loop as the WAV sink, big-endian PCM
 * through the shared packers. au_render_song_to_aiff is the depth
 * entry point (16|24 else rc 2); the 54-byte header comes from
 * auf_aiff_header (sr fixed at RI_AUDIO_SR — live rate threading
 * is its own slice). */
int au_render_song_to_aiff(struct AudioObject *ao, const char *path,
    uint32_t chunk, uint64_t cap_total, uint8_t depth) {
    static float fbuf[RI_DEVICE_FRAMES];
    FILE *f;
    uint64_t left, total;
    unsigned char hdr[54];
    if (!ao || !path || chunk == 0 || chunk > RI_DEVICE_FRAMES)
        return 2;
    if (depth != 16u && depth != 24u)
        return 2;
    if (auf_render_source(ao) < 0)
        return 2;
    au_rewind(ao);
    if (ao->eng.nev == 0)
        return 2;
    total = ao->eng.total;
    if (cap_total > 0)
        total = cap_total;
    if (total > 0xffffffffu)
        return 2;
    f = fopen(path, "wb");
    if (!f)
        return 2;
    if (auf_aiff_header(hdr, (uint32_t)total, depth, RI_AUDIO_SR) != 0) {
        fclose(f);
        return 2;
    }
    if (fwrite(hdr, 1, 54, f) != 54) {
        fclose(f);
        return 2;
    }
    left = total;
    while (left > 0) {
        uint32_t want = left > chunk ? chunk : (uint32_t)left;
        uint32_t got, k;
        got = au_render_frames(ao, fbuf, want);
        if (got == 0) {
            for (k = 0; k < want; k++)
                fbuf[k] = 0.0f;
            got = want;
        }
        for (k = 0; k < got; k++) {
            if (depth == 24u) {
                int32_t s = auf_f32_to_s24(fbuf[k]);
                unsigned char b[3];
                b[0] = (unsigned char)((s >> 16) & 0xff);
                b[1] = (unsigned char)((s >> 8) & 0xff);
                b[2] = (unsigned char)(s & 0xff);
                if (fwrite(b, 1, 3, f) != 3) {
                    fclose(f);
                    return 2;
                }
            } else {
                int16_t s = auf_f32_to_s16(fbuf[k]);
                unsigned char b[2];
                b[0] = (unsigned char)((s >> 8) & 0xff);
                b[1] = (unsigned char)(s & 0xff);
                if (fwrite(b, 1, 2, f) != 2) {
                    fclose(f);
                    return 2;
                }
            }
        }
        left -= got;
    }
    fclose(f);
    return 0;
}

int AuRenderToFileAiff(struct AudioObject *ao, const char *path,
    uint32_t ms, uint8_t depth) {
    uint64_t cap = 0;
    if (!ao || !path || !ao->started)
        return 2;
    if (depth != 16u && depth != 24u)
        return 2;
    if (ms > 0) {
        cap = (uint64_t)ms * (RI_AUDIO_SR / 1000u);
        if (cap == 0)
            return 2;
    }
    return au_render_song_to_aiff(ao, path, RI_AUDIO_BLOCK, cap, depth);
}

int AuRenderToFile(struct AudioObject *ao, const char *path, uint32_t ms) {
    uint64_t cap = 0;
    if (!ao || !path || !ao->started)
        return 2;
    if (ms > 0) {
        cap = (uint64_t)ms * (RI_AUDIO_SR / 1000u);
        if (cap == 0)
            return 2;
    }
    return au_render_song_to_wav(ao, path, RI_AUDIO_BLOCK, cap);
}

const char *AuBackendName(struct AudioObject *ao) {
    if (!ao)
        return NULL;
    return ao->backend;
}

int AuDumpEvents(struct AudioObject *ao, const char *path) {
    FILE *f;
    uint32_t i;
    if (!ao || !path || ao->eng.nev == 0)
        return 2;
    f = fopen(path, "w");
    if (!f)
        return 2;
    for (i = 0; i < ao->eng.nev; i++)
        fprintf(f, "%llu %u %u %u %u %u %u\n", (unsigned long long)ao->ev[i].sample,
            ao->ev[i].type, ao->ev[i].device, ao->ev[i].voice,
            ao->ev[i].value, ao->ev[i].flags, ao->ev[i].seq);
    fclose(f);
    return 0;
}
