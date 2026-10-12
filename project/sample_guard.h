/* sample_guard.h — the ONE non-finite guard every WAV writer in this tree
 * uses (owner ruling 2026-10-12).
 *
 * THE PROBLEM THIS EXISTS TO SOLVE. Four places wrote samples to a file:
 * `project/stem_wav.c` (the stems), `audio_io/audio.c`'s two WAV sinks, and
 * `tools/render.c`'s own mix writer. Each had its own copy of the
 * non-finite test, and while the copies agreed they were four copies that
 * could stop agreeing. Worse, the copies disagreed with each other about
 * CLAMPING — the stem writer clipped float at +-1 and the mix writers did not
 * — and the only reason the disagreement was caught at all is that a test
 * happened to read the samples. A test that reads the samples catches the
 * instance; it does not stop the next writer from repeating it.
 *
 * SO THE TEST IS NOT THE DEFENCE. THE STRUCTURE IS. One static inline, four
 * call sites, and no call site can have its own opinion.
 *
 * HEADER-ONLY AND `static inline` ON PURPOSE. A shared *function* would be a
 * new translation unit, which in this tree means editing `ri_build_aros.sh`,
 * `ri_build_v11.sh` AND `build/portable.mk` -- a cross-module call that
 * builds and passes every host test and then fails the AROS RIAPP link. That
 * has happened twice. A header-only inline has no such entry in any list, so
 * it cannot be half-wired, and every build already compiles with `-I$ROOT`.
 *
 * WHY THE RANGE CONJUNCTION AND NOT `x != x`. A NaN fails BOTH comparisons,
 * so `x > -1e20f && x < 1e20f` is false and the negation is true. The
 * tempting `x > 1e20f || x < -1e20f` is ALSO false for NaN -- so negating it
 * makes NaN look fine. This is the form the Leviasynth's own 2026-09-28
 * latch-guard fix uses (`engine/dsp/levi.c`, the filter NaN that silenced
 * the device; see `docs/evidence/portability/levi-filter-nan.md`), and the
 * project's history is exactly why a guard is wanted rather than a clamp.
 *
 * It also rejects a FINITE value above 1e20, deliberately and consistently
 * with that fix: 1e20 in a file is unusable whatever its bit pattern says,
 * and the owner should never see one silently delivered as audio.
 */
#ifndef RI_SAMPLE_GUARD_H
#define RI_SAMPLE_GUARD_H

/* Replace `*v` with silence if it is not a usable sample. Returns 1 when it
 * did, so the caller can COUNT the repair.
 *
 * COUNTING IS NOT OPTIONAL IN SPIRIT. The caller may choose not to count
 * (pass NULL) but the SUBSTITUTION ALWAYS HAPPENS, because the guard is the
 * requirement and the count is only the report: a caller that declines to
 * count still gets a file with no NaN in it. A report nobody asked for is
 * cheap; a poisoned DAW track is not recoverable by re-rendering. */
static inline int ri_sample_guard(float *v) {
    float x = *v;
    if (x > -1e20f && x < 1e20f)
        return 0;               /* the ordinary case, and it costs two compares */
    *v = 0.0f;
    return 1;
}

#endif /* RI_SAMPLE_GUARD_H */
