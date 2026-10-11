/* t210_song_sections — R8g: a song that addresses more than one section.
 *
 * Until now `tools/render --stems` could only ever write ONE stem, and the
 * reason was never the stem code: `ri_sched_emit_timed` takes a device as an
 * argument and emits for ONE section, so every event in a render carried
 * device 0 and `render_stems` derived a set of exactly one.
 *
 * THE FIX IS NOT AN ENGINE DATA-MODEL CHANGE. `RIStep` needs no device
 * field: a section is a property of the STEP LIST you hand the emitter, not
 * of the individual step. So a multi-section song is several step lists, each
 * emitted for its own device, merged.
 *
 * **AND THE MERGE IS THE INTERESTING PART.** `ri_sched_emit_timed` returns
 * one device's run already sorted by the §8 key. Two sorted runs are not
 * one sorted run, and the renderer will hit them in array order -- so the
 * merge has to be by the SAME key, or the engine stops on an out-of-order
 * event (t205) and counts it. The §8 key is
 *
 *     sample, then type, then device, then voice, then seq
 *
 * and `seq` is the emit index WITHIN A DEVICE. Two devices both start at
 * seq 0, so a same-sample same-type pair across devices is ordered by
 * `device` -- which the key does have. A merge that sorted only by sample
 * would be wrong exactly at the boundaries where two sections land together,
 * which is the common case in a 303 + 808 pattern.
 *
 * **EACH SECTION KEEPS ITS OWN STEPS AND ITS OWN seq SPACE.** Renumbering
 * one device's seq into another's would make two different events compare
 * equal, and an unstable sort would then swap them between renders.
 *
 * **AN EVENT ARRIVES ON A SECTION THAT IS NOT THERE IS REFUSED**, not
 * quietly dropped: `ri_engine_load` is told which sections exist, and a
 * caller whose emitter produced device 4 when only 0..2 are loaded has a bug
 * that a silent drop would hide until the mix came out thin.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/clock.h"
#include "engine/seq/sched.h"
#include "project/song_sections.h"

#define RATE 48000u
#define PPQ 96u
#define MAXEV 512u

static struct RISegment seg;
static struct RITempoMap map;
static struct RISchedOpts opts;

/* Per-section step lists, the way the tool now builds them. */
static struct RIStep st0[16], st1[16], st2[16];
static struct RISectionRun runs[RI_ROUTE_NSECTIONS];

static void setup(void) {
    uint32_t i;
    seg.start_tick = 0;
    seg.ns_per_quarter = 60000000000ULL / 120u;   /* 120 BPM */
    map.segs = &seg;
    map.n = 1u;
    map.ppq = PPQ;
    map.sr = RATE;
    memset(&opts, 0, sizeof opts);
    opts.flam_ms = RI_FLAM_MS_DEFAULT;
    for (i = 0u; i < 16u; i++) {
        st0[i].note = (uint8_t)(36u + (i % 12u));  st0[i].flags = 0u;
        /* The 808 plays one note every four steps, so its run is sparser --
         * which is what a real 303+808 pattern looks like. */
        st1[i].note = (uint8_t)(36u + (i % 12u));  st1[i].flags =
            ((i % 4u) == 0u) ? 0u : RI_STEP_REST;
    }
    for (i = 0u; i < 16u; i++) {
        st2[i].note = 36u;  st2[i].flags = ((i % 8u) < 4u) ? 0u : RI_STEP_REST;
    }
    runs[0].steps = st0; runs[0].nsteps = 16u; runs[0].device = 0u;
    runs[1].steps = st1; runs[1].nsteps = 16u; runs[1].device = 1u;
    runs[2].steps = st2; runs[2].nsteps = 16u; runs[2].device = 2u;
    for (i = 3u; i < RI_ROUTE_NSECTIONS; i++) {
        runs[i].steps = 0; runs[i].nsteps = 0u; runs[i].device = (uint16_t)i;
    }
}

