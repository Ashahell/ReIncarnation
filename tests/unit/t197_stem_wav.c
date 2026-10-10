/* t197_stem_wav — R8 stem export: the WAV writer.
 *
 * Stems are the thing a DAW can actually take away, so the file has to be
 * RIGHT rather than nearly right, and a WAV has no error reporting: a file
 * with a placeholder RIFF size opens in nothing and tells you nothing.
 *
 * The laws, in the order they bite:
 *
 *  - THE RIFF AND DATA SIZES ARE REAL BYTE COUNTS. Writing 0, or
 *    0xFFFFFFFF for "streamed", is the single most common way to produce a
 *    WAV that no DAW will open, and it is invisible until someone tries.
 *  - STEMS ARE SAMPLE-ALIGNED AND OF EQUAL LENGTH. A stem that is short is
 *    padded to the common length AND COUNTED -- because a stem silently
 *    truncated at the end is a song that ends early on one channel.
 *  - FORMAT 1 is PCM and FORMAT 3 is IEEE float, and the bit depth has to
 *    agree with both. 24-bit is three bytes per sample and the most common
 *    place to be off by one, because it is not a byte count.
 *  - The tempo and the LOOP-EXACT length travel with the set, because a
 *    loop rendered at a tempo the DAW does not know about does not warp.
 *
 * This is the writer only. Rendering is the offline renderer's job, and
 * keeping them apart means these laws are testable without an engine.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "project/stem_wav.h"

int main(void) {
    static float a[64];
    static uint8_t buf[4096];
    uint32_t n, i;

    for (i = 0u; i < 64u; i++)
        a[i] = (float)i * 0.01f;

    /* --- one 16-bit stereo stem -------------------------------------- */
    n = stem_wav_write(buf, sizeof buf, a, 64u, 1u, 48000u,
        STEM_WAV_PCM16, "RAM:STEM-303A.wav");
    RI_ASSERT(n > 0u, "a stem writes (%u)", (unsigned)n);
    RI_ASSERT(buf[0] == 'R' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == 'F',
        "starts with RIFF");
    RI_ASSERT(buf[8] == 'W' && buf[9] == 'A' && buf[10] == 'V' && buf[11] == 'E',
        "then WAVE");
    RI_ASSERT(buf[12] == 'f' && buf[13] == 'm' && buf[14] == 't' && buf[15] == ' ',
        "then fmt ");
    /* fmt chunk is 16 bytes for PCM/WAVE_FORMAT_EXTENSIBLE-lite, and the
     * sizes have to be REAL: n-8 for RIFF, 16 for fmt, data for data. */
    RI_ASSERT(stem_wav_u32(buf + 4u) == n - 8u,
        "the RIFF size is the real byte count, n-8 (%u vs %u)",
        (unsigned)stem_wav_u32(buf + 4u), (unsigned)(n - 8u));
    RI_ASSERT(stem_wav_u32(buf + 16u) == 16u, "fmt chunk is 16 bytes");
    RI_ASSERT(stem_wav_u16(buf + 20u) == 1u, "format 1 is PCM");
    RI_ASSERT(stem_wav_u16(buf + 22u) == 1u, "mono is one channel");
    RI_ASSERT(stem_wav_u32(buf + 24u) == 48000u, "the sample rate is in the header");
    RI_ASSERT(stem_wav_u32(buf + 28u) == 96000u, "byte rate is rate * align (%u)",
        (unsigned)stem_wav_u32(buf + 28u));
    /* 32 is BLOCK ALIGN, 34 is BITS PER SAMPLE. I read them the wrong way
     * round first, which is the same class of mistake as writing 24-bit as
     * a byte count. */
    RI_ASSERT(stem_wav_u16(buf + 32u) == 2u, "block align is 2 bytes (%u)",
        (unsigned)stem_wav_u16(buf + 32u));
    RI_ASSERT(stem_wav_u16(buf + 34u) == 16u, "and the depth is 16 bits (%u)",
        (unsigned)stem_wav_u16(buf + 34u));
    /* data chunk follows fmt immediately at 36 for an 8+16+4+16 header. */
    RI_ASSERT(buf[36] == 'd' && buf[37] == 'a' && buf[38] == 't' && buf[39] == 'a',
        "then data");
    RI_ASSERT(stem_wav_u32(buf + 40u) == 64u * 2u,
        "the data size is the real sample count (%u)", (unsigned)stem_wav_u32(buf + 40u));
    RI_ASSERT(n == 44u + 128u, "44-byte header plus the payload (%u)", (unsigned)n);

    /* --- stereo doubles the data, not the header --------------------- */
    n = stem_wav_write(buf, sizeof buf, a, 64u, 2u, 48000u, STEM_WAV_PCM16, "x.wav");
    RI_ASSERT(stem_wav_u16(buf + 22u) == 2u, "stereo is two channels");
    RI_ASSERT(stem_wav_u16(buf + 32u) == 4u, "block align is 4 bytes (%u)",
        (unsigned)stem_wav_u16(buf + 32u));
    RI_ASSERT(stem_wav_u16(buf + 34u) == 16u, "still 16 bits, only the align grows");
    RI_ASSERT(stem_wav_u32(buf + 40u) == 64u * 4u, "and the data is doubled");
    RI_ASSERT(stem_wav_u32(buf + 4u) == n - 8u, "RIFF size still exact");

    /* --- 24-bit is THREE bytes a sample ------------------------------ */
    n = stem_wav_write(buf, sizeof buf, a, 64u, 1u, 48000u, STEM_WAV_PCM24, "x.wav");
    RI_ASSERT(stem_wav_u16(buf + 20u) == 1u && stem_wav_u16(buf + 34u) == 24u,
        "24-bit PCM says 24 bits, and it is not 32 (%u)",
        (unsigned)stem_wav_u16(buf + 34u));
    RI_ASSERT(stem_wav_u16(buf + 32u) == 3u, "block align is THREE bytes (%u)",
        (unsigned)stem_wav_u16(buf + 32u));
    RI_ASSERT(stem_wav_u32(buf + 40u) == 64u * 3u, "and the data is 3 bytes a frame");

    /* --- 32-bit float is FORMAT 3 ------------------------------------- */
    n = stem_wav_write(buf, sizeof buf, a, 64u, 1u, 48000u, STEM_WAV_F32, "x.wav");
    RI_ASSERT(stem_wav_u16(buf + 20u) == 3u,
        "IEEE float is format 3, not 1 (%u)", (unsigned)stem_wav_u16(buf + 20u));
    RI_ASSERT(stem_wav_u16(buf + 34u) == 32u, "and 32 bits wide (%u)",
        (unsigned)stem_wav_u16(buf + 34u));
    RI_ASSERT(stem_wav_u32(buf + 40u) == 64u * 4u, "4 bytes a frame");

    /* --- INTERLEAVED, which a mono test cannot see ------------------- */
    /* Interleaved and planar produce IDENTICAL bytes for one channel, so
     * every mono case above is blind to this. A file written planar plays
     * one channel then silence, and the header still looks perfect. */
    {
        static float st[128];      /* 64 frames, 2 channels, interleaved */
        uint16_t l, r;
        for (i = 0u; i < 64u; i++) {
            st[i * 2u + 0u] = 1.0f;      /* left at full scale  */
            st[i * 2u + 1u] = -1.0f;     /* right at full scale */
        }
        n = stem_wav_write(buf, sizeof buf, st, 64u, 2u, 48000u,
            STEM_WAV_PCM16, "x.wav");
        RI_ASSERT(n == 44u + 64u * 4u, "a stereo stem is 4 bytes a frame (%u)",
            (unsigned)n);
        l = (uint16_t)stem_wav_u16(buf + 44u);
        r = (uint16_t)stem_wav_u16(buf + 46u);
        RI_ASSERT(l == 32767, "frame 0 left is full scale (%d)", (int)l);
        RI_ASSERT(r == (uint16_t)(int16_t)-32767,
            "frame 0 right is full negative, i.e. INTERLEAVED (%d)", (int)(int16_t)r);
    }

    /* --- EQUAL LENGTH: a short stem is padded AND counted ------------ */
    {
        struct RIStemSet set;
        float shortst[32];
        for (i = 0u; i < 32u; i++)
            shortst[i] = 0.5f;
        stem_set_init(&set, 48000u, 120000u);
        RI_ASSERT(stem_set_add(&set, a, 64u, 1u) == 0, "stem 1 added");
        RI_ASSERT(stem_set_add(&set, shortst, 32u, 1u) == 0, "stem 2 added");
        RI_ASSERT(stem_set_frames(&set) == 64u,
            "the set is as long as its LONGEST stem (%u)", (unsigned)stem_set_frames(&set));
        RI_ASSERT(stem_set_padded(&set) == 1u,
            "and the short stem is COUNTED as padded, not silently trimmed (%u)",
            (unsigned)stem_set_padded(&set));
        /* Every stem written from the set is the same length. */
        {
            uint32_t w0 = stem_set_write(&set, 0u, buf, sizeof buf);
            uint32_t w1 = stem_set_write(&set, 1u, buf, sizeof buf);
            RI_ASSERT(w0 == w1,
                "a padded stem is written at the set's length (%u vs %u)",
                (unsigned)w0, (unsigned)w1);
            /* Equal is not enough: equal to the LONGEST. A set that trims
             * every stem to the shortest is still "equal", and loses the
             * ending of the longer ones. 24-bit mono: 44 + 64*3. */
            RI_ASSERT(w0 == 44u + 64u * 3u,
                "and the length is the LONGEST stem, not the shortest (%u)",
                (unsigned)w0);
        }
    }

    /* --- the SHORT stem added FIRST, so first != longest -------------- */
    /* My first version added the long stem first, which made "use the
     * first stem's length" and "use the longest" indistinguishable -- and a
     * test that cannot tell those apart is not testing either. */
    {
        struct RIStemSet set2;
        float longst[64], shortst2[16];
        for (i = 0u; i < 64u; i++)
            longst[i] = 0.25f;
        for (i = 0u; i < 16u; i++)
            shortst2[i] = 0.75f;
        stem_set_init(&set2, 48000u, 120000u);
        RI_ASSERT(stem_set_add(&set2, shortst2, 16u, 1u) == 0, "short stem FIRST");
        RI_ASSERT(stem_set_add(&set2, longst, 64u, 1u) == 0, "long stem second");
        RI_ASSERT(stem_set_frames(&set2) == 64u,
            "the common length is the LONGEST even when it was added last (%u)",
            (unsigned)stem_set_frames(&set2));
        RI_ASSERT(stem_set_padded(&set2) == 1u,
            "and the earlier short stem is the padded one (%u)",
            (unsigned)stem_set_padded(&set2));
        RI_ASSERT(stem_set_write(&set2, 0u, buf, sizeof buf) == 44u + 64u * 3u,
            "the stem added first is still written at the full length");
        {
            /* PADDING MUST BE SILENCE, and proving that needs the scratch
             * buffer to be DIRTY first. The pad buffer is static, so on a
             * first write it is already zero and a writer that memsets the
             * wrong length still looks correct -- which is why a mutant
             * that truncates the memset survived the first pass. Writing a
             * full-length stem first fills the tail with audio, and only
             * then is the padding visible. */
            uint32_t w, k;
            stem_set_write(&set2, 1u, buf, sizeof buf);   /* dirties the tail */
            w = stem_set_write(&set2, 0u, buf, sizeof buf);
            RI_ASSERT(w == 44u + 64u * 3u, "the short stem is still full length");
            for (k = 16u * 3u; k < 64u * 3u; k++) {
                RI_ASSERT(buf[44u + k] == 0u,
                    "padding byte %u is silence, not stale data (%02X)",
                    (unsigned)k, buf[44u + k]);
            }
        }
    }

    /* --- the tempo and loop length travel with the set ---------------- */
    {
        struct RIStemSet set;
        static char meta[128];
        stem_set_init(&set, 48000u, 140000u);
        stem_set_add(&set, a, 64u, 1u);
        RI_ASSERT(stem_set_meta(&set, meta, sizeof meta) > 0u, "a set writes metadata");
        /* 64 frames at 48 kHz is 1/750 s; the bar length must come out of
         * the tempo rather than being a constant the caller remembers. */
        RI_ASSERT(strstr(meta, "tempo=") != 0, "the tempo is recorded (%s)", meta);
        RI_ASSERT(strstr(meta, "rate=") != 0, "the sample rate is recorded");
        RI_ASSERT(strstr(meta, "frames=") != 0, "the loop length is recorded");
        RI_ASSERT(strstr(meta, "tempo=140") != 0, "and it is the real tempo (%s)", meta);
    }

    /* --- refusals ----------------------------------------------------- */
    RI_ASSERT(stem_wav_write(0, sizeof buf, a, 64u, 1u, 48000u, STEM_WAV_PCM16, "x") == 0u,
        "NULL samples");
    RI_ASSERT(stem_wav_write(buf, sizeof buf, a, 0u, 1u, 48000u, STEM_WAV_PCM16, "x") == 0u,
        "zero frames is not a stem");
    RI_ASSERT(stem_wav_write(buf, sizeof buf, a, 64u, 0u, 48000u, STEM_WAV_PCM16, "x") == 0u,
        "zero channels");
    RI_ASSERT(stem_wav_write(buf, 20u, a, 64u, 1u, 48000u, STEM_WAV_PCM16, "x") == 0u,
        "a buffer too small writes nothing rather than a truncated stem");
    {
        /* A bit depth we do not write must be refused, not rounded to
         * something that opens and plays the wrong thing. */
        RI_ASSERT(stem_wav_write(buf, sizeof buf, a, 64u, 1u, 48000u, 7u, "x") == 0u,
            "an unsupported bit depth is refused");
        RI_ASSERT(stem_wav_write(buf, sizeof buf, a, 64u, 1u, 0u, STEM_WAV_PCM16, "x") == 0u,
            "a zero sample rate is refused");
    }

    RI_RESULT("stem-wav");
}