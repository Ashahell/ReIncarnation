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
 * the mixer land in later tasks). M1.1 UNMEASURED: latency reports the
 * RI_DEVICE_FRAMES hypothesis default (256); the AROS low-level driver
 * hookup lands once M1.1 numbers exist — all targets boot the null
 * backend in this task.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "audio_io/audio.h"
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
    /* Render cursor built from the lowest connected source. */
    struct RB303Voice voice;
    struct RIEvent ev[RI_SCHED_MAX_EVENTS];
    uint32_t nev;
    uint32_t evpos;
    uint64_t cursor;
    uint64_t total;
};

static struct AudioObject s_pool[AU_MAX_OBJECTS];

/* ---- small deterministic helpers (same conventions as tools/render.c) ---- */

static void write_u16(unsigned char *p, uint16_t v) {
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
}

static void write_u32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
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
static void auf_voice_defaults(struct RB303Voice *v) {
    rb303_init(v);
    rb303_set_param(v, RI_CTL_303A_CUTOFF, 80);
    rb303_set_param(v, RI_CTL_303A_RESO, 40);
    rb303_set_param(v, RI_CTL_303A_ENVMOD, 64);
    rb303_set_param(v, RI_CTL_303A_DECAY, 64);
    rb303_set_param(v, RI_CTL_303A_ACCENT, 96);
    rb303_set_param(v, RI_CTL_303A_WAVE, 0);
    rb303_set_param(v, RI_CTL_303A_VOLUME, 127);
}

static void auf_apply_event(struct RB303Voice *v, const struct RIEvent *e) {
    switch (e->type) {
    case RI_EV_NOTE_ON:
        rb303_note(v, (uint8_t)(e->value & 127u), (e->flags & RI_EVFLAG_SLIDE) != 0,
            (e->flags & RI_EVFLAG_ACCENT) != 0);
        break;
    case RI_EV_NOTE_CONTINUE:
        rb303_slide_to(v, (uint8_t)(e->value & 127u));
        break;
    case RI_EV_NOTE_OFF:
        rb303_release(v);
        break;
    case RI_EV_ACCENT:
        rb303_accent(v);
        break;
    default:
        break;
    }
}

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
            s_pool[i].nev = 0;
            s_pool[i].evpos = 0;
            s_pool[i].cursor = 0;
            s_pool[i].total = 0;
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
        return RI_DEVICE_FRAMES; /* M1.1 hypothesis default */
    case AUQA_XRUN_COUNT:
        return ao->xruns;
    default:
        return 0;
    }
}

/* The ONE renderer (realtime-safe: no alloc, no IO, bounded loops).
 * Renders up to n samples from the cursor, applying walker events at exact
 * sample positions. Chunk-size agnostic: splitting n into a+b renders
 * sample-identical output (voice state is purely sequential). Returns the
 * frames actually rendered (0 at end of song). */
uint32_t au_render_frames(struct AudioObject *ao, float *out, uint32_t n) {
    uint32_t done = 0;
    if (!ao || !out)
        return 0;
    while (n > 0 && ao->cursor < ao->total) {
        uint64_t next = ao->total;
        uint64_t run;
        if (ao->evpos < ao->nev && ao->ev[ao->evpos].sample < next)
            next = ao->ev[ao->evpos].sample;
        if (next > ao->cursor + n)
            next = ao->cursor + n;
        run = next - ao->cursor;
        if (run > 0) {
            /* rb303_render is purely sequential in voice state, so any
             * caller chunking renders sample-identical output (D1). The
             * file sink pumps 64-frame engine blocks, the null drain
             * pumps RI_DEVICE_FRAMES device chunks — same core. */
            uint32_t cc = (uint32_t)run;
            rb303_render(&ao->voice, out + done, cc, (float)RI_AUDIO_SR);
            done += cc;
            ao->cursor += cc;
            n -= cc;
        } else {
            while (ao->evpos < ao->nev && ao->ev[ao->evpos].sample == ao->cursor) {
                auf_apply_event(&ao->voice, &ao->ev[ao->evpos]);
                ao->evpos++;
            }
        }
    }
    return done;
}

