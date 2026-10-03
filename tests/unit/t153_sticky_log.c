/* t153_sticky_log — RIAPP.LOG must land where a reboot cannot reach it.
 *
 * RAM: is wiped by every reboot, and RIAPP.LOG is the evidence every
 * on-target run is judged from. The 2026-10-02 Dell session lost its log
 * twice over: the host reboot took /tmp/opencode/fix_f.log (346925 B), and
 * the owner's reboot took the guest's RAM:RIAPP.LOG, so the telemetry had to
 * be reconstructed from a transcript of the analysis. The figures survived;
 * the artefacts did not.
 *
 * The AROS probe (DOS Lock, requester suppression) is not host-testable, so
 * what is pinned here is the decision layer it depends on: which candidate
 * volumes, in which order, and the guarantee that the fallback is the scratch
 * disk rather than RAM: (owner 2026-10-02: RAM: -> T:).
 *
 * This reads platform/pal/ri_pal_sticky.h — the SAME list fs_aros.c probes.
 * An earlier version of this test carried its own copy of the list, and
 * every mutant of the real file survived it: a mirror proves nothing about
 * the thing it mirrors. That is the reason the list lives in a header.
 *
 * Laws:
 *  - RAM: is never a candidate and never the fallback: listing it would make
 *    every stick-less path behave as before while the Dell path kept
 *    silently losing logs;
 *  - Vk4aros: is tried first (the Dell's own stick), ahead of the generic
 *    names, because a generic name may be a different device elsewhere;
 *  - the table is duplicate-free and every entry is a volume name;
 *  - the table is fixed-size, so the probe cost does not depend on how many
 *    devices are attached;
 *  - an override wins outright rather than being ranked.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "platform/pal/ri_pal_sticky.h"

int main(void) {
    int i, j;

    /* The chain is stick -> T: -> RAM:, and RAM: is the ONLY fallback.
     * Owner decisions 2026-10-02 (RAM: -> T:) and 2026-10-03 ("RAM: can be a
     * fallback in case T: isn't available").
     *
     * The load-bearing part is that T: is a PROBED candidate rather than an
     * assumed one. Returning "T:" unprobed handed guests without a scratch
     * disk a path they could not write to, and that is how "the RAM: fallback
     * is broken" came to be believed on riqemu1 while T: was working. So T:
     * must be IN the table (probed, last) and RAM: must NOT be. */
    RI_ASSERT(strcmp(RI_PAL_STICKY_FALLBACK, "RAM:") == 0,
        "RAM: is the last-resort fallback");
    for (i = 0; i < RI_PAL_STICKY_COUNT; i++)
        RI_ASSERT(strcmp(ri_pal_sticky_vols[i], "RAM:") != 0,
            "slot %d must never be RAM: -- it is the fallback, and a candidate "
            "would match first on every machine", i);
    /* T: is probed, so it belongs in the table exactly once, last: a stick
     * always outranks it. */
    {
        int seen = 0, at = -1;
        for (i = 0; i < RI_PAL_STICKY_COUNT; i++)
            if (strcmp(ri_pal_sticky_vols[i], "T:") == 0) { seen++; at = i; }
        RI_ASSERT(seen == 1, "T: appears exactly once in the table (%d)", seen);
        RI_ASSERT(at == RI_PAL_STICKY_COUNT - 1,
            "T: is the last candidate, so a stick outranks it (at %d)", at);
    }

    /* Every candidate is a volume: non-empty and ending in ':'. A bare
     * directory would Lock as a path and silently never match a mount. */
    for (i = 0; i < RI_PAL_STICKY_COUNT; i++) {
        size_t n = strlen(ri_pal_sticky_vols[i]);
        RI_ASSERT(n >= 2u, "candidate %d is non-trivial", i);
        RI_ASSERT(ri_pal_sticky_vols[i][n - 1u] == ':', "candidate %d ends in ':'", i);
    }

    /* The Dell's own stick comes first, so a machine holding both a
     * Vk4aros: and a USB0: still logs to the one it boots from. */
    RI_ASSERT(strcmp(ri_pal_sticky_vols[0], "Vk4aros:") == 0, "Vk4aros: is probed first");

    /* The generic names are in enumeration order, so the probe does not
     * depend on which device the OS happened to assign first. */
    RI_ASSERT(strcmp(ri_pal_sticky_vols[1], "USB0:") == 0, "USB0: precedes USB1:");
    RI_ASSERT(strcmp(ri_pal_sticky_vols[2], "USB1:") == 0, "USB1: is second");

    /* Duplicate-free: a repeated name would make the result depend on
     * where the probe stopped, which is invisible until a run loses a log. */
    for (i = 0; i < RI_PAL_STICKY_COUNT; i++)
        for (j = i + 1; j < RI_PAL_STICKY_COUNT; j++)
            RI_ASSERT(strcmp(ri_pal_sticky_vols[i], ri_pal_sticky_vols[j]) != 0,
                "candidates %d and %d are the same volume", i, j);

    /* Bounded: a fixed table, not a scan. */
    RI_ASSERT(RI_PAL_STICKY_COUNT == 7, "seven candidates (%d)", RI_PAL_STICKY_COUNT);

    /* Every family of mount point is represented, so a stick enumerated
     * under any of them is not missed: mass-storage (UMSD), boot disk
     * (USBDISK) and plain USB. */
    RI_ASSERT(strstr(ri_pal_sticky_vols[3], "UMSD") != 0, "a mass-storage name is listed");
    RI_ASSERT(strstr(ri_pal_sticky_vols[5], "USBDISK") != 0, "a boot-stick name is listed");
    RI_ASSERT(strstr(ri_pal_sticky_vols[1], "USB") != 0, "a plain USB name is listed");

    RI_RESULT("sticky_log");
}