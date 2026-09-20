/* undo.c — fixed 200-deep undo (no allocation, no libm). */
#include "project/undo.h"

void ri_undo_init(struct RIUndo *u) {
    u->len = 0;
    u->pos = 0;
}

int ri_undo_commit(struct RIUndo *u, uint32_t ctl, uint8_t val) {
    if (!u)
        return 1;
    if (u->pos >= RI_UNDO_DEPTH && u->len >= RI_UNDO_DEPTH)
        return 1;
    if (u->pos >= RI_UNDO_DEPTH)
        return 1;
    u->hist[u->pos].ctl = ctl;
    u->hist[u->pos].val = val;
    u->pos++;
    u->len = u->pos; /* redo tail cleared */
    return 0;
}

int ri_undo_undo(struct RIUndo *u, uint32_t *ctl, uint8_t *val) {
    if (!u || u->pos == 0u)
        return 1;
    u->pos--;
    if (ctl)
        *ctl = u->hist[u->pos].ctl;
    if (val)
        *val = u->hist[u->pos].val;
    return 0;
}

int ri_undo_redo(struct RIUndo *u, uint32_t *ctl, uint8_t *val) {
    if (!u || u->pos >= u->len)
        return 1;
    if (ctl)
        *ctl = u->hist[u->pos].ctl;
    if (val)
        *val = u->hist[u->pos].val;
    u->pos++;
    return 0;
}

uint32_t ri_undo_depth(const struct RIUndo *u) {
    return u ? u->pos : 0u;
}
