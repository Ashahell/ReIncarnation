/* project/stem_wav.h — R8 stem export: the WAV writer and the set that keeps
 * stems aligned. Pure C, host-tested, no IO (the caller supplies a buffer).
 *
 * SCOPE. This is the FILE side of stem export only. Rendering each mixer
 * strip is the offline renderer's job, and keeping the two apart is what
 * lets these laws be tested without an engine: a WAV has no schema and no
 * error reporting, so every one of these mistakes produces a file that
 * either will not open or will open and be subtly wrong.
 *
 * WHY SIZES ARE THE WHOLE POINT. Writing 0, or 0xFFFFFFFF for "streamed",
 * in the RIFF and data chunk sizes is the most common way to produce a WAV
 * that no DAW opens. `stem_wav_write` therefore COMPUTES both from the
 * frame count and asserts nothing about the caller: there is no way to ask
 * it for a placeholder.
 *
 * WHY THE SET EXISTS. "Sample-aligned and of equal length" is a property of
 * a GROUP of files, not of one, so it cannot be enforced by a single-file
 * writer. `RIStemSet` owns the common length (the longest stem) and COUNTS
 * the stems it had to pad, because a stem silently trimmed at the end is a
 * song that ends early on one channel and nobody hears why.
 *
 * BIT DEPTHS: 16-bit PCM (format 1), 24-bit PCM (format 1, THREE bytes a
 * sample -- the usual off-by-one, because it is not a byte count), and
 * 32-bit IEEE float (format 3, which is not format 1 with 32 bits).
 */
#ifndef RI_STEMWAV_H
#define RI_STEMWAV_H
#include <stdint.h>

#define STEM_WAV_PCM16 16u
#define STEM_WAV_PCM24 24u
#define STEM_WAV_F32   32u   /* IEEE float: format code 3, not 1 */

#define RI_STEM_MAX 16u

struct RIStemSet {
    const float *samples[RI_STEM_MAX];
    uint32_t frames[RI_STEM_MAX];   /* as supplied, before padding */
    uint8_t  channels[RI_STEM_MAX];
    uint8_t  nstems;
    uint8_t  depth;
    uint8_t  pad;                   /* unused: the struct is explicit */
    uint32_t padded;                /* stems shorter than the common length */
    /* Samples that were NOT FINITE and were written as silence, accumulated
     * across every stem this set has written. See the clamp law below: the
     * number is the deliverable, because the silence is invisible. */
    uint32_t nonfinite;
    uint32_t rate;
    uint32_t bpm_milli;
    uint32_t common_frames;         /* the LONGEST stem: nothing is trimmed */
};

/* THE CLAMP LAW, AND THE GUARD THAT REPLACED IT (owner ruling 2026-10-12).
 *
 * **FLOAT IS NOT CLAMPED.** The stems are taken before the master and before
 * the limiter, so a stem above full scale is ORDINARY rather than exceptional,
 * and headroom above 1.0 is the entire reason anyone exports 32-bit float: a
 * DAW imports those samples untouched and the user turns the gain down.
 * Clamping float throws that away permanently, in the file.
 *
 * **16 AND 24 STILL CLAMP**, because they cannot represent anything above
 * full scale in the first place; there is no headroom to keep, so the honest
 * result is the nearest value the format holds.
 *
 * **A NON-FINITE SAMPLE IS WRITTEN AS SILENCE AND COUNTED**, at every depth.
 * Before this, the clamp did not catch NaN at all -- `v > 1.0f` and
 * `v < -1.0f` are both FALSE for NaN, so it sailed through to
 * `(int32_t)(NaN * 32767.0f)`, which is undefined behaviour and on x86 gave
 * whatever the conversion happened to produce. A single NaN in a float file
 * poisons a DAW track, and this project has the history to mean it:
 * `docs/evidence/portability/levi-filter-nan.md` (Dell 2026-09-28) is a
 * latched filter NaN that silenced the whole device until release.
 *
 * The test used is the Leviasynth's own, from that same fix -- the range
 * conjunction, which is false for NaN because both comparisons are false:
 *      if (!(v > -1e20f && v < 1e20f)) { v = 0.0f; nonfinite++; }
 * One idiom in the tree, not two.
 *
 * `nonfinite` may be NULL when the caller does not want the count. Pass a
 * `uint32_t` to be told what the file is not telling you.
 *
 * One stem to a caller-supplied buffer. Returns bytes written, or 0 for any
 * refusal: NULL, zero frames, zero channels, a zero rate, an unsupported
 * bit depth, or a buffer too small. A stem is never truncated. */
uint32_t stem_wav_write(uint8_t *out, uint32_t cap, const float *samples,
    uint32_t frames, uint32_t channels, uint32_t rate, uint32_t depth,
    const char *path, uint32_t *nonfinite);

/* Little-endian readers, exposed because the test asserts on the BYTES and
 * a test that trusts the writer's own view of the file proves nothing. */
uint32_t stem_wav_u32(const uint8_t *p);
uint32_t stem_wav_u16(const uint8_t *p);

void stem_set_init(struct RIStemSet *s, uint32_t rate, uint32_t bpm_milli);
int stem_set_add(struct RIStemSet *s, const float *samples, uint32_t frames,
    uint32_t channels);
uint32_t stem_set_frames(const struct RIStemSet *s);
uint32_t stem_set_padded(const struct RIStemSet *s);
uint32_t stem_set_count(const struct RIStemSet *s);
/* Write stem `idx` at the SET's common length, padding with silence.
 *
 * NOT const, and that is the price of the count: this accumulates
 * `s->nonfinite` across every stem written, which is what makes the number a
 * figure for the RENDER rather than for one file. A const `s` would have
 * made that impossible, and a per-file count nobody sums is a count nobody
 * reads. The audio itself is still only read. */
uint32_t stem_set_write(struct RIStemSet *s, uint32_t idx, uint8_t *out,
    uint32_t cap);
/* tempo= / rate= / frames= / stems=, so a loop rendered at a tempo the DAW
 * does not know about does not silently fail to warp. */
uint32_t stem_set_meta(const struct RIStemSet *s, char *out, uint32_t cap);

#endif