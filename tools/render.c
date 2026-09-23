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
 *     shuffle=50       (optional, Task 7: 0..100, default 0)
 *     legato=1         (optional, Task 7: 0/1, default 0)
 *     flam_ms=35.0     (optional, Task 7: 0..500 ms, default 35.0 = P-05)
 *     step note=45 [slide=0/1] [accent=0/1] [flam=0/1]
 *     step rest=1 [slide=0/1]
 *   max 64 steps. Unknown lines/values: fatal (exit 2), never silent.
 * --math dc: 2 s of DC 0.5 through the core ladder (fc=1000, k=0).
 * --math sine: 2 s of 1 kHz / 0.1 sine through the core ladder (fc=8k, k=0).
 * Output: 16-bit mono WAV, 48 kHz. Engine block 64 frames (spec §2.3).
 * Exit: 0 ok, 2 usage/IO/parse error.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "engine/seq/clock.h"
#include "engine/seq/sched.h"
#include "audio_io/audio.h"
#include "engine/dsp/rb303.h"
#include "engine/dsp/rb808.h"
#include "engine/dsp/rb909.h"
#include "engine/dsp/kernels.h"
#include "engine/fx/pcf.h"
#include "engine/fx/fx.h"
#include "engine/mixer/mixer.h"
#include "project/rbnm.h"
#include "project/rbng.h"

#define RI_SR 48000u
#define RI_BLOCK 64u
#define RI_MAX_STEPS 64u
#define RI_TAIL_SMP 48000u /* 1 s release tail after the last event */

/* Output sample depth, set by --depth (16 default). Process-global:
 * every render mode writes through write_wav with this depth. */
static int g_depth = 16;

static float clampf(float x) {
    if (x > 1.0f)
        return 1.0f;
    if (x < -1.0f)
        return -1.0f;
    return x;
}

/* Deterministic float -> int16 (round-half-away, no libm). Depth-16
 * path of write_wav converts through this exactly as before. */
static int16_t f32_to_s16(float x) {
    float c = clampf(x) * 32767.0f;
    if (c >= 0.0f)
        return (int16_t)(c + 0.5f);
    return (int16_t)(c - 0.5f);
}

/* Returns 0 on success. Depth 16|24 (TC-2.13.4); the 16-bit path
 * converts through f32_to_s16 exactly as before, so existing goldens
 * are byte-identical by construction. */
