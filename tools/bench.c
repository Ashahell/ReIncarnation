/* tools/bench — worst-case fixture benchmark (Task 14, gate G14).
 * Plan file list (Task 1 §file-list) named this TU; it lands here.
 *
 * Worst case per 64-frame block: 2x 303 voices retriggered, all 15 808
 * voices at max decay retriggered every block, all 6 909 voices
 * retriggered every block, PCF + delay/dist/comp chain, 4-bus mixer
 * render, meter tap. Two full passes must checksum-identical (FNV-1a
 * over output float bits) or the bench fails: a soak that cannot prove
 * determinism proves nothing.
 *
 * usage: bench [blocks]   (default 45000 = 60 s of audio at 48 kHz)
 * Exit: 0 ok (+ "BENCH OK"), 1 nondeterministic/internal, 2 usage.
 * No allocation, no file IO except stdout. Timing idiom mirrors the
 * t1_808 storm gate (clock() + uname).
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/utsname.h>
#include "engine/dsp/rb303.h"
#include "engine/dsp/rb808.h"
#include "engine/dsp/rb909.h"
#include "engine/dsp/kernels.h"
#include "engine/fx/pcf.h"
#include "engine/fx/fx.h"
#include "engine/mixer/mixer.h"

#define BENCH_SR 48000.0f
#define BENCH_BLOCK 64u
#define BENCH_DEFAULT_BLOCKS 45000u
#define BENCH_MAX_BLOCKS 450000u /* 10x default: 10 min audio, hard cap */

static float B_B0[BENCH_BLOCK];
static float B_B1[BENCH_BLOCK];
static float B_B2[BENCH_BLOCK];
static float B_B3[BENCH_BLOCK];
static float B_T0[BENCH_BLOCK];
static float B_T1[BENCH_BLOCK];
static float B_OUT[BENCH_BLOCK];
static float B_SEND[BENCH_BLOCK];
static float B_DLINE[4096];

static uint64_t fnv1a_step(uint64_t h, float f) {
    uint32_t w;
    unsigned int i;
    memcpy(&w, &f, 4);
    for (i = 0; i < 4u; i++) {
        h ^= (uint64_t)((w >> (i * 8u)) & 0xffu);
        h *= 1099511628211ull;
    }
    return h;
}

