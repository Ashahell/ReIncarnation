/*
 * rbng.datatype.c — RBNG datatype class shell (Task 13, gate G13).
 *
 * AROS-ONLY. Song files load through the datatypes system (spec §13:
 * skins/samples through picture.class/sound.class; 8SVX/SMUS-style
 * conventions reused where they fit). The CODEC lives in
 * project/rbng.c (host-tested); this shell only wires the datatype
 * dispatcher to rbng_read_song. Missing SKIN art takes the
 * partial-art fallback (rbng_art_fallback), never a load failure.
 * Must NEVER enter the host build (probe_ahi.c precedent).
 */

#ifndef __AROS__
#error "rbng.datatype.c is AROS-only: datatype shell, never in the host build"
#endif

#include <exec/types.h>
#include "project/rbng.h"

/* Magic probe: FORM .... RBNG (big-endian IFF, even-pad chunks). */
LONG ri_rbng_datatype_probe(const UBYTE *magic, ULONG n) {
    if (!magic || n < 12u)
        return 0;
    if (magic[0] != 'F' || magic[1] != 'O' || magic[2] != 'R' ||
        magic[3] != 'M')
        return 0;
    if (magic[8] != 'R' || magic[9] != 'B' || magic[10] != 'N' ||
        magic[11] != 'G')
        return 0;
    return 1;
}

/* Load entry: validates + decodes; reports the art-fallback state so
 * the app can mount the placeholder skin (TC-2.12.x partial-art). */
LONG ri_rbng_datatype_load(const char *path, struct RISong *song,
    LONG *fallback) {
    static char err[192];
    LONG rc;
    if (!path || !song)
        return 0;
    rc = (LONG)rbng_read_song(path, song, err, sizeof err);
    if (rc != 0)
        return 0;
    if (fallback)
        *fallback = (LONG)rbng_art_fallback(song);
    return 1;
}