/* Reset the voice + event cursor from the lowest connected source. */
void au_rewind(struct AudioObject *ao) {
    static struct RISegment segs[1];
    struct RITempoMap map;
    int si;
    if (!ao)
        return;
    ao->nev = 0;
    ao->evpos = 0;
    ao->cursor = 0;
    ao->total = 0;
    auf_voice_defaults(&ao->voice);
    si = auf_render_source(ao);
    if (si < 0 || ao->src[(uint32_t)si].nsteps == 0)
        return;
    segs[0].start_tick = 0;
    segs[0].ns_per_quarter = 60000000000ULL / (uint64_t)ao->src[(uint32_t)si].tempo;
    map.segs = segs;
    map.n = 1;
    map.ppq = 96;
    map.sr = RI_AUDIO_SR;
    ao->nev = ri_sched_emit_sorted(&map, 0, 96, ao->src[(uint32_t)si].steps,
        ao->src[(uint32_t)si].nsteps, 0, ao->ev, RI_SCHED_MAX_EVENTS);
    if (ao->nev == 0)
        return;
    ao->total = ao->ev[ao->nev - 1u].sample + AU_TAIL_SMP;
}

static int auf_write_wav_header(FILE *f, uint32_t total) {
    unsigned char hdr[44];
    uint32_t bytes = total * 2u;
    memset(hdr, 0, sizeof hdr);
    memcpy(hdr, "RIFF", 4);
    write_u32(hdr + 4, 36u + bytes);
    memcpy(hdr + 8, "WAVEfmt ", 8);
    write_u32(hdr + 16, 16u);
    write_u16(hdr + 20, 1u);
    write_u16(hdr + 22, 1u);
    write_u32(hdr + 24, RI_AUDIO_SR);
    write_u32(hdr + 28, RI_AUDIO_SR * 2u);
    write_u16(hdr + 32, 2u);
    write_u16(hdr + 34, 16u);
    memcpy(hdr + 36, "data", 4);
    write_u32(hdr + 40, bytes);
    if (fwrite(hdr, 1, 44, f) != 44)
        return 2;
    return 0;
}

/* Shared file-sink writer: rewind, pump the core in caller-chosen chunks,
 * convert, stream to disk (no multi-megabyte buffer — §17 #6 memory
 * pressure). AuRenderToFile uses RI_AUDIO_BLOCK engine blocks; the null
 * (live-stub) drain uses RI_DEVICE_FRAMES device chunks. Same core. */
int au_render_song_to_wav(struct AudioObject *ao, const char *path,
    uint32_t chunk, uint64_t cap_total) {
    static float fbuf[RI_DEVICE_FRAMES];
    FILE *f;
    uint64_t left;
    if (!ao || !path || chunk == 0 || chunk > RI_DEVICE_FRAMES)
        return 2;
    if (auf_render_source(ao) < 0)
        return 2;
    au_rewind(ao);
    if (ao->nev == 0)
        return 2;
    if (cap_total > 0)
        ao->total = cap_total; /* preview cap (may shorten OR pad — same core) */
    if (ao->total > 0xffffffffu)
        return 2;
    f = fopen(path, "wb");
    if (!f)
        return 2;
    if (auf_write_wav_header(f, (uint32_t)ao->total) != 0) {
        fclose(f);
        return 2;
    }
    left = ao->total;
    while (left > 0) {
        uint32_t want = left > chunk ? chunk : (uint32_t)left;
        uint32_t got, k;
        got = au_render_frames(ao, fbuf, want);
        if (got == 0) {
            /* Song ended early: pad silence (clock never rewinds, §17 #5). */
            for (k = 0; k < want; k++)
                fbuf[k] = 0.0f;
            got = want;
            ao->cursor += want;
        }
        for (k = 0; k < got; k++) {
            int16_t s = auf_f32_to_s16(fbuf[k]);
            unsigned char b[2];
            b[0] = (unsigned char)(s & 0xff);
            b[1] = (unsigned char)((s >> 8) & 0xff);
            if (fwrite(b, 1, 2, f) != 2) {
                fclose(f);
                return 2;
            }
        }
        left -= got;
    }
    fclose(f);
    return 0;
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
    if (!ao || !path || ao->nev == 0)
        return 2;
    f = fopen(path, "w");
    if (!f)
        return 2;
    for (i = 0; i < ao->nev; i++)
        fprintf(f, "%llu %u %u %u %u %u %u\n", (unsigned long long)ao->ev[i].sample,
            ao->ev[i].type, ao->ev[i].device, ao->ev[i].voice,
            ao->ev[i].value, ao->ev[i].flags, ao->ev[i].seq);
    fclose(f);
    return 0;
}
