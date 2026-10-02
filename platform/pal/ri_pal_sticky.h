/* ri_pal_sticky.h — the durable-volume list, in a header so both the AROS
 * probe and its host test read ONE copy.
 *
 * RAM: is wiped by every reboot, and RIAPP.LOG is the evidence every
 * on-target run is judged from. The 2026-10-02 Dell session lost its log
 * twice: the host reboot took /tmp/opencode/fix_f.log (346925 B), and the
 * owner's reboot took the guest's RAM:RIAPP.LOG. The figures survived in a
 * transcript; the artefacts did not.
 *
 * This is a header rather than a static array in fs_aros.c for a reason that
 * is easy to get wrong: fs_aros.c is AROS-only and `#error`s a host build, so
 * a host test cannot include it. The first version of t153 therefore tested a
 * *mirror* of the list, and every mutant of the real file survived — a
 * mutation set of pure theatre. One definition, included by both, is what
 * makes the test able to fail.
 *
 * Order is the contract: Vk4aros: is the Dell's own stick and is tried
 * first, because a generic USB name may be a different device on another
 * machine. RAM: is deliberately absent from the table and is the fallback
 * (RI_PAL_STICKY_FALLBACK) — listing it would make every stick-less path
 * behave exactly as before while the Dell path kept silently losing logs.
 *
 * C99, <stdint.h> only (portability plan §2 gate).
 */
#ifndef RI_PAL_STICKY_H
#define RI_PAL_STICKY_H

/* Probed in order; the first that mounts wins. A fixed table, not a scan,
 * so the cost does not depend on how many devices are attached. */
#define RI_PAL_STICKY_COUNT 6
static const char *const ri_pal_sticky_vols[RI_PAL_STICKY_COUNT] = {
    "Vk4aros:", "USB0:", "USB1:", "UMSD0:", "UMSD1:", "USBDISK0:"
};

/* Used when nothing in the table is mounted: a machine with neither a stick
 * nor a scratch disk still logs somewhere rather than not at all. */
#define RI_PAL_STICKY_FALLBACK "RAM:"

#endif