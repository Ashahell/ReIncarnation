/* probe_rate.c — what rate does this machine's sound card actually run at?
 *
 * AROS-ONLY. Same #error contract as probe_ahi.c and for the same reason: it
 * uses ahi.library/ahi.device and must never enter the host build, and
 * ri_audit.sh gates that fact.
 *
 * WHY THIS EXISTS. The riqemu1 lane plays 8.1 % slow because QEMU's AC97 is
 * fixed at 44100 Hz while the engine negotiates 48000, so QEMU resamples. The
 * obvious fix is to run AHI at 44100 -- but that is only correct if 44100 is
 * actually available everywhere. On the Dell the card is a different device
 * whose native rate is unknown, and assuming it is the same as AC97's is exactly
 * the kind of assumption that produced the original problem. So: ask.
 *
 * The question has three answers and all three matter:
 *
 *   1. What the DRIVER reports as its supported range
 *      (AHIDB_MinMixFreq / AHIDB_MaxMixFreq). Cheap, and already enough to
 *      reject a rate the card cannot do at all.
 *   2. The list of frequencies the driver actually offers
 *      (AHIDB_Frequencies with AHIDB_FrequencyArg as the index). This is the
 *      authoritative list, and it is what a rate change should be chosen from.
 *   3. What AHI hands back for a REQUESTED rate -- read from the allocated
 *      handle, never assumed, because a request that is not met returns
 *      something else and the difference is invisible from the request alone.
 *
 * (3) is the one that caught the resample in the first place: the request was
 * 48000 and the host was playing 44100, and only reading the mode back showed
 * the truth. So the probe always reads the mode back, whatever it asked for.
 *
 * Method. AHI_BestAudioID walks the driver's own preference order for a
 * requested rate; probe_ahi uses it once for 48k. Here it is used for each
 * candidate rate so the same mode-selection path the engine uses is the one
 * under test -- no separate enumeration that could disagree with it.
 *
 * Then each candidate is allocated and read back, because BestAudioID succeeding
 * does not mean AllocAudioA honoured it. Two rates that negotiate to the same
 * mode id are the signature of a resampling driver, which is the case this
 * probe exists to detect.
 *
 * Output is one RI_RATE line per finding, prefixed so a log can be grepped, and
 * a final summary. No floating point, no allocation, static storage only -- the
 * same hygiene the render path is measured under.
 */

#ifndef __AROS__
#error "probe_rate.c is AROS-only: it uses ahi.library/ahi.device and must never enter the host build"
#endif

#include <exec/types.h>
#include <exec/io.h>
#include <exec/tasks.h>
#include <devices/ahi.h>
#include <proto/exec.h>
/* Printf comes from dos_protos.h via proto/dos.h; without it this file fails to
 * compile with an implicit declaration. probe_ahi.c gets it the same way. */
#include <proto/dos.h>
#include <proto/ahi.h>
#include <proto/utility.h>
#include <utility/tagitem.h>

/* AHIBase is ours to define: taken from the session request's io_Device
 * (device-as-library), never via OpenLibrary -- there is no LIBS:ahi.library by
 * design. DOSBase is NOT defined here; <proto/dos.h> declares it as
 * `struct DosLibrary *` and startup.o provides the storage. Defining it again
 * as `struct Library *` is a conflicting-types error, which is what probe_ahi.c
 * avoids by only defining the AHI base. */
struct Library *AHIBase = NULL;

/* Candidates, widest first. 44100 and 48000 are the two this project has
 * actually used; the rest bracket them so a card that only does, say, 32000
 * still reports something truthful rather than "no". */
static const ULONG s_cand[] = { 48000UL, 44100UL, 32000UL, 22050UL, 11025UL, 8000UL };
#define NCAND (sizeof s_cand / sizeof s_cand[0])

static char s_name[64];

static void put(const char *a, ULONG v) {
    /* Printf is the only output channel here; the agent reads it off the
     * console, so one line per fact keeps it greppable. */
    Printf("RI_RATE %s=%lu\n", (STRPTR) a, v);
}

