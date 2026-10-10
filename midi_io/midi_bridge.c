/* midi_bridge.c — MIDI bridge core bodies (M2). See header for the law.
 * Render-contract safe: bounded, caller storage, no static state. */
#include "midi_io/midi_bridge.h"

#define CH_MASK (RI_MBR_CH_CAP - 1u)
#define IN_MASK (RI_MBR_IN_CAP - 1u)

void midi_bridge_init(struct RIMidiBridge *b) {
    uint32_t i;
    if (!b)
        return;
    midi_follow_init(&b->follow);
    for (i = 0u; i < RI_MBR_CH_CAP; i++) {
        b->ch[i].b[0] = b->ch[i].b[1] = b->ch[i].b[2] = 0u;
        b->ch[i].n = 0u;
        b->ch[i].pad[0] = b->ch[i].pad[1] = b->ch[i].pad[2] = 0u;
        b->ch[i].t_us = 0u;
    }
    for (i = 0u; i < RI_MBR_IN_CAP; i++) {
        b->in[i].it.kind = RI_FOLLOW_NONE;
        b->in[i].it.pad[0] = b->in[i].it.pad[1] = b->in[i].it.pad[2] = 0u;
        b->in[i].it.seek_16ths = 0u;
        b->in[i].t_us = 0u;
    }
    /* Init on the owning task before sharing; plain stores safe here. */
    b->ch_head.v = 0u;
    b->ch_tail.v = 0u;
    b->ch_dropped = 0u;
    b->in_head.v = 0u;
    b->in_tail.v = 0u;
    b->in_dropped = 0u;
    b->camd_dropped = 0u;
    b->link_lost = 0u;
}

static void queue_ch(struct RIMidiBridge *b, uint8_t st, uint8_t d1,
    uint8_t d2, uint8_t n, uint64_t now_us) {
    uint32_t h = ri_atomic_load_acq(&b->ch_head);
    uint32_t t = ri_atomic_load_acq(&b->ch_tail);
    if (h - t >= RI_MBR_CH_CAP) { /* flood: oldest dropped and counted */
        ri_atomic_store_rel(&b->ch_tail, t + 1u);
        b->ch_dropped++;
    }
    b->ch[h & CH_MASK].b[0] = st;
    b->ch[h & CH_MASK].b[1] = d1;
    b->ch[h & CH_MASK].b[2] = d2;
    b->ch[h & CH_MASK].n = n;
    b->ch[h & CH_MASK].t_us = now_us;
    ri_atomic_store_rel(&b->ch_head, h + 1u);
}

static void queue_in(struct RIMidiBridge *b, const struct RIFollowIntent *it,
    uint64_t now_us) {
    uint32_t h = ri_atomic_load_acq(&b->in_head);
    uint32_t t = ri_atomic_load_acq(&b->in_tail);
    if (h - t >= RI_MBR_IN_CAP) {
        ri_atomic_store_rel(&b->in_tail, t + 1u);
        b->in_dropped++;
    }
    b->in[h & IN_MASK].it = *it;
    b->in[h & IN_MASK].t_us = now_us;
    ri_atomic_store_rel(&b->in_head, h + 1u);
}

static uint32_t voice_len(uint8_t st) {
    uint32_t hi = (uint32_t)st & 0xF0u;
    return (hi == 0xC0u || hi == 0xD0u) ? 2u : 3u;
}

void midi_bridge_feed(struct RIMidiBridge *b, uint8_t status, uint8_t d1,
    uint8_t d2, uint64_t now_us) {
    struct RIFollowIntent it;
    uint8_t n;
    if (!b)
        return;
    if (status >= 0xF0u) {
        /* Realtime + system common run the follower (one owner); an F2
         * pair always arrives whole in one CAMD message. */
        if (status == 0xF2u) {
            midi_follow_rt(&b->follow, status, now_us, &it);
            midi_follow_rt(&b->follow, (uint8_t)(d1 & 0x7Fu), now_us, &it);
            if (midi_follow_rt(&b->follow, (uint8_t)(d2 & 0x7Fu), now_us, &it) == 0 &&
                it.kind != RI_FOLLOW_NONE)
                queue_in(b, &it, now_us);
        } else if (midi_follow_rt(&b->follow, status, now_us, &it) == 0 &&
            it.kind != RI_FOLLOW_NONE) {
            queue_in(b, &it, now_us);
        }
        n = status >= 0xF8u ? 1u : 3u; /* CAMD triples; realtime is 1 byte */
        queue_ch(b, status, d1, d2, n, now_us);
        return;
    }
    /* Channel voice: the GUI owns it (follower never sees it). */
    n = (uint8_t)voice_len(status);
    queue_ch(b, status, d1, d2, n, now_us);
}

