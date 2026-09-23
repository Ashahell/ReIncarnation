/* audio_ahi.c — AROS low-level AHI backend glue, negotiate step (WBS 2.7).
 * Mirrors probe_ahi.c P1 byte-for-byte in behavior (device-as-library:
 * OpenDevice NO_UNIT, base from io_Device, BestAudioID, AllocAudioA,
 * attrs read-back), minus PlayerFunc (no hook: negotiate-only) and
 * minus the Min/MaxPlayerFreq clamp (defaults). If AllocAudioA refuses
 * hookless operation on real hardware, the probe's hook pattern is
 * the documented fallback (add PlayerFunc + freq triple).
 * AROS-only; ABI-portable C (v1 + v11 headers).
 * Playback lives in audio_ahi_play.c (AuPlay) — keeping this TU
 * negotiate-only means a negotiator binary never drags in the engine.
 */
#ifndef __AROS__
#error "audio_ahi.c is AROS-only"
#endif

#include <exec/types.h>
#include <exec/io.h>
#include <utility/tagitem.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/ahi.h>
#include "audio_io/audio_ahi.h"

extern struct DosLibrary *DOSBase;
extern struct Library *AHIBase;

int au_ahi_negotiate(struct AuAhiSession *out) {
    struct MsgPort *port;
    struct AHIRequest *req;
    struct TagItem best_tags[] = {
        { AHIDB_Frequency, 48000UL },
        { AHIDB_Stereo,    (IPTR)TRUE },
        { AHIDB_HiFi,      (IPTR)TRUE },
        { TAG_DONE,        0 }
    };
    struct TagItem alloc_tags[] = {
        { AHIA_AudioID,  0UL }, /* patched below */
        { AHIA_MixFreq,  48000UL },
        { AHIA_Channels, 1UL },
        { AHIA_Sounds,   1UL },
        { TAG_DONE,      0 }
    };
    struct AHIAudioCtrl *actl;
    ULONG q_freq = 0UL, q_bits = 0UL, q_stereo = 0UL;
    ULONG q_hifi = 0UL, q_maxch = 0UL;
    struct TagItem query_tags[] = {
        { AHIDB_Frequency,   (IPTR)&q_freq },
        { AHIDB_Bits,        (IPTR)&q_bits },
        { AHIDB_Stereo,      (IPTR)&q_stereo },
        { AHIDB_HiFi,        (IPTR)&q_hifi },
        { AHIDB_MaxChannels, (IPTR)&q_maxch },
        { TAG_DONE,          0 }
    };
    ULONG mode_id;
    if (!out)
        return 2;
    port = CreateMsgPort();
    if (!port)
        return 2;
    req = (struct AHIRequest *)CreateIORequest(port, sizeof *req);
    if (!req) {
        DeleteMsgPort(port);
        return 2;
    }
    if (OpenDevice((STRPTR)"ahi.device", AHI_NO_UNIT,
                   (struct IORequest *)req, 0) != 0) {
        DeleteIORequest((struct IORequest *)req);
        DeleteMsgPort(port);
        return 10;
    }
    AHIBase = (struct Library *)req->ahir_Std.io_Device;
    mode_id = AHI_BestAudioID(best_tags);
    if (mode_id == AHI_INVALID_ID) {
        CloseDevice((struct IORequest *)req);
        DeleteIORequest((struct IORequest *)req);
        DeleteMsgPort(port);
        return 10;
    }
    alloc_tags[0].ti_Data = (IPTR)mode_id;
    actl = AHI_AllocAudioA(alloc_tags);
    if (!actl) {
        CloseDevice((struct IORequest *)req);
        DeleteIORequest((struct IORequest *)req);
        DeleteMsgPort(port);
        return 10;
    }
    if (!AHI_GetAudioAttrsA(AHI_INVALID_ID, actl, query_tags)) {
        AHI_FreeAudio(actl);
        CloseDevice((struct IORequest *)req);
        DeleteIORequest((struct IORequest *)req);
        DeleteMsgPort(port);
        return 10;
    }
    out->mode_id = mode_id;
    out->freq = q_freq;
    out->bits = q_bits;
    out->maxch = q_maxch;
    (void)q_stereo;
    (void)q_hifi;
    /* Release everything: a held AllocAudio blocks all later opens
     * (driver-global hardware reservation — measured 2026-09-22: one
     * leaked alloc wedged unit-0 AND fresh NO_UNIT opens until
     * reboot). No leaks by design. */
    AHI_FreeAudio(actl);
    CloseDevice((struct IORequest *)req);
    DeleteIORequest((struct IORequest *)req);
    DeleteMsgPort(port);
    return 0;
}
