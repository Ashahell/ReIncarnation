/* tools/render — offline renderer CLI (Task 4, gate G4).
 * One-renderer path (spec §0.1): scheduler -> 303 voice -> master bus ->
 * WAV file, deterministically (D1). Everything later modules add must be
 * exercisable from this command.
 *
 * NOTE: this golden proves determinism + skeleton, NOT parity. The 303
 * filter candidate is UNVALIDATED (docs/evidence/303/filter-candidate.md).
 *
 * usage:
 *   render --song FILE --out FILE [--dump-events FILE]
 *   render --math dc|sine --out FILE
 *
 * --song: Task-4 text scaffold (NOT RBNG; real codec is Task 13):
 *     tempo=140
 *     step note=45 [slide=0/1] [accent=0/1]
 *     step rest=1 [slide=0/1]
 *   max 64 steps. Unknown lines/values: fatal (exit 2), never silent.
 * --math dc: 2 s of DC 0.5 through the core ladder (fc=1000, k=0).
 * --math sine: 2 s of 1 kHz / 0.1 sine through the core ladder (fc=8k, k=0).
 * Output: 16-bit mono WAV, 48 kHz. Engine block 64 frames (spec §2.3).
 * Exit: 0 ok, 2 usage/IO/parse error.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "engine/seq/clock.h"
#include "engine/seq/sched.h"
#include "engine/dsp/rb303.h"
#include "engine/dsp/kernels.h"

#define RI_SR 48000u
#define RI_BLOCK 64u
#define RI_MAX_STEPS 64u
#define RI_TAIL_SMP 48000u /* 1 s release tail after the last event */

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

/* Returns 0 on success. out_smp must hold total samples (int16). */
static int write_wav(const char *path, const int16_t *pcm, uint32_t total) {
    unsigned char hdr[44];
    uint32_t bytes = total * 2u;
    FILE *f = fopen(path, "wb");
    if (!f) {
        printf("render: cannot open --out %s\n", path);
        return 2;
    }
    memset(hdr, 0, sizeof hdr);
    memcpy(hdr, "RIFF", 4);
    write_u32(hdr + 4, 36u + bytes);
    memcpy(hdr + 8, "WAVEfmt ", 8);
    write_u32(hdr + 16, 16u);
    write_u16(hdr + 20, 1u);
    write_u16(hdr + 22, 1u);
    write_u32(hdr + 24, RI_SR);
    write_u32(hdr + 28, RI_SR * 2u);
    write_u16(hdr + 32, 2u);
    write_u16(hdr + 34, 16u);
    memcpy(hdr + 36, "data", 4);
    write_u32(hdr + 40, bytes);
    if (fwrite(hdr, 1, 44, f) != 44 || fwrite(pcm, 2, total, f) != total) {
        printf("render: write failed %s\n", path);
        fclose(f);
        return 2;
    }
    fclose(f);
    return 0;
}

static float clampf(float x) {
    if (x > 1.0f)
        return 1.0f;
    if (x < -1.0f)
        return -1.0f;
    return x;
}

/* Deterministic float -> int16 (round-half-away, no libm). */
static int16_t f32_to_s16(float x) {
    float c = clampf(x) * 32767.0f;
    if (c >= 0.0f)
        return (int16_t)(c + 0.5f);
    return (int16_t)(c - 0.5f);
}

