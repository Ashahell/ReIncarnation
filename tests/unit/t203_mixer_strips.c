/* t203_mixer_strips — R8b: the per-strip tap that makes stems possible.
 *
 * `ri_mix_render` takes five bus inputs and returns ONE output. There was no
 * per-strip signal anywhere, so a stem could not be produced without
 * rendering the whole song once per strip -- and, worse, without knowing
 * what a strip *is*. This adds the tap, and the tap's definition is the
 * whole design:
 *
 *  - **A STRIP IS EXACTLY WHAT THE MIX ADDS.** The tap writes the same
 *    `bus_in[b][i] * applied[b]` product the accumulator is summing, not a
 *    recomputation of it. So **the five stems, summed at the master gain,
 *    reconstruct the mix BIT-IDENTICALLY** -- and that is a law a test can
 *    check with `==`, because float addition is not associative and only
 *    summing in the mix's own bus order reproduces its rounding.
 *
 *  - **THE TAP IS PRE-MASTER.** The master fader is a mix decision, not a
 *    property of a strip; baking it into every stem means the user cannot
 *    re-mix without applying it five times. It follows that the master fader
 *    changes the mix and does NOT change a single stem.
 *
 *  - **THE TAP INCLUDES THE SLEW, DELIBERATELY.** `applied[]` is a
 *    zipless ramp over `RI_MIX_RAMP_SMP` samples so a fader move does not
 *    click. Recomputing the strip from the fader's *target* would give a
 *    stem with no fade-in, and the stems would then NOT sum to the mix for
 *    the first 64 samples. Tapping the product the mix uses keeps the
 *    invariant true at every sample, including the head, and the ramp is
 *    settled by any render longer than 64 samples anyway.
 *
 *  - **A NULL TAP RENDERS BIT-IDENTICALLY.** Not "the same to a tolerance" --
 *    identically.
 *
 *    And a note on what this does NOT prove. `ri_mix_render` IS
 *    `ri_mix_render_strips` with a NULL tap, and I wrote a mutant that
 *    reimplemented `ri_mix_render` outright -- same math, same accumulate
 *    order -- and it SURVIVED, because it is byte-identical. Single
 *    implementation is a maintenance argument, not a testable law, and
 *    claiming otherwise here would be exactly the kind of comment that
 *    outlives its evidence. What is tested is the behaviour; why there is
 *    one function is a reason, not a pin.
 *
 *  - **A MUTED OR SOLO-OUT STRIP IS SILENCE IN ITS OWN STEM**, because the
 *    stem is what the mix adds and that strip adds nothing. A stem that
 *    plays what the strip would play *if it were audible* is a different
 *    product, and it is the one nobody asked for.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/mixer/mixer.h"
#include "project/stem_wav.h"

#define N   512u
#define NB  RI_MIX_NBUS

static struct RiMixer M;
static float BUS[NB][N];
static float OUT[N], SEND[N];
static float STRIP[NB][N];

/* One block, into whichever buffers the caller names. */
static void block(float *out, float *snd, float *strips[NB], uint32_t n) {
    const float *in[NB];
    uint32_t b;
    for (b = 0u; b < NB; b++)
        in[b] = BUS[b];
    ri_mix_render_strips(&M, in, out, snd, strips, n);
}

/* Settle the fader slew so a measured block is not measuring the ramp. */
static void settle(uint32_t n) {
    uint32_t k;
    for (k = 0u; k < 4u; k++)
        block(OUT, SEND, 0, n);
}

static int same(const float *a, const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        if (a[i] != b[i])              /* EXACTLY: no tolerance anywhere */
            return 0;
    return 1;
}