static int write_wav(const char *path, const float *pcm, uint32_t total,
    int depth) {
    unsigned char hdr[44];
    uint32_t i;
    FILE *f = fopen(path, "wb");
    if (!f) {
        printf("render: cannot open --out %s\n", path);
        return 2;
    }
    if (depth != 16 && depth != 24) {
        printf("render: bad --depth %d (want 16|24)\n", depth);
        fclose(f);
        return 2;
    }
    if (auf_wav_header(hdr, total, (uint8_t)depth, RI_SR) != 0) {
        fclose(f);
        return 2;
    }
    if (fwrite(hdr, 1, 44, f) != 44) {
        printf("render: write failed %s\n", path);
        fclose(f);
        return 2;
    }
    for (i = 0; i < total; i++) {
        if (depth == 24) {
            int32_t s = auf_f32_to_s24(pcm[i]);
            unsigned char b[3];
            b[0] = (unsigned char)(s & 0xff);
            b[1] = (unsigned char)((s >> 8) & 0xff);
            b[2] = (unsigned char)((s >> 16) & 0xff);
            if (fwrite(b, 1, 3, f) != 3) {
                printf("render: write failed %s\n", path);
                fclose(f);
                return 2;
            }
        } else {
            int16_t s = f32_to_s16(pcm[i]);
            if (fwrite(&s, 2, 1, f) != 1) {
                printf("render: write failed %s\n", path);
                fclose(f);
                return 2;
            }
        }
    }
    fclose(f);
    return 0;
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

/* Parse one "k=v" millisecond value (NNN[.mmm], 0..500). Returns 0 ok,
 * 1 no match, 2 bad value. Tool-side only (engine takes the double). */
static int take_ms(const char *tok, const char *key, double *val) {
    size_t kl = strlen(key);
    const char *p;
    long ip = 0;
    long fp = 0;
    long div = 1;
    if (strncmp(tok, key, kl) != 0 || tok[kl] != '=')
        return 1;
    p = tok + kl + 1;
    if (*p < '0' || *p > '9')
        return 2;
    while (*p >= '0' && *p <= '9') {
        ip = ip * 10 + (*p - '0');
        p++;
    }
    if (*p == '.') {
        int nd = 0;
        p++;
        if (*p < '0' || *p > '9')
            return 2;
        while (*p >= '0' && *p <= '9' && nd < 3) {
            fp = fp * 10 + (*p - '0');
            div *= 10;
            nd++;
            p++;
        }
        while (*p >= '0' && *p <= '9')
            p++; /* extra digits: truncate, still well-formed */
    }
    if (*p != '\0')
        return 2;
    *val = (double)ip + (double)fp / (double)div;
    if (*val < 0.0 || *val > 500.0)
        return 2;
    return 0;
}

/* Parse the Task-4 song scaffold + Task-7 timing directives. Returns 0 ok
 * (fills steps/tempo/timing), else 2. Timing headers (all optional):
 *   shuffle=N (0..100)  legato=0/1  flam_ms=NNN[.mmm] (0..500, def 35.0)
 * Per-step extra: flam=0/1 (needs a note; rest+flam parses, walker ignores).
 * Songs without timing headers render exactly as Task 4 (straight). */
static int parse_song(const char *path, struct RIStep *steps, uint32_t *nsteps, int *tempo,
    int *shuffle_pct, int *legato, double *flam_ms) {
    char line[256];
    FILE *f = fopen(path, "r");
    *nsteps = 0;
    *tempo = 0;
    *shuffle_pct = 0;
    *legato = 0;
    *flam_ms = RI_FLAM_MS_DEFAULT;
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
            int note = -1, slide = 0, accent = 0, rest = 0, flam = 0;
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
                } else if ((r = take_int(word, "flam", &v)) == 0) {
                    if (v != 0 && v != 1)
                        bad = 1;
                    flam = v;
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
                (slide ? RI_STEP_SLIDE : 0u) | (accent ? RI_STEP_ACCENT : 0u) |
                (flam ? RI_STEP_FLAM : 0u));
            (*nsteps)++;
            continue;
        }
        { /* Task-7 timing headers */
            int v;
            double ms;
            int r;
            if ((r = take_int(line, "shuffle", &v)) == 0) {
                if (v < 0 || v > 100) {
                    printf("render: bad shuffle line: %s\n", line);
                    fclose(f);
                    return 2;
                }
                *shuffle_pct = v;
                continue;
            } else if (r == 2) {
                printf("render: bad shuffle line: %s\n", line);
                fclose(f);
                return 2;
            }
            if ((r = take_int(line, "legato", &v)) == 0) {
                if (v != 0 && v != 1) {
                    printf("render: bad legato line: %s\n", line);
                    fclose(f);
                    return 2;
                }
                *legato = v;
                continue;
            } else if (r == 2) {
                printf("render: bad legato line: %s\n", line);
                fclose(f);
                return 2;
            }
            if ((r = take_ms(line, "flam_ms", &ms)) == 0) {
                *flam_ms = ms;
                continue;
            } else if (r == 2) {
                printf("render: bad flam_ms line: %s\n", line);
                fclose(f);
                return 2;
            }
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
    case RI_EV_AUTOMATION:
        /* AUTO lane: shared control IDs; the 0x0300 block drives the
         * 303A voice directly (value = ctl, flags = 0..127 value). */
        if ((e->value & 0xff00u) == 0x0300u)
            rb303_set_param(v, e->value, (uint8_t)(e->flags & 127u));
        break;
    default:
        break; /* FLAM (909 second hit, Task 9 voice-side) has no 303 effect */
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
    int shuffle_pct = 0, legato = 0;
    double flam_ms = RI_FLAM_MS_DEFAULT;
    struct RISchedOpts opts;
    uint32_t nev, k;
    uint64_t total, cursor = 0, evpos = 0;
    /* pcm worst case: 64 steps * full bar @30bpm + tail; static bound 2^22 */
    static float pcm[4194304];
    static float fbuf[RI_BLOCK];
    int rc;
    if ((rc = parse_song(song_path, steps, &nsteps, &tempo,
            &shuffle_pct, &legato, &flam_ms)) != 0)
        return rc;
    segs[0].start_tick = 0;
    segs[0].ns_per_quarter = 60000000000ULL / (uint64_t)tempo;
    map.segs = segs;
    map.n = 1;
    map.ppq = 96;
    map.sr = RI_SR;
    opts.shuffle_pct = (uint8_t)shuffle_pct;
    opts.legato = (uint8_t)legato;
    opts.flam_ms = flam_ms;
    nev = ri_sched_emit_timed(&map, 0, 96, steps, nsteps, 0, &opts, ev, RI_SCHED_MAX_EVENTS);
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
                pcm[c + k] = fbuf[k];
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
    if ((rc = write_wav(out_path, pcm, (uint32_t)total, g_depth)) != 0)
        return rc;
    printf("render: %u events, %llu samples -> %s\n", nev, (unsigned long long)total, out_path);
    return 0;
}

