/* midi_io/midi_chan.c — device instances and their channels (M4a). */
#include "midi_io/midi_chan.h"

void midi_chan_defaults(struct RIMidiChan *t) {
    uint32_t i;
    if (!t)
        return;
    for (i = 0u; i < RI_MCHAN_MAX; i++) {
        t->role[i] = 0u;
        t->ch[i] = 0u;
    }
    t->n = 2u;
    t->pad = 0u;
    t->role[RI_MCHAN_REMOTE] = RI_MCHAN_REMOTE;
    t->ch[RI_MCHAN_REMOTE] = 1u;
    t->role[RI_MCHAN_LEVI] = RI_MCHAN_LEVI;
    t->ch[RI_MCHAN_LEVI] = 2u;
}

int midi_chan_role(const struct RIMidiChan *t, uint8_t ch) {
    uint32_t i;
    if (!t || ch < 1u || ch > 16u)
        return -1;
    for (i = 0u; i < t->n && i < RI_MCHAN_MAX; i++)
        if (t->ch[i] == ch)
            return (int)t->role[i];
    return -1;
}

int midi_chan_bind(struct RIMidiChan *t, uint8_t role, uint8_t ch) {
    uint32_t i, mine = RI_MCHAN_MAX;
    if (!t || ch < 1u || ch > 16u)
        return 2;
    for (i = 0u; i < t->n && i < RI_MCHAN_MAX; i++)
        if (t->role[i] == role) {
            mine = i;
            break;
        }
    if (mine >= RI_MCHAN_MAX) {              /* a new device */
        if (t->n >= RI_MCHAN_MAX)
            return 2;
        mine = t->n++;
    }
    /* Another device's channel is refused, not stolen: the remote must
     * keep working while a second instrument is plugged in. */
    for (i = 0u; i < t->n; i++)
        if (i != mine && t->ch[i] == ch)
            return RI_MCHAN_TAKEN;
    t->ch[mine] = ch;
    t->role[mine] = role;
    return 0;
}

uint32_t midi_chan_n(const struct RIMidiChan *t) {
    return t ? t->n : 0u;
}