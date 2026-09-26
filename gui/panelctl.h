/* gui/panelctl.h — front-panel value change -> control plane (§12.11 G9b Step 2).
 * Pure C, host-tested. One path to the engine for every value control on
 * a canvas: reg_id (section << 8 | index, the canvas last-hit vocabulary)
 * -> lane key via ri_ctlreg_auto_id -> ri_ctl_send. The AROS panel calls
 * this (never ri_ctl_send directly); pattern edits and transport use
 * their own pure routes (banks/track, au_live_request).
 */
#ifndef RI_PANELCTL_H
#define RI_PANELCTL_H
#include <stdint.h>

struct RIControlPlane;

/* One canvas value change. Returns 0 sent (exactly one message queued),
 * 1 nothing sent (unknown reg_id, or the control has no engine delivery
 * and its key is 0, or the key was refused), 2 bad args (NULL plane).
 * value is clamped to 0..127. */
int ri_panel_ctl_send(struct RIControlPlane *ctl, uint16_t reg_id, int value);
#endif