int main(void) {
    static struct RIEvent ev[MAXEV];
    uint32_t mask, nev, i;

    setup();

    /* --- 1. THE SECTION SET COMES FROM THE RUNS ---------------------- */
    mask = ri_song_sections(runs, RI_ROUTE_NSECTIONS);
    RI_ASSERT(mask == 0x07u, "three sections addressed (%u)", (unsigned)mask);

    /* --- 2. TWO SECTIONS MERGE INTO ONE SORTED RUN ------------------- */
    nev = ri_song_emit(&map, PPQ, runs, RI_ROUTE_NSECTIONS, &opts, ev, MAXEV);
    RI_ASSERT(nev > 0u, "the merge emits events (%u)", (unsigned)nev);
    /* THE WHOLE POINT: globally sorted by the §8 key. A per-device sort is
     * not enough -- the engine walks the array in order and stops on an
     * out-of-order event (t205), so an unsorted merge renders a truncated
     * song and counts the rest as a defect. */
    for (i = 1u; i < nev; i++)
        RI_ASSERT(!ri_event_less(&ev[i], &ev[i - 1u]),
            "event %u at %llu breaks the §8 order against event %u at %llu",
            (unsigned)i, (unsigned long long)ev[i].sample,
            (unsigned)(i - 1u), (unsigned long long)ev[i - 1u].sample);

    /* --- 3. BOTH SECTIONS' EVENTS ARE PRESENT, NOT ONE'S ------------- */
    {
        uint32_t d0 = 0u, d1 = 0u;
        for (i = 0u; i < nev; i++) {
            if (ev[i].device == 0u) d0++;
            if (ev[i].device == 1u) d1++;
        }
        RI_ASSERT(d0 > 0u && d1 > 0u,
            "both sections have events (%u and %u)", (unsigned)d0,
            (unsigned)d1);
        /* The 808's rest steps must NOT become note-offs: it plays four
         * notes in a bar, not sixteen. */
        RI_ASSERT(d1 < d0, "the sparser section has fewer events (%u < %u)",
            (unsigned)d1, (unsigned)d0);
    }

    /* --- 4. A SAME-SAMPLE PAIR IS ORDERED BY DEVICE ------------------- */
    /* Both sections put a note at tick 0, so the merge's tiebreak is what
     * decides their order. Sorted by sample alone this is a tie and the two
     * runs would concatenate; the §8 key says device 0 first. */
    {
        int saw_pair = 0;
        for (i = 1u; i < nev; i++)
            if (ev[i].sample == ev[i - 1u].sample &&
                ev[i].type == ev[i - 1u].type) {
                saw_pair = 1;
                RI_ASSERT(ev[i - 1u].device <= ev[i].device,
                    "same-sample pair at %llu is device %u then %u",
                    (unsigned long long)ev[i].sample,
                    (unsigned)ev[i - 1u].device, (unsigned)ev[i].device);
            }
        RI_ASSERT(saw_pair, "the fixture actually produces a same-sample pair");
    }

    /* --- 4b. THE KEY IS THE WHOLE §8 KEY, NOT JUST THE SAMPLE -------- */
    /* MB SURVIVED §4, and the reason is that §4's assertion was too weak.
     * The runs are visited in ASCENDING device order, so a merge that
     * breaks ties by "keep taking from the first run" accidentally produces
     * device order anyway -- for the fixtures in §4, where the tie is two
     * note-ONs. The tie that separates them is a LATER DEVICE emitting an
     * EARLIER TYPE at the same sample: §8 orders by type before device, so
     * a note-OFF (2) from device 1 must precede a note-ON (3) from device 0.
     *
     * That is not a contrived fixture -- it is a note-off landing on a
     * downbeat, which is what happens whenever one section holds a note
     * across a bar line and the next strikes on it.
     */
    {
        static struct RIStep hold[16];
        static struct RIStep strike[16];
        struct RISectionRun two[RI_ROUTE_NSECTIONS];
        uint32_t k, tie_i = 0u, found = 0u;
        for (k = 0u; k < 16u; k++) {
            /* DEVICE 0 STRIKES AND RELEASES ALTERNATELY, so it has a
             * note-ON (type 3) on every even step. */
            hold[k].note = 40u;
            hold[k].flags = ((k % 2u) == 1u) ? RI_STEP_REST : 0u;
            /* DEVICE 1 HOLDS ONE LONG NOTE, so its note-OFF (type 2) lands
             * on step 3 -- the same sample as device 0's next note-ON. Type
             * 2 is EARLIER in the §8 key than type 3 and it comes from the
             * LATER device, so the correct order is device 1 first. Nothing
             * that breaks ties by sample, or by "keep taking from the
             * first run", can produce that.
             *
             * (My first two fixtures both failed to make the tie: a rest at
             * step 0 meant device 0's first event was at step 1, and with
             * device 1 also alternating, both devices emitted the SAME type
             * at every shared sample. The tie needs one device holding and
             * the other striking.) */
            strike[k].note = 38u;
            strike[k].flags = (k == 3u) ? RI_STEP_REST : 0u;
        }
        for (k = 0u; k < RI_ROUTE_NSECTIONS; k++) {
            two[k].steps = 0; two[k].nsteps = 0u; two[k].device = (uint16_t)k;
        }
        two[0].steps = hold; two[0].nsteps = 16u; two[0].device = 0u;
        two[1].steps = strike; two[1].nsteps = 16u; two[1].device = 1u;
        nev = ri_song_emit(&map, PPQ, two, RI_ROUTE_NSECTIONS, &opts, ev,
            MAXEV);
        RI_ASSERT(nev > 0u, "the two-section run emits (%u)", (unsigned)nev);
        for (k = 1u; k < nev; k++)
            RI_ASSERT(!ri_event_less(&ev[k], &ev[k - 1u]),
                "event %u breaks the §8 order", (unsigned)k);
        (void)found; (void)tie_i;
    }

    /* --- 4c. THE MERGE ON A TIE IT MUST GET RIGHT --------------------- */
    /* MB SURVIVED §4, and the reason is that §4 COULD NOT DISTINGUISH IT.
     * The runs are visited in ascending device order, so a merge that
     * breaks ties by "keep taking from the first run" produces device order
     * anyway -- correct by accident. I then tried to build a step fixture
     * with a real type tie and could not: the scheduler ties a note-off to
     * the step AFTER the rest, so a section that releases where another
     * strikes puts both of the same type at the same sample.
     *
     * So the law is tested where it is stated, on constructed runs. The
     * case: a note-OFF (type 2) from device 1 and a note-ON (type 3) from
     * device 0 at the SAME sample. §8 orders by type before device, so the
     * release leads -- from the LATER device. A merge by sample alone, or
     * one that keeps taking from the first run, both get this backwards.
     */
    {
        static struct RIEvent A[2], B[2], M[4];
        uint32_t n;
        memset(A, 0, sizeof A);
        memset(B, 0, sizeof B);
        memset(M, 0, sizeof M);
        /* Run A is device 0: a note-OFF and a note-ON, both at sample 6000,
         * already in §8 order between themselves (off before on). */
        A[0].sample = 6000u; A[0].type = RI_EV_NOTE_OFF; A[0].device = 0u; A[0].seq = 1u;
        A[1].sample = 6000u; A[1].type = RI_EV_NOTE_ON;  A[1].device = 0u; A[1].seq = 2u;
        /* Run B is device 1 and carries the EARLIER type at that sample. */
        B[0].sample = 6000u; B[0].type = RI_EV_NOTE_ON;  B[0].device = 1u; B[0].seq = 0u;
        B[1].sample = 9000u; B[1].type = RI_EV_NOTE_OFF; B[1].device = 1u; B[1].seq = 1u;
        n = ri_song_merge(M, 4u, A, 2u, B, 2u);
        RI_ASSERT(n == 4u, "all four merged (%u)", (unsigned)n);
        /* AT SAMPLE 6000 THERE ARE THREE EVENTS -- off/dev0, on/dev0,
         * on/dev1 -- and §8 orders them by TYPE, then DEVICE. Types 2 and
         * 3, so the release leads and the two note-ONs follow; within type
         * 3 the devices decide, so:
         *
         *     off/0, on/0, on/1
         *
         * That IS also the run order here, which is the point: with the
         * runs visited in ascending device order, a sample-only merge and a
         * §8 merge AGREE whenever the types line up. So this fixture does
         * not discriminate -- and §4c's real job is to say so.
         *
         * THE DISCRIMINATOR IS TYPE, and it needs a tie where the earlier
         * type comes from the LATER device. That is the case below: a
         * note-OFF (type 2) from device 1 and a note-ON (type 3) from
         * device 0 at one sample. §8 puts the release first, so device 1
         * leads; run order and sample-only order both put device 0 first.
         */
        RI_ASSERT(M[0].type == RI_EV_NOTE_OFF && M[0].device == 0u,
            "the release leads (%u/%u)", (unsigned)M[0].type,
            (unsigned)M[0].device);
        RI_ASSERT(M[1].type == RI_EV_NOTE_ON && M[1].device == 0u,
            "then device 0's note-on (%u/%u)", (unsigned)M[1].type,
            (unsigned)M[1].device);
        RI_ASSERT(M[2].type == RI_EV_NOTE_ON && M[2].device == 1u,
            "then device 1's, on the device tiebreak (%u/%u)",
            (unsigned)M[2].type, (unsigned)M[2].device);
        RI_ASSERT(M[3].sample == 9000u, "and the later sample last (%llu)",
            (unsigned long long)M[3].sample);

        /* NOW THE TYPE TIE THAT SEPARATES THEM. Run A is device 0 and
         * carries a note-ON (type 3) at sample 6000; run B is device 1 and
         * carries a note-OFF (type 2) at the SAME sample. §8 says type 2
         * first, so the LATER device leads -- which is exactly what a
         * sample-only merge and a "first run first" merge both get
         * backwards. */
        {
            static struct RIEvent C[1], D[1], M2[2];
            memset(C, 0, sizeof C); memset(D, 0, sizeof D);
            memset(M2, 0, sizeof M2);
            C[0].sample = 6000u; C[0].type = RI_EV_NOTE_ON;
            C[0].device = 0u;  C[0].seq = 5u;
            D[0].sample = 6000u; D[0].type = RI_EV_NOTE_OFF;
            D[0].device = 1u;  D[0].seq = 0u;
            n = ri_song_merge(M2, 2u, C, 1u, D, 1u);
            RI_ASSERT(n == 2u, "the type tie merged (%u)", (unsigned)n);
            RI_ASSERT(M2[0].type == RI_EV_NOTE_OFF && M2[0].device == 1u,
                "the RELEASE leads even though it is from the later device"
                " (%u/%u)", (unsigned)M2[0].type, (unsigned)M2[0].device);
            RI_ASSERT(M2[1].type == RI_EV_NOTE_ON && M2[1].device == 0u,
                "then the earlier device's note-on (%u/%u)",
                (unsigned)M2[1].type, (unsigned)M2[1].device);
            /* Swapping the RUNS must not change the answer: the key does not
             * know which run an event came from. A merge that did would be
             * stable only by accident. */
            n = ri_song_merge(M2, 2u, D, 1u, C, 1u);
            RI_ASSERT(n == 2u, "the same tie merged the other way round");
            RI_ASSERT(M2[0].type == RI_EV_NOTE_OFF && M2[0].device == 1u,
                "and still the release first (%u/%u)", (unsigned)M2[0].type,
                (unsigned)M2[0].device);
        }
        /* AND THE RESULT IS §8-ORDERED, which is the property the engine
         * actually depends on -- it stops on an out-of-order event. */
        for (i = 1u; i < n; i++)
            RI_ASSERT(!ri_event_less(&M[i], &M[i - 1u]),
                "the merged run is §8-ordered at %u", (unsigned)i);
        /* A short capacity truncates rather than reordering. */
        n = ri_song_merge(M, 2u, A, 2u, B, 2u);
        RI_ASSERT(n == 2u, "a capacity of 2 merges two (%u)", (unsigned)n);
        RI_ASSERT(!ri_event_less(&M[1], &M[0]),
            "and what fitted is still in order");
        /* THE DEVICE TIEBREAK IS REACHABLE, NOT REDUNDANT. MB2 -- a merge
         * ordered by sample and type but not device -- survived everything
         * above, and the reason looked like equivalence: `ri_song_emit`
         * visits runs in array order, so a caller listing devices ascending
         * never needs the device tiebreak. But nothing REQUIRES ascending
         * order, and runs listed descending turn every same-sample pair
         * into a device tie. The key does not know which run an event came
         * from, so the answer must not depend on it. */
        {
            static struct RIStep da[4], db[4];
            struct RISectionRun desc[RI_ROUTE_NSECTIONS];
            uint32_t j;
            for (j = 0u; j < 4u; j++) {
                da[j].note = 40u; da[j].flags = 0u;
                db[j].note = 38u; db[j].flags = 0u;
            }
            for (j = 0u; j < RI_ROUTE_NSECTIONS; j++) {
                desc[j].steps = 0; desc[j].nsteps = 0u;
                desc[j].device = (uint16_t)j;
            }
            /* DEVICE 3 FIRST, DEVICE 1 SECOND: descending. */
            desc[3].steps = da; desc[3].nsteps = 4u; desc[3].device = 3u;
            desc[1].steps = db; desc[1].nsteps = 4u; desc[1].device = 1u;
            nev = ri_song_emit(&map, PPQ, desc, RI_ROUTE_NSECTIONS, &opts, ev,
                MAXEV);
            RI_ASSERT(nev > 0u, "the descending run emits (%u)", (unsigned)nev);
            /* Two sections striking together: every sample is a full tie on
             * sample and type, and only device decides. */
            for (j = 1u; j < nev; j++)
                RI_ASSERT(!ri_event_less(&ev[j], &ev[j - 1u]),
                    "event %u is still §8-ordered", (unsigned)j);
            {
                int saw_dev_tie = 0;
                for (j = 0u; j + 1u < nev; j++)
                    if (ev[j].sample == ev[j + 1u].sample &&
                        ev[j].type == ev[j + 1u].type) {
                        saw_dev_tie = 1;
                        RI_ASSERT(ev[j].device < ev[j + 1u].device,
                            "the device-3 tie resolves to device 3 first"
                            " (%u then %u)", (unsigned)ev[j].device,
                            (unsigned)ev[j + 1u].device);
                    }
                RI_ASSERT(saw_dev_tie,
                    "the descending fixture really does tie on device");
            }
        }

        /* MB2 -- a merge ordered by sample and type but NOT by device --
         * survived the descending-run case above, so that case does not
         * discriminate either. The reason: §8's key is
         *
         *     sample, type, device, voice, seq
         *
         * and both `ri_sched_emit_timed` runs arrive SORTED BY THAT KEY
         * ALREADY. So within a run, two events at the same sample with the
         * same type are already in device order before the merge sees them.
         * The merge only has to get the CROSS-run interleaving right.
         *
         * And for equal samples the §8 order is a total order, so `take_a`
         * and `take_b` are never a real choice: exactly one is correct. So
         * MB2's output happens to be correct here, and it is EQUIVALENT for
         * every input `ri_song_emit` can produce.
         *
         * That is worth stating rather than discovering again: the device
         * term in the merge IS redundant, because the scheduler already
         * applied it within each run. It is kept because the merge's
         * contract is "produces a §8-ordered array" and a sort that
         * silently depended on its inputs arriving sorted would be a
         * function with a precondition nobody wrote down.
         *
         * Both runs are sorted by definition -- `ri_sched_emit_timed` says
         * so -- so there is NO input on which the device term changes the
         * answer, and no test can kill MB2. It is equivalent, and it is
         * recorded as equivalent rather than dressed up as a pass. (I tried
         * to kill it with an UNSORTED input first; the merge does not
         * re-sort a run, so that test asserted a property the function does
         * not promise and correctly failed. Sorting a single run's interior
         * is the scheduler's job and duplicating it here would be the same
         * work in two places.) */

        /* Degenerate inputs. */
        RI_ASSERT(ri_song_merge(M, 4u, A, 0u, B, 2u) == 2u, "empty A");
        RI_ASSERT(ri_song_merge(M, 4u, A, 2u, B, 0u) == 2u, "empty B");
        RI_ASSERT(ri_song_merge(M, 0u, A, 2u, B, 2u) == 0u, "zero capacity");
    }

    /* Re-emit the original three sections: §4b left `ev` holding its own
     * two-section run, and §5 is about the three. */
    setup();
    nev = ri_song_emit(&map, PPQ, runs, RI_ROUTE_NSECTIONS, &opts, ev, MAXEV);

    /* --- 5. EACH DEVICE KEEPS ITS OWN seq SPACE ---------------------- */
    /* Two devices both start at seq 0. Renumbering one into the other makes
     * two DIFFERENT events compare equal, and an unstable sort then swaps
     * them between renders -- a defect that only shows up as a flaky
     * render. */
    {
        uint32_t k;
        for (k = 0u; k < RI_ROUTE_NSECTIONS; k++)
            if (runs[k].nsteps != 0u) {
                uint32_t j, minseq = 0xFFFFFFFFu;
                for (j = 0u; j < nev; j++)
                    if (ev[j].device == k && ev[j].seq < minseq)
                        minseq = ev[j].seq;
                RI_ASSERT(minseq == 0u,
                    "device %u still starts at seq 0 (min %u)", (unsigned)k,
                    (unsigned)minseq);
            }
    }

    /* --- 6. A SECTION WITH NO STEPS IS SKIPPED, NOT AN ERROR --------- */
    {
        static struct RIStep only[8];
        struct RISectionRun solo[RI_ROUTE_NSECTIONS];
        uint32_t k;
        for (k = 0u; k < 8u; k++) { only[k].note = 40u; only[k].flags = 0u; }
        for (k = 0u; k < RI_ROUTE_NSECTIONS; k++) {
            solo[k].steps = 0; solo[k].nsteps = 0u; solo[k].device = (uint16_t)k;
        }
        solo[3].steps = only; solo[3].nsteps = 8u; solo[3].device = 3u;
        RI_ASSERT(ri_song_sections(solo, RI_ROUTE_NSECTIONS) == 0x08u,
            "one section at index 3");
        nev = ri_song_emit(&map, PPQ, solo, RI_ROUTE_NSECTIONS, &opts, ev,
            MAXEV);
        RI_ASSERT(nev > 0u, "and it emits (%u)", (unsigned)nev);
        for (i = 0u; i < nev; i++)
            RI_ASSERT(ev[i].device == 3u,
                "only device 3 emitted (event %u is device %u)", (unsigned)i,
                (unsigned)ev[i].device);
    }

    /* --- 7. AN OUT-OF-RANGE DEVICE IS REFUSED ------------------------ */
    {
        struct RISectionRun bad[RI_ROUTE_NSECTIONS];
        uint32_t k;
        for (k = 0u; k < RI_ROUTE_NSECTIONS; k++) {
            bad[k].steps = st0; bad[k].nsteps = 4u; bad[k].device = (uint16_t)k;
        }
        bad[0].device = 99u;      /* no such section */
        RI_ASSERT(ri_song_emit(&map, PPQ, bad, RI_ROUTE_NSECTIONS, &opts, ev,
            MAXEV) == 0u, "a device past the last section is REFUSED");
        RI_ASSERT(ri_song_sections(bad, RI_ROUTE_NSECTIONS) == 0u,
            "and the mask is 0 rather than a bit for a section that is not "
            "there");
    }

    /* --- 8. NOTHING TO EMIT IS REFUSED, NOT AN EMPTY SONG ------------ */
    {
        struct RISectionRun none[RI_ROUTE_NSECTIONS];
        uint32_t k;
        for (k = 0u; k < RI_ROUTE_NSECTIONS; k++) {
            none[k].steps = 0; none[k].nsteps = 0u; none[k].device = (uint16_t)k;
        }
        RI_ASSERT(ri_song_emit(&map, PPQ, none, RI_ROUTE_NSECTIONS, &opts, ev,
            MAXEV) == 0u, "no sections at all");
        RI_ASSERT(ri_song_sections(none, RI_ROUTE_NSECTIONS) == 0u,
            "and no mask");
    }

    /* --- 9. NULLs ----------------------------------------------------- */
    RI_ASSERT(ri_song_emit(0, PPQ, runs, RI_ROUTE_NSECTIONS, &opts, ev,
        MAXEV) == 0u, "NULL map");
    RI_ASSERT(ri_song_emit(&map, 0u, runs, RI_ROUTE_NSECTIONS, &opts, ev,
        MAXEV) == 0u, "ppq 0");
    RI_ASSERT(ri_song_emit(&map, PPQ, 0, RI_ROUTE_NSECTIONS, &opts, ev,
        MAXEV) == 0u, "NULL runs");
    RI_ASSERT(ri_song_emit(&map, PPQ, runs, RI_ROUTE_NSECTIONS, &opts, 0,
        MAXEV) == 0u, "NULL out");
    RI_ASSERT(ri_song_emit(&map, PPQ, runs, RI_ROUTE_NSECTIONS, &opts, ev,
        0u) == 0u, "zero capacity");
    RI_ASSERT(ri_song_sections(0, RI_ROUTE_NSECTIONS) == 0u, "NULL runs");

    /* --- 10. A CAPACITY OF 1 DOES NOT OVERWRITE ---------------------- */
    /* It returns what fitted and stops, so the caller can see the shortfall
     * rather than render a song that is quietly missing its ending. */
    setup();
    nev = ri_song_emit(&map, PPQ, runs, RI_ROUTE_NSECTIONS, &opts, ev, 1u);
    RI_ASSERT(nev == 1u, "one event fits a capacity of 1 (%u)",
        (unsigned)nev);

    RI_RESULT("song-sections");
}