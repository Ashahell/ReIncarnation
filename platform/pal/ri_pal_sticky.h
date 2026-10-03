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
 * machine.
 *
 * WHERE RIAPP.LOG GOES -- the rule, because it has been got wrong repeatedly:
 *
 *   stick if one mounts -> T: if it mounts -> RAM: as a last resort
 *
 *   NEVER hand-place a log. ri_pal_path(RI_PATH_TEMP, ...) owns this decision
 *   and app/riapp.c asks it. Placing a log by hand is how RAM:RIAPP.LOG came to
 *   be read as "the fallback is broken" on riqemu1 when T: had been working all
 *   along (owner, 2026-10-03).
 *
 * History, because the change is easy to over-read:
 *
 *  - 2026-10-02: fallback moved RAM: -> T: (owner decision 2). T: is the
 *    scratch disk, itself RAM-backed on this guest, so it is "somewhere better
 *    than RAM:", NOT durable. Durability is the candidate table's job, and only
 *    a mounted stick provides it.
 *  - 2026-10-03: T: joined the table as a PROBED last candidate and RAM: became
 *    the final fallback. Previously "T:" was returned unprobed, so a guest
 *    without a scratch disk got a path it could not write to and the log went
 *    nowhere at all. The owner: "RAM: can be a fallback in case T: isn't
 *    available."
 *  - On the Dell all of this is a no-op: Vk4aros: is present and mounts first.
 *
 * C99, <stdint.h> only (portability plan §2 gate).
 */
#ifndef RI_PAL_STICKY_H
#define RI_PAL_STICKY_H

/* Probed in order; the first that mounts wins. A fixed table, not a scan,
 * so the cost does not depend on how many devices are attached.
 *
 * T: is LAST in the table and is PROBED, not assumed (owner 2026-10-03).
 * The earlier version returned "T:" unconditionally when no stick was
 * mounted, which meant a guest without a scratch disk was handed a path it
 * could not write to and the log went nowhere -- which is exactly how
 * "the RAM: fallback is broken" got believed on riqemu1 when T: was in fact
 * working the whole time. Probing costs one Lock() and makes the answer true
 * on every guest rather than on the ones that happen to have T:. */
#define RI_PAL_STICKY_COUNT 7
static const char *const ri_pal_sticky_vols[RI_PAL_STICKY_COUNT] = {
    "Vk4aros:", "USB0:", "USB1:", "UMSD0:", "UMSD1:", "USBDISK0:", "T:"
};

/* Last resort, used ONLY when the whole table fails to probe -- no stick AND
 * no scratch disk. RAM: is wiped by every reboot, so it is the worst place for
 * evidence and the reason it is last (owner 2026-10-03: "RAM: can be a
 * fallback in case T: isn't available"). A log that cannot be written at all
 * is worse than one that dies at the next reboot, so the chain ends here rather
 * than refusing. */
#define RI_PAL_STICKY_FALLBACK "RAM:"

#endif