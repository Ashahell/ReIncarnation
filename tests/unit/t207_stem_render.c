/* t207_stem_render — R8d: the bridge from the engine's tap to the stem set.
 *
 * R8a built the WAV writer, R8c built the tap. What is between them is this:
 * a helper that hands the tap a whole render's worth of caller buffers,
 * zeroes them, and on completion interleaves each strip and gives it to
 * `RIStemSet`. The laws are mostly about what happens when a render does NOT
 * go to the end.
 *
 *  - **THE BUFFERS ARE ZEROED BEFORE THE RENDER, ALWAYS.** The tap writes
 *    only the samples the engine actually renders. A render that stops short
 *    -- out-of-order events (t205), a song shorter than the buffer, a
 *    `total` that arrives early -- leaves the tail holding whatever the
 *    caller had there. In a stem file that is not silence, it is the last
 *    song's audio. Zeroing is the helper's job precisely because the caller
 *    cannot be relied on to remember, and "it worked in the test" is exactly
 *    how that bug survives.
 *
 *  - **A SHORT RENDER IS REFUSED, NOT PADDED.** If the engine rendered fewer
 *    frames than asked for, `ri_stemr_finish` says so and counts it. Handing
 *    the set a full-length stem anyway would put silence at the end of a
 *    stem the caller believes is complete.
 *
 *  - **INTERLEAVED, NOT TWO MONO FILES.** `stem_wav_write` reads
 *    `samples[i * channels + c]`, so a stereo strip has to be interleaved and
 *    that copy is the only place it happens. Ten mono files would have been
 *    simpler and a worse product.
 *
 *  - **THE SET'S LENGTH IS WHAT WAS RENDERED**, not what was requested.
 *
 *  - **ONE STEM PER SECTION, IN SECTION ORDER**, so stem N is always the same
 *    instrument across runs. A caller who wants one file must be able to name
 *    it without reading the set.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/engine.h"
#include "engine/seq/sched.h"
#include "project/stem_render.h"

#define FRAMES 1024u

static struct RIEngine E;
static struct RIEvent EV[8];
static struct RIStemr R;
static struct RIStemrSlot S[3];
static float L0[FRAMES], R0[FRAMES], O0[FRAMES * 2u];
static float L1[FRAMES], R1[FRAMES], O1[FRAMES * 2u];
static float L2[FRAMES], R2[FRAMES], O2[FRAMES * 2u];

static void load(uint32_t sections, uint64_t total) {
    uint32_t k = 0u;
    memset(EV, 0, sizeof EV);
    EV[k].type = RI_EV_NOTE_ON; EV[k].device = 0u; EV[k].voice = 0u;
    EV[k].value = 36u; EV[k].sample = 0u; k++;
    EV[k].type = RI_EV_NOTE_ON; EV[k].device = 1u; EV[k].voice = 0u;
    EV[k].value = 48u; EV[k].sample = FRAMES / 4u; k++;
    ri_engine_init(&E);
    ri_engine_load(&E, EV, k, total, sections);
}

static void slots(void) {
    S[0].l = L0; S[0].r = R0; S[0].out = O0;
    S[1].l = L1; S[1].r = R1; S[1].out = O1;
    S[2].l = L2; S[2].r = R2; S[2].out = O2;
}

int main(void) {
    uint32_t done, i;

    /* --- 1. BEGIN SETS UP THE SET ------------------------------------ */
    slots();
    RI_ASSERT(ri_stemr_begin(&R, 3u, FRAMES, 48000u, STEM_WAV_PCM16,
        140000u, S) == 0, "begin accepts three sections");
    RI_ASSERT(stem_set_count(&R.set) == 0u, "and the set starts empty");
    RI_ASSERT(R.set.rate == 48000u, "rate carried (%u)",
        (unsigned)R.set.rate);
    RI_ASSERT(R.set.depth == STEM_WAV_PCM16, "depth carried (%u)",
        (unsigned)R.set.depth);
    RI_ASSERT(R.frames == FRAMES, "frames recorded (%u)",
        (unsigned)R.frames);

    /* --- 2. BIND ZEROES THE BUFFERS ----------------------------------- */
    /* Poison every buffer first: if bind does not zero them, the stems
     * would be the previous song's audio. */
    for (i = 0u; i < FRAMES; i++) {
        L0[i] = R0[i] = L1[i] = R1[i] = L2[i] = R2[i] = 9.0f;
    }
    load(RI_ENGINE_S303A | RI_ENGINE_S303B, FRAMES);
    ri_stemr_bind(&R, &E);
    RI_ASSERT(L0[FRAMES - 1u] == 0.0f,
        "bind ZEROES the buffers (L0 tail = %g)", (double)L0[FRAMES - 1u]);
    RI_ASSERT(R1[0] == 0.0f, "every slot, not just the first");
    RI_ASSERT(R2[FRAMES / 2u] == 0.0f, "and the whole buffer");

    /* --- 3. A FULL RENDER COMPLETES ---------------------------------- */
    done = ri_engine_render(&E, L1, R1, FRAMES, 48000.0f);
    /* The engine's own output went into slot 1's buffers here, which is a
     * fixture artefact; the point is the render ran to the end. */
    RI_ASSERT(done == FRAMES, "the engine rendered the whole block (%u)",
        (unsigned)done);
    RI_ASSERT(ri_stemr_finish(&R, done) == 0,
        "finish accepts a complete render");
    RI_ASSERT(stem_set_count(&R.set) == 2u,
        "one stem per RENDERED section, not per requested one (%u)",
        (unsigned)stem_set_count(&R.set));
    RI_ASSERT(stem_set_frames(&R.set) == FRAMES,
        "the set's length is what was rendered (%u)",
        (unsigned)stem_set_frames(&R.set));
    RI_ASSERT(R.set.samples[0] == O0, "stem 0 is slot 0's interleaved buffer");
    RI_ASSERT(R.set.samples[1] == O1, "stem 1 is slot 1's, in section order");
    RI_ASSERT(R.set.channels[0] == 2u, "and it is stereo");

    /* --- 4. INTERLEAVED CORRECTLY ------------------------------------ */
    /* O0[i*2] is slot 0's left, O0[i*2+1] its right. Sample 0 of section 0
     * is the 303A note, so it must be non-zero and the two sides must agree
     * (a mono section mirrors -- t206). */
    RI_ASSERT(O0[0] == L0[0], "interleaved left is the left buffer (%g vs %g)",
        (double)O0[0], (double)L0[0]);
    RI_ASSERT(O0[1] == R0[0], "interleaved right is the right buffer");
    RI_ASSERT(O0[0] != 0.0f, "and section 0 really has audio at sample 0 (%g)",
        (double)O0[0]);
    RI_ASSERT(O0[0] == O0[1],
        "a mono section's stem is centred, as the mix is (%g vs %g)",
        (double)O0[0], (double)O0[1]);
    /* The interleave must not stop early. */
    RI_ASSERT(O0[(FRAMES - 1u) * 2u] != 0.0f ||
              L0[FRAMES - 1u] == 0.0f,
        "the interleave covers the whole buffer, not just the head");

    /* --- 5. A SHORT RENDER IS REFUSED, NOT PADDED --------------------- */
    {
        static struct RIStemr R2;
        struct RIStemrSlot s2[3];
        s2[0] = S[0]; s2[1] = S[1]; s2[2] = S[2];
        RI_ASSERT(ri_stemr_begin(&R2, 3u, FRAMES, 48000u, STEM_WAV_PCM16,
            140000u, s2) == 0, "begin again");
        /* A song SHORTER than the buffer: the engine renders `total` and
         * stops. Passing `done` short must be refused. */
        load(RI_ENGINE_S303A, FRAMES / 2u);
        ri_stemr_bind(&R2, &E);
        done = ri_engine_render(&E, O1, O1 + FRAMES, FRAMES, 48000.0f);
        RI_ASSERT(done == FRAMES / 2u, "a short song renders short (%u)",
            (unsigned)done);
        RI_ASSERT(ri_stemr_finish(&R2, done) != 0,
            "and finishing a SHORT render is REFUSED, not padded");
        RI_ASSERT(R2.short_count == 1u,
            "and it is counted (%u)", (unsigned)R2.short_count);
        RI_ASSERT(stem_set_count(&R2.set) == 0u,
            "with nothing handed to the set (%u)",
            (unsigned)stem_set_count(&R2.set));
        /* The buffers it did NOT render are silence, not the previous
         * song -- which is the whole reason bind zeroes. */
        RI_ASSERT(L0[FRAMES - 1u] == 0.0f,
            "and the unrendered tail is silence (%g)", (double)L0[FRAMES - 1u]);
        /* Asking for more than asked is also not a thing. */
        RI_ASSERT(ri_stemr_finish(&R2, FRAMES * 2u) != 0,
            "a render longer than requested is refused too");
    }

    /* --- 4b. THE INTERLEAVE ORDER, WITH SIDES THAT ACTUALLY DIFFER --- */
    /* Every other section here is MONO, and t206 says a mono section
     * mirrors -- so l == r and swapping them changes nothing. My first
     * interleave assertions were satisfied by a swapped interleave. Panning
     * the section hard left is what separates the two sides: pan 0 is
     * gl=1, gr=0, so the stems are maximally asymmetric. */
    {
        static struct RIStemr R4;
        struct RIStemrSlot s4[1];
        s4[0] = S[0];
        RI_ASSERT(ri_stemr_begin(&R4, 1u, FRAMES, 48000u, STEM_WAV_PCM16,
            140000u, s4) == 0, "one slot");
        load(RI_ENGINE_S303A, FRAMES);
        E.pan[0] = 0u;                    /* hard left: gl=1, gr=0 */
        ri_stemr_bind(&R4, &E);
        done = ri_engine_render(&E, O2, O2 + FRAMES, FRAMES, 48000.0f);
        RI_ASSERT(ri_stemr_finish(&R4, done) == 0, "finish");
        RI_ASSERT(O0[0] != 0.0f, "the panned section has left audio (%g)",
            (double)O0[0]);
        RI_ASSERT(O0[0] != O0[1],
            "and the two sides genuinely DIFFER (%g vs %g)", (double)O0[0],
            (double)O0[1]);
        RI_ASSERT(O0[0] == L0[0] && O0[1] == R0[0],
            "interleaved left-then-right, in that order (%g/%g vs %g/%g)",
            (double)O0[0], (double)O0[1], (double)L0[0], (double)R0[0]);
    }

    /* --- 4c. FINISH IS NOT IDEMPOTENT-BROKEN -------------------------- */
    /* Calling finish twice would add every stem a second time, and the
     * second copy is a file of the same audio under a different index --
     * which is how a set ends up with eleven stems and ten instruments. */
    {
        static struct RIStemr R5;
        struct RIStemrSlot s5[1];
        uint32_t before;
        s5[0] = S[0];
        RI_ASSERT(ri_stemr_begin(&R5, 1u, FRAMES, 48000u, STEM_WAV_PCM16,
            140000u, s5) == 0, "one slot");
        load(RI_ENGINE_S303A, FRAMES);
        ri_stemr_bind(&R5, &E);
        done = ri_engine_render(&E, O2, O2 + FRAMES, FRAMES, 48000.0f);
        RI_ASSERT(ri_stemr_finish(&R5, done) == 0, "first finish");
        before = stem_set_count(&R5.set);
        RI_ASSERT(ri_stemr_finish(&R5, done) != 0,
            "a second finish is REFUSED");
        RI_ASSERT(stem_set_count(&R5.set) == before,
            "and the set did not grow (%u -> %u)", (unsigned)before,
            (unsigned)stem_set_count(&R5.set));
    }

    /* --- 5b. A SLOT FOR AN UNRENDERED SECTION IS NOT A STEM ---------- */
    /* The set above has 3 slots offered and 2 sections enabled, so it holds
     * 2 stems: a third file of silence labelled as an instrument would be
     * worse than one file fewer. */
    {
        static struct RIStemr R3;
        struct RIStemrSlot s3[3];
        s3[0] = S[0]; s3[1] = S[1]; s3[2] = S[2];
        RI_ASSERT(ri_stemr_begin(&R3, 3u, FRAMES, 48000u, STEM_WAV_PCM16,
            140000u, s3) == 0, "three slots offered");
        load(RI_ENGINE_S303A, FRAMES);
        ri_stemr_bind(&R3, &E);
        done = ri_engine_render(&E, O0, O1, FRAMES, 48000.0f);
        RI_ASSERT(ri_stemr_finish(&R3, done) == 0, "finish");
        RI_ASSERT(stem_set_count(&R3.set) == 1u,
            "only the RENDERED section becomes a stem, not every slot (%u)",
            (unsigned)stem_set_count(&R3.set));
    }

    /* --- 6. META CARRIES RATE AND TEMPO ------------------------------ */
    {
        char meta[128];
        uint32_t n = stem_set_meta(&R.set, meta, sizeof meta);
        RI_ASSERT(n > 0u, "meta written (%u)", (unsigned)n);
        RI_ASSERT(strstr(meta, "48000") != 0, "rate in the meta: %s", meta);
        RI_ASSERT(strstr(meta, "140.000") != 0, "tempo in the meta: %s", meta);
        RI_ASSERT(strstr(meta, "stems=2") != 0, "stem count in the meta: %s",
            meta);
    }

    /* --- 7. A WRITABLE FILE PER STEM --------------------------------- */
    {
        static uint8_t wav[16384];
        uint32_t w = stem_set_write(&R.set, 0u, wav, sizeof wav);
        RI_ASSERT(w > 44u, "stem 0 writes a file (%u)", (unsigned)w);
        RI_ASSERT(stem_wav_u32(wav + 4) == w - 8u, "with a correct RIFF size");
    }

    /* --- 8. NULLS AND BAD COUNTS ------------------------------------- */
    RI_ASSERT(ri_stemr_begin(0, 1u, FRAMES, 48000u, STEM_WAV_PCM16, 0u, S) != 0,
        "NULL render");
    RI_ASSERT(ri_stemr_begin(&R, 0u, FRAMES, 48000u, STEM_WAV_PCM16, 0u, S) != 0,
        "zero sections");
    RI_ASSERT(ri_stemr_begin(&R, 9u, FRAMES, 48000u, STEM_WAV_PCM16, 0u, S) != 0,
        "more sections than there are");
    RI_ASSERT(ri_stemr_begin(&R, 1u, 0u, 48000u, STEM_WAV_PCM16, 0u, S) != 0,
        "zero frames");
    RI_ASSERT(ri_stemr_begin(&R, 1u, FRAMES, 0u, STEM_WAV_PCM16, 0u, S) != 0,
        "zero rate");
    RI_ASSERT(ri_stemr_begin(&R, 1u, FRAMES, 48000u, 0u, 0u, S) != 0,
        "an unsupported bit depth");
    RI_ASSERT(ri_stemr_begin(&R, 1u, FRAMES, 48000u, STEM_WAV_PCM16, 0u, 0) != 0,
        "NULL slots");
    {
        struct RIStemrSlot bad[2];
        bad[0] = S[0];
        bad[1].l = 0; bad[1].r = R1; bad[1].out = O1;
        RI_ASSERT(ri_stemr_begin(&R, 2u, FRAMES, 48000u, STEM_WAV_PCM16, 0u,
            bad) != 0, "a slot with no left buffer");
    }
    ri_stemr_bind(0, &E);
    ri_stemr_bind(&R, 0);
    RI_ASSERT(ri_stemr_finish(0, FRAMES) != 0, "NULL finish");
    RI_ASSERT(ri_stemr_finish(&R, 0u) != 0, "a zero-length render");

    RI_RESULT("stem-render");
}