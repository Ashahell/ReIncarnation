/* t30_modpack — M2.5, TC-2.12.1 (shipped mod round-trip) +
 * TC-2.12.5 (S909 layers change sound):
 *
 *   - Inventory: the shipped classic-01 pack lists 14 layers via
 *     rbnm_pack_layers (ids/voices/rates/frames/lo-hi sane:
 *     44.1/48 kHz rates only, frames > 0, lo <= hi).
 *   - Every layer's samples load finite via rbnm_load_smpl (no
 *     empty/shorted layer hides in the pack).
 *   - Round-trip: reserialize → byte-identical (mirrors t1 §15 on
 *     the shipped file, the TC-2.12.1 "saves unchanged" half).
 *   - S909 render-diff (TC-2.12.5): two shipped layers (BD-LOW vs
 *     BD-HI) rendered through ri_layer_mix at the same tune differ
 *     pervasively (the pack changes sound vs any single default);
 *     same-layer double render identical (D1).
 *
 * CWD convention: repo root (pack path, same as t1_formats).
 * Analysis may use libm (tests/ only).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "project/rbnm.h"
#include "engine/dsp/rb909.h"

#define T30_SR 48000u
#define T30_PACK "reference/packs/classic-01/pack.rbnm"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static float LA[88200], LB[88200];
static float OA[4096], OB[4096];

static int finite_buf(const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++) {
        float v = b[i] < 0.0f ? -b[i] : b[i];
        if (!(v < 1e30f))
            return 0;
    }
    return 1;
}

int main(void) {
    struct RBNMLayerInfo info[16];
    char err[192];
    int32_t nl, f0, f1;
    uint32_t r0, r1, i;
    static unsigned char A[1 << 20], B[1 << 20];
    FILE *f;
    long na = 0, nb = 0;

    /* --- inventory: 14 layers, sane rows --- */
    nl = rbnm_pack_layers(T30_PACK, info, 16, err, sizeof err);
    CHECK(nl == 14, "layers %d want 14 (%s)", nl, err);
    for (i = 0; i < 14 && i < 16; i++) {
        CHECK(info[i].frames > 0, "layer %u empty", i);
        CHECK(info[i].lo <= info[i].hi, "layer %u lo>hi", i);
        CHECK(info[i].rate == 44100u || info[i].rate == 48000u,
            "layer %u rate %u", i, info[i].rate);
    }
    if (nl == 14)
        printf("inventory: 14 layers sane\n");

    /* --- every layer loads finite --- */
    for (i = 0; i < 14 && fails == 0; i++) {
        int32_t fr = rbnm_load_smpl(T30_PACK, info[i].id, LA, 88200u,
            &r0, err, sizeof err);
        CHECK(fr > 0, "load %s: %s", info[i].id, err);
        if (fr > 0)
            CHECK(finite_buf(LA, (uint32_t)fr), "nonfinite %s",
                info[i].id);
    }

    /* --- round-trip: reserialize byte-identical --- */
    CHECK(rbnm_reserialize(T30_PACK, "/tmp/ri/run/t30_pack2.rbnm", err,
        sizeof err) == 0, "reser: %s", err);
    f = fopen(T30_PACK, "rb");
    if (f) {
        na = (long)fread(A, 1, sizeof A, f);
        fclose(f);
    }
    f = fopen("/tmp/ri/run/t30_pack2.rbnm", "rb");
    if (f) {
        nb = (long)fread(B, 1, sizeof B, f);
        fclose(f);
    }
    CHECK(na > 0 && na == nb && memcmp(A, B, (size_t)na) == 0,
        "pack round-trip %ld/%ld", na, nb);

    /* --- S909 render-diff: BD-LOW vs BD-HI differ pervasively --- */
    f0 = rbnm_load_smpl(T30_PACK, "BD-LOW", LA, 88200u, &r0, err,
        sizeof err);
    f1 = rbnm_load_smpl(T30_PACK, "BD-HI", LB, 88200u, &r1, err,
        sizeof err);
    CHECK(f0 > 4096 && f1 > 4096, "fixture frames %d/%d", f0, f1);
    if (f0 > 4096 && f1 > 4096) {
        struct RISampleLayer L[2];
        uint32_t ndiff = 0;
        memset(L, 0, sizeof L);
        L[0].data = LA;
        L[0].frames = (uint32_t)f0;
        L[0].rate = r0;
        L[0].lo = 0;
        L[0].hi = 127;
        L[1].data = LB;
        L[1].frames = (uint32_t)f1;
        L[1].rate = r1;
        L[1].lo = 0;
        L[1].hi = 127;
        for (i = 0; i < 4096; i++) {
            OA[i] = ri_layer_mix(L, 1, 64, (float)i);
            OB[i] = ri_layer_mix(L + 1, 1, 64, (float)i);
            if (OA[i] != OB[i])
                ndiff++;
        }
        CHECK(ndiff > 4000u, "layers too close (%u/4096 differ)", ndiff);
        for (i = 0; i < 4096; i++)
            OB[i] = ri_layer_mix(L, 1, 64, (float)i);
        for (i = 0; i < 4096; i++)
            CHECK(OA[i] == OB[i], "remix nondet at %u", i);
        printf("S909 render-diff: %u/4096 differ, remix identical\n", ndiff);
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t30_modpack\n");
    return fails != 0;
}