/* --math stimulus: core ladder directly (ledger D6). */
static int render_math(int is_sine, const char *out_path) {
    struct RB303Voice v;
    static float pcm[96000];
    uint32_t i;
    float phase = 0.0f;
    rb303_init(&v);
    if (is_sine) {
        rb303_set_cutoff_hz(&v, 8000.0f);
        rb303_set_reso(&v, 0.0f);
        for (i = 0; i < 96000u; i++) {
            float in = 0.1f * ri_sin(phase);
            pcm[i] = rb303_filter_step(&v, in, (float)RI_SR);
            phase += 2.0f * 3.14159265f * 1000.0f / (float)RI_SR;
            if (phase > 2.0f * 3.14159265f)
                phase -= 2.0f * 3.14159265f;
        }
    } else {
        rb303_set_cutoff_hz(&v, 1000.0f);
        rb303_set_reso(&v, 0.0f);
        for (i = 0; i < 96000u; i++)
            pcm[i] = rb303_filter_step(&v, 0.5f, (float)RI_SR);
    }
    return write_wav(out_path, pcm, 96000u, g_depth);
}

/* --808 stimulus (Task 8, gate G8): one voice (accent 0, tune 0, default
 * decay) or the storm fixture (all 15, max decay, full accent), rendered
 * in 64-frame blocks at 48 kHz. Deterministic (D1): fixed LFSR seeds at
 * trigger. Durations mirror tests/unit/t1_808.c voice_secs. */
static float voice_secs808(uint32_t v) {
    if (v == RB808_CY)
        return 3.0f;
    if (v == RB808_OH)
        return 2.0f;
    if (v == RB808_BD || v == RB808_LT || v == RB808_MT || v == RB808_HT ||
        v == RB808_CB)
        return 1.5f;
    if (v == RB808_RS || v == RB808_CL || v == RB808_CH)
        return 0.5f;
    return 1.0f;
}

static int render_808(const char *name, const char *out_path) {
    struct RB808Set s;
    static float pcm[144000];
    static float fbuf[RI_BLOCK];
    uint32_t total, pos = 0, k, v = 0;
    float secs;
    int storm = strcmp(name, "storm") == 0;
    if (!storm) {
        for (k = 0; k < RI_808_NVOICES; k++)
            if (strcmp(name, rb808_name(k)) == 0) {
                v = k;
                break;
            }
        if (k == RI_808_NVOICES) {
            printf("render: --808 wants bd|sd|lt|mt|ht|lc|mc|hc|rs|cl|cp|ch|oh|cy|cb|storm\n");
            return 2;
        }
        secs = voice_secs808(v);
    } else {
        secs = 2.0f;
    }
    total = (uint32_t)(secs * (float)RI_SR);
    if (total > 144000u) {
        printf("render: --808 fixture too long\n");
        return 2;
    }
    rb808_init_set(&s);
    if (storm) {
        rb808_max_decay(&s);
        for (k = 0; k < RI_808_NVOICES; k++)
            rb808_trigger(&s, k, 1, 0.0f);
    } else {
        rb808_trigger(&s, v, 0, 0.0f);
    }
    while (pos < total) {
        uint32_t cc = total - pos, j;
        if (cc > RI_BLOCK)
            cc = RI_BLOCK;
        rb808_render_mix(&s, fbuf, cc, (float)RI_SR);
        for (j = 0; j < cc; j++)
            pcm[pos + j] = fbuf[j];
        pos += cc;
    }
    printf("render: 808 %s, %u samples -> %s\n", name, total, out_path);
    return write_wav(out_path, pcm, total, g_depth);
}

/* --909 stimulus (Task 9, gate G9): one voice (accent 0, tune 64) from
 * the DEFAULT built-in layers (synthesized below with ri_sin — a
 * different recipe from the clean pack, so the S909 render-diff is
 * real), or from the clean pack RBNM (--909pack). 64-frame blocks at
 * 48 kHz. Deterministic (D1): baked data, fixed playheads. */
static float voice_secs909(const char *name) {
    if (strcmp(name, "cr") == 0 || strcmp(name, "rd") == 0)
        return 2.5f;
    if (strcmp(name, "oh") == 0)
        return 2.0f;
    if (strcmp(name, "bd") == 0)
        return 1.5f;
    if (strcmp(name, "sd") == 0)
        return 1.0f;
    if (strcmp(name, "ch") == 0)
        return 0.5f;
    return -1.0f;
}

static uint32_t voice_idx909(const char *name) {
    if (strcmp(name, "bd") == 0)
        return RB909_BD;
    if (strcmp(name, "sd") == 0)
        return RB909_SD;
    if (strcmp(name, "ch") == 0)
        return RB909_CH;
    if (strcmp(name, "oh") == 0)
        return RB909_OH;
    if (strcmp(name, "cr") == 0)
        return RB909_CR;
    if (strcmp(name, "rd") == 0)
        return RB909_RD;
    return RI_909_NVOICES;
}

/* Default-layer recipe (tool-side stand-in, NOT the pack): detuned sine
 * pairs per voice, start-at-zero, peak ~0.7.
 * Phase discipline: ri_sin clamps cycles to ±64, so recipe args are
 * phase-reduced to [0,1) cycles before the call (t >= 0, truncation ==
 * floor). Unreduced args froze CR/OH/RD to digital silence. */