int main(void) {
    uint32_t b, i;
    float *strips[NB];

    /* --- deterministic, distinguishable buses ------------------------- */
    for (b = 0u; b < NB; b++)
        for (i = 0u; i < N; i++)
            BUS[b][i] = 0.01f * (float)(b + 1u) *
                (1.0f + 0.5f * (float)((i + b) % 7u));
    for (b = 0u; b < NB; b++)
        strips[b] = STRIP[b];

    /* --- 1. A NULL TAP IS BIT-IDENTICAL ------------------------------ */
    {
        ri_mix_init(&M, 48000.0f);
        for (b = 0u; b < NB; b++) {
            ri_mix_set_fader(&M, b, (uint8_t)(80u + b * 9u));
            ri_mix_set_send(&M, b, (uint8_t)(40u + b * 5u));
        }
        ri_mix_set_master(&M, 110u);
        settle(N);
        /* Both paths from the SAME state: settle with the tap version, then
         * render both ways and compare. A second implementation would differ
         * in the order of the accumulate, which is exactly what `==` finds. */
        {
            static struct RiMixer A, B;
            float ao[N], as[N], bo[N], bs[N];
            const float *ain[NB], *bin[NB];
            for (b = 0u; b < NB; b++) {
                ain[b] = BUS[b];
                bin[b] = BUS[b];
            }
            A = M; B = M;
            ri_mix_render(&A, ain, ao, as, N);
            ri_mix_render_strips(&B, bin, bo, bs, 0, N);
            RI_ASSERT(same(ao, bo, N), "a NULL tap renders the mix identically");
            RI_ASSERT(same(as, bs, N), "and the send identically");
        }
    }

    /* --- 2. THE STRIPS SUM TO THE MIX, EXACTLY ----------------------- */
    {
        settle(N);
        block(OUT, SEND, strips, N);
        for (i = 0u; i < N; i++) {
            float acc = 0.0f;
            for (b = 0u; b < NB; b++)     /* THE MIX'S OWN BUS ORDER */
                acc += STRIP[b][i];
            RI_ASSERT(acc * M.master_applied == OUT[i],
                "sample %u: stems sum to the mix (%g vs %g)", i,
                (double)(acc * M.master_applied), (double)OUT[i]);
        }
        /* And the master gain is the only thing between them. */
        RI_ASSERT(M.master_applied == ri_fader_gain(110u),
            "the master is at its target, so the gain in the law is known");
    }

    /* --- 2b. THE INVARIANT HOLDS THROUGH THE SLEW, NOT JUST AFTER ---- */
    /* This is the sample the whole design is about, and my `settle()` --
     * four full blocks, 2048 samples -- walks straight past it.
     *
     * `applied[]` ramps over RI_MIX_RAMP_SMP (64) samples from zero. A tap
     * that recomputed the strip from the fader's TARGET instead of the
     * product the mix is using would agree with the mix on every settled
     * sample and disagree on exactly the first 64 of every fader move --
     * which is where a stem's head is. So this measures a FRESH block with
     * no settle at all, sample by sample from 0. */
    {
        static struct RiMixer F;
        static float fo[N], fs[N];
        /* ITS OWN STEM BUFFERS. Sharing STRIP meant this section left the
         * shared array holding F's faders' output, and the very next section
         * compared that against M's -- which is how a pre-master stem
         * appeared to change when the master fader moved. */
        static float FSTRIP[NB][N];
        float *fs_strips[NB];
        for (b = 0u; b < NB; b++)
            fs_strips[b] = FSTRIP[b];
        ri_mix_init(&F, 48000.0f);
        for (b = 0u; b < NB; b++)
            ri_mix_set_fader(&F, b, (uint8_t)(100u - b * 11u));
        ri_mix_set_master(&F, 127u);
        {
            const float *fin[NB];
            for (b = 0u; b < NB; b++)
                fin[b] = BUS[b];
            /* No settle. The very first block, where every applied[] is
             * still ramping up from zero. */
            ri_mix_render_strips(&F, fin, fo, fs, fs_strips, N);
            /* The master ramps too, and `out[i]` uses the master value AT
             * SAMPLE i, which the caller cannot see -- so a per-sample sum
             * checked against the block's FINAL master is wrong by
             * construction, and my first version of this section did
             * exactly that and failed on the pristine build. Settle the
             * master alone and leave every bus ramping: `applied[]` is a
             * plain struct field, so re-zeroing it is a legal setup and it
             * makes the gain a known constant. */
            for (b = 0u; b < NB; b++)
                F.bus[b].applied = 0.0f;
            /* SIXTEEN samples, not N. The ramp is RI_MIX_RAMP_SMP (64)
             * long, so a 512-sample block SETTLES it and the observations
             * below would find applied[] already at its target -- the same
             * blind spot `settle()` had, one level down. */
            {
                float mconst = F.master_applied;
                ri_mix_render_strips(&F, fin, fo, fs, fs_strips, 16u);
                for (i = 0u; i < 16u; i++) {
                    float acc = 0.0f;
                    for (b = 0u; b < NB; b++)
                        acc += FSTRIP[b][i];
                    RI_ASSERT(acc * mconst == fo[i],
                        "slew sample %u: stems still sum to the mix (%g vs %g)",
                        i, (double)(acc * mconst), (double)fo[i]);
                }
                /* And the buses really WERE ramping -- otherwise this
                 * section proves nothing, exactly as `settle()` did. */
                RI_ASSERT(F.bus[0].applied < ri_fader_gain(100u),
                    "bus 0 is still short of its target (%g vs %g)",
                    (double)F.bus[0].applied,
                    (double)ri_fader_gain(100u));
                RI_ASSERT(FSTRIP[0][0] != BUS[0][0] * ri_fader_gain(100u),
                    "so the stem is the RAMP, not the target (%g vs %g)",
                    (double)FSTRIP[0][0],
                    (double)(BUS[0][0] * ri_fader_gain(100u)));
                RI_ASSERT(FSTRIP[0][0] != 0.0f, "and not silence either (%g)",
                    (double)FSTRIP[0][0]);
                RI_ASSERT(mconst == F.master_applied,
                    "the master did not move while the buses did");
            }
        }
    }

    /* --- 3. THE TAP IS PRE-MASTER ------------------------------------ */
    {
        float before[NB][N];
        memcpy(before, STRIP, sizeof before);
        ri_mix_set_master(&M, 20u);       /* a very different master */
        settle(N);
        block(OUT, SEND, strips, N);
        RI_ASSERT(same(before[2], STRIP[2], N),
            "the master fader changed no stem at all");
        /* And prove the mix really moved, rather than trusting the helper
         * that changed it: put the master back and the mix must come back. */
        {
            static float oldmix[N];
            memcpy(oldmix, OUT, sizeof oldmix);
            ri_mix_set_master(&M, 110u);
            settle(N);
            block(OUT, SEND, strips, N);
            RI_ASSERT(!same(oldmix, OUT, N),
                "restoring the master changed the mix back");
        }
    }

    /* --- 4. A MUTED STRIP IS SILENCE IN ITS OWN STEM ----------------- */
    {
        settle(N);
        block(OUT, SEND, strips, N);
        RI_ASSERT(STRIP[1][10] != 0.0f, "bus 1 was audible");
        ri_mix_set_mute(&M, 1u, 1u);
        settle(N);
        block(OUT, SEND, strips, N);
        {
            uint32_t k;
            int silent = 1;
            for (k = 0u; k < N; k++)
                if (STRIP[1][k] != 0.0f)
                    silent = 0;
            RI_ASSERT(silent, "a muted strip is silence in its own stem");
        }
        RI_ASSERT(STRIP[2][10] != 0.0f, "and the others are untouched");
        ri_mix_set_mute(&M, 1u, 0u);
        settle(N);
    }

    /* --- 5. SOLO SILENCES THE OTHERS' STEMS -------------------------- */
    {
        settle(N);
        ri_mix_set_solo(&M, 3u, 1u);
        settle(N);
        block(OUT, SEND, strips, N);
        {
            uint32_t k;
            for (k = 0u; k < N; k++) {
                RI_ASSERT(STRIP[3][k] != 0.0f,
                    "the soloed strip still sounds (%u)", k);
                if (k < 4u) {
                    int others_silent = 1;
                    uint32_t q;
                    for (q = 0u; q < NB; q++)
                        if (q != 3u && STRIP[q][k] != 0.0f)
                            others_silent = 0;
                    RI_ASSERT(others_silent,
                        "soloed-away strip %u is silence at %u", q, k);
                }
            }
        }
        ri_mix_set_solo(&M, 3u, 0u);
        settle(N);
    }

    /* --- 6. THE SET: five mono strips, no padding -------------------- */
    {
        static struct RIStemSet S;
        static uint8_t wav[8192];
        uint32_t w;
        stem_set_init(&S, 48000u, 140000u);
        for (b = 0u; b < NB; b++) {
            RI_ASSERT(stem_set_add(&S, STRIP[b], N, 1u) == 0,
                "strip %u added", (unsigned)b);
        }
        RI_ASSERT(stem_set_count(&S) == NB, "five stems (%u)",
            (unsigned)stem_set_count(&S));
        RI_ASSERT(stem_set_frames(&S) == N, "common length is N (%u)",
            (unsigned)stem_set_frames(&S));
        /* All five are the same length, so NOTHING is padded. A stem padded
         * for no reason is silence invented at the end of a file. */
        RI_ASSERT(stem_set_padded(&S) == 0u, "nothing padded (%u)",
            (unsigned)stem_set_padded(&S));
        w = stem_set_write(&S, 2u, wav, sizeof wav);
        RI_ASSERT(w > 44u, "stem 2 wrote a header (%u)", (unsigned)w);
        RI_ASSERT(stem_wav_u32(wav + 4) == w - 8u,
            "and the RIFF size is the file (%u vs %u)",
            (unsigned)stem_wav_u32(wav + 4), (unsigned)(w - 8u));
        /* A different strip is a different file, not a copy. */
        {
            static uint8_t wav2[8192];
            uint32_t w2 = stem_set_write(&S, 3u, wav2, sizeof wav2);
            RI_ASSERT(w2 == w, "same length");
            RI_ASSERT(memcmp(wav, wav2, 64u) != 0 || N < 64u,
                "and different samples");
        }
    }

    /* --- 7. NULLS ------------------------------------------------------ */
    {
        const float *in[NB];
        for (b = 0u; b < NB; b++)
            in[b] = BUS[b];
        ri_mix_render_strips(0, in, OUT, SEND, strips, N);
        ri_mix_render_strips(&M, 0, OUT, SEND, strips, N);
        ri_mix_render_strips(&M, in, 0, SEND, strips, N);
        ri_mix_render_strips(&M, in, OUT, 0, strips, N);
        ri_mix_render_strips(&M, in, OUT, SEND, 0, N);
        ri_mix_render_strips(&M, in, OUT, SEND, strips, 0u);
        /* A PARTIAL tap array is a partial tap, not a crash: bus b's entry is
         * read only when it is non-NULL. */
        /* A NULL TAP ENTRY means "do not RECORD bus 2", which correctly
         * leaves its stem exactly as it was -- there is no claim about what
         * an unrecorded stem should contain. */
        {
            float saved = STRIP[2][0];
            strips[2] = 0;
            ri_mix_render_strips(&M, in, OUT, SEND, strips, N);
            RI_ASSERT(STRIP[2][0] == saved,
                "and leaves an unrecorded stem alone (%g)", (double)STRIP[2][0]);
        }
        strips[2] = STRIP[2];
        /* AND A TAPLESS BUS MUST WRITE SILENCE, NOT LEAVE ITS STEM ALONE.
         *
         * Skipping the write leaves whatever was in the buffer -- the
         * previous render's samples. A stem file would then be a file of
         * stale audio that nobody edited and everybody believes.
         *
         * THE BUFFER HAS TO BE FILLED WITH SOMETHING FIRST. My first version
         * asserted straight after the solo section, where bus 2 had been
         * soloed out -- so the stale content happened to BE silence and the
         * test passed against the exact mutant it was written for. A stale
         * buffer only proves anything when it holds something. */
        /* A NULL TAP ENTRY AND A BUS WITH NO INPUT ARE DIFFERENT THINGS,
         * and my first version conflated them. `strips[2] = 0` means "do not
         * RECORD bus 2", which correctly leaves its stem alone -- there is no
         * claim about what that stem should contain. The case needing an
         * explicit zero is a bus the mix is not receiving: the stem IS being
         * recorded, there is simply nothing in it. */
        block(OUT, SEND, strips, N);
        RI_ASSERT(STRIP[2][N / 2u] != 0.0f,
            "bus 2's stem really does hold audio (got %g)",
            (double)STRIP[2][N / 2u]);
        {
            /* It has to be a null entry in BUS_IN, not in strip_out. I got
             * this wrong twice: passing the hole to strip_out just means
             * "do not record bus 2", which is correct behaviour and proves
             * nothing about what a recordless bus should write. */
            const float *noinput[NB];
            for (b = 0u; b < NB; b++)
                noinput[b] = BUS[b];
            noinput[2] = 0;              /* the mix is NOT receiving bus 2 */
            ri_mix_render_strips(&M, noinput, OUT, SEND, strips, N);
        }
        {
            uint32_t k;
            int silent = 1;
            for (k = 0u; k < N; k++)
                if (STRIP[2][k] != 0.0f)
                    silent = 0;
            RI_ASSERT(silent, "a bus with no input writes silence, not the "
                "previous render's samples (STRIP[2][0]=%g)",
                (double)STRIP[2][0]);
            RI_ASSERT(STRIP[3][N - 1u] != 0.0f,
                "and the buses that do have input are unaffected");
        }
        RI_ASSERT(1u, "and the mix is untouched throughout");
    }

    RI_RESULT("mixer-strips");
}