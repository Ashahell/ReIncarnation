/* ctlplane.c — control-plane bodies (G9.1). See header for the law.
 * Render-contract safe: bounded loops, caller-owned storage only. */
#include "engine/seq/ctlplane.h"
#include "engine/seq/autolane.h"

#define RI_CTL_MASK (RI_CTL_CAP - 1u)

static uint16_t ctl_device(uint16_t key) {
    uint32_t blk = (uint32_t)key & 0xFF00u;
    if (blk == RI_AUTO_BLK_808)
        return 2u;
    if (blk == RI_AUTO_BLK_909)
        return 3u;
    if (blk != 0x0300u)
        return 0u;
    return (uint16_t)(((uint32_t)key >> 4u) & 1u);
}

void ri_ctl_init(struct RIControlPlane *p) {
    uint32_t i;
    if (!p)
        return;
    for (i = 0u; i < RI_CTL_CAP; i++) {
        p->buf[i].key = 0u;
        p->buf[i].val = 0u;
        p->buf[i].flags = 0u;
    }
    p->head = 0u;
    p->tail = 0u;
    p->dropped = 0u;
    p->refused = 0u;
}

int ri_ctl_send(struct RIControlPlane *p, uint16_t key, uint8_t val) {
    uint32_t pending, i;
    if (!p)
        return 2;
    if (!ri_auto_allowed(key)) {
        p->refused++;
        return 2;
    }
    pending = p->head - p->tail;
    if (pending < RI_CTL_CAP) {
        p->buf[p->head & RI_CTL_MASK].key = key;
        p->buf[p->head & RI_CTL_MASK].val = val;
        p->buf[p->head & RI_CTL_MASK].flags = 0u;
        p->head++;
        return 0;
    }
    for (i = p->head; i > p->tail; i--) {
        uint32_t at = (i - 1u) & RI_CTL_MASK;
        if (p->buf[at].key == key) {
            p->buf[at].val = val;
            p->buf[at].flags = 0u;
            p->dropped++;
            return 0;
        }
    }
    p->tail++;
    p->buf[p->head & RI_CTL_MASK].key = key;
    p->buf[p->head & RI_CTL_MASK].val = val;
    p->buf[p->head & RI_CTL_MASK].flags = 0u;
    p->head++;
    p->dropped++;
    return 0;
}

uint32_t ri_ctl_pending(const struct RIControlPlane *p) {
    if (!p)
        return 0u;
    if (p->head < p->tail)
        return 0u;
    return p->head - p->tail;
}

uint32_t ri_ctl_drain(struct RIControlPlane *p, struct RIEvent *out,
    uint32_t cap, uint64_t sample, uint32_t *seq) {
    uint32_t n = 0u;
    if (!p || !out || cap == 0u || !seq)
        return 0u;
    if (p->head < p->tail)
        return 0u;
    while (p->tail < p->head && n < cap) {
        struct RIControlMsg m = p->buf[p->tail & RI_CTL_MASK];
        out[n].sample = sample;
        out[n].type = RI_EV_AUTOMATION;
        out[n].device = ctl_device(m.key);
        out[n].voice = 0u;
        out[n].value = m.key;
        out[n].flags = (uint16_t)(m.val & 127u);
        out[n].seq = (*seq)++;
        p->tail++;
        n++;
    }
    return n;
}