uint32_t midi_bridge_read_ch(struct RIMidiBridge *b, struct RIMidiMsg *out,
    uint32_t cap) {
    uint32_t t, h, n = 0u;
    if (!b || !out || cap == 0u)
        return 0u;
    t = ri_atomic_load_acq(&b->ch_tail);
    h = ri_atomic_load_acq(&b->ch_head);
    while (t < h && n < cap) {
        out[n] = b->ch[t & CH_MASK];
        t++;
        n++;
    }
    ri_atomic_store_rel(&b->ch_tail, t);
    return n;
}

uint32_t midi_bridge_read_in(struct RIMidiBridge *b, struct RIMidiIntent *out,
    uint32_t cap) {
    uint32_t t, h, n = 0u;
    if (!b || !out || cap == 0u)
        return 0u;
    t = ri_atomic_load_acq(&b->in_tail);
    h = ri_atomic_load_acq(&b->in_head);
    while (t < h && n < cap) {
        out[n] = b->in[t & IN_MASK];
        t++;
        n++;
    }
    ri_atomic_store_rel(&b->in_tail, t);
    return n;
}

uint32_t midi_bridge_pending_ch(const struct RIMidiBridge *b) {
    if (!b)
        return 0u;
    return ri_atomic_load_acq(&b->ch_head) - ri_atomic_load_acq(&b->ch_tail);
}

uint32_t midi_bridge_pending_in(const struct RIMidiBridge *b) {
    return b ? ri_atomic_load_acq(&b->in_head) - ri_atomic_load_acq(&b->in_tail) : 0u;
}

void midi_bridge_note_camd_drop(struct RIMidiBridge *b) {
    if (b)
        b->camd_dropped++;
}

void midi_bridge_note_link(struct RIMidiBridge *b, uint32_t lost) {
    if (b)
        b->link_lost = lost ? 1u : 0u;
}

void midi_settings_defaults(struct RIMidiSettings *s) {
    static const char dflt[] = "riapp";
    uint32_t i;
    if (!s)
        return;
    for (i = 0u; i < sizeof s->cluster; i++)
        s->cluster[i] = 0;
    for (i = 0u; dflt[i] && i < sizeof s->cluster - 1u; i++)
        s->cluster[i] = dflt[i];
    s->channel = 1u;
    s->sync = RI_SYNC_INTERNAL;
    s->levi_ch = 2u;
    s->clk_out = 0u;
    s->lat_ms = 0;
    s->mmc_out = 0u;   /* E0: off */
    s->pad = 0;
}

int midi_settings_set(struct RIMidiSettings *s, uint32_t field, long v) {
    if (!s)
        return 1;
    switch (field) {
    case RI_MIDI_SET_CHANNEL:
        if (v < 1L || v > 16L)
            return 1;
        s->channel = (uint8_t)v;
        return 0;
    case RI_MIDI_SET_SYNC:
        if (v != (long)RI_SYNC_INTERNAL && v != (long)RI_SYNC_MIDI)
            return 1;
        s->sync = (uint8_t)v;
        return 0;
    case RI_MIDI_SET_LEVI_CH:
        if (v < 1L || v > 16L)
            return 1;
        s->levi_ch = (uint8_t)v;
        return 0;
    case RI_MIDI_SET_CLK_OUT:
        if (v != 0L && v != 1L)
            return 1;
        s->clk_out = (uint8_t)v;
        return 0;
    case RI_MIDI_SET_LAT_MS:
        if (v < -200L || v > 200L)
            return 1;
        s->lat_ms = (int16_t)v;
        return 0;
    case RI_MIDI_SET_MMC_OUT:
        /* Strictly 0/1, like CLK_OUT. A truthy value here would enable a
         * feature that drives other people's transport from an E0 that
         * reads as a slider. */
        if (v != 0L && v != 1L)
            return 1;
        s->mmc_out = (uint8_t)v;
        return 0;
    default:
        break;
    }
    return 1;
}
