/* t35_808_deactivate.c — voices go quiet AND inactive (§2.3 fix proof).
 * After triggering, each voice must clear `active` in bounded time, the
 * WIN outputs before the flip must already sit below -100 dBFS (1e-5)
 * with margin (one-window envelope growth), and post-death renders must
 * return exactly 0.0.
 * RED-first: nothing ever clears `active`.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb808.h"

#define SR 8000.0f
#define CAP ((uint32_t)(SR * 30.0f)) /* 30 s cap per voice */
/* Death window: the last WIN outputs before the flip must already sit
 * below -100 dBFS (1e-5) with margin (one-window envelope growth). */
#define TAILN 4096u
#define WIN 64u
#define QUIET 1.5e-5f

int main(void) {
    static float ring[TAILN];
    uint32_t v;
    for (v = 0; v < RI_808_NSOUNDS; v++) {
        struct RB808Set s;
        uint32_t n = 0, i;
        float tm = 0.0f;
        rb808_init_set(&s);
        rb808_trigger(&s, v, 0u, 0.0f);
        RI_ASSERT(s.v[v].active, "voice %s not active after trigger", rb808_name(v));
        for (i = 0; i < TAILN; i++)
            ring[i] = 0.0f;
        while (s.v[v].active && n < CAP) {
            float y = rb808_voice_render(&s.v[v], SR);
            ring[n % TAILN] = y < 0.0f ? -y : y;
            n++;
        }
        RI_ASSERT(!s.v[v].active, "voice %s never deactivated", rb808_name(v));
        /* last WIN outputs live at ring[(n-WIN) .. (n-1)] mod TAILN. */
        for (i = 0; i < WIN && i < n; i++) {
            float a = ring[(n - 1 - i) % TAILN];
            if (a > tm)
                tm = a;
        }
        RI_ASSERT(tm < QUIET, "voice %s loud at death: %g", rb808_name(v), tm);
        for (i = 0; i < 64; i++)
            RI_ASSERT(rb808_voice_render(&s.v[v], SR) == 0.0f,
                "voice %s nonzero after death", rb808_name(v));
    }
    RI_RESULT("808_deactivate");
}
