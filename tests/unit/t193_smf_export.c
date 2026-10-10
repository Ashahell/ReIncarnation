/* t193_smf_export — SMF type 1 export (R7).
 *
 * An SMF is a byte format with several places to be subtly wrong and no
 * compiler to catch any of them, so the laws here are the format's own:
 *
 *  - CHUNK LENGTHS ARE FIXED 4-BYTE BIG-ENDIAN INTEGERS, not variable-length
 *    quantities. Only DELTA TIMES are VLQ. I got this backwards first --
 *    "every length in the format is variable" is a natural and wrong
 *    assumption -- and it produced an eleven-byte header. Pinned first
 *    because everything after it is measured from the wrong offset.
 *  - Delta times are variable-length too, and the running sum is what
 *    places an event in time. A writer that emits absolute times where the
 *    format wants deltas produces a file whose tempo map is meaningless.
 *  - A track chunk's declared length must be EXACT. Too long and every
 *    subsequent track is read at the wrong offset; too short and the tail
 *    is lost. Neither is reported by any player.
 *  - Note events must be note-on then note-off, or a DAW leaves a note
 *    hanging forever -- the classic "stuck note" import failure.
 *  - A track with no events still needs its EOT, or readers treat it as
 *    truncated.
 *
 * The mapping itself (accent to velocity, slide to legato) is an OWNER
 * REVIEW ITEM per the interop spec R7, so it is isolated in one function
 * and pinned only as "deterministic and in range", not as a chosen
 * convention.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "project/smf_export.h"


/* Find a byte subsequence. The writer's output has to be checked as BYTES,
 * not as the functions that produced it: three mutants survived a first
 * pass that only asserted on ri_smf_delta_ticks() and ri_smf_velocity()
 * while never once looking at what landed in the buffer. */
static int has(const uint8_t *hay, uint32_t n, const uint8_t *nee, uint32_t m) {
    uint32_t i;
    if (!hay || !nee || m == 0u || n < m)
        return 0;
    for (i = 0u; i + m <= n; i++) {
        uint32_t j = 0u;
        while (j < m && hay[i + j] == nee[j])
            j++;
        if (j == m)
            return 1;
    }
    return 0;
}

/* The body of the SECOND MTrk, so the conductor track cannot satisfy an
 * assertion meant for the data. */
static uint32_t track2(const uint8_t *f, uint32_t n, uint32_t *at) {
    uint32_t p = 14u, seen = 0u;
    while (p + 8u <= n) {
        uint32_t len;
        if (f[p] != 'M' || f[p+1] != 'T' || f[p+2] != 'r' || f[p+3] != 'k')
            break;
        len = ri_smf_chunk_len(f + p + 4u, n - (p + 4u));
        /* BOUND the declared length before stepping by it. A walk that
         * trusts it runs off the end of the buffer on a malformed file and
         * takes the test process with it -- which is how a real mutant got
         * scored as "survived": it crashed the harness instead of failing
         * an assertion, and a crash reports nothing. */
        if (len == 0u || (p + 8u + len) > n)
            break;
        seen++;
        if (seen == 2u) { *at = p + 8u; return len; }
        p += 8u + len;
    }
    *at = 0u;
    return 0u;
}

