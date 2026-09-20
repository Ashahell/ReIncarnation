/* audio.h — W1 backend graph API + render task (Task 6, gate G6).
 * Spec §4 (realtime contract), §4.2 (low-level AHI path first, ahi.device
 * stays the fallback), §5 (ONE renderer: live playback and offline export
 * share the engine, different sinks only).
 *
 * Routing (Task 6 dispatch): M1.1 UNMEASURED — the low-level backend is the
 * default per spec §4.2 and the device-buffer floor stays a hypothesis:
 * RI_DEVICE_FRAMES 256 (AHI-doc-suggested >= 240 rung, /64-clean per the
 * engine-block lock in spec §2.3; Task 5 concern 2: never assume 64).
 *
 * Host/CI: no AHI exists, so AuStart selects the null backend (render task
 * runs, sink discards) and prints RI_AUDIO_NULL_MSG. AROS: the same object
 * drives the low-level AHI path (AllocAudioA + PlayerFunc Signal, §4.2).
 */
#ifndef RI_AUDIO_H
#define RI_AUDIO_H
#include <stdint.h>

#ifdef __AROS__
#include <exec/libraries.h>
#include <utility/tagitem.h>
#else
/* Host-shim TagItem: layout-compatible with AROS (tag + pointer-sized data)
 * so the exact brief signatures compile on both targets. */
struct Library {
    unsigned long lib_dummy;
};
struct TagItem {
    uint32_t ti_Tag;
    uintptr_t ti_Data;
};
#endif

/* Query attrs (AuQueryAttr). */
#define AUQA_LatencyFrames 1u
#define AUQA_XRUN_COUNT 2u

/* Source tags (AuAddSource). */
#define AUTA_SongPath 0x41550001u /* ti_Data = (const char *) scaffold path */

/* Engine + device geometry [LOCKED where noted]. */
#define RI_AUDIO_SR 48000u
#define RI_AUDIO_BLOCK 64u     /* engine block, spec §2.3 LOCKED */
#define RI_DEVICE_FRAMES 256u  /* HYPOTHESIS default until M1.1 measures it */

/* Documented AHI-missing boot message. Printed by AuStart when no AHI
 * device exists; pinned byte-exact by tests/unit/t6_w1backend.c. */
#define RI_AUDIO_NULL_MSG \
    "audio: AHI unavailable - null backend active (offline render only)"

struct AudioObject;

/* Brief-exact header subset (verbatim signatures). */
struct AudioObject *AuCreateObject(struct Library *AudioBase, struct TagItem *tags);
uint32_t AuAddSource(struct AudioObject *ao, struct TagItem *tags);
uint32_t AuAddBus(struct AudioObject *ao, const char *name, struct TagItem *tags);
int AuConnect(struct AudioObject *ao, uint32_t src, uint32_t bus);
int AuStart(struct AudioObject *ao);
void AuStop(struct AudioObject *ao);
uint32_t AuQueryAttr(struct AudioObject *ao, uint32_t attr); /* AUQA_LatencyFrames, AUQA_XRUN_COUNT */
int AuRenderToFile(struct AudioObject *ao, const char *path, uint32_t ms);

/* Host-test/CI extras (NOT part of the frozen subset above). */
const char *AuBackendName(struct AudioObject *ao); /* "null" | "ahi-lowlevel" */
int AuDumpEvents(struct AudioObject *ao, const char *path); /* last render's events */
/* Single render core: render up to n float samples (mono, 48 kHz) through
 * the shared engine (scheduler -> 303 -> master). BOTH sinks (file export
 * and the live device drain) call this — it is the one renderer. */
uint32_t au_render_frames(struct AudioObject *ao, float *out, uint32_t n);
void au_rewind(struct AudioObject *ao); /* reset voice + event cursor */
/* Shared file sink: rewind, pump au_render_frames in `chunk`-frame calls,
 * stream 16-bit mono WAV. File export uses RI_AUDIO_BLOCK, the null drain
 * uses RI_DEVICE_FRAMES; cap_total 0 = full song. */
int au_render_song_to_wav(struct AudioObject *ao, const char *path,
    uint32_t chunk, uint64_t cap_total);
/* Host stub backend (audio_io/backend_null.c): fixed RI_DEVICE_FRAMES drain
 * of the same core into a WAV + events pair. */
int au_null_render_to_wav(struct AudioObject *ao, const char *wav_path, const char *ev_path);

#endif
