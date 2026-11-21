/* t37_engine_single.c — one-renderer contract (§12.3 fix proof).
 * The shared engine core (event walker + 303A/303B + stereo buses) must:
 *  (a) route NOTE events by device (A-only run: B-region silent and vv.),
 *  (b) render centre-unity stereo (L == R sample-exact, mono fold == legacy),
 *  (c) be chunk-agnostic (whole vs sliced renders identical),
 *  (d) reproduce the legacy walker bit-exactly (independent in-test walker),
 *  (e) route AUTOMATION by ctl block (303B automation bends B, not A).
 * RED-first: no engine exists.
 */
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "engine/engine.h"
#include "engine/seq/sched.h"
#include "engine/dsp/rb303.h"

#define SR 48000.0f
#define N 19200u
#define H (N / 2u)

static float engL[N], engR[N], ref[N], mono[N];

/* Two notes: A plays [0,4800), B plays [9600,14400). */
static void load_duet(struct RIEngine *e, uint32_t sections) {
    static struct RIEvent ev[4];
    ev[0].sample = 0;     ev[0].type = RI_EV_NOTE_ON;  ev[0].device = 0;
    ev[0].voice = 0;      ev[0].value = 69;            ev[0].flags = 0; ev[0].seq = 0;
    ev[1].sample = 4800;  ev[1].type = RI_EV_NOTE_OFF; ev[1].device = 0;
    ev[1].voice = 0;      ev[1].value = 0;             ev[1].flags = 0; ev[1].seq = 1;
    ev[2].sample = 9600;  ev[2].type = RI_EV_NOTE_ON;  ev[2].device = 1;
    ev[2].voice = 0;      ev[2].value = 81;            ev[2].flags = 0; ev[2].seq = 2;
    ev[3].sample = 14400; ev[3].type = RI_EV_NOTE_OFF; ev[3].device = 1;
    ev[3].voice = 0;      ev[3].value = 0;             ev[3].flags = 0; ev[3].seq = 3;
    ri_engine_init(e);
    ri_engine_defaults(e);
    ri_engine_load(e, ev, 4, N, sections);
}

static float rms(const float *b, uint32_t n) {
    double acc = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++)
        acc += (double)b[i] * (double)b[i];
    return (float)(acc / (double)(n ? n : 1));
}

/* Legacy walker (pre-unification render_song algorithm, 303A only). */
static void legacy_mono(struct RIEvent *ev, uint32_t nev, float *out) {
    struct RB303Voice v;
    uint64_t cursor = 0, evpos = 0;
    uint32_t i;
    for (i = 0; i < N; i++)
        out[i] = 0.0f;
    rb303_init(&v);
    rb303_set_param(&v, RI_CTL_303A_CUTOFF, 80);
    rb303_set_param(&v, RI_CTL_303A_RESO, 40);
    rb303_set_param(&v, RI_CTL_303A_ENVMOD, 64);
    rb303_set_param(&v, RI_CTL_303A_DECAY, 64);
    rb303_set_param(&v, RI_CTL_303A_ACCENT, 96);
    rb303_set_param(&v, RI_CTL_303A_WAVE, 0);
    rb303_set_param(&v, RI_CTL_303A_VOLUME, 127);
    while (cursor < N) {
        uint64_t next = N, c;
        if (evpos < nev && ev[evpos].sample < next)
            next = ev[evpos].sample;
        if (next == cursor) {
            while (evpos < nev && ev[evpos].sample == cursor) {
                const struct RIEvent *e = &ev[evpos];
                if (e->type == RI_EV_NOTE_ON)
                    rb303_note(&v, (uint8_t)(e->value & 127u),
                        (e->flags & RI_EVFLAG_SLIDE) != 0, (e->flags & RI_EVFLAG_ACCENT) != 0);
                else if (e->type == RI_EV_NOTE_OFF)
                    rb303_release(&v);
                evpos++;
            }
            continue;
        }
        for (c = cursor; c < next; c++)
            rb303_render(&v, out + c, 1, SR);
        cursor = next;
        while (evpos < nev && ev[evpos].sample == cursor) {
            const struct RIEvent *e = &ev[evpos];
            if (e->type == RI_EV_NOTE_ON)
                rb303_note(&v, (uint8_t)(e->value & 127u),
                    (e->flags & RI_EVFLAG_SLIDE) != 0, (e->flags & RI_EVFLAG_ACCENT) != 0);
            else if (e->type == RI_EV_NOTE_OFF)
                rb303_release(&v);
            evpos++;
        }
    }
}