/* One full pass over `blocks` worst-case blocks. Returns checksum. */
static uint64_t bench_pass(uint32_t blocks) {
    struct RB303Voice v303a, v303b;
    struct RB808Set s808;
    struct RB909Set s909;
    struct PCF pcf;
    struct RiFXDelay dly;
    struct RiFXDist dst;
    struct RiFXComp cmp;
    struct RiMixer mx;
    struct RiMeter mt;
    const float *buses[RI_MIX_NBUS];
    uint64_t h = 1469598103934665603ull;
    uint32_t b, i, v;
    int flip = 0;

    rb303_init(&v303a);
    rb303_init(&v303b);
    rb808_init_set(&s808);
    rb808_max_decay(&s808);
    rb909_init_set(&s909);
    pcf_init(&pcf);
    pcf_set_tempo(&pcf, 174.0f); /* densest chase grid */
    ri_fxdelay_init(&dly, B_DLINE, (uint32_t)(sizeof B_DLINE / sizeof B_DLINE[0]));
    ri_fxdelay_sync(&dly, 174.0f, 0.75f, BENCH_SR);
    ri_fxdelay_set(&dly, 96u, 64u);
    ri_fxdist_init(&dst);
    ri_fxdist_set(&dst, 96u, 64u);
    ri_fxcomp_init(&cmp, BENCH_SR);
    ri_fxcomp_set(&cmp, 64u);
    ri_mix_init(&mx, BENCH_SR);
    ri_mix_set_fader(&mx, 0u, 127u);
    ri_mix_set_fader(&mx, 1u, 127u);
    ri_mix_set_fader(&mx, 2u, 127u);
    ri_mix_set_fader(&mx, 3u, 127u);
    ri_mix_set_master(&mx, 127u);
    ri_meter_init(&mt, BENCH_SR);
    buses[0] = B_B0;
    buses[1] = B_B1;
    buses[2] = B_B2;
    buses[3] = B_B3;

    for (b = 0; b < blocks; b++) {
        /* Max-density retrigger: every voice, every block. */
        if ((b & 7u) == 0u) {
            flip = !flip;
            rb303_note(&v303a, (uint8_t)(flip ? 45 : 33), 1, 1);
            rb303_note(&v303b, (uint8_t)(flip ? 57 : 40), 0, 1);
        }
        for (v = 0; v < RI_808_NVOICES; v++)
            rb808_trigger(&s808, v, 1u, 0.0f);
        for (v = 0; v < RI_909_NVOICES; v++)
            rb909_trigger(&s909, v, 1u, 64u, RI_909_FLAM_DEFAULT_SMP);
        rb303_render(&v303a, B_B0, BENCH_BLOCK, BENCH_SR);
        rb303_render(&v303b, B_T0, BENCH_BLOCK, BENCH_SR);
        for (i = 0; i < BENCH_BLOCK; i++)
            B_B0[i] = B_B0[i] + B_T0[i]; /* 303 pair sums to bus 0 */
        rb808_render_mix(&s808, B_B1, BENCH_BLOCK, BENCH_SR);
        rb909_render_mix(&s909, B_B2, BENCH_BLOCK, BENCH_SR);
        for (i = 0; i < BENCH_BLOCK; i++)
            B_B3[i] = 0.0f;
        pcf_render(&pcf, B_B0, B_T0, BENCH_BLOCK, BENCH_SR);
        for (i = 0; i < BENCH_BLOCK; i++)
            B_B0[i] = B_T0[i];
        ri_fxdelay_render(&dly, B_B1, B_T1, BENCH_BLOCK);
        for (i = 0; i < BENCH_BLOCK; i++)
            B_B1[i] = B_T1[i];
        ri_fxdist_render(&dst, B_B2, B_T1, BENCH_BLOCK);
        for (i = 0; i < BENCH_BLOCK; i++)
            B_B2[i] = B_T1[i];
        ri_mix_render(&mx, buses, B_OUT, B_SEND, BENCH_BLOCK);
        ri_fxcomp_render(&cmp, B_OUT, B_T1, BENCH_BLOCK);
        ri_meter_feed(&mt, B_T1, BENCH_BLOCK);
        for (i = 0; i < BENCH_BLOCK; i++)
            h = fnv1a_step(h, B_T1[i]);
    }
    return h;
}

int main(int argc, char **argv) {
    struct utsname un;
    uint32_t blocks = BENCH_DEFAULT_BLOCKS;
    uint64_t h1, h2;
    clock_t c0, c1;
    double cpu, audio_sec;
    long acc = 0;
    const char *p;
    if (argc > 2) {
        printf("usage: bench [blocks]\n");
        return 2;
    }
    if (argc == 2) {
        p = argv[1];
        if (*p == '\0') {
            printf("bench: empty blocks\n");
            return 2;
        }
        while (*p >= '0' && *p <= '9') {
            acc = acc * 10 + (*p - '0');
            if (acc > (long)BENCH_MAX_BLOCKS) {
                printf("bench: blocks cap %u\n", BENCH_MAX_BLOCKS);
                return 2;
            }
            p++;
        }
        if (*p != '\0' || acc <= 0) {
            printf("bench: bad blocks '%s'\n", argv[1]);
            return 2;
        }
        blocks = (uint32_t)acc;
    }
    uname(&un);
    c0 = clock();
    h1 = bench_pass(blocks);
    c1 = clock();
    cpu = (double)(c1 - c0) / (double)CLOCKS_PER_SEC;
    h2 = bench_pass(blocks);
    if (h1 != h2) {
        printf("bench: NONDETERMINISTIC %016llx vs %016llx\n",
            (unsigned long long)h1, (unsigned long long)h2);
        return 1;
    }
    audio_sec = (double)blocks * (double)BENCH_BLOCK / 48000.0;
    printf("BENCH machine=%s/%s blocks=%u audio=%.1fs cpu=%.3fs ratio=%.4f headroom=%.1fx checksum=%016llx DETERMINISTIC\n",
        un.sysname, un.machine, blocks, audio_sec, cpu,
        cpu / audio_sec, audio_sec / cpu,
        (unsigned long long)h1);
    printf("BENCH OK\n");
    return 0;
}
