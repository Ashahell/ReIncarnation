/* t137_levi_voices8 — Levi P6d eight voices (owner call 2026-09-30).
 * Laws: allocator fills all 8 (rotate order + steal-advance); unison
 * fires 8; reassign spans 8; mono still voice 0; pattern lanes stay 6
 * (direct 0..5 bit-identical, 6..7 live-only, lane 8 refused); default
 * limit 8; voices 6/7 init silent + sound when triggered.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"

#define SR 48000.0f
#define N 4800u

static struct RILeviSet A, B;
static float oa[N], ob[N];

static int active_count(struct RILeviSet *s) {
    int n = 0, v;
    for (v = 0; v < (int)RI_LEVI_NVOICES; v++)
        n += s->v[v].active ? 1 : 0;
    return n;
}

static int differ(const float *a, const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        if (a[i] != b[i])
            return 1;
    return 0;
}

int main(void) {
    int v, n;

    RI_ASSERT(RI_LEVI_NVOICES == 8u, "8 voices");

    /* Rotate fills 0..7 in order, then steals from 0 advancing. */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, 0u);
    for (v = 0; v < 8; v++) {
        n = levi_note_on(&A, (uint8_t)(60 + v));
        RI_ASSERT(n == 1 && A.v[v].note == 60 + v, "rotate order %d", v);
    }
    RI_ASSERT(active_count(&A) == 8, "rotate full");
    n = levi_note_on(&A, 80u);
    RI_ASSERT(n == 1 && A.v[0].note == 80, "steal oldest");
    n = levi_note_on(&A, 81u);
    RI_ASSERT(n == 1 && A.v[1].note == 81, "steal advances");

    /* Unison fires all 8; reassign spans all 8; mono stays on voice 0. */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, 5u);
    n = levi_note_on(&A, 60u);
    RI_ASSERT(n == 8, "unison 8, got %d", n);
    levi_init_set(&A);
    levi_set_alloc_ui(&A, 1u);
    for (v = 0; v < 8; v++)
        levi_note_on(&A, (uint8_t)(60 + v));
    RI_ASSERT(active_count(&A) == 8, "reassign 8");
    levi_init_set(&A);
    levi_set_alloc_ui(&A, 2u);
    levi_note_on(&A, 60u);
    levi_note_on(&A, 64u);
    RI_ASSERT(active_count(&A) == 1 && A.v[0].note == 64, "mono voice 0");

    /* Default limit follows the voice count. */
    levi_init_set(&A);
    RI_ASSERT(A.ulimit == RI_LEVI_NVOICES, "default limit 8");

    /* Pattern lanes 0..5 direct sound bit-identical to a clean twin;
     * voices 6..7 sound when triggered directly; lane 8 refused. */
    levi_init_set(&A);
    levi_init_set(&B);
    for (v = 0; v < 6; v++) {
        levi_trigger(&A, (uint32_t)v, 60u);
        levi_trigger(&B, (uint32_t)v, 60u);
    }
    levi_voice_render_sum(&A, oa, N, SR);
    levi_voice_render_sum(&B, ob, N, SR);
    RI_ASSERT(!differ(oa, ob, N), "lanes 0..5 identical");
    levi_trigger(&A, 6u, 60u);
    levi_trigger(&A, 7u, 64u);
    RI_ASSERT(A.v[6].active && A.v[7].active, "voices 6-7 live");
    levi_voice_render_sum(&A, oa, N, SR);
    RI_ASSERT(differ(oa, ob, N), "voices 6-7 sound");
    RI_ASSERT(levi_trigger(&A, 8u, 60u) == 2, "lane 8 refused");
    levi_release(&A, 8u);

    /* New voices init silent (no garbage in the mix). */
    levi_init_set(&A);
    levi_voice_render_sum(&A, oa, N, SR);
    for (v = 0; v < (int)N; v++)
        if (oa[v] != 0.0f)
            break;
    RI_ASSERT(v == (int)N, "idle silence");

    RI_RESULT("t137_levi_voices8");
}
