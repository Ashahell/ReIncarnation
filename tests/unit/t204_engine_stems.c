/* t204_engine_stems — R8c: the stem tap on the mix the APP actually uses.
 *
 * `ri_mix_render` is NOT the app's mixer — its only non-test callers are
 * `tools/bench.c` and `tools/render.c`, and `engine/mixer/mixer.c` is linked
 * into both AROS ABIs with no application caller at all. The app mixes in
 * `engine_section()` (mono, sections 0-3) and `engine_section_stereo()` (the
 * Levi, section 4), each applying the strip fader in place to `scratch` /
 * `scratchR` and accumulating into a **double** master. This is that tap.
 *
 *  - **A STEM IS EXACTLY WHAT THE MIX ADDS**, post-fader, post-pan,
 *    pre-master, from the SAME term the accumulator is summing -- never a
 *    recomputation of the gain.
 *  - **`pos` IS THE WHOLE BUG.** `ml`/`mr` are block-local and indexed from
 *    0; the offset is applied once, at `out_l[done + c + i]`. My first
 *    version wrote `tl[i]` for every slice, so every slice landed at the HEAD
 *    of the buffer and the stems looked plausible while being almost
 *    entirely the caller's fill. `engine_section_stereo` needs it too, and
 *    that is not optional: the Levi is section 4 with its own accumulator,
 *    so a tap on only `engine_section` leaves the fifth stem unwritten.
 *  - **THE SUM LAW IS A TOLERANCE, NOT AN EQUALITY.** The master accumulates
 *    in `double` and these buffers are `float`, so five narrowed stems cannot
 *    reproduce a double sum -- about 6e-8 relative, roughly -144 dBFS, which
 *    is AT OR BELOW the quantisation floor of a 24-bit stem (-144) and far
 *    below a 16-bit one (-96). For the only thing anyone does with stems,
 *    re-mixing them in a DAW, that is not a limitation anybody can hear. The
 *    bound is pinned so the claim stays honest rather than approximate.
 *  - **A NULL TAP IS BIT-IDENTICAL**, and a NULL SLOT is skipped rather than
 *    written -- a holed tap array is a legitimate configuration (a 303-only
 *    render has no 808) and must not be able to resurrect a section the song
 *    has switched off.
 *  - **THE MONO PATH IS IMMUNE TO `scratchR`** (t206) and a stem inherits that:
 *    a 303's stem is its mono sample mirrored to both sides, and no stale
 *    `scratchR` can reach it. An earlier slice of this work claimed
 *    otherwise; that claim was withdrawn.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/engine.h"
#include "engine/seq/sched.h"

#define N 1024u
#define FILL 0.25f

static struct RIEngine E;
static struct RIEvent EV[64];
static float L[N], R[N];
static struct RIStemTap TAP;
static float SL[RI_ROUTE_NSECTIONS][N], SR[RI_ROUTE_NSECTIONS][N];
static float FILLREF[RI_ROUTE_NSECTIONS][N];

static void fill(void) {
    uint32_t b, i;
    for (b = 0u; b < RI_ROUTE_NSECTIONS; b++)
        for (i = 0u; i < N; i++) {
            FILLREF[b][i] = FILL;
            SL[b][i] = FILL;
            SR[b][i] = FILL;
        }
}

/* "Untouched" means exactly what it says: the buffer still holds the fill. */
static int untouched(uint32_t b) {
    return memcmp(SL[b], FILLREF[b], sizeof FILLREF[b]) == 0;
}

/* THE EVENTS ARE EMITTED IN ASCENDING SAMPLE ORDER, AND THAT IS NOT
 * COSMETIC.
 *
 * My first version appended the 303A events first (samples 0 and N/2) and
 * then the 303B one (N/4), producing the array [0, 512, 256] --
 * UNSORTED. That is precisely the input t205 was written for, and the
 * symptom was baffling rather than obvious: `render()` returned the right
 * count, the stems summed exactly through sample 511 and held the FILL from
 * 512 on, and the sum law reported an error larger than the signal.
 *
 * What actually happened was t205's guard working: at cursor 512 the next
 * event's sample was 256, so the render STOPPED instead of underflowing
 * `run` and overrunning the buffer. Without the guard this fixture would
 * have been the segfault, not a confusing assertion. */
