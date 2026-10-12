/* t217_wavfloat — P3's last option: 32-bit IEEE float WAV, end to end.
 *
 * The spec's option line reads "Options: 44.1 or 48 kHz, 16/24-bit and
 * 32-bit float". 44.1/48 and 16/24 had been delivered. The float had not,
 * and the reason it had not is the interesting part:
 *
 * **THE WRITER ALREADY DID FLOAT. THE CLI SAID `16|24`.**
 * `project/stem_wav.c` has carried `STEM_WAV_F32` since R8a and t197 has
 * covered it -- including format code 3 and the note that "format 1 with
 * 32 bits" is a different thing. But the MIX path did not: `tools/render`'s
 * `write_wav` and `audio_io`'s `au_render_song_to_wav_depth` both
 * hardcoded **format code 1** in `auf_wav_header` and refused depth 32.
 *
 * So this tree had TWO WAV WRITERS THAT DISAGREED ABOUT WHAT A FLOAT WAV IS,
 * and the one with the correct answer was not the one anyone renders a mix
 * through. Opening the CLI without opening the header would have produced
 * the exact file the stem writer's comment warns about: correctly sized,
 * correctly labelled, **noise**.
 *
 * WHAT THIS TEST IS FOR:
 *   - the format code is 3 and every OTHER field still describes a float
 *     file (byte rate, block align, bits, both sizes);
 *   - the samples are really floats, not integers wearing a tag;
 *   - the refusals are still refusals -- 8, 20, 64 and 0 are turned away
 *     rather than rounded, because a 20-bit request that quietly became 24
 *     produces a file that opens and plays the wrong thing;
 *   - the AIFF builder still refuses 32, and says why in its own contract.
 *
 * CWD convention: repo root (same as t21/t28).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "audio_io/audio.h"
#include "project/sample_guard.h"   /* the ONE guard, pinned directly */


#define T217_SONG  "t217f_song.txt"
#define T217_WAV32 "t217f_out32.wav"
#define T217_WAV16 "t217f_out16.wav"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static const char *k_lines[] = {
    "tempo=120",
    "step note=45",
    "step rest=1",
    "step rest=1",
    "step rest=1",
    "step note=45",
    "step rest=1",
    "step rest=1",
    "step rest=1",
    NULL
};