static float red_phase(float cycles) {
    float q = (float)(int)cycles;
    return (cycles - q) * 6.2831853f;
}

static void bake_default909(uint32_t v, float *a, float *b, uint32_t n) {
    uint32_t i;
    float f0 = 55.0f, f1 = 350.0f, tau = 0.4f, nz = 0.0f;
    if (v == RB909_BD) {
        f0 = 55.0f;
        f1 = 0.0f;
        tau = 0.40f;
    } else if (v == RB909_SD) {
        f0 = 200.0f;
        f1 = 350.0f;
        tau = 0.20f;
        nz = 0.15f;
    } else if (v == RB909_CH) {
        f0 = 420.0f;
        f1 = 840.0f;
        tau = 0.035f;
    } else if (v == RB909_OH) {
        f0 = 420.0f;
        f1 = 840.0f;
        tau = 0.60f;
    } else if (v == RB909_CR) {
        f0 = 320.0f;
        f1 = 640.0f;
        tau = 1.20f;
    } else {
        f0 = 560.0f;
        f1 = 820.0f;
        tau = 1.50f;
    }
    for (i = 0; i < n; i++) {
        float t = (float)i / 48000.0f;
        float e;
        /* exp via the kernel (tool links it; keeps host determinism) */
        e = ri_exp(-t / tau);
        a[i] = (0.55f * ri_sin(red_phase(f0 * t)) +
            0.25f * (f1 > 0.0f ? ri_sin(red_phase(f1 * t)) : 0.0f)) * e;
        b[i] = (0.55f * ri_sin(red_phase(f0 * 1.12f * t)) +
            0.25f * (f1 > 0.0f ? ri_sin(red_phase(f1 * 1.12f * t)) : 0.0f)) * e;
        if (nz > 0.0f) {
            /* deterministic hash noise (no RNG state in tools either) */
            uint32_t h = i * 1664525u + 1013904223u;
            float w = (float)(h >> 8) * (1.0f / 8388608.0f) - 1.0f;
            h = (uint32_t)((float)i * 0.5f) * 1664525u + 1013904223u;
            a[i] += nz * w * e;
            b[i] += nz * ((float)(h >> 8) * (1.0f / 8388608.0f) - 1.0f) * e;
        }
    }
    /* peak-guard to ~0.7 (deterministic scan, no libm) */
    {
        float pk = 0.000001f;
        for (i = 0; i < n; i++) {
            float x = a[i] < 0.0f ? -a[i] : a[i];
            float y = b[i] < 0.0f ? -b[i] : b[i];
            if (x > pk)
                pk = x;
            if (y > pk)
                pk = y;
        }
        for (i = 0; i < n; i++) {
            a[i] *= 0.7f / pk;
            b[i] *= 0.7f / pk;
        }
    }
}

static int render_909_common(struct RB909Set *s, uint32_t v, float secs,
    const char *out_path, const char *tag) {
    static float pcm[120000];
    static float fbuf[RI_BLOCK];
    uint32_t total = (uint32_t)(secs * (float)RI_SR);
    uint32_t pos = 0, j;
    if (total > 120000u || total == 0u) {
        printf("render: --909 fixture too long\n");
        return 2;
    }
    rb909_trigger(s, v, 0, 64, 0);
    while (pos < total) {
        uint32_t cc = total - pos;
        if (cc > RI_BLOCK)
            cc = RI_BLOCK;
        rb909_render_mix(s, fbuf, cc, (float)RI_SR);
        for (j = 0; j < cc; j++)
            pcm[pos + j] = fbuf[j];
        pos += cc;
    }
    printf("render: 909 %s, %u samples -> %s\n", tag, total, out_path);
    return write_wav(out_path, pcm, total, g_depth);
}

static int render_909(const char *name, const char *out_path) {
    struct RB909Set s;
    static float la[120000], lb[120000];
    static struct RISampleLayer lay[2];
    uint32_t v = voice_idx909(name);
    float secs;
    if (v >= RI_909_NVOICES) {
        printf("render: --909 wants bd|sd|ch|oh|cr|rd\n");
        return 2;
    }
    secs = voice_secs909(name);
    bake_default909(v, la, lb, (uint32_t)(secs * (float)RI_SR));
    lay[0].data = la;
    lay[0].frames = (uint32_t)(secs * (float)RI_SR);
    lay[0].rate = RI_SR;
    lay[0].lo = 0;
    lay[0].hi = 63;
    lay[1].data = lb;
    lay[1].frames = lay[0].frames;
    lay[1].rate = RI_SR;
    lay[1].lo = 64;
    lay[1].hi = 127;
    rb909_init_set(&s);
    if (rb909_set_layers(&s, v, lay, 2) != 0) {
        printf("render: default layer install failed\n");
        return 2;
    }
    return render_909_common(&s, v, secs, out_path, name);
}

