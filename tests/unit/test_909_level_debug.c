#include <stdio.h>
#include <string.h>
#include "engine/engine.h"
#include "gui/panelctl.h"
#include "gui/ctlreg.h"
#include "engine/seq/ctlplane.h"
#include "tests/helpers/ri_assert.h"

int main(void) {
    struct RIEngine e;
    memset(&e, 0, sizeof e);
    ri_engine_init(&e);
    ri_engine_defaults(&e);
    
    struct RIControlPlane pl;
    ri_ctl_init(&pl);
    
    // Check initial BD level
    printf("Initial BD level: %f\n", e.s909.v[0].level);
    
    // Send BD Level = 50 via panel
    const struct RICtlDef *d = ri_ctlreg_find(0x0301);
    printf("Found control: %s/%s bind=%d engine_id=%04x voice=%d\n", 
           d->group, d->legend, d->bind, d->engine_id, d->voice);
    
    uint16_t key = ri_ctlreg_auto_id(d);
    printf("Auto ID: %04x\n", key);
    
    int rc = ri_panel_ctl_send(&pl, d->reg_id, 50);
    printf("Panel send rc: %d\n", rc);
    
    struct RIEvent ev[8];
    uint32_t seq = 0, nd;
    nd = ri_ctl_drain(&pl, ev, 8u, 0u, &seq);
    printf("Drained %u events\n", nd);
    for (uint32_t q = 0; q < nd; q++) {
        printf("Event: type=%d value=%04x flags=%04x\n", ev[q].type, ev[q].value, ev[q].flags);
        ri_engine_apply_event(&e, &ev[q]);
    }
    
    printf("BD level after: %f\n", e.s909.v[0].level);
    RI_RESULT("test_909_level_debug");
    return 0;
}
