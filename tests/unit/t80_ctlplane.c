/* t80_ctlplane — G9.1 control plane (SPSC ring of lane keys).
 * FIFO order; coalescing law; wrap-around; refused keys; drained events
 * render-identical to the setter path (t79 pattern).
 * RED-first: stub bodies return 0/empty.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/ctlplane.h"
#include "engine/seq/autolane.h"
#include "engine/engine.h"

#define SR 48000.0f
#define N 6400u

static float LA[N], RA[N], LB[N], RB[N];

int main(void) {
    struct RIControlPlane q;
    struct RIEvent ev[RI_CTL_CAP + 8u];
    uint32_t seq = 0u, n;
    uint32_t i;

    ri_ctl_init(&q);
    RI_ASSERT(ri_ctl_pending(&q) == 0u, "init empty");
    RI_ASSERT(q.dropped == 0u && q.refused == 0u, "init counters");

    /* FIFO order over three allowed keys. */
    RI_ASSERT(ri_ctl_send(&q, RI_CTL_303A_CUTOFF, 10u) == 0, "send cutoff");
    RI_ASSERT(ri_ctl_send(&q, RI_CTL_303A_RESO, 20u) == 0, "send reso");
    RI_ASSERT(ri_ctl_send(&q, RI_AUTO_ID_MIX(0u, RI_AUTO_MIX_PAN), 64u) == 0, "send pan");
    RI_ASSERT(ri_ctl_pending(&q) == 3u, "pending 3, got %u", ri_ctl_pending(&q));
    seq = 7u;
    n = ri_ctl_drain(&q, ev, 8u, 100u, &seq);
    RI_ASSERT(n == 3u, "drain 3, got %u", n);
    RI_ASSERT(ev[0].value == RI_CTL_303A_CUTOFF && ev[0].flags == 10u, "fifo first");
    RI_ASSERT(ev[1].value == RI_CTL_303A_RESO && ev[1].flags == 20u, "fifo second");
    RI_ASSERT(ev[2].value == RI_AUTO_ID_MIX(0u, RI_AUTO_MIX_PAN) && ev[2].flags == 64u, "fifo third");
    RI_ASSERT(ev[0].sample == 100u && ev[0].type == RI_EV_AUTOMATION, "drain sample/type");
    RI_ASSERT(ev[0].seq == 7u && ev[2].seq == 9u && seq == 10u, "seq chain");
    RI_ASSERT(ri_ctl_pending(&q) == 0u, "drained empty");

    /* Refused keys: never stored, counted. */
    RI_ASSERT(ri_ctl_send(&q, 0x0E00u, 5u) == 2, "refuse unknown block");
    RI_ASSERT(ri_ctl_send(&q, 0x0B00u, 5u) == 2, "refuse legacy panel id");
    RI_ASSERT(ri_ctl_pending(&q) == 0u, "refused stores nothing");
    RI_ASSERT(q.refused == 2u, "refused counted %u", q.refused);

    /* Wrap-around: 600 sends with periodic drains never lose order. */
    ri_ctl_init(&q);
    seq = 0u;
    for (i = 0u; i < 600u; i++) {
        RI_ASSERT(ri_ctl_send(&q, RI_CTL_303A_CUTOFF, (uint8_t)(i & 127u)) == 0, "wrap send %u", i);
        if ((i % 50u) == 49u) {
            n = ri_ctl_drain(&q, ev, 64u, 0u, &seq);
            RI_ASSERT(n == 50u, "wrap drain %u", n);
        }
    }
    RI_ASSERT(ri_ctl_pending(&q) == 0u, "wrap ends empty");

    /* Overflow coalesce: full queue, same key updates in place. */
    ri_ctl_init(&q);
    for (i = 0u; i < RI_CTL_CAP; i++)
        RI_ASSERT(ri_ctl_send(&q, RI_CTL_303A_CUTOFF, 1u) == 0, "fill %u", i);
    RI_ASSERT(ri_ctl_pending(&q) == RI_CTL_CAP, "full");
    RI_ASSERT(ri_ctl_send(&q, RI_CTL_303A_CUTOFF, 99u) == 0, "coalesce ok");
    RI_ASSERT(ri_ctl_pending(&q) == RI_CTL_CAP, "coalesce keeps count");
    RI_ASSERT(q.dropped == 1u, "coalesce counted %u", q.dropped);
    seq = 0u;
    n = ri_ctl_drain(&q, ev, RI_CTL_CAP, 0u, &seq);
    RI_ASSERT(n == RI_CTL_CAP, "drain full");
    {
        uint32_t fresh = 0u;
        for (i = 0u; i < n; i++)
            if (ev[i].flags == 99u)
                fresh++;
        RI_ASSERT(fresh >= 1u, "newest value survives (%u)", fresh);
    }

    /* Overflow drop-oldest: full of one key, a new allowed key evicts oldest. */
    ri_ctl_init(&q);
    for (i = 0u; i < RI_CTL_CAP; i++)
        RI_ASSERT(ri_ctl_send(&q, RI_CTL_303A_CUTOFF, 1u) == 0, "fill2 %u", i);
    RI_ASSERT(ri_ctl_send(&q, RI_CTL_303A_RESO, 77u) == 0, "drop-oldest ok");
    RI_ASSERT(q.dropped == 1u, "drop counted");
    seq = 0u;
    n = ri_ctl_drain(&q, ev, RI_CTL_CAP, 0u, &seq);
    RI_ASSERT(n == RI_CTL_CAP && n > 0u && ev[n - 1u].value == RI_CTL_303A_RESO && ev[n - 1u].flags == 77u, "newest last");

    /* Cap pressure: leftover stays queued, resumes next buffer. */
    ri_ctl_init(&q);
    RI_ASSERT(ri_ctl_send(&q, RI_CTL_303A_CUTOFF, 11u) == 0, "cap a");
    RI_ASSERT(ri_ctl_send(&q, RI_CTL_303A_RESO, 22u) == 0, "cap b");
    seq = 0u;
    n = ri_ctl_drain(&q, ev, 1u, 5u, &seq);
    RI_ASSERT(n == 1u && ri_ctl_pending(&q) == 1u, "partial drain holds one");
    n = ri_ctl_drain(&q, ev, 8u, 5u, &seq);
    RI_ASSERT(n == 1u && ev[0].value == RI_CTL_303A_RESO, "resume drains second");
    RI_ASSERT(ri_ctl_drain(0, ev, 8u, 0u, &seq) == 0u, "null plane safe");
    RI_ASSERT(ri_ctl_drain(&q, 0, 8u, 0u, &seq) == 0u, "null out safe");

    /* Drained events render-identical to the setter path. */
    {
        struct RIEngine e, f;
        struct RIEvent one;
        uint32_t s = 0u;
        ri_ctl_init(&q);
        RI_ASSERT(ri_ctl_send(&q, RI_AUTO_ID_MIX(0u, RI_AUTO_MIX_LEVEL), 64u) == 0, "lvl send");
        RI_ASSERT(ri_ctl_drain(&q, &one, 1u, 0u, &s) == 1u, "lvl drain");
        ri_engine_init(&e);
        ri_engine_defaults(&e);
        ri_engine_apply_event(&e, &one);
        ri_engine_init(&f);
        ri_engine_defaults(&f);
        RI_ASSERT(ri_engine_set_level(&f, 0u, 64u) == 0, "lvl setter");
        RI_ASSERT(e.level[0] == f.level[0], "drained == setter state");
        {
            static struct RIEvent song[2];
            song[0].sample = 0; song[0].type = RI_EV_NOTE_ON; song[0].device = 0;
            song[0].voice = 0; song[0].value = 57; song[0].flags = 0; song[0].seq = 0;
            song[1].sample = 5000; song[1].type = RI_EV_NOTE_OFF; song[1].device = 0;
            song[1].voice = 0; song[1].value = 0; song[1].flags = 0; song[1].seq = 1;
            ri_engine_load(&e, song, 2, N, RI_ENGINE_S303A);
            ri_engine_load(&f, song, 2, N, RI_ENGINE_S303A);
            /* Level was set before load on both (same state); render must match. */
            RI_ASSERT(ri_engine_render(&e, LA, RA, N, SR) == N, "render e");
            RI_ASSERT(ri_engine_render(&f, LB, RB, N, SR) == N, "render f");
            RI_ASSERT(!memcmp(LA, LB, N * sizeof LA[0]), "render-identical L");
            RI_ASSERT(!memcmp(RA, RB, N * sizeof RA[0]), "render-identical R");
        }
    }

    RI_RESULT("ctlplane");
}
