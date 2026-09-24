/* engine.c — the one renderer (spec §5; §12.3 skeleton, 303s first).
 * Event-boundary walker over a caller-owned sorted list; enabled sections
 * render into the scratch bus and accumulate sample-wise through the f64
 * master with a single final f32 rounding (spec §15). Centre-unity pan.
 * Chunk-agnostic: voice state is purely sequential, so run splits never
 * change samples (same property the live backend already relies on).
 * Kernels only via the voice code; no libm, no allocation, no IO.
 */
#include "engine/engine.h"

void ri_engine_init(struct RIEngine *e) {
    uint32_t i;
    if (!e)
        return;
    rb303_init(&e->v303a);
    rb303_init(&e->v303b);
    e->ev = 0;
    e->nev = 0;
    e->evpos = 0;
    e->cursor = 0;
    e->total = 0;
    e->sections = 0;
    for (i = 0; i < RI_ENGINE_BLOCK; i++)
        e->scratch[i] = 0.0f;
}

/* First-light knob defaults (identical set both voices; mirrors the legacy
 * offline/live defaults so rewired paths render byte-identical songs). */
void ri_engine_defaults(struct RIEngine *e) {
    struct RB303Voice *vs[2];
    int k;
    if (!e)
        return;
    vs[0] = &e->v303a;
    vs[1] = &e->v303b;
    for (k = 0; k < 2; k++) {
        rb303_init(vs[k]);
        rb303_set_param(vs[k], RI_CTL_303A_CUTOFF, 80);
        rb303_set_param(vs[k], RI_CTL_303A_RESO, 40);
        rb303_set_param(vs[k], RI_CTL_303A_ENVMOD, 64);
        rb303_set_param(vs[k], RI_CTL_303A_DECAY, 64);
        rb303_set_param(vs[k], RI_CTL_303A_ACCENT, 96);
        rb303_set_param(vs[k], RI_CTL_303A_WAVE, 0);
        rb303_set_param(vs[k], RI_CTL_303A_VOLUME, 127);
    }
}

void ri_engine_load(struct RIEngine *e, const struct RIEvent *ev,
    uint32_t nev, uint64_t total, uint32_t sections) {
    if (!e)
        return;
    e->ev = ev;
    e->nev = ev ? nev : 0;
    e->evpos = 0;
    e->cursor = 0;
    e->total = total;
    e->sections = sections;
}

/* Shared event routing (replaces the three per-path copies): NOTE-family by
 * device (0 = 303A, 1 = 303B), AUTOMATION by ctl block. Unknown devices and
 * blocks are ignored — 808/909 arrive with their slices, never misrouted. */
void ri_engine_apply_event(struct RIEngine *e, const struct RIEvent *ev) {
    struct RB303Voice *v = 0;
    if (!e || !ev)
        return;
    switch (ev->type) {
    case RI_EV_NOTE_ON:
    case RI_EV_NOTE_CONTINUE:
    case RI_EV_NOTE_OFF:
    case RI_EV_ACCENT:
        if (ev->device == 0)
            v = &e->v303a;
        else if (ev->device == 1)
            v = &e->v303b;
        else
            return;
        break;
    case RI_EV_AUTOMATION:
        if ((ev->value & 0xfff0u) == 0x0300u)
            v = &e->v303a;
        else if ((ev->value & 0xfff0u) == 0x0310u)
            v = &e->v303b;
        else
            return;
        break;
    default:
        return; /* FLAM and friends: voice-side later slices */
    }
    switch (ev->type) {
    case RI_EV_NOTE_ON:
        rb303_note(v, (uint8_t)(ev->value & 127u),
            (ev->flags & RI_EVFLAG_SLIDE) != 0, (ev->flags & RI_EVFLAG_ACCENT) != 0);
        break;
    case RI_EV_NOTE_CONTINUE:
        rb303_slide_to(v, (uint8_t)(ev->value & 127u));
        break;
    case RI_EV_NOTE_OFF:
        rb303_release(v);
        break;
    case RI_EV_ACCENT:
        rb303_accent(v);
        break;
    case RI_EV_AUTOMATION:
        rb303_set_param(v, ev->value, (uint8_t)(ev->flags & 127u));
        break;
    default:
        break;
    }
}