int main(void) {
    static struct RISmfTrack tr[4];
    static struct RISmfSong song;
    struct RISmfEvent evs[4];
    uint8_t buf[4096];
    uint32_t n, i;

    /* --- an empty song is still a valid file -------------------------- */
    ri_smf_song_init(&song, 480u /* ppq */, 120000u /* 120.000 BPM milli */, tr, 4u);
    n = ri_smf_write(&song, buf, sizeof buf);
    RI_ASSERT(n > 0u, "an empty song still writes a file (%u)", (unsigned)n);
    RI_ASSERT(buf[0] == 'M' && buf[1] == 'T' && buf[2] == 'h' && buf[3] == 'd',
        "starts with MThd (%c%c%c%c)", buf[0], buf[1], buf[2], buf[3]);
    /* The header length is a FIXED 4-byte big-endian integer. My first
     * version wrote it as a VLQ -- on the reasoning that "every length in
     * the format is variable" -- which produced an ELEVEN-byte header whose
     * MTrk magic landed where the division field belongs. VLQ is for delta
     * times inside a track and for nothing else. */
    RI_ASSERT(buf[4] == 0u && buf[5] == 0u && buf[6] == 0u && buf[7] == 6u,
        "MThd length is the fixed 4-byte 00 00 00 06 (%02X %02X %02X %02X)",
        buf[4], buf[5], buf[6], buf[7]);
    RI_ASSERT(ri_smf_chunk_len(buf + 4u, 4u) == 6u, "read back as 6");
    /* Format sits AFTER the 4-byte length, so at offset 8 -- not 5. */
    RI_ASSERT(buf[8] == 0u && buf[9] == 1u, "format 1, as R7 specifies");
    RI_ASSERT(buf[10] == 0u && buf[11] == 1u, "an empty song declares the conductor");
    /* Big endian, as the format requires: the count 1 is 00 01, not 01 00.
     * Writing these little-endian yields a header that parses and a track
     * count of 256, which no reader reports. */
    RI_ASSERT(buf[12] == 0x01u && buf[13] == 0xE0u,
        "ppq 480 is 01 E0 big endian (%02X %02X)", buf[12], buf[13]);

    /* --- variable-length quantities are the format's whole risk -------- */
    {
        uint8_t vlq[8];
        uint32_t vn;
        /* 0x40 is BELOW 0x80, so it is a single byte -- the case that
         * distinguishes a real VLQ from "always two bytes". A writer that
         * pads small values still parses in some readers and not others. */
        vn = ri_smf_vlq(vlq, sizeof vlq, 0x40u);
        RI_ASSERT(vn == 1u && vlq[0] == 0x40u,
            "0x40 is ONE VLQ byte (%u: %02X)", (unsigned)vn, vlq[0]);
        vn = ri_smf_vlq(vlq, sizeof vlq, 0x7Fu);
        RI_ASSERT(vn == 1u && vlq[0] == 0x7Fu, "0x7F is still one byte (%u)",
            (unsigned)vn);
        vn = ri_smf_vlq(vlq, sizeof vlq, 0x80u);
        RI_ASSERT(vn == 2u && vlq[0] == 0x81u && vlq[1] == 0x00u,
            "0x80 crosses the boundary: 81 00 (%u: %02X %02X)", (unsigned)vn,
            vlq[0], vlq[1]);
        vn = ri_smf_vlq(vlq, sizeof vlq, 0x2000u);
        RI_ASSERT(vn == 2u && vlq[0] == 0xC0u && vlq[1] == 0x00u,
            "0x2000 is C0 00 (%u: %02X %02X)", (unsigned)vn, vlq[0], vlq[1]);
        vn = ri_smf_vlq(vlq, sizeof vlq, 0x100000u);
        RI_ASSERT(vn == 3u && vlq[0] == 0xC0u && vlq[1] == 0x80u && vlq[2] == 0x00u,
            "0x100000 needs three bytes and a continuation chain (%u: %02X %02X %02X)",
            (unsigned)vn, vlq[0], vlq[1], vlq[2]);
        /* Every byte but the last must have its high bit set. */
        vn = ri_smf_vlq(vlq, sizeof vlq, 0x0FFFFFFFu);
        RI_ASSERT(vn == 4u, "0x0FFFFFFF is four bytes (%u)", (unsigned)vn);
        for (i = 0u; i + 1u < vn; i++)
            RI_ASSERT((vlq[i] & 0x80u) != 0u, "byte %u carries the continuation", (unsigned)i);
        RI_ASSERT((vlq[vn - 1u] & 0x80u) == 0u, "the last byte terminates");
        /* A buffer too small must refuse rather than truncate: a truncated
         * VLQ is a different number, which is worse than no number. */
        vn = ri_smf_vlq(vlq, 2u, 0x100000u);
        RI_ASSERT(vn == 0u, "a 2-byte buffer refuses a 3-byte VLQ (%u)", (unsigned)vn);
    }

    evs[0].sample = 0u;     evs[0].type = RI_SMF_EV_NOTE_ON;  evs[0].channel = 0u; evs[0].note = 36u; evs[0].flags = RI_SMF_ACCENT;
    evs[1].sample = 4800u;  evs[1].type = RI_SMF_EV_NOTE_OFF; evs[1].channel = 0u; evs[1].note = 36u; evs[1].flags = 0u;
    evs[2].sample = 4800u;  evs[2].type = RI_SMF_EV_NOTE_ON;  evs[2].channel = 0u; evs[2].note = 36u; evs[2].flags = 0u;
    evs[3].sample = 9600u;  evs[3].type = RI_SMF_EV_NOTE_OFF; evs[3].channel = 0u; evs[3].note = 36u; evs[3].flags = 0u;

    /* --- delta times, not absolute ones ------------------------------- */
    {
        static struct RISmfTrack one;
        uint32_t cnt;
        one.events = evs;
        one.count = 4u;
        one.channel = 0u;
        one.name = "303A";
        ri_smf_song_init(&song, 480u, 120000u, tr, 4u);
        ri_smf_song_add(&song, &one);
        (void)cnt;
        RI_ASSERT(ri_smf_delta_ticks(&one, 480u, 0u) == 0u, "first event is at delta 0");
        /* 4800 samples at 48 kHz is a tenth of a second; at 480 ppq that is
         * 48 ticks. My first expectation here was 10, which is what you get
         * if you forget the sample rate entirely. */
        RI_ASSERT(ri_smf_delta_ticks(&one, 480u, 1u) == 48u,
            "4800 samples at 48 kHz and 480 ppq is 48 ticks (%u)",
            (unsigned)ri_smf_delta_ticks(&one, 480u, 1u));
        RI_ASSERT(ri_smf_delta_ticks(&one, 480u, 2u) == 0u,
            "a same-sample event has delta 0, never a negative or wrapped value");
        RI_ASSERT(ri_smf_delta_ticks(&one, 480u, 3u) == 48u, "another 48 ticks");
        /* Out-of-range and NULL are refusals, never a wild read. */
        RI_ASSERT(ri_smf_delta_ticks(&one, 480u, 4u) == 0u, "past the end is 0");
        RI_ASSERT(ri_smf_delta_ticks(&one, 480u, 99u) == 0u, "far past the end is 0");
        RI_ASSERT(ri_smf_delta_ticks(0, 480u, 1u) == 0u, "NULL track is 0");
        RI_ASSERT(ri_smf_delta_ticks(&one, 0u, 1u) == 0u, "ppq 0 divides by nothing");
    }

    /* --- notes are on-then-off, and the mapping is in range ----------- */
    RI_ASSERT(ri_smf_velocity(RI_SMF_ACCENT) > ri_smf_velocity(0u),
        "an accent is louder than a plain note (%u vs %u)",
        (unsigned)ri_smf_velocity(RI_SMF_ACCENT), (unsigned)ri_smf_velocity(0u));
    RI_ASSERT(ri_smf_velocity(RI_SMF_ACCENT) <= 127u &&
        ri_smf_velocity(0u) >= 1u, "velocities stay in 1..127");

    /* --- a full song writes, and the track count matches -------------- */
    {
        static struct RISmfTrack t0, t1;
        t0.events = evs; t0.count = 4u; t0.channel = 0u; t0.name = "303A";
        t1.events = evs; t1.count = 4u; t1.channel = 9u; t1.name = "EXT";
        ri_smf_song_init(&song, 480u, 120000u, tr, 4u);
        ri_smf_song_add(&song, &t0);
        ri_smf_song_add(&song, &t1);
        n = ri_smf_write(&song, buf, sizeof buf);
        RI_ASSERT(n > 0u, "a two-track song writes (%u)", (unsigned)n);
        RI_ASSERT(buf[10] == 0u && buf[11] == 3u,
            "header says three tracks: two data plus a conductor (%02X %02X)", buf[10], buf[11]);
        RI_ASSERT(buf[14] == 'M' && buf[15] == 'T' && buf[16] == 'r' && buf[17] == 'k',
            "the first chunk starts at 14, immediately after the header");
        /* --- THE BYTES THEMSELVES, which is where the bugs were --- */
        {
            static const uint8_t on1[] = { 0x00u, 0x90u, 36u, 112u };
            static const uint8_t off1[] = { 0x30u, 0x80u, 36u, 0x40u };
            static const uint8_t on2[] = { 0x00u, 0x90u, 36u, 0x40u };
            static const uint8_t off2[] = { 0x30u, 0x80u, 36u, 0x40u };
            static const uint8_t eot[] = { 0xFFu, 0x2Fu, 0x00u };
            uint32_t at2 = 0u, len2 = track2(buf, n, &at2);
            RI_ASSERT(len2 > 0u, "found the second track");
            /* Delta 0 then note-on, velocity 112 for the accent. */
            RI_ASSERT(has(buf + at2, len2, on1, sizeof on1),
                "delta 0 then note-on 36 at accent velocity 112");
            /* Delta 48 is the single VLQ byte 0x30, then a REAL note-off. */
            RI_ASSERT(has(buf + at2, len2, off1, sizeof off1),
                "delta 48 (VLQ 0x30) then note-off 0x80, not note-on vel 0");
            RI_ASSERT(has(buf + at2, len2, on2, sizeof on2),
                "the second note-on follows at delta 0, not at the last gap");
            RI_ASSERT(has(buf + at2, len2, off2, sizeof off2),
                "and its note-off at delta 48");
            /* End of track terminates the chunk; without it readers treat
             * the track as truncated. */
            RI_ASSERT(buf[at2 + len2 - 3u] == 0xFFu && buf[at2 + len2 - 2u] == 0x2Fu &&
                buf[at2 + len2 - 1u] == 0x00u,
                "the track ends with FF 2F 00 (%02X %02X %02X)",
                buf[at2 + len2 - 3u], buf[at2 + len2 - 2u], buf[at2 + len2 - 1u]);
            RI_ASSERT(has(buf + at2, len2, eot, sizeof eot),
                "an end-of-track meta is present");
        }
        /* Every 'MTrk' must sit exactly where the previous chunk's declared
         * length says it does. */
        {
            uint32_t p = 14u, chunks = 0u;
            while (p + 8u <= n) {
                RI_ASSERT(buf[p] == 'M' && buf[p+1] == 'T' && buf[p+2] == 'r' && buf[p+3] == 'k',
                    "chunk %u at %u is MTrk", (unsigned)chunks, (unsigned)p);
                {
                    /* p + 8 + len, not p + len: the walk has to clear the
                     * magic AND the length field before the body. And the
                     * length is bounded before it is used as a step, so a
                     * malformed file fails an assertion instead of walking
                     * off the buffer. */
                    uint32_t len = ri_smf_chunk_len(buf + p + 4u, n - (p + 4u));
                    RI_ASSERT(len > 0u && (p + 8u + len) <= n,
                        "chunk %u declares a length that fits (%u at %u)",
                        (unsigned)chunks, (unsigned)len, (unsigned)p);
                    if (len == 0u || (p + 8u + len) > n)
                        break;
                    p += 8u + len;
                }
                chunks++;
            }
            RI_ASSERT(chunks == 3u, "three chunks walked exactly (%u)", (unsigned)chunks);
            RI_ASSERT(p == n, "the walk consumed the file exactly (%u of %u)",
                (unsigned)p, (unsigned)n);
        }
    }

    /* --- refusals, not silent truncation ------------------------------ */
    n = ri_smf_write(0, buf, sizeof buf);
    RI_ASSERT(n == 0u, "a NULL song writes nothing (%u)", (unsigned)n);
    n = ri_smf_write(&song, buf, 8u);
    RI_ASSERT(n == 0u, "a buffer too small for the header writes nothing (%u)",
        (unsigned)n);

    RI_RESULT("smf-export");
}