/* Parse one "k=v" token. Returns 0 ok, 1 no match, 2 bad value. */
static int take_int(const char *tok, const char *key, int *val) {
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

/* Parse the Task-4 song scaffold. Returns 0 ok (fills steps/tempo), else 2. */
static int parse_song(const char *path, struct RIStep *steps, uint32_t *nsteps, int *tempo) {
    char line[256];
    FILE *f = fopen(path, "r");
    *nsteps = 0;
    *tempo = 0;
    if (!f) {
        printf("render: cannot open --song %s\n", path);
        return 2;
    }
    while (fgets(line, sizeof line, f)) {
        char *tok;
        size_t ln = strlen(line);
        while (ln > 0 && (line[ln - 1] == '\n' || line[ln - 1] == '\r'))
            line[--ln] = '\0';
        if (ln == 0 || line[0] == '#')
            continue;
        if (strncmp(line, "tempo=", 6) == 0) {
            int v;
            if (take_int(line, "tempo", &v) || v < 30 || v > 300) {
                printf("render: bad tempo line: %s\n", line);
                fclose(f);
                return 2;
            }
            *tempo = v;
            continue;
        }
        if (strncmp(line, "step ", 5) == 0) {
            int note = -1, slide = 0, accent = 0, rest = 0;
            int bad = 0;
            if (*nsteps >= RI_MAX_STEPS) {
                printf("render: too many steps (max %u)\n", RI_MAX_STEPS);
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
                if ((r = take_int(word, "note", &v)) == 0) {
                    if (v < 0 || v > 127)
                        bad = 1;
                    note = v;
                } else if (r == 2) {
                    bad = 1;
                } else if ((r = take_int(word, "slide", &v)) == 0) {
                    if (v != 0 && v != 1)
                        bad = 1;
                    slide = v;
                } else if (r == 2) {
                    bad = 1;
                } else if ((r = take_int(word, "accent", &v)) == 0) {
                    if (v != 0 && v != 1)
                        bad = 1;
                    accent = v;
                } else if (r == 2) {
                    bad = 1;
                } else if ((r = take_int(word, "rest", &v)) == 0) {
                    if (v != 0 && v != 1)
                        bad = 1;
                    rest = v;
                } else {
                    bad = 1;
                }
            }
            if (bad || (rest == 0 && note < 0) || (rest == 1 && note >= 0)) {
                printf("render: bad step line: %s\n", line);
                fclose(f);
                return 2;
            }
            steps[*nsteps].note = (uint8_t)(note < 0 ? 0 : note);
            steps[*nsteps].flags = (uint8_t)((rest ? RI_STEP_REST : 0u) |
                (slide ? RI_STEP_SLIDE : 0u) | (accent ? RI_STEP_ACCENT : 0u));
            (*nsteps)++;
            continue;
        }
        printf("render: unknown line: %s\n", line);
        fclose(f);
        return 2;
    }
    fclose(f);
    if (*tempo == 0 || *nsteps == 0) {
        printf("render: song needs tempo= and >= 1 step\n");
        return 2;
    }
    return 0;
}

static int dump_events(const char *path, const struct RIEvent *ev, uint32_t n) {
    FILE *f = fopen(path, "w");
    uint32_t i;
    if (!f) {
        printf("render: cannot open --dump-events %s\n", path);
        return 2;
    }
    for (i = 0; i < n; i++)
        fprintf(f, "%llu %u %u %u %u %u %u\n", (unsigned long long)ev[i].sample,
            ev[i].type, ev[i].device, ev[i].voice, ev[i].value, ev[i].flags, ev[i].seq);
    fclose(f);
    return 0;
}

/* Apply one walker event to the voice. */
static void apply_event(struct RB303Voice *v, const struct RIEvent *e) {
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
        break; /* walker emits only the above for one 303 section */
    }
}