static int render_909pack(const char *name, const char *out_path,
    const char *pack_opt) {
    struct RB909Set s;
    static float pa[88200], pb[88200], pc[88200];
    static struct RISampleLayer lay[3];
    static struct RBNMLayerInfo info[16];
    static char err[192];
    const char *pack = pack_opt ? pack_opt : "reference/packs/classic-01/pack.rbnm";
    uint32_t v = voice_idx909(name);
    float secs;
    int32_t nl, k;
    uint32_t nl_v = 0;
    float *bufs[3] = { pa, pb, pc };
    uint32_t rate0 = 0;
    if (v >= RI_909_NVOICES) {
        printf("render: --909pack wants bd|sd|ch|oh|cr|rd\n");
        return 2;
    }
    secs = voice_secs909(name);
    nl = rbnm_pack_layers(pack, info, 16, err, sizeof err);
    if (nl < 0) {
        printf("render: pack inventory failed: %s\n", err);
        return 2;
    }
    for (k = 0; k < nl && nl_v < 3u; k++) {
        int32_t got;
        uint32_t rate = 0;
        if (info[k].voice != v)
            continue;
        if (info[k].frames > 88200u) {
            printf("render: pack layer too long\n");
            return 2;
        }
        got = rbnm_load_smpl(pack, info[k].id, bufs[nl_v], 88200u, &rate,
            err, sizeof err);
        if (got < 0) {
            printf("render: pack load failed: %s\n", err);
            return 2;
        }
        lay[nl_v].data = bufs[nl_v];
        lay[nl_v].frames = (uint32_t)got;
        lay[nl_v].rate = rate;
        lay[nl_v].lo = info[k].lo;
        lay[nl_v].hi = info[k].hi;
        if (nl_v == 0u)
            rate0 = rate;
        else if (rate != rate0) {
            printf("render: pack rate mismatch\n");
            return 2;
        }
        nl_v++;
    }
    if (nl_v == 0u) {
        printf("render: no pack layers for %s\n", name);
        return 2;
    }
    rb909_init_set(&s);
    if (rb909_set_layers(&s, v, lay, nl_v) != 0) {
        printf("render: pack layer install failed\n");
        return 2;
    }
    return render_909_common(&s, v, secs, out_path, name);
}

/* --pcf / --fx stimuli (Task 10, gate G10): deterministic (D1) fixtures
 * for the pcf goldens. All synthesis via ri_* (host determinism); 64-frame
 * blocks at 48 kHz; static buffers only. */
static int render_pcf(const char *name, const char *out_path) {
    struct PCF p;
    static float in[96000], out[RI_BLOCK];
    static float pcm[96000];
    uint32_t total = 96000u, pos = 0;
    float phase = 0.0f;
    if (strcmp(name, "sweep") != 0) {
        printf("render: --pcf wants sweep\n");
        return 2;
    }
    pcf_init(&p);
    p.q = 2.0f;
    p.amt_oct = 0.0f;
    while (pos < total) {
        /* input log sweep 100 -> 8000 Hz over the fixture */
        float t0 = (float)pos / (float)total;
        float f0 = 100.0f * ri_pow2(t0 * 6.321928f); /* 100->8000 */
        uint32_t cc = total - pos, j;
        /* base_fc ramps 200 -> 8000 alongside (tool-side automation) */
        p.base_fc = 200.0f * ri_pow2(t0 * 5.321928f); /* 200->8000 */
        if (cc > RI_BLOCK)
            cc = RI_BLOCK;
        for (j = 0; j < cc; j++) {
            phase += f0 / (float)RI_SR;
            if (phase >= 1.0f)
                phase -= 1.0f;
            in[pos + j] = 0.5f * ri_sin(phase * 6.2831853f);
        }
        pcf_render(&p, in + pos, out, cc, (float)RI_SR);
        for (j = 0; j < cc; j++)
            pcm[pos + j] = out[j];
        pos += cc;
    }
    printf("render: pcf %s, %u samples -> %s\n", name, total, out_path);
    return write_wav(out_path, pcm, total, g_depth);
}

/* Shared 2 s chord input for the fx dry/chain pair (identical stimulus). */
static void fx_chord(float *b, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++) {
        float t = (float)i / 48000.0f;
        b[i] = 0.30f * ri_sin(red_phase(220.0f * t)) +
            0.22f * ri_sin(red_phase(277.18f * t)) +
            0.22f * ri_sin(red_phase(329.63f * t));
    }
}

/* Delay fixture input: 440 Hz sine at 0.4 plus a leading impulse so the
 * echo grid is visible in the golden. */
static void sine_probe_delay_in(float *b, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++) {
        float t = (float)i / 48000.0f;
        b[i] = 0.4f * ri_sin(red_phase(440.0f * t));
    }
    b[0] += 0.5f;
}

