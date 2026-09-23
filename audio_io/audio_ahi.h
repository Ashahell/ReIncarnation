/* audio_ahi.h — AROS low-level AHI backend glue (WBS 2.7).
 * AROS-only (like probe_ahi.c): host build must never see this file.
 * Scope: negotiate (open + best mode + alloc, leak-free) + AuPlay/AuPlayEx
 * (whole-song render then blocking CMD_WRITE stream; AuPlayEx polls a stop
 * flag between chunks and releases everything on stop). Verified by Dell
 * runs (expected: best_mode 0x003E0001 @44100/16-bit, M1.1 numbers;
 * playlist 250286 B mono PCM, rc=0) + AROS-compile audit gate.
 * ABI-portable C (v1 + v11 headers).
 */
#ifndef RI_AUDIO_AHI_H
#define RI_AUDIO_AHI_H

#ifndef __AROS__
#error "audio_ahi.h is AROS-only"
#endif

#include <exec/types.h>

struct AudioObject; /* audio_io/audio.h (opaque; no link dependency) */

/* Negotiated session (caller-owned storage for numbers; no allocation). */
struct AuAhiSession {
    ULONG mode_id;
    ULONG freq;
    ULONG bits;
    ULONG maxch;
};

/* Open ahi.device NO_UNIT, BestAudioID, AllocAudioA stereo HiFi.
 * Returns 0 ok (session filled) or nonzero (nothing opened/held).
 * No playback started. Releases EVERYTHING before returning (FreeAudio
 * + CloseDevice + port/req delete): a leaked AllocAudio holds the
 * driver's hardware reservation and blocks all later opens (measured
 * 2026-09-22: one leaked alloc wedged unit-0 AND fresh NO_UNIT opens
 * until reboot). */
int au_ahi_negotiate(struct AuAhiSession *out);

/* Render ao's song to RAM: and play it through unit 0 (CMD_WRITE loop).
 * Self-contained: renders via AuRenderToFile, opens/closes its own
 * device handle (independent of any negotiated session), deletes the
 * temp file. Returns 0 played clean / nonzero with a printed reason.
 * First slice: full-song render before first sound (streaming later).
 * Thin wrapper: AuPlayEx(ao, NULL). */
int AuPlay(struct AudioObject *ao);

/* Interruptible playback: like AuPlay, but polls *stop between CMD_WRITE
 * chunks (granularity <= 1 chunk ~171 ms at 16384 B / 48 kHz mono 16-bit).
 * On a stop request the stream releases EVERYTHING (Close + CloseDevice +
 * DeleteIORequest + DeleteMsgPort + DeleteFile) and returns 1 (evidence
 * printed: "RI_AUPLAY stopped %lu bytes rc=1"). Returns 0 played clean;
 * errors as AuPlay. The flag is caller-owned storage; the pointer is read
 * volatile. A leaked release wedges ahi.device unit-0 until reboot. */
int AuPlayEx(struct AudioObject *ao, const volatile int *stop);

#endif