int main(void) {
    ULONG i;
    struct MsgPort *port = NULL;
    struct AHIRequest *req_api = NULL;
    ULONG listed = 0;   /* the frequency list has been walked once already */
    /* The first mode that negotiated successfully. Used to re-test any rate
     * BestAudioID refuses, which is the only way to tell a selector that cannot
     * match a rate apart from a driver that cannot play one. */
    ULONG good_mode = AHI_INVALID_ID;

    DOSBase = (struct DosLibrary *) OpenLibrary((STRPTR)"dos.library", 0);
    if (!DOSBase)
        return 20;

    Printf("RI_RATE probe=probe_rate candidates=%lu\n", (ULONG)NCAND);

    /* P1: low-level session handle (device-as-library; there is no
     * LIBS:ahi.library on AROS by design). req_api and port stay open for the
     * WHOLE probe -- AHIBase is the session base and every AHI_ call below
     * goes through it, so tearing the port down here would leave the base
     * dangling. They are closed on the way out, mirroring probe_ahi.c. */
    port = CreateMsgPort();
    if (port)
        req_api = (struct AHIRequest *)CreateIORequest(port, sizeof(struct AHIRequest));
    if (req_api == NULL) {
        Printf("RI_RATE open=NO_PORT_OR_REQUEST\n");
        if (port)
            DeleteMsgPort(port);
        CloseLibrary((struct Library *)DOSBase);
        return 21;
    }
    if (OpenDevice((STRPTR)"ahi.device", AHI_NO_UNIT,
            (struct IORequest *)req_api, 0) != 0) {
        Printf("RI_RATE open=FAIL\n");
        DeleteIORequest((struct IORequest *)req_api);
        DeleteMsgPort(port);
        CloseLibrary((struct Library *)DOSBase);
        return 22;
    }
    AHIBase = (struct Library *)req_api->ahir_Std.io_Device;
    put("version", (ULONG)AHIBase->lib_Version);

    /* P2: per-candidate negotiate, allocate, and READ THE MODE BACK. */
    for (i = 0; i < NCAND; ++i) {
        struct TagItem best_tags[] = {
            { AHIDB_Frequency, (IPTR)s_cand[i] },
            { AHIDB_Stereo,    (IPTR) TRUE },
            { AHIDB_HiFi,      (IPTR) TRUE },
            { TAG_DONE,        0 }
        };
        ULONG mode_id = AHI_BestAudioID(best_tags);
        struct AHIAudioCtrl *actl;
        struct TagItem alloc_tags[5];
        ULONG q_freq = 0, q_bits = 0, q_stereo = 0, q_hifi = 0, q_maxch = 0;
        ULONG q_min = 0, q_max = 0, nfreq = 0, k;
        /* Separate storage for the frequency LIST. The list query below writes
         * through AHIDB_Frequency as an OUTPUT, exactly like the mode query
         * does, so reusing q_freq for it silently overwrote the mode's own
         * frequency with the last list entry -- which reported
         * "CONVERTED_TO=192000" for a mode that had actually come back 44100.
         * A resampling tell that reports the wrong number is worse than none,
         * because it looks like a measurement. Two variables, one per query. */
        ULONG l_freq = 0, got;
        struct TagItem query_tags[] = {
            { AHIDB_Frequency,   (IPTR)&q_freq },
            { AHIDB_Bits,        (IPTR)&q_bits },
            { AHIDB_Stereo,      (IPTR)&q_stereo },
            { AHIDB_HiFi,        (IPTR)&q_hifi },
            { AHIDB_MaxChannels, (IPTR)&q_maxch },
            { AHIDB_MinMixFreq,  (IPTR)&q_min },
            { AHIDB_MaxMixFreq,  (IPTR)&q_max },
            { AHIDB_Frequencies, (IPTR)&nfreq },
            { TAG_DONE,          0 }
        };

        if (mode_id == AHI_INVALID_ID) {
            /* BestAudioID said no. That is NOT the same as "this card cannot do
             * this rate", and on this lane the two came apart: the driver's own
             * frequency list contains 44100 while BestAudioID rejects a 44100
             * request outright. So a rejection is re-tried against a mode that
             * is already known to work, which separates "the selector cannot
             * match this rate" from "the driver cannot play this rate". The
             * distinction decides whether running AHI at 44100 -- the fix for
             * the riqemu1 resample -- is even reachable. */
            Printf("RI_RATE req=%lu mode=INVALID (BestAudioID)\n", s_cand[i]);
            if (good_mode == AHI_INVALID_ID)
                continue;   /* no known-good mode yet: nothing to retry against */
            mode_id = good_mode;
            Printf("RI_RATE req=%lu retry_with_known_mode=0x%08lx\n", s_cand[i], mode_id);
        }
        else if (good_mode == AHI_INVALID_ID) {
            good_mode = mode_id;   /* first success: the retry target from here on */
        }

        /* Truncating tag list: TAG_END rather than a computed count, because
         * the list length is a compile-time constant and a runtime one would be
         * an untested shape. */
        alloc_tags[0].ti_Tag = AHIA_AudioID; alloc_tags[0].ti_Data = (IPTR)mode_id;
        alloc_tags[1].ti_Tag = AHIA_MixFreq; alloc_tags[1].ti_Data = (IPTR)s_cand[i];
        alloc_tags[2].ti_Tag = AHIA_Channels; alloc_tags[2].ti_Data = 1;
        alloc_tags[3].ti_Tag = AHIA_Sounds;   alloc_tags[3].ti_Data = 1;
        alloc_tags[4].ti_Tag = TAG_DONE;      alloc_tags[4].ti_Data = 0;

        actl = AHI_AllocAudioA(alloc_tags);
        if (actl == NULL) {
            Printf("RI_RATE req=%lu mode=0x%08lx alloc=FAIL\n", s_cand[i], mode_id);
            continue;
        }
        if (!AHI_GetAudioAttrsA(AHI_INVALID_ID, actl, query_tags)) {
            Printf("RI_RATE req=%lu mode=0x%08lx attrs=FAIL\n", s_cand[i], mode_id);
            AHI_FreeAudio(actl);
            continue;
        }

        /* Snapshot immediately: everything below reuses these query slots. */
        got = q_freq;

        Printf("RI_RATE req=%lu mode=0x%08lx got=%lu bits=%lu stereo=%lu hifi=%lu maxch=%lu "
               "range=%lu-%lu nfreq=%lu\n",
               s_cand[i], mode_id, got, q_bits, q_stereo, q_hifi, q_maxch,
               q_min, q_max, nfreq);

        /* The driver's own frequency list, which is the authoritative answer
         * to "what rate is native here". Walked once, on the first mode that
         * allocated successfully: every mode of the same driver returns the
         * same list, so repeating it per candidate is pure noise. */
        if (!listed) {
            struct TagItem n_tags[] = {
                { AHIDB_Driver, (IPTR)&s_name },
                { TAG_DONE,      0 }
            };
            listed = 1;
            for (k = 0; k < nfreq && k < 24u; ++k) {
                struct TagItem f_tags[] = {
                    { AHIDB_FrequencyArg, (IPTR) k },
                    { AHIDB_Frequency,    (IPTR)&l_freq },
                    { TAG_DONE,           0 }
                };
                if (AHI_GetAudioAttrsA(AHI_INVALID_ID, actl, f_tags))
                    Printf("RI_RATE list[%lu]=%lu\n", k, l_freq);
                else
                    Printf("RI_RATE list[%lu]=FAIL\n", k);
            }
            /* Driver name, asked once and in the same block as the list so both
             * come off one allocated handle. On this lane it came back as an
             * empty string -- recorded rather than hidden, because an empty
             * driver name is itself a small finding (the field is populated but
             * empty, not absent) and a future reader needs to know it was asked. */
            if (AHI_GetAudioAttrsA(AHI_INVALID_ID, actl, n_tags))
                Printf("RI_RATE driver=[%s]\n", (STRPTR)s_name);
            else
                Printf("RI_RATE driver=FAIL\n");
        }

        /* The resampling tell, read from the snapshot and not from the live
         * query slot: a request that comes back as a DIFFERENT rate is a driver
         * converting rather than a mode that does not exist. Equal mode ids for
         * different requests say the same thing. */
        if (got != s_cand[i])
            Printf("RI_RATE req=%lu CONVERTED_TO=%lu\n", s_cand[i], got);

        AHI_FreeAudio(actl);
    }

    Printf("RI_RATE done\n");

    /* Out, in the reverse of the way in: base, then request, then port. */
    CloseDevice((struct IORequest *)req_api);
    DeleteIORequest((struct IORequest *)req_api);
    DeleteMsgPort(port);
    AHIBase = NULL;
    req_api = NULL;
    port = NULL;
    CloseLibrary((struct Library *)DOSBase);
    return 0;
}