static int render_fx(const char *name, const char *out_path) {
    static float in[96000], tmp[96000];
    static float dl[96000];
    static float pcm[96000];
    struct PCF p;
    struct RiFXDelay d;
    struct RiFXDist ds;
    struct RiFXComp c;
    uint32_t total, pos = 0, j;
    if (strcmp(name, "dry") != 0 && strcmp(name, "delay") != 0 &&
        strcmp(name, "chain") != 0) {
        printf("render: --fx wants dry|delay|chain\n");
        return 2;
    }
    if (strcmp(name, "delay") == 0) {
        total = 72000u; /* 1.5 s */
        sine_probe_delay_in(in, total);
        ri_fxdelay_init(&d, dl, 96000u);
        ri_fxdelay_sync(&d, 140.0f, 0.75f, (float)RI_SR);
        ri_fxdelay_set(&d, 44, 51); /* fb 0.35, mix 0.4 */
        while (pos < total) {
            uint32_t cc = total - pos;
            if (cc > RI_BLOCK)
                cc = RI_BLOCK;
            ri_fxdelay_render(&d, in + pos, tmp + pos, cc);
            pos += cc;
        }
    } else {
        total = 96000u; /* 2 s */
        fx_chord(in, total);
        if (strcmp(name, "dry") == 0) {
            for (j = 0; j < total; j++)
                tmp[j] = in[j];
        } else {
            pcf_init(&p);
            p.base_fc = 1500.0f;
            p.q = 2.0f;
            ri_fxdist_init(&ds);
            ri_fxdist_set(&ds, 48, 16);
            ri_fxdelay_init(&d, dl, 96000u);
            ri_fxdelay_sync(&d, 140.0f, 0.75f, (float)RI_SR);
            ri_fxdelay_set(&d, 38, 44); /* fb 0.3, mix 0.35 */
            ri_fxcomp_init(&c, (float)RI_SR);
            ri_fxcomp_set(&c, 64);
            while (pos < total) {
                uint32_t cc = total - pos;
                if (cc > RI_BLOCK)
                    cc = RI_BLOCK;
                ri_fxdist_render(&ds, in + pos, tmp + pos, cc);
                pcf_render(&p, tmp + pos, tmp + pos, cc, (float)RI_SR);
                ri_fxdelay_render(&d, tmp + pos, tmp + pos, cc);
                ri_fxcomp_render(&c, tmp + pos, tmp + pos, cc);
                pos += cc;
            }
        }
    }
    for (j = 0; j < total; j++)
        pcm[j] = tmp[j];
    printf("render: fx %s, %u samples -> %s\n", name, total, out_path);
    return write_wav(out_path, pcm, total, g_depth);
}

/* --mix stimuli (Task 11, gate G11): 4 sine buses through the real
 * RiMixer into the mono master. "four": faders 127/96/64/112 with bus1
 * muted; "solo": same inputs and faders, solo bus2 only. 2 s, 64-frame
 * blocks at 48 kHz, static buffers, deterministic (D1). */
static int render_mix(const char *name, const char *out_path) {
    static float bus[4][96000];
    static float tmp[96000], snd[96000];
    static float pcm[96000];
    struct RiMixer m;
    uint32_t total = 96000u, pos = 0, j, b;
    static const float F[4] = { 110.0f, 138.59f, 164.81f, 220.0f };
    static const uint8_t FD[4] = { 127u, 96u, 64u, 112u };
    int solo;
    if (strcmp(name, "four") == 0)
        solo = 0;
    else if (strcmp(name, "solo") == 0)
        solo = 1;
    else {
        printf("render: --mix wants four|solo\n");
        return 2;
    }
    for (b = 0; b < 4u; b++) {
        for (j = 0; j < total; j++)
            bus[b][j] = 0.35f * ri_sin(red_phase(F[b] * (float)j / 48000.0f));
    }
    ri_mix_init(&m, (float)RI_SR);
    for (b = 0; b < 4u; b++)
        ri_mix_set_fader(&m, b, FD[b]);
    if (solo)
        ri_mix_set_solo(&m, 2, 1);
    else
        ri_mix_set_mute(&m, 1, 1);
    while (pos < total) {
        uint32_t cc = total - pos;
        const float *blk[4];
        if (cc > RI_BLOCK)
            cc = RI_BLOCK;
        for (b = 0; b < 4u; b++)
            blk[b] = bus[b] + pos;
        ri_mix_render(&m, blk, tmp + pos, snd + pos, cc);
        pos += cc;
    }
    for (j = 0; j < total; j++)
        pcm[j] = tmp[j];
    printf("render: mix %s, %u samples -> %s (meter %.4f)\n", name, total,
        out_path, (double)ri_meter_peak(&m.meter));
    return write_wav(out_path, pcm, total, g_depth);
}

/* --rbngsong (Task 13, gate G13): the real RBNG codec path. Same
 * walker + voice + bus as the text scaffold (one renderer): PATT maps
 * 1:1 onto RIStep (flag bits equal by contract), AUTO ticks convert
 * tick->sample at song ppq and merge as AUTOMATION events, MODR
 * missing mods print the warn prompt and render continues without
 * them, CPRG prints the on-load hook line. Exit 0 ok, 2 usage/IO/
 * parse error (same contract as --song). */
