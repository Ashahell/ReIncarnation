/* backend_null.c — host stub backend for CI (Task 6, gate G6).
 * Task 5 probe pattern: on real hardware the render task would be woken by
 * the AHI PlayerFunc Signal (§4.2) and fill a device double-buffer. Here
 * there is no device, so this stub drains the SAME core (au_render_frames)
 * in fixed RI_DEVICE_FRAMES device chunks — the live-path chunking — via
 * the shared file sink. tools/compare of that pair against the
 * AuRenderToFile pair is the one-renderer proof (spec §5).
 *
 * Portable C99 (host AND AROS compile); realtime-unsafe file sink only
 * (the sink lives in audio.c, never in au_render_frames).
 */
#include <stdint.h>
#include "audio_io/audio.h"

int au_null_render_to_wav(struct AudioObject *ao, const char *wav_path, const char *ev_path) {
    int rc;
    if (!ao || !wav_path)
        return 2;
    /* Live-path shape: fixed device-chunk drain of the shared core. */
    rc = au_render_song_to_wav(ao, wav_path, RI_DEVICE_FRAMES, 0);
    if (rc != 0)
        return rc;
    if (ev_path && AuDumpEvents(ao, ev_path) != 0)
        return 2;
    return 0;
}
