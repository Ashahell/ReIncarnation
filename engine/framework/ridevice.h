/* ridevice.h — RIDevice static registry, Classic (Task 11, gate G11).
 * Spec §6 + WBS §0 (LOCKED architecture): RIDevice = DSP + panel +
 * control map + mod hooks; the four Classic sections are the first four
 * registered devices; the dummy-device generality proof (TC-2.1.5 /
 * TC-2.9.5) registers, renders, and unregisters through these three
 * calls with zero framework edits (registration = storing a device
 * struct through the returned pointer; removal = restoring it).
 * No dynamic loading in Classic: the table is static, four slots, fixed
 * at init. Appendix D C signatures are sketch, NOT ABI (freeze
 * checklist still open, OPEN-07); the signatures below are
 * executor-defined. Render path: no allocation, no IO.
 */
#ifndef RI_RIDEVICE_H
#define RI_RIDEVICE_H
#include <stdint.h>

#define RI_DEVICE_COUNT 4u

struct RIDevice {
    const char *name; /* static string, never owned */
    void (*render)(void *ctx, float *out, uint32_t n, float sr);
    void *ctx;
};

/* Exact registry surface (no register/unregister calls: the table is
 * static by design; tests prove generality without framework edits). */
void ri_devices_init(void);
struct RIDevice *ri_device_get(uint32_t index);
uint32_t ri_device_count(void);
#endif