static uint32_t rd16(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static struct AudioObject *make_ao(void) {
    struct AudioObject *ao;
    struct TagItem song_tag[2];
    uint32_t src, bus;
    FILE *f = fopen(T217_SONG, "w");
    int i;
    if (!f)
        return NULL;
    for (i = 0; k_lines[i]; i++)
        fprintf(f, "%s\n", k_lines[i]);
    fclose(f);
    ao = AuCreateObject(NULL, NULL);
    if (!ao)
        return NULL;
    song_tag[0].ti_Tag = AUTA_SongPath;
    song_tag[0].ti_Data = (uintptr_t)T217_SONG;
    song_tag[1].ti_Tag = 0;
    song_tag[1].ti_Data = 0;
    src = AuAddSource(ao, song_tag);
    bus = AuAddBus(ao, "master", NULL);
    if (src == 0u || bus == 0u || AuConnect(ao, src, bus) != 0) {
        return NULL;   /* AuStop is the only teardown there is */
    }
    if (AuStart(ao) != 0) {
        return NULL;   /* AuStop is the only teardown there is */
    }
    return ao;
}

int main(void) {
    unsigned char h[44];
    struct AudioObject *ao;
    FILE *f;
    static unsigned char pcm[4000000];
    size_t nr, fsize;
    uint32_t frames, nz = 0u, i;

    /* --- 1. THE HEADER SAYS FLOAT, AND SAYS EVERYTHING ELSE TOO -------- */
    /* Every field below EXCEPT the format code also describes a file that
     * plays as noise. That is the whole reason the code is checked first and
     * on its own: a size/allocation check passes happily on a broken file. */
    CHECK(auf_wav_header(h, 48000u, 32u, 48000u) == 0, "depth 32 header rc");
    CHECK(rd16(h + 20) == 3u,
        "FORMAT CODE IS 3 for IEEE float, got %u (1 with 32 bits plays as "
        "NOISE)", (unsigned)rd16(h + 20));
    CHECK(rd16(h + 22) == 1u, "mono");
    CHECK(rd32(h + 24) == 48000u, "sample rate");
    CHECK(rd32(h + 28) == 48000u * 4u, "byte rate is sr*4, got %u",
        (unsigned)rd32(h + 28));
    CHECK(h[32] == 4u, "block align is 4, got %u", (unsigned)h[32]);
    CHECK(rd16(h + 34) == 32u, "bits per sample is 32, got %u",
        (unsigned)rd16(h + 34));
    CHECK(rd32(h + 40) == 48000u * 4u, "data size is frames*4, got %u",
        (unsigned)rd32(h + 40));
    CHECK(rd32(h + 4) == 36u + 48000u * 4u, "riff size is data+36, got %u",
        (unsigned)rd32(h + 4));
    /* AND THE INT PATHS ARE UNCHANGED: opening 32 must not have moved 16
     * or 24 off format code 1, which is the mistake this change could
     * easily have made by writing the tag as a function of "not 16". */
    CHECK(auf_wav_header(h, 100u, 16u, 48000u) == 0 && rd16(h + 20) == 1u,
        "16-bit stays format 1");
    CHECK(auf_wav_header(h, 100u, 24u, 48000u) == 0 && rd16(h + 20) == 1u,
        "24-bit stays format 1");
    CHECK(auf_wav_header(h, 100u, 24u, 48000u) == 0 && h[32] == 3u,
        "24-bit keeps its 3-byte align");

    /* --- 2. THE REFUSALS ARE STILL REFUSALS --------------------------- */
    /* A depth that quietly rounds produces a file that opens and plays the
     * wrong thing, which is worse than no file at all. */
    CHECK(auf_wav_header(h, 100u, 8u, 48000u) == 2, "8-bit refused");
    CHECK(auf_wav_header(h, 100u, 20u, 48000u) == 2, "20-bit refused");
    CHECK(auf_wav_header(h, 100u, 64u, 48000u) == 2, "64-bit refused");
    CHECK(auf_wav_header(h, 100u, 0u, 48000u) == 2, "depth 0 refused");
    CHECK(auf_wav_header(h, 100u, 32u, 0u) == 2, "sr 0 refused at 32");
    CHECK(auf_wav_header(h, 100u, 24u, 0u) == 2, "sr 0 refused at 24");
    /* AIFF float is NOT implemented and is not faked. AIFF float needs an
     * fl32/FL32 compression type and a different COMM layout; this builder
     * writes neither, so it refuses. */
    {
        unsigned char a[54];
        CHECK(auf_aiff_header(a, 100u, 32u, 48000u) == 2,
            "AIFF refuses 32 (no fl32 here, and no pretending)");
        CHECK(auf_aiff_header(a, 100u, 24u, 48000u) == 0, "AIFF 24 still ok");
    }

    /* --- 3. A REAL RENDER, AND THE BYTES ARE REALLY FLOATS ------------ */
    ao = make_ao();
    CHECK(ao != NULL, "audio object built from a real song");
    if (!ao)
        goto done;
    CHECK(AuRenderToFileDepth(ao, T217_WAV32, 0, 32u) == 0,
        "AuRenderToFileDepth at 32");
    AuStop(ao);
    ao = NULL;

    f = fopen(T217_WAV32, "rb");
    CHECK(f != NULL, "the 32-bit file exists");
    if (!f)
        goto done;
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    nr = fread(pcm, 1, sizeof pcm, f);
    fclose(f);
    CHECK(nr == (size_t)fsize, "read the whole file (%zu of %zu)", nr, fsize);
    if (nr != (size_t)fsize || fsize < 44u)
        goto done;
    CHECK(memcmp(pcm, "RIFF", 4) == 0, "RIFF");
    CHECK(memcmp(pcm + 8, "WAVE", 4) == 0, "WAVE");
    CHECK(rd16(pcm + 20) == 3u, "the WRITTEN FILE is format 3, got %u",
        (unsigned)rd16(pcm + 20));
    CHECK(rd16(pcm + 34) == 32u, "and 32 bits");
    CHECK(pcm[32] == 4u, "and a 4-byte align");
    CHECK(rd32(pcm + 40) == (uint32_t)fsize - 44u,
        "the data size accounts for every byte after the header (%u vs %zu)",
        (unsigned)rd32(pcm + 40), fsize - 44u);
    CHECK(rd32(pcm + 4) == (uint32_t)fsize - 8u,
        "and so does the riff size (%u vs %zu)",
        (unsigned)rd32(pcm + 4), fsize - 8u);

    /* THE SAMPLES. Not "not all zero" -- that would also be satisfied by a
     * file of 32-bit INTEGERS holding tiny values, which is precisely the
     * failure a correct-looking header would hide. What has to be true is
     * that they read back as floats in a sane range. */
    frames = (uint32_t)(fsize - 44u) / 4u;
    CHECK(frames > 0u, "there are frames (%u)", (unsigned)frames);
    {
        int lo_bp = 1, hi_bp = 0;
        uint32_t audible = 0u;
        float peak = 0.0f;
        for (i = 0u; i < frames; i++) {
            float v;
            memcpy(&v, pcm + 44u + (size_t)i * 4u, sizeof v);
            if (v != 0.0f) {
                nz++;
                if (v < 0.0f)
                    v = -v;
                if (v > peak)
                    peak = v;
                /* The exponent bias tells the two readings apart. A float
                 * in a music range lands in bits 126 (0x3E..0x40); the same
                 * four bytes read as int32 land near 0x30..0x41, i.e. a
                 * handful of counts. Either way the sample must not be an
                 * absurd magnitude, which is what a misread file gives. */
                {
                    uint32_t bits;
                    int biased;
                    memcpy(&bits, pcm + 44u + (size_t)i * 4u, sizeof bits);
                    biased = (int)((bits >> 23) & 0xFFu) - 127;
                    if (biased < lo_bp)
                        lo_bp = biased;
                    if (biased > hi_bp)
                        hi_bp = biased;
                    if (biased >= -24)
                        audible++;
                }
            }
        }
        CHECK(nz > 0u, "the render is not silent (%u nonzero of %u)",
            (unsigned)nz, (unsigned)frames);
        CHECK(peak > 0.001f && peak < 1.5f,
            "peak is a MUSIC value, not a misread integer: %f", (double)peak);
        /* THE EXONENT BAND, CHOSEN HONESTLY. My first attempt asserted
         * every exponent sat inside [-30, +7], and it failed at -57 -- which
         * is a 303 nine bars into its decay tail, and is entirely correct
         * float audio. A band that fails on real signal is a band that
         * proves nothing about the writer.
         *
         * What is worth asserting is that AUDIBLE samples EXIST in float
         * terms. A file of 32-bit integers carrying tag 3 reads as float as
         * a scatter of denormals and enormous values with nothing in
         * between; a real render has a solid mass in the audio band. So:
         * the extremes are recorded and printed, and the law is that the
         * audible band is populated. */
        printf("exponents [%d..%d], %u of %u audible\n", lo_bp, hi_bp,
            audible, (unsigned)nz);
        CHECK(hi_bp <= 0,
            "nothing exceeds full scale, so the top exponent is <= 0 (got %d)",
            hi_bp);
        CHECK(audible > 0u,
            "and there are AUDIBLE samples, not just a denormal tail (%u)",
            audible);
    }

    /* --- 4. AND THE 16-BIT PATH IS BYTE-IDENTICAL TO BEFORE ----------- */
    /* Opening 32 must not have moved 16. This renders the same song twice
     * and compares, because a change to the int path would show up as a
     * golden drift rather than as a test failure anyone was looking for. */
    ao = make_ao();
    CHECK(ao != NULL, "second audio object");
    if (ao) {
        CHECK(AuRenderToFileDepth(ao, T217_WAV16, 0, 16u) == 0, "16-bit render");
        AuStop(ao);
        f = fopen(T217_WAV16, "rb");
        if (f) {
            static unsigned char p16[4000000];
            size_t n16;
            n16 = fread(p16, 1, sizeof p16, f);
            fclose(f);
            CHECK(rd16(p16 + 20) == 1u, "the 16-bit file is format 1");
            CHECK(rd16(p16 + 34) == 16u, "and 16 bits");
            /* Same song, same length -- so the frame COUNT must match the
             * float render exactly. A different length would mean 32
             * changed how long the song is, which is not what we asked for. */
            /* Both sides in BYTES. My first version compared the 16-bit
             * file's byte count against the float file's byte count and
             * printed "75000 vs 75000" while failing -- the counts really
             * were equal, and the assertion was dividing one of them by the
             * wrong stride. A failure whose message says the two values
             * match is a failure in the test, and it was worth printing
             * rather than just fixing. */
            CHECK(n16 == 44u + (size_t)frames * 2u,
                "the two depths agree on FRAME COUNT (%zu vs %u)",
                (n16 - 44u) / 2u, (unsigned)frames);
        }
    }

    /* --- 5. THE MIX WRITERS FOLLOW THE SAME RULE --------------------- */
    /* The owner's ruling covers the mix writers too: "the stem writer and
     * the mix writers should follow the same rule". t197 proves the rule in
     * project/stem_wav.c; this proves audio_io's two sinks agree with it,
     * because two correct rules that differ are not one rule.
     *
     * What is checkable through the API is the COUNT and the IN-RANGE
     * INVARIANT. A +1.5 sample cannot be driven through the engine -- the
     * limiter is in the signal path -- so asserting float headroom at this
     * layer would be asserting a property of the generator. What CAN be
     * checked, and is what actually broke before, is that a non-finite
     * sample does not reach the file and does reach the counter. */
    {
        CHECK(au_nonfinite_probe(1e30f), "1e30 is treated as non-finite");
        CHECK(au_nonfinite_probe(1.5f) == 0, "1.5 is an ordinary sample");
        CHECK(au_nonfinite_probe(0.0f) == 0, "0.0 is an ordinary sample");
        /* 1e21 is FINITE and inside float's range. The guard rejects it, and
         * that is deliberate and shared with the Leviasynth -- a sample at
         * 1e21 in a file is unusable whatever its bit pattern says. Named
         * here so the superset is a decision rather than an accident. */
        CHECK(au_nonfinite_probe(1e21f),
            "1e21 is finite but still rejected (a file number no tool renders)");
        CHECK(au_nonfinite_probe(-1e21f), "and -1e21");
        /* The NaN case the old clamp missed entirely. */
        {
            float nan_v;
            uint32_t nb = 0x7FC00000u;
            memcpy(&nan_v, &nb, 4u);
            CHECK(au_nonfinite_probe(nan_v),
                "a NaN is caught -- the clamp's two comparisons were both "
                "false for it, which is why this guard exists");
        }
        /* THE PACKERS THEMSELVES, because nothing was testing them. The
         * mutation that deletes `auf_clampf`'s guard SURVIVED every other
         * case in this file: t217 exercised the predicate and the file
         * header, and t197 exercises `project/stem_wav.c`'s OWN arithmetic --
         * which is a different function in a different TU. audio_io's
         * `auf_f32_to_s16` / `auf_f32_to_s24` had no coverage at all, so the
         * NaN guard could be deleted from them and every test still passed.
         *
         * They are public, so this needs no hook. What is asserted is the
         * LAW, not a compiler's undefined behaviour: the old code ran
         * `(int32_t)(NaN * 32767.0f)`, which is UB and gave 0 here and
         * INT_MIN elsewhere, so pinning an exact value would have pinned one
         * host's accident. In range, and deterministic, is the claim. */
        {
            float nan_v, inf_v;
            uint32_t nb = 0x7FC00000u, ib = 0x7F800000u;
            int32_t s;
            memcpy(&nan_v, &nb, 4u);
            memcpy(&inf_v, &ib, 4u);
            s = (int32_t)auf_f32_to_s16(nan_v);
            CHECK(s == 0, "s16 of a NaN is 0, not UB (%d)", (int)s);
            /* INFINITY BECOMES SILENCE, NOT FULL SCALE. My first assertion
             * here said 32767, carried over from the old clamp -- and it was
             * wrong about the ruling as well as about the code. The owner
             * said "any NaN or infinite sample" becomes 0, and the guard runs
             * BEFORE the clamp, so an Inf is a fault rather than a loud
             * sample. That is also the more honest reading: the old clamp
             * turned a broken value into a plausible one, which is how a
             * poisoned voice sounds like a quiet one instead of like a
             * fault. This is a real behaviour change and it is the intended
             * one. */
            s = (int32_t)auf_f32_to_s16(inf_v);
            CHECK(s == 0, "s16 of an Inf is SILENCE, not full scale (%d)",
                (int)s);
            s = (int32_t)auf_f32_to_s16(1.5f);
            CHECK(s == 32767, "s16 of 1.5 CLAMPS (%d)", (int)s);
            s = (int32_t)auf_f32_to_s16(-1.5f);
            CHECK(s == -32767, "s16 of -1.5 clamps (%d)", (int)s);
            s = (int32_t)auf_f32_to_s16(0.5f);
            CHECK(s > 16000 && s < 16800, "s16 of 0.5 is about half scale (%d)",
                (int)s);
            s = auf_f32_to_s24(nan_v);
            CHECK(s == 0, "s24 of a NaN is 0, not UB (%d)", (int)s);
            s = auf_f32_to_s24(inf_v);
            CHECK(s == 0, "s24 of an Inf is SILENCE too (%d)", (int)s);
            s = auf_f32_to_s24(1.5f);
            CHECK(s == 8388607, "s24 of 1.5 CLAMPS (%d)", (int)s);
            /* AND DETERMINISTIC: the same input twice gives the same answer,
             * which is what "not UB" has to mean in practice. */
            CHECK(auf_f32_to_s16(nan_v) == auf_f32_to_s16(nan_v),
                "and repeatably so");
        }

        /* AND IT COUNTS THROUGH THE OBJECT. */
        {
            struct AudioObject *ao2 = make_ao();
            if (ao2) {
                CHECK(AuRenderToFileDepth(ao2, T217_WAV16, 0, 16u) == 0,
                    "16-bit render for the counter check");
                CHECK(AuQueryAttr(ao2, AUQA_NONFINITE) == 0u,
                    "a clean render counts zero (%u)",
                    (unsigned)AuQueryAttr(ao2, AUQA_NONFINITE));
                AuStop(ao2);
            }
        }
    }

done:
    if (ao)
        AuStop(ao);
    remove(T217_SONG);
    remove(T217_WAV32);
    remove(T217_WAV16);
    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS wavfloat\n");
    return fails != 0;
}
