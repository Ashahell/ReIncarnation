/* ridevice.c — RIDevice static registry, Classic (Task 11, gate G11).
 * No allocation; no IO; the table is file-static, four slots.
 */
#include "engine/framework/ridevice.h"

static struct RIDevice RI_DEVICES[RI_DEVICE_COUNT];

void ri_devices_init(void) {
    uint32_t i;
    RI_DEVICES[0].name = "303A";
    RI_DEVICES[1].name = "303B";
    RI_DEVICES[2].name = "808";
    RI_DEVICES[3].name = "909";
    for (i = 0; i < RI_DEVICE_COUNT; i++) {
        RI_DEVICES[i].render = 0;
        RI_DEVICES[i].ctx = 0;
    }
}

struct RIDevice *ri_device_get(uint32_t index) {
    if (index >= RI_DEVICE_COUNT)
        return 0;
    return &RI_DEVICES[index];
}

uint32_t ri_device_count(void) {
    return RI_DEVICE_COUNT;
}
