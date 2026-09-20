/*
 * rbnm.datatype.c — RBNM datatype class shell (Task 13, gate G13).
 *
 * AROS-ONLY. Sample packs load through the datatypes system (spec
 * §13). The CODEC lives in project/rbnm.c (host-tested S909 subset +
 * Task-13 full: CPRG + verbatim reserialize); this shell only wires
 * the dispatcher to rbnm_validate_file/rbnm_read_cprg. A pack without
 * CPRG loads with the copyright fallback (CPRG-on-load hook reports
 * absent, TC-2.12.x) — never a load failure. Must NEVER enter the
 * host build (probe_ahi.c precedent).
 */

#ifndef __AROS__
#error "rbnm.datatype.c is AROS-only: datatype shell, never in the host build"
#endif

#include <exec/types.h>
#include "project/rbnm.h"

/* Magic probe: FORM .... RBNM (big-endian IFF, even-pad chunks). */
LONG ri_rbnm_datatype_probe(const UBYTE *magic, ULONG n) {
    if (!magic || n < 12u)
        return 0;
    if (magic[0] != 'F' || magic[1] != 'O' || magic[2] != 'R' ||
        magic[3] != 'M')
        return 0;
    if (magic[8] != 'R' || magic[9] != 'B' || magic[10] != 'N' ||
        magic[11] != 'M')
        return 0;
    return 1;
}

/* Load entry: validates the pack; reports the CPRG hook state
 * (present=0 → copyright fallback mounted by the app). */
LONG ri_rbnm_datatype_load(const char *path, LONG *cprg_present) {
    static char err[192];
    static char cprg[136];
    int present = 0;
    if (!path)
        return 0;
    if (rbnm_validate_file(path, err, sizeof err) != 0)
        return 0;
    if (rbnm_read_cprg(path, cprg, sizeof cprg, &present, err,
            sizeof err) != 0)
        return 0;
    if (cprg_present)
        *cprg_present = (LONG)present;
    return 1;
}
