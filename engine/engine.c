/* engine.c — the one renderer (spec §5; §12.3 skeleton, 303s first).
 * Event-boundary walker over a caller-owned sorted list; enabled sections
 * render into the scratch bus and accumulate sample-wise through the f64
 * master with a single final f32 rounding (spec §15). Centre-unity pan.
 * Chunk-agnostic: voice state is purely sequential, so run splits never
 * change samples (same property the live backend already relies on).
 * Kernels only via the voice code; no libm, no allocation, no IO.
 */
#include "engine/engine.h"
#include "engine/seq/pattern.h"

void ri_engine_init(struct RIEngine *e) {
    uint32_t i;
    if (!e)
        return;
    rb303_init(&e->v303a);
    rb303_init(&e->v303b);
    rb808_init_set(&e->s808);
    rb909_init_set(&e->s909);
    for (i = 0; i < RI_808_NSOUNDS; i++)
        e->tag808[i] = ~(uint64_t)0u;
    for (i = 0; i < RI_909_NVOICES; i++)
        e->tag909[i] = ~(uint64_t)0u;
    e->ev = 0;
    e->nev = 0;
    e->evpos = 0;
    e->cursor = 0;
    e->total = 0;
    e->sections = 0;
    for (i = 0; i < RI_ROUTE_NSECTIONS; i++)
        ri_meter_init(&e->sec_meter[i], 48000.0f); /* display ballistics rate */
    for (i = 0; i < RI_ENGINE_FX_COUNT; i++)
        ri_meter_init(&e->fx_meter[i], 48000.0f);
    for (i = 0; i < RI_ENGINE_BLOCK; i++)
        e->scratch[i] = 0.0f;
    /* Routing neutral: no owners, sends 0, pans centre, delay dry. */
    ri_route_init(&e->route);
    ri_fxdist_init(&e->dist);
    pcf_init(&e->pcf);
    e->pcf_base = 64;
    e->pcf_q = 64;
    e->pcf_amt = 64;
    e->pcf_mode = 0;
    e->pcf_pattern = 0;
    e->pcf_decay = 64;
    ri_fx_pcf_apply_raw(&e->pcf, 64, 64, 64, 0, 0, 64);
    ri_fxcomp_init(&e->comp, 48000.0f);
    for (i = 0; i < RI_ROUTE_NSECTIONS; i++) {
        e->pan[i] = RI_ENGINE_PAN_CENTER;
        e->send[i] = 0;
    }
    e->tempo = RI_ENGINE_TEMPO_DEFAULT;
    e->dline = 0;
    e->dcap = 0;
    /* Delay knob defaults (match ri_fxdelay_init; mix forced wet). */
    e->delay.buf = 0;
    e->delay.cap = 0;
    e->delay.pos = 0;
    e->delay.delay_smp = 0;
    e->delay.target_smp = 0;
    e->delay.beats = 0.75f;
    e->delay.steps = 3u;
    e->delay.triplet = 0u;
    e->delay.fb = 0.0f;
    e->delay.mix = 1.0f;
    e->dret_pan = RI_ENGINE_PAN_CENTER;
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

/* Pan law (starting point, pending §8 ear-fit): linear wings with an
 * exact centre detent. v = 64 -> (1, 1) exactly (neutral bit-identical);
 * v < 64 -> (1, v/64); v > 64 -> ((127-v)/63, 1). Continuous at 64. */
static void engine_pan_gains(uint8_t v, double *gl, double *gr) {
    if (v >= 127u) {
        *gl = 0.0;
        *gr = 1.0;
    } else if (v == RI_ENGINE_PAN_CENTER) {
        *gl = 1.0;
        *gr = 1.0;
    } else if (v < RI_ENGINE_PAN_CENTER) {
        *gl = 1.0;
        *gr = (double)v / 64.0;
    } else {
        *gl = (double)(127u - v) / 63.0;
        *gr = 1.0;
    }
}

/* One section bus: inserts (Dist->PCF->Comp) in series, post-insert mono
 * send tap, pan into the f64 master. No allocation; scratch is the bus. */
static void engine_section(struct RIEngine *e, uint32_t section,
    double *ml, double *mr, float *sendbus, uint32_t cc, float sr) {
    uint32_t i, mask;
    double gl, gr, t, sg;
    mask = ri_route_section_mask(&e->route, section);
    if (mask & (1u << RI_ROUTE_DIST)) {
        ri_fxdist_render(&e->dist, e->scratch, e->scratch, cc);
        ri_meter_feed(&e->fx_meter[RI_ENGINE_FX_DIST], e->scratch, cc);
    }
    if (mask & (1u << RI_ROUTE_PCF)) {
        pcf_render(&e->pcf, e->scratch, e->scratch, cc, sr);
        ri_meter_feed(&e->fx_meter[RI_ENGINE_FX_PCF], e->scratch, cc);
    }
    if (mask & (1u << RI_ROUTE_COMP)) {
        ri_fxcomp_render(&e->comp, e->scratch, e->scratch, cc);
        ri_meter_feed(&e->fx_meter[RI_ENGINE_FX_COMP], e->scratch, cc);
    }
    if (section < RI_ROUTE_NSECTIONS)
        ri_meter_feed(&e->sec_meter[section], e->scratch, cc);
    t = (double)e->send[section] / 127.0;
    sg = t * t; /* square law (E0 P-17), 0 -> exactly 0 */
    engine_pan_gains(e->pan[section], &gl, &gr);
    for (i = 0; i < cc; i++) {
        double s = (double)e->scratch[i];
        sendbus[i] += (float)(s * sg);
        ml[i] += s * gl;
        mr[i] += s * gr;
    }
}

int ri_engine_assign_insert(struct RIEngine *e, uint32_t unit, int owner) {
    if (!e)
        return -2;
    return ri_route_assign(&e->route, unit, owner);
}

void ri_engine_fx_set(struct RIEngine *e, uint32_t id, uint8_t value) {
    if (!e)
        return;
    switch (id) {
    case RI_FXID_DIST_DRIVE:
        e->dist.drive = value > 127u ? 127u : value;
        break;
    case RI_FXID_DIST_SHAPE:
        e->dist.shape = value > 127u ? 127u : value;
        break;
    case RI_FXID_COMP_THRESH:
        ri_fxcomp_set(&e->comp, value);
        break;
    case RI_FXID_COMP_RATIO:
        ri_fxcomp_set_ratio(&e->comp, value);
        break;
    case RI_FXID_DELAY_STEPS:
        e->delay.steps = value < 1u ? 1u : (value > 32u ? 32u : value);
        break;
    case RI_FXID_DELAY_TRIPLET:
        e->delay.triplet = value ? 1u : 0u;
        break;
    case RI_FXID_DELAY_FB:
        /* Send topology is always wet (no dry path): mix hardwired. */
        e->delay.fb = (float)(value > 127u ? 127u : value) *
            (0.8f / 127.0f);
        e->delay.mix = 1.0f;
        break;
    case RI_FXID_DELAY_MIX:
        break; /* send topology is always wet (no dry path) */
    case RI_FXID_DELAY_RETPAN:
        e->dret_pan = value > 127u ? 127u : value;
        break;
    case RI_FXID_PCF_BASE:
        e->pcf_base = value;
        break;
    case RI_FXID_PCF_Q:
        e->pcf_q = value;
        break;
    case RI_FXID_PCF_AMT:
        e->pcf_amt = value;
        break;
    case RI_FXID_PCF_MODE:
        e->pcf_mode = value > 2u ? 2u : value;
        break;
    case RI_FXID_PCF_PATTERN:
        e->pcf_pattern = value > 54u ? 54u : value;
        break;
    case RI_FXID_PCF_DECAY:
        e->pcf_decay = value;
        break;
    default:
        return;
    }
    if (id == RI_FXID_PCF_BASE || id == RI_FXID_PCF_Q ||
        id == RI_FXID_PCF_AMT || id == RI_FXID_PCF_MODE ||
        id == RI_FXID_PCF_PATTERN || id == RI_FXID_PCF_DECAY)
        ri_fx_pcf_apply_raw(&e->pcf, e->pcf_base, e->pcf_q, e->pcf_amt,
            e->pcf_mode, e->pcf_pattern, e->pcf_decay);
}

int ri_engine_set_pan(struct RIEngine *e, uint32_t section, uint8_t v) {
    if (!e || section >= RI_ROUTE_NSECTIONS)
        return 2;
    e->pan[section] = v > 127u ? 127u : v;
    return 0;
}

int ri_engine_set_send(struct RIEngine *e, uint32_t section, uint8_t v) {
    if (!e || section >= RI_ROUTE_NSECTIONS)
        return 2;
    e->send[section] = v > 127u ? 127u : v;
    return 0;
}

void ri_engine_set_tempo(struct RIEngine *e, float bpm) {
    if (!e)
        return;
    if (!(bpm >= 20.0f))
        bpm = 20.0f;
    if (bpm > 500.0f)
        bpm = 500.0f;
    e->tempo = bpm;
}

int ri_engine_set_delay(struct RIEngine *e, float *buf, uint32_t cap) {
    uint8_t steps, triplet, fb128;
    if (!e)
        return 2;
    if (!buf) {
        e->dline = 0; /* detach: dry default */
        e->dcap = 0;
        return 0;
    }
    if (cap < 64u)
        return 2;
    /* init wipes knob fields: snapshot and restore (attach keeps sound). */
    steps = e->delay.steps;
    triplet = e->delay.triplet;
    fb128 = (uint8_t)(e->delay.fb * (127.0f / 0.8f));
    if (ri_fxdelay_init(&e->delay, buf, cap) != 0)
        return 2;
    e->delay.steps = steps < 1u ? 1u : (steps > 32u ? 32u : steps);
    e->delay.triplet = triplet ? 1u : 0u;
    e->delay.fb = (float)(fb128 > 127u ? 127u : fb128) * (0.8f / 127.0f);
    e->delay.mix = 1.0f; /* send topology: always wet */
    e->dline = buf;
    e->dcap = cap;
    return 0;
}

float ri_engine_comp_gr(const struct RIEngine *e) {
    if (!e)
        return 0.0f;
    return ri_fxcomp_gr_db(&e->comp);
}

float ri_engine_section_peak(const struct RIEngine *e, uint32_t section) {
    if (!e || section >= RI_ROUTE_NSECTIONS)
        return 0.0f;
    return ri_meter_peak(&e->sec_meter[section]);
}

float ri_engine_fx_peak(const struct RIEngine *e, uint32_t unit) {
    if (!e || unit >= RI_ENGINE_FX_COUNT)
        return 0.0f;
    return ri_meter_peak(&e->fx_meter[unit]);
}

int ri_engine_909_bind(struct RIEngine *e, uint32_t voice,
    const struct RISampleLayer *layers, uint32_t n) {
    if (!e)
        return 2;
    return rb909_set_layers(&e->s909, voice, layers, n) == 0 ? 0 : 2;
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
    uint32_t lane, sound, voice;
    if (!e || !ev)
        return;
    /* Drum sections (§12.7a/m64): lane state arrives as NOTE_ON
     * (voice = value = lane, ACCENT flag = 909 high level), flam as
     * FLAM (value = width), total accent as ACCENT to voice ALL.
     * The AC event sorts after same-sample hits, so it accents the
     * voices stamped at this cursor (303-style retroactive accent). */
    if (ev->device == 2u || ev->device == 3u) {
        switch (ev->type) {
        case RI_EV_NOTE_ON:
            lane = ev->voice;
            if (lane >= RI_DRUM_CLASSIC_LANES)
                return; /* reserved rack lanes: later slice */
            if (ev->device == 2u) {
                sound = e->s808.slot[lane];
                rb808_trigger(&e->s808, sound,
                    (ev->flags & RI_EVFLAG_ACCENT) ? 1u : 0u, 0.0f);
                e->tag808[sound] = e->cursor;
            } else {
                voice = RI_LANE_TO_RB909_VOICE[lane];
                rb909_trigger(&e->s909, voice,
                    (ev->flags & RI_EVFLAG_ACCENT) ? 1u : 0u, 64, 0);
                e->tag909[voice] = e->cursor;
            }
            return;
        case RI_EV_FLAM:
            /* Emitter sets voice = lane; map to the engine voice id. */
            if (ev->device == 3u && ev->voice < RI_DRUM_CLASSIC_LANES)
                rb909_arm_flam(&e->s909,
                    RI_LANE_TO_RB909_VOICE[ev->voice], ev->value);
            return; /* 808 has no flam */
        case RI_EV_ACCENT:
            if (ev->voice != RI_VOICE_ALL)
                return;
            if (ev->device == 2u) {
                for (sound = 0; sound < RI_808_NSOUNDS; sound++)
                    if (e->tag808[sound] == e->cursor)
                        e->s808.v[sound].accent = 1u;
            } else {
                for (voice = 0; voice < RI_909_NVOICES; voice++)
                    if (e->tag909[voice] == e->cursor)
                        e->s909.v[voice].accent = 1u;
            }
            return;
        default:
            return; /* one-shots: no gate to release */
        }
    }
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
        if (((ev->value & 0xFF00u) == 0x0A00u)) {
            /* Task 5a: FX block is voiceless — straight to the knob
             * setter (unknown IDs are ignored inside, MIX stays put
             * by topology). */
            ri_engine_fx_set(e, ev->value, (uint8_t)(ev->flags & 127u));
            return;
        }
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
            float sendbus[RI_ENGINE_BLOCK], retbus[RI_ENGINE_BLOCK];
            double rgl, rgr;
            uint32_t i;
            if (cc > RI_ENGINE_BLOCK)
                cc = RI_ENGINE_BLOCK;
            for (i = 0; i < cc; i++)
                ml[i] = mr[i] = 0.0;
            for (i = 0; i < cc; i++)
                sendbus[i] = 0.0f;
            if (e->sections & RI_ENGINE_S303A) {
                rb303_render(&e->v303a, e->scratch, cc, sr);
                engine_section(e, 0, ml, mr, sendbus, cc, sr);
            }
            if (e->sections & RI_ENGINE_S303B) {
                rb303_render(&e->v303b, e->scratch, cc, sr);
                engine_section(e, 1, ml, mr, sendbus, cc, sr);
            }
            if (e->sections & RI_ENGINE_S808) {
                rb808_render_mix(&e->s808, e->scratch, cc, sr);
                engine_section(e, 2, ml, mr, sendbus, cc, sr);
            }
            if (e->sections & RI_ENGINE_S909) {
                rb909_render_mix(&e->s909, e->scratch, cc, sr);
                engine_section(e, 3, ml, mr, sendbus, cc, sr);
            }
            /* Shared delay send: one line over the summed post-insert
             * sends; stereo return with its own pan (NULL = dry). */
            if (e->dline) {
                ri_fxdelay_resync(&e->delay, e->tempo, sr);
                ri_fxdelay_render(&e->delay, sendbus, retbus, cc);
                ri_meter_feed(&e->fx_meter[RI_ENGINE_FX_DELAY], retbus, cc);
                engine_pan_gains(e->dret_pan, &rgl, &rgr);
                for (i = 0; i < cc; i++) {
                    ml[i] += (double)retbus[i] * rgl;
                    mr[i] += (double)retbus[i] * rgr;
                }
            }
            /* Master comp: stereo-linked (one detector, both buses). */
            if (ri_route_owner(&e->route, RI_ROUTE_COMP) ==
                RI_ROUTE_MASTER) {
                float tl[RI_ENGINE_BLOCK], tr[RI_ENGINE_BLOCK];
                ri_fxcomp_set_rate(&e->comp, sr);
                for (i = 0; i < cc; i++) {
                    tl[i] = (float)ml[i];
                    tr[i] = (float)mr[i];
                }
                ri_fxcomp_render_linked(&e->comp, tl, tr, cc);
                ri_meter_feed(&e->fx_meter[RI_ENGINE_FX_COMP], tl, cc);
                for (i = 0; i < cc; i++) {
                    ml[i] = (double)tl[i];
                    mr[i] = (double)tr[i];
                }
            }
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
