/* t6_w1backend — Task 6 one-renderer proof (gate G6).
 * First-light song through the FILE path (AuRenderToFile) vs the LIVE-STUB
 * path (AuStart + backend_null fixed device-chunk drain): PCM + events must
 * be byte-identical (same engine, different sinks — spec §5). Also pins the
 * latency floor (RI_DEVICE_FRAMES, M1.1 hypothesis default) and the exact
 * AHI-missing fallback string.
 *
 * Hermetic: the song text (== tests/golden/303/first-light.rbng body) is
 * embedded and written to /tmp/ri/run/t6/song.rbng, so the test needs no CWD.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "audio_io/audio.h"

#define T6_DIR "/tmp/ri/run/t6"
#define T6_SONG T6_DIR "/song.rbng"
#define T6_FILE_WAV T6_DIR "/file.wav"
#define T6_LIVE_WAV T6_DIR "/live.wav"
#define T6_FILE_EV T6_DIR "/file.events"
#define T6_LIVE_EV T6_DIR "/live.events"

/* Must stay byte-equal to tests/golden/303/first-light.rbng (minus comments). */
static const char *k_song_lines[] = {
    "tempo=140",
    "step note=45",
    "step note=45 slide=1",
    "step note=48 accent=1",
    "step note=45",
    "step note=52 slide=1",
    "step note=50",
    "step rest=1",
    "step note=48",
    "step note=45 slide=1 accent=1",
    "step note=43",
    "step rest=1 slide=1",
    "step note=45",
    "step note=47",
    "step note=48 slide=1",
    "step note=50 accent=1",
    "step rest=1",
    NULL
};

/* Documented AHI-missing boot message (audio.h RI_AUDIO_NULL_MSG).
 * The test pins the literal so the user-visible string cannot drift silently. */
static const char *k_want_null_msg =
    "audio: AHI unavailable - null backend active (offline render only)";

static int write_song(void) {
    FILE *f = fopen(T6_SONG, "w");
    int i;
    if (!f) {
        printf("FAIL cannot open %s for write\n", T6_SONG);
        return 1;
    }
    for (i = 0; k_song_lines[i]; i++)
        fprintf(f, "%s\n", k_song_lines[i]);
    fclose(f);
    return 0;
}

/* Read a whole file into a static bounded buffer. Returns size, -1 on error. */
static long read_whole(const char *path, unsigned char *dst, long cap) {
    FILE *f = fopen(path, "rb");
    long n = 0;
    int ch;
    if (!f)
        return -1;
    while ((ch = fgetc(f)) != EOF) {
        if (n >= cap) {
            fclose(f);
            return -1;
        }
        dst[n++] = (unsigned char)ch;
    }
    fclose(f);
    return n;
}

int main(void) {
    struct AudioObject *ao;
    struct TagItem song_tag[2];
    uint32_t src, bus;
    uint32_t lat, xruns;
    static unsigned char fa[8388608], fb[8388608];
    long na, nb;
    int rc;

    RI_ASSERT(write_song() == 0, "song scaffold write failed");

    ao = AuCreateObject(NULL, NULL);
    RI_ASSERT(ao != NULL, "AuCreateObject returned NULL");

    song_tag[0].ti_Tag = AUTA_SongPath;
    song_tag[0].ti_Data = (uintptr_t)T6_SONG;
    song_tag[1].ti_Tag = 0; /* TAG_DONE */
    song_tag[1].ti_Data = 0;
    src = AuAddSource(ao, song_tag);
    RI_ASSERT(src != 0, "AuAddSource failed");

    bus = AuAddBus(ao, "master", NULL);
    RI_ASSERT(bus != 0, "AuAddBus failed");
    RI_ASSERT(AuConnect(ao, src, bus) == 0, "AuConnect failed");

    /* Live-stub boot on a box without AHI: documented fallback. */
    rc = AuStart(ao);
    RI_ASSERT(rc == 0, "AuStart rc %d", rc);
    RI_ASSERT(AuBackendName(ao) != NULL && strcmp(AuBackendName(ao), "null") == 0,
        "backend is '%s', want 'null'", AuBackendName(ao) ? AuBackendName(ao) : "(null)");
    RI_ASSERT(strcmp(RI_AUDIO_NULL_MSG, k_want_null_msg) == 0,
        "fallback string drifted: '%s'", RI_AUDIO_NULL_MSG);

    /* Latency floor: M1.1 UNMEASURED, so the Task 6 default (256, spec §4.2
     * low-level path, doc-suggested >= 240 rung, /64-clean). */
    lat = AuQueryAttr(ao, AUQA_LatencyFrames);
    RI_ASSERT(lat == 256 && lat == RI_DEVICE_FRAMES, "latency %u, want 256", lat);
    xruns = AuQueryAttr(ao, AUQA_XRUN_COUNT);
    RI_ASSERT(xruns == 0, "xrun count %u, want 0", xruns);

    /* FILE path: full song (ms=0). */
    RI_ASSERT(AuRenderToFile(ao, T6_FILE_WAV, 0) == 0, "AuRenderToFile failed");
    RI_ASSERT(AuDumpEvents(ao, T6_FILE_EV) == 0, "AuDumpEvents/file failed");

    /* LIVE-STUB path: same object, fixed device-chunk drain. */
    RI_ASSERT(au_null_render_to_wav(ao, T6_LIVE_WAV, T6_LIVE_EV) == 0, "null drain failed");

    /* One-renderer assertion: byte-identical sinks. */
    na = read_whole(T6_FILE_WAV, fa, (long)sizeof fa);
    nb = read_whole(T6_LIVE_WAV, fb, (long)sizeof fb);
    RI_ASSERT(na > 44 && na == nb, "wav sizes %ld vs %ld", na, nb);
    if (na > 44 && na == nb)
        RI_ASSERT(memcmp(fa, fb, (size_t)na) == 0, "wav payloads differ (not one renderer)");
    na = read_whole(T6_FILE_EV, fa, (long)sizeof fa);
    nb = read_whole(T6_LIVE_EV, fb, (long)sizeof fb);
    RI_ASSERT(na > 0 && na == nb, "event sizes %ld vs %ld", na, nb);
    if (na > 0 && na == nb)
        RI_ASSERT(memcmp(fa, fb, (size_t)na) == 0, "event streams differ");

    AuStop(ao);
    RI_RESULT("w1backend");
}
