/* t21_seq — TC-2.1.1 master-clock 10-minute accumulation (gate: WBS 2.1).
 * Simulates 10 minutes of 64-frame buffers through RiSeqAdvanceFrames and
 * pins, per buffer boundary: (a) MasterClock equals the exact integer
 * sample total (no float accumulation anywhere in the seq path); (b) the
 * nearest-integer tick to the closed-form ideal
 * (samples*PPQ*BPM/(60*SR), long-double oracle) maps back through
 * clock.c within +-1 sample (the clock's documented rounding bound).
 * Final totals must be exact integers (115200 ticks both rates).
 * A second sweep runs the same pin over a 44100 Hz map (device-rate
 * representative; the seq itself runs at locked RI_AUDIO_SR) to prove
 * exactness is rate-independent.
 * Deterministic: no randomness, no wall clock. ~863k boundaries, fast.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/seq/riseq.h"
#include "engine/seq/clock.h"

#define T21_PPQ 96u
#define T21_BPM 120u
#define T21_BLOCK 64u
#define T21_SECS 600u

/* One buffer step: advance, check counter exactness, and at
 * integer-ideal boundaries check map agreement within +-1. Returns 1
 * on failure. */
static int check_buf(struct RISeq *s, const struct RITempoMap *map,
                     uint32_t sr, uint64_t b, uint32_t frames,
                     uint64_t *samples, uint64_t *exact_points) {
    uint64_t want_samples = *samples + (uint64_t)frames;
    long double ideal, dist;
    uint64_t t_near;
    RiSeqAdvanceFrames(s, frames);
    *samples += (uint64_t)frames;
    if (RiSeqMasterClock(s) != want_samples) {
        printf("FAIL sr %u buf %llu: clock %llu want %llu\n",
               sr, (unsigned long long)b,
               (unsigned long long)RiSeqMasterClock(s),
               (unsigned long long)want_samples);
        return 1;
    }
    ideal = (long double)*samples * (long double)T21_PPQ *
            (long double)T21_BPM / ((long double)60 * (long double)sr);
    dist = fabsl(ideal - roundl(ideal));
    if (dist < 1e-6L) {
        long double back, berr;
        t_near = (uint64_t)(ideal + 0.5L);
        back = (long double)ri_map_tick(map, t_near);
        berr = fabsl(back - (long double)*samples);
        (*exact_points)++;
        if (!(berr <= 1.0L)) {
            printf("FAIL sr %u buf %llu: map err %Lf (tick %llu)\n",
                   sr, (unsigned long long)b, berr,
                   (unsigned long long)t_near);
            return 1;
        }
    }
    return 0;
}

static int check_run(uint32_t sr, uint64_t total_samples) {
    struct RISegment seg;
    struct RITempoMap map;
    static struct RISeq s_storage;
    struct RISeq *s = &s_storage;
    uint64_t nbufs, b, samples = 0ULL, exact_points = 0ULL;
    seg.start_tick = 0ULL;
    seg.ns_per_quarter = 500000000ULL; /* 120 BPM */
    map.segs = &seg;
    map.n = 1u;
    map.ppq = T21_PPQ;
    map.sr = sr;
    s = &s_storage;
    RiSeqInit(s, NULL, T21_PPQ);
    nbufs = total_samples / T21_BLOCK;
    for (b = 0ULL; b < nbufs; b++) {
        if (check_buf(s, &map, sr, b, T21_BLOCK, &samples, &exact_points))
            return 1;
    }
    if (total_samples % T21_BLOCK) {
        if (check_buf(s, &map, sr, nbufs, (uint32_t)(total_samples % T21_BLOCK),
                      &samples, &exact_points))
            return 1;
    }
    if (samples != total_samples) {
        printf("FAIL sr %u: total %llu want %llu\n", sr,
               (unsigned long long)samples,
               (unsigned long long)total_samples);
        return 1;
    }
    if (exact_points == 0ULL) {
        printf("FAIL sr %u: no integer-ideal boundaries checked\n", sr);
        return 1;
    }
    /* Final ideal must be an exact integer (115200 both rates). */
    {
        long double ideal = (long double)total_samples * T21_PPQ * T21_BPM /
                            ((long double)60 * (long double)sr);
        uint64_t t_final = (uint64_t)(ideal + 0.5L);
        if (t_final != 115200ULL || fabsl(ideal - 115200.0L) > 1e-9L) {
            printf("FAIL sr %u: final ideal %Lf (tick %llu), want 115200\n",
                   sr, ideal, (unsigned long long)t_final);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    int fails = 0;
    /* Seq path at locked engine rate. */
    fails += check_run(48000u, 48000u * T21_SECS);
    /* Map-scale pin at the device-representative rate (rate-independence). */
    fails += check_run(44100u, 44100u * T21_SECS);
    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t21_seq\n");
    return fails != 0;
}