int main(void) {
    struct RIEngine e;
    uint32_t i, done;
    /* (a) device routing. */
    load_duet(&e, RI_ENGINE_S303A);
    done = ri_engine_render(&e, engL, engR, N, SR);
    RI_ASSERT(done == N, "short render %u", done);
    RI_ASSERT(rms(engL, H) > 1e-4f, "A silent in A-only run");
    RI_ASSERT(rms(engL + H, N - H) < 1e-6f, "B leaks into A-only run: %g", rms(engL + H, N - H));
    load_duet(&e, RI_ENGINE_S303B);
    ri_engine_render(&e, engL, engR, N, SR);
    RI_ASSERT(rms(engL, H) < 1e-6f, "A leaks into B-only run: %g", rms(engL, H));
    RI_ASSERT(rms(engL + H, N - H) > 1e-4f, "B silent in B-only run");
    /* (b) centre unity: L == R exactly. */
    load_duet(&e, RI_ENGINE_S303A | RI_ENGINE_S303B);
    ri_engine_render(&e, engL, engR, N, SR);
    for (i = 0; i < N; i++)
        RI_ASSERT(engL[i] == engR[i], "stereo not unity at %u", i);
    /* (c) chunk-agnostic. */
    {
        static float cL[N], cR[N];
        uint32_t off = 0;
        load_duet(&e, RI_ENGINE_S303A | RI_ENGINE_S303B);
        while (off < N) {
            uint32_t cc = N - off > 2400u ? 2400u : N - off;
            done = ri_engine_render(&e, cL + off, cR + off, cc, SR);
            RI_ASSERT(done == cc, "short chunk %u", off);
            off += cc;
        }
        for (i = 0; i < N; i++)
            RI_ASSERT(cL[i] == engL[i] && cR[i] == engR[i], "chunk differs at %u", i);
    }
    /* (d) legacy bit-exact (303A-only mono fold vs independent walker). */
    {
        static struct RIEvent solo[2];
        solo[0].sample = 0;    solo[0].type = RI_EV_NOTE_ON;  solo[0].device = 0;
        solo[0].voice = 0;     solo[0].value = 69;            solo[0].flags = 0; solo[0].seq = 0;
        solo[1].sample = 4800; solo[1].type = RI_EV_NOTE_OFF; solo[1].device = 0;
        solo[1].voice = 0;     solo[1].value = 0;             solo[1].flags = 0; solo[1].seq = 1;
        legacy_mono(solo, 2, ref);
        ri_engine_init(&e);
        ri_engine_defaults(&e);
        ri_engine_load(&e, solo, 2, N, RI_ENGINE_S303A);
        ri_engine_render_mono(&e, mono, N, SR);
        for (i = 0; i < N; i++)
            RI_ASSERT(mono[i] == ref[i], "engine != legacy at %u: %g %g", i, mono[i], ref[i]);
    }
    /* (e) automation routing: 303B envmod bends B-runs, not A-runs.
     * NOTE: lists are sample-sorted per the engine contract (all
     * producers sort; the legacy walkers assume the same). */
    {
        static struct RIEvent sev[5];
        static float noB[N], withB[N], noA[N], withA[N];
        struct RIEngine f;
        uint32_t m;
        sev[0].sample = 0;    sev[0].type = RI_EV_NOTE_ON;  sev[0].device = 0;
        sev[0].voice = 0;     sev[0].value = 69;            sev[0].flags = 0; sev[0].seq = 0;
        sev[1].sample = 0;    sev[1].type = RI_EV_NOTE_ON;  sev[1].device = 1;
        sev[1].voice = 0;     sev[1].value = 81;            sev[1].flags = 0; sev[1].seq = 1;
        sev[2].sample = 2400; sev[2].type = RI_EV_AUTOMATION; sev[2].device = 1;
        sev[2].voice = 0;     sev[2].value = 0x0312u;       sev[2].flags = 127; sev[2].seq = 2;
        sev[3].sample = 4800; sev[3].type = RI_EV_NOTE_OFF; sev[3].device = 0;
        sev[3].voice = 0;     sev[3].value = 0;             sev[3].flags = 0; sev[3].seq = 3;
        sev[4].sample = 4800; sev[4].type = RI_EV_NOTE_OFF; sev[4].device = 1;
        sev[4].voice = 0;     sev[4].value = 0;             sev[4].flags = 0; sev[4].seq = 4;
        /* B-run with vs without the automation event. */
        ri_engine_init(&f);
        ri_engine_defaults(&f);
        ri_engine_load(&f, sev, 5, N, RI_ENGINE_S303B);
        ri_engine_render_mono(&f, withB, N, SR);
        ri_engine_init(&f);
        ri_engine_defaults(&f);
        {
            static struct RIEvent noauto[4];
            noauto[0] = sev[0];
            noauto[1] = sev[1];
            noauto[2] = sev[3];
            noauto[3] = sev[4];
            ri_engine_load(&f, noauto, 4, N, RI_ENGINE_S303B);
        }
        ri_engine_render_mono(&f, noB, N, SR);
        {
            int diff = 0;
            for (m = 2400; m < N; m++)
                if (withB[m] != noB[m])
                    diff = 1;
            RI_ASSERT(diff, "303B automation had no effect");
        }
        /* A-run with vs without the 303B automation. */
        ri_engine_init(&f);
        ri_engine_defaults(&f);
        ri_engine_load(&f, sev, 5, N, RI_ENGINE_S303A);
        ri_engine_render_mono(&f, withA, N, SR);
        ri_engine_init(&f);
        ri_engine_defaults(&f);
        {
            static struct RIEvent noauto[4];
            noauto[0] = sev[0];
            noauto[1] = sev[1];
            noauto[2] = sev[3];
            noauto[3] = sev[4];
            ri_engine_load(&f, noauto, 4, N, RI_ENGINE_S303A);
        }
        ri_engine_render_mono(&f, noA, N, SR);
        for (m = 0; m < N; m++)
            RI_ASSERT(withA[m] == noA[m], "303B automation leaked to A at %u", m);
    }
    RI_RESULT("engine_single");
}
