/* project/stem_render.c — R8d. See stem_render.h for the laws. */
#include <string.h>
#include "project/stem_render.h"

/* The five section names, in section order. These are the engine's section
 * indices (0=303A 1=303B 2=808 3=909 4=Levi), not the panel's lane names --
 * a stem is a MIXER STRIP, so it is named for the machine that mixed it. */
static const char *const RI_STEMR_NAMES[RI_ROUTE_NSECTIONS] = {
    "303a", "303b", "808", "909", "levi"
};

uint32_t ri_stemr_name(char *out, uint32_t cap, uint32_t index,
    uint32_t section) {
    static const char dec[] = "0123456789";
    uint32_t at, i;
    if (!out || cap < 16u || section >= RI_ROUTE_NSECTIONS)
        return 0u;
    /* Two DECIMAL digits, zero padded, and refused above 99 rather than
     * printed with more: a three-digit index would sort before "01-" in
     * some file browsers and break the ordering the index is there for.
     * (I first wrote this with hex shifts and produced "00-808.wav" for
     * index 3 -- a two-digit field is not a two-hex-digit field.) */
    if (index == 0u || index > 99u)
        return 0u;
    out[0] = dec[(index / 10u) % 10u];
    out[1] = dec[index % 10u];
    at = 2u;
    out[at++] = '-';
    for (i = 0u; RI_STEMR_NAMES[section][i] != '\0'; i++)
        out[at++] = RI_STEMR_NAMES[section][i];
    out[at++] = '.';
    out[at++] = 'w';
    out[at++] = 'a';
    out[at++] = 'v';
    out[at] = '\0';
    return at;
}

/* The only bit depths stem_wav_write can actually produce. Refusing rather
 * than clamping: the caller's file format is not this module's to choose, and
 * a silent 16 would be a file that is not what anybody asked for. */
static int depth_ok(uint32_t d) {
    return d == STEM_WAV_PCM16 || d == STEM_WAV_PCM24 || d == STEM_WAV_F32;
}

int ri_stemr_begin(struct RIStemr *r, uint32_t nstems, uint32_t frames,
    uint32_t rate, uint32_t depth, uint32_t bpm_milli,
    const struct RIStemrSlot *slots) {
    uint32_t i;
    if (!r || !slots || nstems == 0u || nstems > RI_ROUTE_NSECTIONS ||
        frames == 0u || rate == 0u || !depth_ok(depth))
        return 1;
    for (i = 0u; i < nstems; i++)
        if (!slots[i].l || !slots[i].r || !slots[i].out)
            return 1;
    memset(r, 0, sizeof *r);
    stem_set_init(&r->set, rate, bpm_milli);
    r->set.depth = (uint8_t)depth;
    r->frames = frames;
    r->rate = rate;
    r->depth = depth;
    r->nstems = (uint8_t)nstems;
    for (i = 0u; i < nstems; i++) {
        r->tap.l[i] = slots[i].l;
        r->tap.r[i] = slots[i].r;
        r->outs[i] = slots[i].out;
    }
    return 0;
}

void ri_stemr_bind(struct RIStemr *r, struct RIEngine *e) {
    uint32_t i, k;
    if (!r || !e || !r->nstems)
        return;
    /* ZERO FIRST, THEN ATTACH. A caller who attaches and renders without
     * this gets a stem whose tail is the previous song's audio, because the
     * tap writes only what the engine renders. */
    for (i = 0u; i < r->nstems; i++) {
        if (!r->tap.l[i] || !r->tap.r[i])
            continue;
        for (k = 0u; k < r->frames; k++) {
            r->tap.l[i][k] = 0.0f;
            r->tap.r[i][k] = 0.0f;
        }
    }
    ri_engine_set_stems(e, &r->tap);
    /* Read the mask HERE, not at finish: the caller may change the engine's
     * section mask between bind and finish, and the stems should describe
     * the render that actually happened. */
    r->sections = e->sections;
    r->bound = 1u;
}

uint32_t ri_stemr_finish(struct RIStemr *r, uint32_t rendered) {
    uint32_t i, k;
    if (!r || !r->bound || r->finished || rendered == 0u)
        return 1u;
    /* REFUSED, NOT PADDED. A stem the caller believes is complete but which
     * ends in silence is a lie about the song; counting it is how the lie
     * becomes visible. */
    if (rendered != r->frames) {
        r->short_count++;
        return 1u;
    }
    for (i = 0u; i < r->nstems; i++) {
        uint32_t sec = i;
        float *l, *rt, *out;
        if (!r->tap.l[sec] || !r->tap.r[sec])
            continue;
        /* A slot offered for a section the engine did NOT render is not a
         * stem. Handing the set a file of silence labelled as an instrument
         * is worse than handing it one file fewer. */
        if ((r->sections & (1u << sec)) == 0u)
            continue;
        l = r->tap.l[sec];
        rt = r->tap.r[sec];
        /* The out buffer is not in the tap, so it comes from the slots the
         * caller handed at begin; they are the same pointers by contract. */
        out = 0;
        for (k = 0u; k < r->nstems; k++)
            if (r->tap.l[k] == l && r->tap.r[k] == rt && r->outs[k])
                out = r->outs[k];
        if (!out)
            continue;
        for (k = 0u; k < r->frames; k++) {
            out[k * 2u] = l[k];
            out[k * 2u + 1u] = rt[k];
        }
        if (stem_set_add(&r->set, out, r->frames, 2u) != 0)
            return 1u;
    }
    r->finished = 1u;
    return 0u;
}