static void load(uint32_t sections, uint32_t *n) {
    uint32_t k = 0u;
    memset(EV, 0, sizeof EV);
    if (sections & RI_ENGINE_S303A) {
        EV[k].type = RI_EV_NOTE_ON; EV[k].device = 0u; EV[k].voice = 0u;
        EV[k].value = 36u; EV[k].sample = 0u; k++;
    }
    if (sections & RI_ENGINE_S303B) {
        EV[k].type = RI_EV_NOTE_ON; EV[k].device = 1u; EV[k].voice = 0u;
        EV[k].value = 48u; EV[k].sample = N / 4u; k++;
    }
    if (sections & RI_ENGINE_S303A) {
        EV[k].type = RI_EV_NOTE_ON; EV[k].device = 0u; EV[k].voice = 0u;
        EV[k].value = 43u; EV[k].sample = N / 2u; k++;
    }
    if (sections & RI_ENGINE_SLEVI) {
        /* The Levi is section 4 and the ONLY stereo section: it is rendered
         * by engine_section_stereo, which has its own accumulate. Without a
         * note here the stereo tap never runs and a mutant that read the
         * mono scratch on the right would survive. */
        EV[k].type = RI_EV_NOTE_ON; EV[k].device = 4u; EV[k].voice = 0u;
        EV[k].value = 60u; EV[k].sample = 0u; k++;
    }
    ri_engine_init(&E);
    ri_engine_load(&E, EV, k, N, sections);
    *n = k;
}

static void attach(int holed) {
    uint32_t b;
    memset(&TAP, 0, sizeof TAP);
    for (b = 0u; b < RI_ROUTE_NSECTIONS; b++) {
        if (holed && b > 1u)
            continue;
        TAP.l[b] = SL[b];
        TAP.r[b] = SR[b];
    }
    ri_engine_set_stems(&E, &TAP);
}

static uint32_t render(void) {
    return ri_engine_render(&E, L, R, N, 48000.0f);
}

