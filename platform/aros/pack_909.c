/* pack_909.c — classic-01 909 pack load + bind (owner 2026-09-27).
 * AROS-only. Reads <PACKS>/classic-01/pack.rbnm through the shared rbnm
 * codec (stdio via libcrt on v11, stdcio on v1 — same as every tool) and
 * binds each voice's layers idle, exactly like tools/render.c --909pack.
 * Layer buffers are AllocVec'd once and live for the process (the engine
 * never frees; AllocVec/FreeVec pair only on the failure path). Voices
 * without pack layers stay silent (toms/rim/clap have no in-repo samples;
 * the caller logs that, §17 never-silent rule).
 */
#ifndef __AROS__
#error "pack_909.c is AROS-only (909 pack load)"
#endif

#include "platform/pal/ri_pal_fs.h"
#include "project/rbnm.h"
#include "engine/engine.h"
#include "engine/dsp/rb909.h"

#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>

#include <stdio.h>
#include <string.h>

#define PACK_LAYERS_MAX 3u
#define PACK_FRAMES_MAX 88200u /* 2 s at 44100, same cap as the render tool */

int ri_pack_909_bind(struct RIEngine *eng, char *err, uint32_t errcap) {
    char base[96], pack[128];
    struct RBNMLayerInfo info[16];
    int32_t nl, v, bound = 0;
    if (!eng)
        return 0;
    if (ri_pal_path(RI_PATH_PACKS, base, sizeof base) != 0 ||
        ri_pal_path_join(pack, sizeof pack, base, "classic-01/pack.rbnm") != 0) {
        if (err && errcap)
            snprintf(err, errcap, "pack path");
        return 0;
    }
    nl = rbnm_pack_layers(pack, info, 16u, err, errcap);
    if (nl < 0)
        return 0;
    for (v = 0; v < (int32_t)RI_909_NVOICES; v++) {
        struct RISampleLayer lay[PACK_LAYERS_MAX];
        float *bufs[PACK_LAYERS_MAX];
        uint32_t nl_v = 0u, rate0 = 0u;
        int32_t k;
        uint32_t j;
        for (k = 0; k < nl && nl_v < PACK_LAYERS_MAX; k++) {
            uint32_t rate = 0u;
            int32_t got;
            float *b;
            if (info[k].voice != (uint32_t)v || info[k].frames > PACK_FRAMES_MAX)
                continue;
            b = (float *)AllocVec(info[k].frames * sizeof(float), 0);
            if (!b)
                break;
            got = rbnm_load_smpl(pack, info[k].id, b, info[k].frames, &rate,
                err, errcap);
            if (got < 0 || rate == 0u) {
                FreeVec(b);
                break;
            }
            if (nl_v == 0u)
                rate0 = rate;
            else if (rate != rate0) {
                FreeVec(b);
                break;
            }
            bufs[nl_v] = b;
            lay[nl_v].data = b;
            lay[nl_v].frames = (uint32_t)got;
            lay[nl_v].rate = rate;
            lay[nl_v].lo = info[k].lo;
            lay[nl_v].hi = info[k].hi;
            nl_v++;
        }
        if (nl_v == 0u)
            continue;
        if (ri_engine_909_bind(eng, (uint32_t)v, lay, nl_v) != 0) {
            for (j = 0u; j < nl_v; j++)
                FreeVec(bufs[j]);
            continue;
        }
        bound++;
    }
    return bound;
}
