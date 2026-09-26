/* ri_pal_audio.h — device IO only (portability plan T4, §3.2).
 * Policy (double buffer, xrun accounting, transport word, W capture)
 * lives in app/core/live_driver.c (portable). A backend only opens,
 * starts/stops/closes, and calls pull() on its realtime thread.
 * C99, includes only <stdint.h>.
 */
#ifndef RI_PAL_AUDIO_H
#define RI_PAL_AUDIO_H
#include <stdint.h>

#define RI_FMT_S16 1u
#define RI_FMT_F32 2u

struct ri_audio_cfg { uint32_t want_rate, frames, channels; };
struct ri_audio_info {
    uint32_t rate, frames, format;
    char name[32];
};

typedef void (*ri_audio_pull)(void *user, void *out, uint32_t frames);

int ri_pal_audio_open(const struct ri_audio_cfg *c, ri_audio_pull pull,
    void *user, struct ri_audio_info *got);
int ri_pal_audio_start(void);
void ri_pal_audio_stop(void);
void ri_pal_audio_close(void);
uint32_t ri_pal_audio_late(void);

#endif