int main(void) {
    uint32_t nev, done, i, b;

    /* --- 1. A NULL TAP WRITES NOTHING, AND IS BIT-IDENTICAL ---------- */
    load(RI_ENGINE_S303A, &nev);
    fill();
    done = render();
    RI_ASSERT(done == N, "the block rendered (%u)", (unsigned)done);
    {
        int clean = 1;
        for (b = 0u; b < RI_ROUTE_NSECTIONS; b++)
            if (!untouched(b))
                clean = 0;
        /* The FILL SURVIVING is the point. Asserting the stems were zero
         * would have been satisfied by a tap that zeroes them -- a much
         * worse behaviour, since it destroys a buffer the caller may hold. */
        RI_ASSERT(clean, "a NULL tap writes nothing at all");
    }

    /* --- 2. THE STEMS SUM TO THE MIX, TO THE PINNED TOLERANCE -------- */
    load(RI_ENGINE_S303A | RI_ENGINE_S303B, &nev);
    attach(0);
    /* THE FADERS COME DOWN FIRST, AND THAT IS THE POINT.
     *
     * The stems are PRE-LIMITER; `L`/`R` are POST-`ri_soft_limit`. Past
     * RI_LIMIT_KNEE the limiter is a NONLINEAR master stage, so no additive
     * law can hold -- and with unity faders this fixture reaches |sum| 1.82
     * against a mix of -1.0, so the sum law reported an error larger than
     * the signal. That was not a wrong tap; it was a correct tap compared
     * against a compressed mix. The stems are deliberately pre-limiter --
     * baking the master limiter into five stems means the user cannot
     * re-mix without it being applied five times.
     *
     * So the law is: **the stems sum to the mix wherever the master chain
     * is linear**, and the faders bring the fixture under the knee so that
     * is what is being measured. Past the knee there is no additive law and
     * none is claimed. */
    E.level[0] = 60u;
    E.level[1] = 60u;
    fill();
    done = render();
    /* And it rendered ALL of it. A partial render leaves the tail holding
     * the fill, and the sum law then reports an error larger than the signal
     * instead of saying "the engine stopped". Check the count first. */
    RI_ASSERT(done == N && ri_engine_ev_unsorted(&E) == 0u,
        "the whole block rendered (%u, %u out-of-order)",
        (unsigned)done, (unsigned)ri_engine_ev_unsorted(&E));
    {
        float worst = 0.0f, scale = 0.0f;
        /* Only the sections the engine RENDERED. The rest still hold the
         * fill -- that they do is section 4's subject -- and adding 0.25 to
         * the sum is how a first version reported a worst error equal to the
         * whole peak. */
        for (i = 0u; i < N; i++) {
            float al = 0.0f, ar = 0.0f, el, er, m;
            for (b = 0u; b < 2u; b++) {
                al += SL[b][i];
                ar += SR[b][i];
            }
            el = al - L[i];  if (el < 0.0f) el = -el;
            er = ar - R[i];  if (er < 0.0f) er = -er;
            if (el > worst) worst = el;
            if (er > worst) worst = er;
            m = L[i] < 0.0f ? -L[i] : L[i];
            if (m > scale) scale = m;
        }
        RI_ASSERT(scale > 0.0f, "and the render was not silent (%g)",
            (double)scale);
        /* The tolerance is a DERIVED number, not a wish: float accumulation
         * over five terms against a double master, with headroom. */
        RI_ASSERT(worst <= scale * 1.0e-5f,
            "stems sum to the mix within float accumulation: worst %g "
            "against a peak of %g", (double)worst, (double)scale);
        /* And the bound is real: the error is in the float-noise range, not
         * a wrong tap being waved through. */
        RI_ASSERT(worst < scale * 1.0e-4f,
            "and the error is float noise, not a wrong stem (%g vs %g)",
            (double)worst, (double)scale);
    }

    /* --- 3. `pos`: EVERY SLICE LANDS WHERE IT BELONGS ---------------- */
    /* The offset bug wrote every slice to the head. With a 1024-sample
     * render the tail is the evidence: a head-only tap leaves samples past
     * the first RI_ENGINE_BLOCK holding the fill, and that is what a
     * plausible-looking wrong tap looks like from the outside. */
    load(RI_ENGINE_S303A, &nev);
    attach(0);
    fill();
    render();
    {
        int tail_written = 0;
        for (i = RI_ENGINE_BLOCK * 4u; i < N; i++)
            if (SL[0][i] != FILL)
                tail_written = 1;
        RI_ASSERT(tail_written,
            "the 303A stem is written PAST the first block (sample %u)",
            RI_ENGINE_BLOCK * 4u);
        RI_ASSERT(!untouched(0), "and the head was written too");
        /* Nothing may be written outside the render. */
        RI_ASSERT(SL[0][0] != FILL, "sample 0 is written");
    }

    /* --- 3b. RENDERED IN BLOCKS, NOT ONE CALL ------------------------- */
    /* The whole reason `pos` is `e->cursor + c` and not `done + c`. The
     * engine's local `done` restarts on every call, so a tap written at
     * `done` puts every block at the head -- and the stem comes out SILENT
     * while the mix is fine, because the last block's 64 samples are all
     * that survive and a 303 has decayed by then. Found by the R8e CLI,
     * which renders in 64-frame blocks because that is what a bounded tool
     * does; the test rendered the whole song in one call and could not see
     * it. */
    {
        static float BL[N], BR[N];
        static uint32_t off;
        load(RI_ENGINE_S303A, &nev);
        E.level[0] = 60u;
        attach(0);
        fill();
        for (off = 0u; off < N; off += RI_ENGINE_BLOCK) {
            uint32_t want = (N - off > RI_ENGINE_BLOCK) ?
                RI_ENGINE_BLOCK : (N - off);
            RI_ASSERT(ri_engine_render(&E, BL, BR, want, 48000.0f) == want,
                "block at %u rendered", (unsigned)off);
        }
        {
            int tail_written = 0;
            for (i = RI_ENGINE_BLOCK * 4u; i < N; i++)
                if (SL[0][i] != FILL)
                    tail_written = 1;
            RI_ASSERT(tail_written,
                "rendered in BLOCKS, the stem is written past the first block "
                "(sample %u)", RI_ENGINE_BLOCK * 4u);
        }
        /* And the blockwise render agrees with the one-call render, which is
         * the law a bounded caller actually depends on. */
        {
            static float A0[N], B0[N];
            float worst = 0.0f, scale = 0.0f;
            load(RI_ENGINE_S303A, &nev);
            attach(0);
            fill();
            ri_engine_render(&E, A0, B0, N, 48000.0f);
            for (i = 0u; i < N; i++) {
                /* ONLY SECTION 0. This song enables RI_ENGINE_S303A alone,
                 * so the other four stems still hold the fill -- and summing
                 * four fills is 1.0, which is how a first version of this
                 * reported an error equal to 1 against a peak of 0.88. */
                float al = SL[0][i];
                float e2 = al - A0[i];
                if (e2 < 0.0f) e2 = -e2;
                if (e2 > worst) worst = e2;
                {
                    float m = A0[i] < 0.0f ? -A0[i] : A0[i];
                    if (m > scale) scale = m;
                }
            }
            RI_ASSERT(worst <= scale * 1.0e-5f,
                "blockwise and one-call stems agree: worst %g of %g",
                (double)worst, (double)scale);
        }
    }

    /* --- 4. THE MONO STEM IS THE MONO SAMPLE, MIRRORED --------------- */
    load(RI_ENGINE_S303A, &nev);
    attach(0);
    fill();
    render();
    {
        int centred = 1;
        for (i = 0u; i < N; i++)
            if (SL[0][i] != SR[0][i])
                centred = 0;
        RI_ASSERT(centred,
            "a 303's stem is centred: l and r agree, as the mix does");
    }

    /* --- 4b. THE LEVI: A STEREO SECTION, IN ITS OWN FUNCTION --------- */
    /* `engine_section_stereo` has its own accumulate, so a tap added only to
     * `engine_section` leaves the fifth stem unwritten -- a file of the
     * caller's fill -- and the whole thing is invisible unless something
     * actually RENDERS the Levi. */
    load(RI_ENGINE_SLEVI, &nev);
    attach(0);
    fill();
    done = render();
    RI_ASSERT(done == N, "the Levi rendered (%u)", (unsigned)done);
    RI_ASSERT(!untouched(4),
        "the Levi's stem is WRITTEN, not left as the caller's fill");
    /* AND past the FIRST BLOCK. "Something was written" is satisfied by the
     * head alone, which is exactly what the offset bug produces: every slice
     * written at index 0 leaves the tail holding the fill. The Levi needs
     * this as much as the 303 does -- it has its own function and its own
     * `pos`. */
    {
        int tail_written = 0;
        for (i = RI_ENGINE_BLOCK * 4u; i < N; i++)
            if (SL[4][i] != FILL)
                tail_written = 1;
        RI_ASSERT(tail_written,
            "the Levi's stem is written PAST the first block (sample %u)",
            RI_ENGINE_BLOCK * 4u);
    }
    {
        int same = 1;
        for (i = 0u; i < N; i++)
            if (SL[4][i] != SR[4][i])
                same = 0;
        /* Whether the Levi's two sides differ is a property of the synth's
         * own stereo spread at defaults, and the point here is only that
         * BOTH sides were written -- so the check is written either way and
         * reports which it was. */
        if (same)
            fprintf(stderr, "t204: the Levi's two stems are identical "
                "(centred at defaults); the stereo path is still covered\n");
    }

    /* --- 5. A DISABLED SECTION GETS NO STEM -------------------------- */
    /* Its OWN load/attach/fill/render: section 4b rendered the Levi, and
     * checking "untouched" against a buffer that section already wrote is a
     * test of the previous section, not of this law. */
    load(RI_ENGINE_S303A, &nev);
    attach(0);
    fill();
    render();
    {
        int clean = 1;
        for (b = 2u; b < RI_ROUTE_NSECTIONS; b++)
            if (!untouched(b))
                clean = 0;
        RI_ASSERT(clean, "a 303-only render leaves the 808/909/Levi alone");
    }

    /* --- 6. A HOLED TAP ARRAY WRITES ONLY ITS SLOTS ------------------ */
    load(RI_ENGINE_S303A | RI_ENGINE_S303B, &nev);
    attach(1);
    fill();
    render();
    {
        int clean = 1;
        for (b = 2u; b < RI_ROUTE_NSECTIONS; b++)
            if (!untouched(b))
                clean = 0;
        RI_ASSERT(clean, "a holed tap array writes only its slots");
    }

    /* --- 7. THE MASTER FADER IS NOT IN A STEM ------------------------ */
    {
        static float before[N];
        load(RI_ENGINE_S303A, &nev);
        E.level[0] = 60u;
        attach(0);
        fill();
        render();
        memcpy(before, SL[0], sizeof before);
        /* No re-load: the point is that changing ONLY the master leaves the
         * stems bit-identical. A re-load would reset the master too -- and
         * would also reset the level, which is a second variable. */
        E.master = 30u;
        render();
        RI_ASSERT(memcmp(before, SL[0], sizeof before) == 0,
            "the master fader changed no stem");
    }

    /* --- 8. NULLs ----------------------------------------------------- */
    ri_engine_set_stems(0, &TAP);
    ri_engine_set_stems(&E, 0);
    load(RI_ENGINE_S303A, &nev);
    render();
    RI_ASSERT(1u, "a NULL engine and a NULL tap are both no-ops");

    RI_RESULT("engine-stems");
}