uint32_t ri_engine_render(struct RIEngine *e, float *out_l, float *out_r,
    uint32_t n, float sr) {
    uint32_t done = 0;
    if (!e || !out_l || !out_r || sr <= 0.0f)
        return 0;
    while (n > 0 && e->cursor < e->total) {
        uint64_t next = e->total;
        uint64_t run, c;
        if (e->evpos < e->nev && e->ev[e->evpos].sample < next)
            next = e->ev[e->evpos].sample;
        if (next > e->cursor + n)
            next = e->cursor + n;
        run = next - e->cursor;
        if (run == 0) {
            /* event(s) exactly at cursor: apply, no audio to render. */
            while (e->evpos < e->nev && e->ev[e->evpos].sample == e->cursor) {
                ri_engine_apply_event(e, &e->ev[e->evpos]);
                e->evpos++;
            }
            continue;
        }
        /* run through stack f64 buses in blockwise slices (bounded,
         * no allocation — render-contract safe). */
        for (c = 0; c < run;) {
            uint32_t cc = (uint32_t)(run - c);
            double ml[RI_ENGINE_BLOCK], mr[RI_ENGINE_BLOCK];
            uint32_t i;
            if (cc > RI_ENGINE_BLOCK)
                cc = RI_ENGINE_BLOCK;
            for (i = 0; i < cc; i++)
                ml[i] = mr[i] = 0.0;
            if (e->sections & RI_ENGINE_S303A) {
                rb303_render(&e->v303a, e->scratch, cc, sr);
                for (i = 0; i < cc; i++) {
                    ml[i] += (double)e->scratch[i]; /* centre unity */
                    mr[i] += (double)e->scratch[i];
                }
            }
            if (e->sections & RI_ENGINE_S303B) {
                rb303_render(&e->v303b, e->scratch, cc, sr);
                for (i = 0; i < cc; i++) {
                    ml[i] += (double)e->scratch[i];
                    mr[i] += (double)e->scratch[i];
                }
            }
            /* Per-section pan multiplies here once sections carry pan
             * (mixer slice); the f64 master already holds. */
            for (i = 0; i < cc; i++) {
                out_l[done + c + i] = (float)ml[i];
                out_r[done + c + i] = (float)mr[i];
            }
            c += cc;
        }
        done += (uint32_t)run;
        e->cursor += run;
        n -= (uint32_t)run;
    }
    return done;
}

/* Mono sink: (L+R)/2. With centre-unity buses a 303A-only run folds to
 * the legacy single-voice samples bit-exactly. */
uint32_t ri_engine_render_mono(struct RIEngine *e, float *out, uint32_t n,
    float sr) {
    double acc[RI_ENGINE_BLOCK];
    float sl[RI_ENGINE_BLOCK], sr_[RI_ENGINE_BLOCK];
    uint32_t done = 0;
    if (!e || !out || sr <= 0.0f)
        return 0;
    while (n > 0 && e->cursor < e->total) {
        uint32_t cc = n > RI_ENGINE_BLOCK ? RI_ENGINE_BLOCK : n;
        uint32_t got, i;
        if ((uint64_t)cc > e->total - e->cursor)
            cc = (uint32_t)(e->total - e->cursor);
        got = ri_engine_render(e, sl, sr_, cc, sr);
        for (i = 0; i < got; i++)
            acc[i] = ((double)sl[i] + (double)sr_[i]) * 0.5;
        for (i = 0; i < got; i++)
            out[done + i] = (float)acc[i];
        done += got;
        n -= got;
        if (got < cc)
            break;
    }
    return done;
}