static int render_rbngsong(const char *song_path, const char *out_path,
    const char *ev_path) {
    struct RISong song;
    struct RIStep steps[RI_MAX_STEPS];
    struct RIEvent ev[RI_SCHED_MAX_EVENTS];
    static struct RISegment segs[1];
    struct RITempoMap map;
    struct RB303Voice voice;
    static char err[192];
    static char warn[256];
    struct RISchedOpts opts;
    uint32_t nev, k, i;
    uint64_t total, cursor = 0, evpos = 0;
    static float pcm[4194304];
    static float fbuf[RI_BLOCK];
    double nsq, tick2smp;
    uint64_t pat_end;
    int rc;
    if (rbng_read_song(song_path, &song, err, sizeof err) != 0) {
        printf("render: RBNG invalid: %s: %s\n", song_path, err);
        return 2;
    }
    /* MODR warn path: host installs no mods, so every referenced mod
     * warns (asserted prompt string) and the render continues. */
    if (song.nmods > 0u) {
        static const char *have_none[1] = { "" };
        if (rbng_missing_warn(&song, have_none, 1, warn, sizeof warn) != 0)
            printf("render: %s (continuing without the mod)\n", warn);
    }
    if (song.cprg[0] != '\0')
        printf("render: CPRG: %s\n", song.cprg);
    for (i = 0; i < song.nsteps; i++) {
        steps[i].note = song.steps[i].note;
        steps[i].flags = song.steps[i].flags;
    }
    segs[0].start_tick = 0;
    segs[0].ns_per_quarter = 60000000000ULL / (uint64_t)song.tempo;
    map.segs = segs;
    map.n = 1;
    map.ppq = song.ppq;
    map.sr = RI_SR;
    opts.shuffle_pct = 0;
    opts.legato = 0;
    opts.flam_ms = RI_FLAM_MS_DEFAULT;
    nev = ri_sched_emit_timed(&map, 0, song.ppq, steps, song.nsteps, 0,
        &opts, ev, RI_SCHED_MAX_EVENTS);
    if (nev == 0) {
        printf("render: walker emitted no events\n");
        return 2;
    }
    /* AUTO lanes: tick (song ppq) -> sample, merged as AUTOMATION. */
    nsq = 60000000000.0 / (double)song.tempo;
    tick2smp = nsq * (double)RI_SR / ((double)song.ppq * 1e9);
    for (i = 0; i < song.nauto; i++) {
        uint64_t s = (uint64_t)((double)song.auto_ev[i].tick * tick2smp +
            0.5);
        uint32_t p = nev;
        if (nev >= RI_SCHED_MAX_EVENTS) {
            printf("render: AUTO overflow\n");
            return 2;
        }
        ev[nev].sample = s;
        ev[nev].type = RI_EV_AUTOMATION;
        ev[nev].device = 0;
        ev[nev].voice = 0;
        ev[nev].value = song.auto_ev[i].ctl;
        ev[nev].flags = song.auto_ev[i].val;
        ev[nev].seq = nev;
        nev++;
        /* Insertion-sort the newcomer back (list was sorted). */
        while (p > 0u && ev[p].sample < ev[p - 1u].sample) {
            struct RIEvent t = ev[p];
            ev[p] = ev[p - 1u];
            ev[p - 1u] = t;
            p--;
        }
    }
    pat_end = (uint64_t)((double)(song.nsteps * (song.ppq / 4u)) *
        tick2smp + 0.5);
    total = ev[nev - 1].sample + RI_TAIL_SMP;
    if (pat_end + RI_TAIL_SMP > total)
        total = pat_end + RI_TAIL_SMP;
    if (total > 4194304u) {
        printf("render: song too long (%llu samples)\n",
            (unsigned long long)total);
        return 2;
    }
    rb303_init(&voice);
    rb303_set_param(&voice, RI_CTL_303A_CUTOFF, 80);
    rb303_set_param(&voice, RI_CTL_303A_RESO, 40);
    rb303_set_param(&voice, RI_CTL_303A_ENVMOD, 64);
    rb303_set_param(&voice, RI_CTL_303A_DECAY, 64);
    rb303_set_param(&voice, RI_CTL_303A_ACCENT, 96);
    rb303_set_param(&voice, RI_CTL_303A_WAVE, 0);
    rb303_set_param(&voice, RI_CTL_303A_VOLUME, 127);
    while (cursor < total) {
        uint64_t blk = ((cursor + RI_BLOCK) / RI_BLOCK) * RI_BLOCK;
        uint64_t next = total;
        uint64_t c;
        if (blk < next)
            next = blk;
        if (evpos < nev && ev[evpos].sample < next)
            next = ev[evpos].sample;
        if (next == cursor) {
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
                pcm[c + k] = fbuf[k];
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
    if ((rc = write_wav(out_path, pcm, (uint32_t)total, g_depth)) != 0)
        return rc;
    printf("render: %u events, %llu samples -> %s\n", nev,
        (unsigned long long)total, out_path);
    return 0;
}

int main(int argc, char **argv) {
    const char *song = NULL, *out = NULL, *ev = NULL, *math = NULL, *v808 = NULL;
    const char *v909 = NULL, *v909pack = NULL, *pack = NULL, *vpcf = NULL;
    const char *vfx = NULL, *vmix = NULL, *rbngsong = NULL;
    int i;
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        printf("usage: render --song FILE --out FILE [--dump-events FILE]\n");
        printf("       render --rbngsong FILE --out FILE [--dump-events FILE]\n");
        printf("       render --math dc|sine --out FILE\n");
        printf("       render --808 VOICE|storm --out FILE\n");
        printf("       render --909 VOICE --out FILE\n");
        printf("       render --909pack VOICE --out FILE [--pack FILE]\n");
        printf("       render --pcf sweep --out FILE\n");
        printf("       render --fx dry|delay|chain --out FILE\n");
        printf("       render --mix four|solo --out FILE\n");
        printf("       [--depth 16|24, default 16]\n");
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
        else if (strcmp(argv[i], "--808") == 0 && i + 1 < argc)
            v808 = argv[++i];
        else if (strcmp(argv[i], "--909") == 0 && i + 1 < argc)
            v909 = argv[++i];
        else if (strcmp(argv[i], "--909pack") == 0 && i + 1 < argc)
            v909pack = argv[++i];
        else if (strcmp(argv[i], "--pcf") == 0 && i + 1 < argc)
            vpcf = argv[++i];
        else if (strcmp(argv[i], "--fx") == 0 && i + 1 < argc)
            vfx = argv[++i];
        else if (strcmp(argv[i], "--mix") == 0 && i + 1 < argc)
            vmix = argv[++i];
        else if (strcmp(argv[i], "--depth") == 0 && i + 1 < argc) {
            int d = atoi(argv[++i]);
            if (d != 16 && d != 24) {
                printf("render: bad --depth (want 16|24)\n");
                return 2;
            }
            g_depth = d;
        }
        else if (strcmp(argv[i], "--pack") == 0 && i + 1 < argc)
            pack = argv[++i];
        else if (strcmp(argv[i], "--rbngsong") == 0 && i + 1 < argc)
            rbngsong = argv[++i];
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
        if (song || rbngsong || ev) {
            printf("render: --math takes no --song/--dump-events\n");
            return 2;
        }
        if (v808 || vpcf || vfx || vmix) {
            printf("render: --math takes no --808/--pcf/--fx/--mix\n");
            return 2;
        }
        if (strcmp(math, "dc") == 0)
            return render_math(0, out);
        if (strcmp(math, "sine") == 0)
            return render_math(1, out);
        printf("render: --math wants dc|sine\n");
        return 2;
    }
    if (v808) {
        if (song || rbngsong || ev || math) {
            printf("render: --808 takes no --song/--dump-events/--math/--pcf/--fx\n");
            return 2;
        }
        if (vpcf || vfx || v909 || v909pack || vmix) {
            printf("render: --808 takes no --pcf/--fx/--909/--mix\n");
            return 2;
        }
        return render_808(v808, out);
    }
    if (vpcf || vfx) {
        if (song || rbngsong || ev || math || v808 || v909 || v909pack || vmix) {
            printf("render: --pcf/--fx take no --song/--dump-events/--math/--808/--909/--mix\n");
            return 2;
        }
        if (vpcf && vfx) {
            printf("render: --pcf and --fx are exclusive\n");
            return 2;
        }
        if (vpcf)
            return render_pcf(vpcf, out);
        return render_fx(vfx, out);
    }
    if (v909 || v909pack) {
        if (song || rbngsong || ev || math || v808 || vpcf || vfx || vmix) {
            printf("render: --909 takes no --song/--dump-events/--math/--808/--pcf/--fx/--mix\n");
            return 2;
        }
        if (v909 && v909pack) {
            printf("render: --909 and --909pack are exclusive\n");
            return 2;
        }
        if (v909pack)
            return render_909pack(v909pack, out, pack);
        return render_909(v909, out);
    }
    if (vmix) {
        if (song || rbngsong || ev || math || v808 || v909 || v909pack || vpcf || vfx) {
            printf("render: --mix takes no --song/--dump-events/--math/--808/--909/--pcf/--fx\n");
            return 2;
        }
        return render_mix(vmix, out);
    }
    if (!song && !rbngsong) {
        printf("render: --song/--rbngsong required\n");
        return 2;
    }
    if (song && rbngsong) {
        printf("render: --song and --rbngsong are exclusive\n");
        return 2;
    }
    if (rbngsong)
        return render_rbngsong(rbngsong, out, ev);
    return render_song(song, out, ev);
}
