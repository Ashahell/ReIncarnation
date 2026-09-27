/* pack_909.h — classic-01 909 pack load + bind (owner 2026-09-27). AROS-only. */
#ifndef __AROS__
#error "pack_909.h is AROS-only"
#endif
#ifndef RI_PACK_909_H
#define RI_PACK_909_H
#include <stdint.h>

struct RIEngine;

/* Bind every pack voice with samples (6 of 11 in classic-01: BD/SD/CH/OH/
 * CR/RD; toms/rim/clap stay silent). Returns voices bound; err filled
 * when the pack itself is missing/unreadable. Call BEFORE au_live_run
 * (idle-only swap rule). Layer buffers live for the process. */
int ri_pack_909_bind(struct RIEngine *eng, char *err, uint32_t errcap);

#endif
