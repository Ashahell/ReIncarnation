/* panelctl.c — panel value change -> control plane bodies (G9b Step 2).
 * See header for the law. Render-contract safe: bounded, caller storage. */
#include "gui/panelctl.h"
#include "gui/ctlreg.h"
#include "engine/seq/ctlplane.h"

int ri_panel_ctl_send(struct RIControlPlane *ctl, uint16_t reg_id, int value) {
    const struct RICtlDef *d;
    uint16_t key;
    uint8_t v;
    if (!ctl)
        return 2;
    d = ri_ctlreg_find(reg_id);
    if (!d)
        return 1;
    key = ri_ctlreg_auto_id(d);
    if (key == 0u)
        return 1;
    v = (value < 0) ? 0u : (value > 127) ? 127u : (uint8_t)value;
    return (ri_ctl_send(ctl, key, v) == 0) ? 0 : 1;
}