static int render_song(const char *song_path, const char *out_path, const char *ev_path) {
    struct RIStep steps[RI_MAX_STEPS];
    struct RIEvent ev[RI_SCHED_MAX_EVENTS];
    static struct RISegment segs[1];
    struct RITempoMap map;
    struct RB303Voice voice;
    uint32_t nsteps = 0;
    int tempo = 0;
    uint32_t nev, k;
    uint64_t total, cursor = 0, evpos = 0;
    /* pcm worst case: 64 steps * full bar @30bpm + tail; static bound 2^22 */
    static int16_t pcm[4194304];
    static float fbuf[RI_BLOCK];
    int rc;
    if ((rc = parse_song(song_path, steps, &nsteps, &tempo)) != 0)
        return rc;
    segs[0].start_tick = 0;
    segs[0].ns_per_quarter = 60000000000ULL / (uint64_t)tempo;
    map.segs = segs;
    map.n = 1;
    map.ppq = 96;
    map.sr = RI_SR;
    nev = ri_sched_emit_sorted(&map, 0, 96, steps, nsteps, 0, ev, RI_SCHED_MAX_EVENTS);
    if (nev == 0) {
        printf("render: walker emitted no events\n");
        return 2;
    }
    total = ev[nev - 1].sample + RI_TAIL_SMP;
    if (total > 4194304u) {
        printf("render: song too long (%llu samples)\n", (unsigned long long)total);
        return 2;
    }
    rb303_init(&voice);
    /* first-light knob defaults (executor choice, recorded in report) */
    rb303_set_param(&voice, RI_CTL_303A_CUTOFF, 80);
    rb303_set_param(&voice, RI_CTL_303A_RESO, 40);
    rb303_set_param(&voice, RI_CTL_303A_ENVMOD, 64);
    rb303_set_param(&voice, RI_CTL_303A_DECAY, 64);
    rb303_set_param(&voice, RI_CTL_303A_ACCENT, 96);
    rb303_set_param(&voice, RI_CTL_303A_WAVE, 0);
    rb303_set_param(&voice, RI_CTL_303A_VOLUME, 127);
    /* event-boundary + 64-frame block render loop (exact event timing) */
    while (cursor < total) {
        uint64_t blk = ((cursor + RI_BLOCK) / RI_BLOCK) * RI_BLOCK;
        uint64_t next = total;
        uint64_t c;
        if (blk < next)
            next = blk;
        if (evpos < nev && ev[evpos].sample < next)
            next = ev[evpos].sample;
        if (next == cursor) {
            /* event(s) exactly at cursor: apply, no audio to render */
            while (evpos < nev && ev[evpos].sample == cursor) {
                apply_event(&voice, &ev[evpos]);
                evpos++;
            }
            continue;
        }
        c = cursor;
        while (c < next) {
            uint32_t cc = (uint32_t)(next - c);
            if (cc > RI_BLOCK)
                cc = RI_BLOCK;
            rb303_render(&voice, fbuf, cc, (float)RI_SR);
            for (k = 0; k < cc; k++)
                pcm[c + k] = f32_to_s16(fbuf[k]);
            c += cc;
        }
        cursor = next;
        while (evpos < nev && ev[evpos].sample == cursor) {
            apply_event(&voice, &ev[evpos]);
            evpos++;
        }
    }
    if (ev_path && (rc = dump_events(ev_path, ev, nev)) != 0)
        return rc;
    if ((rc = write_wav(out_path, pcm, (uint32_t)total)) != 0)
        return rc;
    printf("render: %u events, %llu samples -> %s\n", nev, (unsigned long long)total, out_path);
    return 0;
}

/* --math stimulus: core ladder directly (ledger D6). */
static int render_math(int is_sine, const char *out_path) {
    struct RB303Voice v;
    static int16_t pcm[96000];
    uint32_t i;
    float phase = 0.0f;
    rb303_init(&v);
    if (is_sine) {
        rb303_set_cutoff_hz(&v, 8000.0f);
        rb303_set_reso(&v, 0.0f);
        for (i = 0; i < 96000u; i++) {
            float in = 0.1f * ri_sin(phase);
            pcm[i] = f32_to_s16(rb303_filter_step(&v, in, (float)RI_SR));
            phase += 2.0f * 3.14159265f * 1000.0f / (float)RI_SR;
            if (phase > 2.0f * 3.14159265f)
                phase -= 2.0f * 3.14159265f;
        }
    } else {
        rb303_set_cutoff_hz(&v, 1000.0f);
        rb303_set_reso(&v, 0.0f);
        for (i = 0; i < 96000u; i++)
            pcm[i] = f32_to_s16(rb303_filter_step(&v, 0.5f, (float)RI_SR));
    }
    return write_wav(out_path, pcm, 96000u);
}

int main(int argc, char **argv) {
    const char *song = NULL, *out = NULL, *ev = NULL, *math = NULL;
    int i;
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        printf("usage: render --song FILE --out FILE [--dump-events FILE]\n");
        printf("       render --math dc|sine --out FILE\n");
        printf("One 303, one pattern, offline, deterministic (D1).\n");
        printf("NOTE: this golden proves determinism + skeleton, NOT parity.\n");
        return 0;
    }
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--song") == 0 && i + 1 < argc)
            song = argv[++i];
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc)
            out = argv[++i];
        else if (strcmp(argv[i], "--dump-events") == 0 && i + 1 < argc)
            ev = argv[++i];
        else if (strcmp(argv[i], "--math") == 0 && i + 1 < argc)
            math = argv[++i];
        else {
            printf("render: bad arg %s (see --help)\n", argv[i]);
            return 2;
        }
    }
    if (!out) {
        printf("render: --out required\n");
        return 2;
    }
    if (math) {
        if (song || ev) {
            printf("render: --math takes no --song/--dump-events\n");
            return 2;
        }
        if (strcmp(math, "dc") == 0)
            return render_math(0, out);
        if (strcmp(math, "sine") == 0)
            return render_math(1, out);
        printf("render: --math wants dc|sine\n");
        return 2;
    }
    if (!song) {
        printf("render: --song required\n");
        return 2;
    }
    return render_song(song, out, ev);
}
