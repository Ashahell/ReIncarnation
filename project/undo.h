/* undo.h — 200-deep parameter undo stack (Task 13, gate G13).
 * Commit-on-release = one undo unit (spec §13 knob behavior): each
 * gesture end pushes exactly one (ctl, value) record. Fixed array, no
 * allocation; new commits clear the redo tail.
 */
#ifndef RI_UNDO_H
#define RI_UNDO_H
#include <stdint.h>

#define RI_UNDO_DEPTH 200u

struct RIUndoEntry {
    uint32_t ctl;
    uint8_t val;
};

struct RIUndo {
    struct RIUndoEntry hist[RI_UNDO_DEPTH];
    uint32_t len; /* committed entries */
    uint32_t pos; /* next undo pops hist[pos-1]; redo replays hist[pos] */
};

void ri_undo_init(struct RIUndo *u);
/* 0 ok, 1 when the stack is full (oldest is NOT rotated: callers must
 * undo or reset — silent rotation would lose user data). */
int ri_undo_commit(struct RIUndo *u, uint32_t ctl, uint8_t val);
int ri_undo_undo(struct RIUndo *u, uint32_t *ctl, uint8_t *val);
int ri_undo_redo(struct RIUndo *u, uint32_t *ctl, uint8_t *val);
uint32_t ri_undo_depth(const struct RIUndo *u);
#endif
