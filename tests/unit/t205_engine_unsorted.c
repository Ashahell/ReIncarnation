/* t205_engine_unsorted — an out-of-order event must be REFUSED, not
 * rendered into a buffer overrun.
 *
 * FOUND BY MY OWN TEST, WHILE WRITING R8c.
 *
 * `ri_engine_load` documents its event array as "caller-owned, sample-sorted"
 * and `ri_engine_render` trusts that completely. I built a fixture with
 * samples 0, 512, 256 — unsorted — and the process segfaulted inside
 * `ri_engine_render`. The mechanism, from `ri_engine_render`:
 *
 *     next = e->total;
 *     if (e->evpos < e->nev && e->ev[e->evpos].sample < next)
 *         next = e->ev[e->evpos].sample;
 *     ...
 *     run = next - e->cursor;            (unsigned)
 *
 * At cursor 512 the next event's sample is 256, so `next` becomes 256 and
 * **`run = 256 - 512` underflows to about 2^64.** The slice loop then runs
 * effectively forever, writing `out_l[done + i]` past the end of the caller's
 * buffer. It is not a wrong answer; it is a memory-corrupting overrun, and on
 * the render task that is a crash rather than a bad song.
 *
 * IS IT REACHABLE IN PRODUCTION? Not from the live path: the scheduler emits
 * sample-sorted, and `RIEvent` ordering is its contract. **But R8 and R9
 * build event arrays BY HAND** — that is exactly what an offline stem or
 * loop render does — so the first piece of work to assemble events itself is
 * the first piece of work that can get this wrong. And this codebase's law
 * everywhere else is "refused, never guessed": a bad input stops the work,
 * it does not corrupt memory on the way past.
 *
 * So: an event behind the cursor STOPS the render and says so. The guard is
 * a comparison, not a sort — sorting here would be silently reinterpreting
 * the caller's data, and the caller's ordering is the bug worth surfacing.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/engine.h"
#include "engine/seq/sched.h"

#define N 1024u

static struct RIEngine E;
static struct RIEvent EV[8];
/* Oversized by GUARD, and rendered only N: without the spare the compiler
 * rejects the canary read outright (-Warray-bounds), which is the right
 * answer but not the one this test is making. */
#define GUARD 8u
static float L[N + GUARD], R[N + GUARD];

static void build(const uint64_t *smp, uint32_t n) {
    uint32_t k;
    memset(EV, 0, sizeof EV);
    for (k = 0u; k < n; k++) {
        EV[k].type = RI_EV_NOTE_ON;
        EV[k].device = 0u;
        EV[k].voice = 0u;
        EV[k].value = 36u;
        EV[k].sample = smp[k];
    }
    ri_engine_init(&E);
    ri_engine_load(&E, EV, n, N, RI_ENGINE_S303A);
}

int main(void) {
    uint32_t done, k;

    /* --- sorted: the normal case, and it must still work -------------- */
    {
        static const uint64_t ok[3] = { 0u, 256u, 512u };
        build(ok, 3u);
        done = ri_engine_render(&E, L, R, N, 48000.0f);
        RI_ASSERT(done == N, "a sorted render completes (%u)", (unsigned)done);
        RI_ASSERT(ri_engine_ev_unsorted(&E) == 0u, "and counts no complaint");
    }

    /* --- unsorted: STOPPED, counted, and nothing written past L ------- */
    {
        static const uint64_t bad[3] = { 0u, 512u, 256u };
        for (k = 0u; k < GUARD; k++) {
            L[N + k] = -1.0f;
            R[N + k] = -1.0f;
        }
        build(bad, 3u);
        done = ri_engine_render(&E, L, R, N, 48000.0f);
        /* It renders up to the point where the array stops making sense and
         * then STOPS. `done` is what it managed, not N: claiming a full
         * render would be a lie about how much audio exists. */
        RI_ASSERT(done <= N, "and never claims more than it wrote (%u)",
            (unsigned)done);
        RI_ASSERT(ri_engine_ev_unsorted(&E) > 0u,
            "the out-of-order event is COUNTED (%u)",
            (unsigned)ri_engine_ev_unsorted(&E));
        for (k = 0u; k < 8u; k++)
            RI_ASSERT(L[N + k] == -1.0f,
                "and nothing was written past the buffer (L[%u]=%g)", N + k,
                (double)L[N + k]);
    }

    /* --- an event BEHIND the cursor from the very start -------------- */
    {
        static const uint64_t behind[2] = { 512u, 0u };
        build(behind, 2u);
        done = ri_engine_render(&E, L, R, N, 48000.0f);
        /* NOT ZERO. The array {512, 0} is fine until the cursor REACHES 512
         * and applies the first event; the bad one is only discovered when
         * it becomes the next event. My first version expected 0 and was
         * wrong about the engine in the right direction -- it stops, but
         * after rendering everything up to the point where the data stopped
         * making sense. */
        RI_ASSERT(done == 512u,
            "it renders up to the bad event and stops there (%u)",
            (unsigned)done);
        RI_ASSERT(ri_engine_ev_unsorted(&E) == 1u,
            "and is counted once (%u)",
            (unsigned)ri_engine_ev_unsorted(&E));
    }

    /* --- AN EVENT AT THE CURSOR IS NOT "BEHIND" ---------------------- */
    /* Zero-length runs are the normal way a note-on arrives exactly on a
     * slice boundary; refusing those would refuse most music. */
    {
        static const uint64_t same[3] = { 0u, 0u, 0u };
        build(same, 3u);
        done = ri_engine_render(&E, L, R, N, 48000.0f);
        RI_ASSERT(done == N, "events at the cursor are fine (%u)",
            (unsigned)done);
        RI_ASSERT(ri_engine_ev_unsorted(&E) == 0u,
            "and are not mistaken for out-of-order (%u)",
            (unsigned)ri_engine_ev_unsorted(&E));
    }

    /* --- the sort is NOT silently repaired --------------------------- */
    /* Repairing it would mean reinterpreting the caller's data and then
     * rendering music the caller did not describe. Refusing is the answer,
     * and the count is how the caller finds out. */
    {
        static const uint64_t rev[3] = { 512u, 256u, 0u };
        build(rev, 3u);
        done = ri_engine_render(&E, L, R, N, 48000.0f);
        /* {512, 256, 0}: the first event is fine, the second is behind the
         * cursor by the time it comes up. Again it stops rather than
         * repairing -- and the return says how much was really rendered. */
        RI_ASSERT(done == 512u,
            "a reversed array stops at the first bad event (%u)",
            (unsigned)done);
        RI_ASSERT(ri_engine_ev_unsorted(&E) == 1u,
            "and says why exactly once (%u)",
            (unsigned)ri_engine_ev_unsorted(&E));
    }

    /* --- NULLs -------------------------------------------------------- */
    RI_ASSERT(ri_engine_ev_unsorted(0) == 0u, "NULL engine");

    RI_RESULT("engine-unsorted");
}