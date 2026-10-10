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
    uint32_t rate;
    uint32_t bpm_milli;
    uint32_t common_frames;         /* the LONGEST stem: nothing is trimmed */
};

/* One stem to a caller-supplied buffer. Returns bytes written, or 0 for any
 * refusal: NULL, zero frames, zero channels, a zero rate, an unsupported
 * bit depth, or a buffer too small. A stem is never truncated. */
uint32_t stem_wav_write(uint8_t *out, uint32_t cap, const float *samples,
    uint32_t frames, uint32_t channels, uint32_t rate, uint32_t depth,
    const char *path);

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
/* Write stem `idx` at the SET's common length, padding with silence. */
uint32_t stem_set_write(const struct RIStemSet *s, uint32_t idx, uint8_t *out,
    uint32_t cap);
/* tempo= / rate= / frames= / stems=, so a loop rendered at a tempo the DAW
 * does not know about does not silently fail to warp. */
uint32_t stem_set_meta(const struct RIStemSet *s, char *out, uint32_t cap);

#endif