/* project/stem_render.c — R8d. See stem_render.h for the laws. */
#include <string.h>
#include "project/stem_render.h"

/* The five section names, in section order. These are the engine's section
 * indices (0=303A 1=303B 2=808 3=909 4=Levi), not the panel's lane names --
 * a stem is a MIXER STRIP, so it is named for the machine that mixed it. */
static const char *const RI_STEMR_NAMES[RI_ROUTE_NSECTIONS] = {
    "303a", "303b", "808", "909", "levi"
};

/* The two FX names, continuing the section numbering. */
static const char *const RI_STEMR_FXNAMES[RI_STEM_FX_N] = {
    "fx-send", "fx-return"
};

uint32_t ri_stemr_fx_name(char *out, uint32_t cap, uint32_t index,
    uint32_t fx) {
    static const char dec[] = "0123456789";
    uint32_t at, i;
    if (!out || cap < 20u || fx >= RI_STEM_FX_N)
        return 0u;
    if (index == 0u || index > 99u)
        return 0u;
    out[0] = dec[(index / 10u) % 10u];
    out[1] = dec[index % 10u];
    at = 2u;
    out[at++] = '-';
    for (i = 0u; RI_STEMR_FXNAMES[fx][i] != '\0'; i++)
        out[at++] = RI_STEMR_FXNAMES[fx][i];
    out[at++] = '.';
    out[at++] = 'w';
    out[at++] = 'a';
    out[at++] = 'v';
    out[at] = '\0';
    return at;
}

void ri_stemr_fx_bind(struct RIStemr *r, uint32_t fx, float *buf) {
    if (!r || fx >= RI_STEM_FX_N)
        return;
    r->fx[fx] = buf;
}

void ri_stemr_fx_out(struct RIStemr *r, float *buf) {
    if (r)
        r->fx_out = buf;
}

uint32_t ri_stemr_fx_set_index(const struct RIStemr *r, uint32_t fx) {
    if (!r || fx >= RI_STEM_FX_N)
        return RI_STEMR_NO_STEM;
    return r->fx_set_of[fx];
}

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
    /* Zero is a VALID set index, so a memset leaves every section looking
     * like it has a stem pointing at entry 0. NO_STEM is the honest
     * starting state. */
    for (i = 0u; i < RI_ROUTE_NSECTIONS; i++)
        r->set_of[i] = RI_STEMR_NO_STEM;
    for (i = 0u; i < RI_STEM_FX_N; i++)
        r->fx_set_of[i] = RI_STEMR_NO_STEM;
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
    /* THE FX BUFFERS ARE ZEROED HERE TOO, AND FOR THE SAME REASON as the
     * section buffers: the engine writes only the samples it renders, so
     * without this a short render leaves the previous song's delay tail at
     * the end of a file. A NULL entry means "not asked for" and is skipped
     * rather than treated as a fault -- every existing caller is in that
     * state and would otherwise have to change. */
    for (i = 0u; i < RI_STEM_FX_N; i++) {
        uint32_t k;
        if (!r->fx[i])
            continue;
        for (k = 0u; k < r->frames; k++)
            r->fx[i][k] = 0.0f;
    }
    for (i = 0u; i < r->nstems; i++) {
        if (!r->tap.l[i] || !r->tap.r[i])
            continue;
        for (k = 0u; k < r->frames; k++) {
            r->tap.l[i][k] = 0.0f;
            r->tap.r[i][k] = 0.0f;
        }
    }
    /* THE FX POINTERS ARE COPIED INTO THE TAP, AND THAT IS THE WHOLE
     * BUG THIS FUNCTION HAD. `r->fx[]` and `r->tap.fx[]` are two different
     * arrays: the caller fills the first (ri_stemr_fx_bind) and the ENGINE
     * reads the second (`e->stems->fx[...]`). Without this copy the engine
     * saw NULL for both buses, skipped the write, and `--fx` produced
     * correctly named, correctly sized, EMPTY files.
     *
     * The section pointers never had this problem because `ri_stemr_begin`
     * fills `r->tap.l[]` directly -- so the FX path acquired a second way
     * of holding a pointer, and only one of them reached the engine. */
    for (i = 0u; i < RI_STEM_FX_N; i++)
        r->tap.fx[i] = r->fx[i];
    ri_engine_set_stems(e, &r->tap);
    /* Read the mask HERE, not at finish: the caller may change the engine's
     * section mask between bind and finish, and the stems should describe
     * the render that actually happened. */
    r->sections = e->sections;
    r->bound = 1u;
}

uint32_t ri_stemr_finish(struct RIStemr *r, uint32_t rendered) {
    uint32_t i, k;
    float *out = 0;
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
        /* Recorded as UNSET first, so a section that is skipped below does
         * not leave a stale index from a previous render pointing at some
         * other section's bytes. */
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
         * caller handed at begin; they are the same pointers by contract.
         * Declared at function scope because the FX loop below reuses it --
         * it is the same interleave buffer the sections wrote into. */
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
        r->set_of[sec] = stem_set_count(&r->set) - 1u;
    }
    /* THE FX STEMS COME LAST, so the set reads strips then effects. They are
     * MONO and are written as a STEREO file with the same sample in both
     * channels rather than as a mono WAV: the set is a stereo stem set and
     * a mono entry among stereo ones is a file that imports with a channel
     * count nobody expected. The delay return is panned on the way into the
     * master, so the stem is the UNPANNED line -- which is the honest thing
     * to hand a DAW, since re-panning it is the point of exporting it. */
    /* THE FX STEMS GET THE CALLER'S OWN BUFFER, OR NOTHING. Looking one of
     * the section slots up -- which is what this did first -- SILENTLY
     * OVERWRITES that section's audio, because the set holds POINTERS and
     * nothing is copied until the write. The 303A stem came out silent. */
    if (r->fx_out) {
        out = r->fx_out;
            for (i = 0u; i < RI_STEM_FX_N; i++) {
            uint32_t k;
            if (!r->fx[i])
                continue;
            /* A bus the engine never rendered is zero for the whole song, which
             * is indistinguishable from a silent delay. It IS added: the caller
             * asked for it by binding it, and an absent file is a worse answer
             * than a silent one that is honestly named. */
            if (r->fx_set_of[i] != RI_STEMR_NO_STEM)
                continue;   /* already added on a re-entry */
            for (k = 0u; k < r->frames; k++) {
                out[k * 2u] = r->fx[i][k];
                out[k * 2u + 1u] = r->fx[i][k];
            }
            if (stem_set_add(&r->set, out, r->frames, 2u) != 0)
                return 1u;
            r->fx_set_of[i] = stem_set_count(&r->set) - 1u;
        }
    }
    r->finished = 1u;
    return 0u;
}

uint32_t ri_stemr_set_index(const struct RIStemr *r, uint32_t section) {
    if (!r || section >= RI_ROUTE_NSECTIONS)
        return RI_STEMR_NO_STEM;
    return r->set_of[section];
}
