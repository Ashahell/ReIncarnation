/* t120_levi_matrix_render — matrix render hook (v2 feature 4b).
 * Per-sample evaluation inside the voice render: NULL matrix and
 * empty matrix render bit-identically (v1 sound preserved); a
 * programmed slot audibly moves the stream; extreme depths stay
 * finite (NaN-crash discipline); voice truth untouched.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_matrix.h"

#define N 256u
static float A[N], B[N];

static void render_mx(struct RILeviSet *s, const struct RILeviMatrix *mx, float *out) {
    uint32_t i, v;
    for (i = 0u; i < N; i++) {
        float m = 0.0f;
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            m += levi_voice_render(&s->v[v], mx, 48000.0f);
        out[i] = m;
    }
}

static uint32_t diff(const float *a, const float *b, uint32_t n) {
    uint32_t i, k = 0u;
    for (i = 0u; i < n; i++)
        if (a[i] != b[i])
            k++;
    return k;
}

static int finite(const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        if (!(b[i] > -1e30f && b[i] < 1e30f))
            return 0;
    return 1;
}

int main(void) {
    struct RILeviSet s, t;
    struct RILeviMatrix mx;
    levi_init_set(&s);
    levi_init_set(&t);
    ri_levi_matrix_init(&mx);
    RI_ASSERT(levi_trigger(&s, 0u, 60u) == 0, "trig");
    RI_ASSERT(levi_trigger(&t, 0u, 60u) == 0, "trig");
    /* NULL matrix == empty matrix, bit-identical (v1 sound law). */
    render_mx(&s, 0, A);
    render_mx(&t, &mx, B);
    RI_ASSERT(diff(A, B, N) == 0u, "empty identical");
    RI_ASSERT(finite(A, N), "finite");
    /* Voice truth untouched by rendering with a matrix. */
    RI_ASSERT(s.v[0].cutoff == t.v[0].cutoff && s.v[0].morph == t.v[0].morph, "truth kept");
    /* Keytrack -> cutoff, full depth: note 60 sits at center (no-op
     * vs bypass), note 120 opens the filter audibly. */
    RI_ASSERT(ri_levi_matrix_set(&mx, 0u, RI_LEVI_MS_NOTE, RI_LEVI_MD_CUTOFF, 100) == 0, "prog");
    levi_init_set(&s);
    levi_init_set(&t);
    RI_ASSERT(levi_trigger(&s, 0u, 60u) == 0, "trig60");
    RI_ASSERT(levi_trigger(&t, 0u, 60u) == 0, "trig60");
    render_mx(&s, 0, A);
    render_mx(&t, &mx, B);
    RI_ASSERT(diff(A, B, N) == 0u, "center no-op");
    levi_init_set(&s);
    levi_init_set(&t);
    RI_ASSERT(levi_trigger(&s, 0u, 120u) == 0, "trig120");
    RI_ASSERT(levi_trigger(&t, 0u, 120u) == 0, "trig120");
    /* Park the base cutoff mid-range so 4x modulation matters. */
    RI_ASSERT(levi_set_param(&s, 0u, RI_LEVI_CUTOFF, 1000.0f) == 0, "cut");
    RI_ASSERT(levi_set_param(&t, 0u, RI_LEVI_CUTOFF, 1000.0f) == 0, "cut");
    render_mx(&s, 0, A);
    render_mx(&t, &mx, B);
    RI_ASSERT(diff(A, B, N) > N / 2u, "audible %u", diff(A, B, N));
    RI_ASSERT(finite(B, N), "finite mod");
    RI_ASSERT(s.v[0].cutoff == 1000.0f && t.v[0].cutoff == 1000.0f, "truth kept audible");
    /* Op contour -> reso at full depth stays finite over release. */
    RI_ASSERT(ri_levi_matrix_set(&mx, 1u, RI_LEVI_MS_OPENV0, RI_LEVI_MD_RESO, 100) == 0, "prog2");
    levi_release(&t, 0u);
    render_mx(&t, &mx, B);
    RI_ASSERT(finite(B, N), "finite release");
    /* Fail-closed render args (exact-0 law kept). */
    RI_ASSERT(levi_voice_render(0, &mx, 48000.0f) == 0.0f, "null voice");
    RI_ASSERT(levi_voice_render(&t.v[1], &mx, 0.0f) == 0.0f, "bad sr");
    RI_RESULT("levimatrixrender");
}
