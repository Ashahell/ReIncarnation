/* t189_live_driver_clkout — M5e1: the render feeds the clock-out producer,
 * and nothing else does.
 *
 * The plan's rule is "no CAMD in the render path". The honest way to keep
 * that is to make the render's only involvement be *filling a ring*: the
 * driver knows the sample position and hands it to the producer, and the
 * actual sending happens elsewhere (M5e2's AROS task). So these laws pin
 * the seam:
 *
 * - with no producer attached (the NULL case, which is every caller
 *   today), the render path behaves exactly as before;
 * - with a producer attached, the bytes queued are exactly what the
 *   schedule owes for the samples actually rendered -- no more, no fewer,
 *   which is the difference between a clock derived from the audio clock
 *   and one derived from a timer;
 * - a Play command opens the stream with FA and a Stop closes it with FC,
 *   because a transport edge that does not reach the wire makes the clock
 *   meaningless;
 * - a producer that is DISABLED (the E0 default) receives nothing at all.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "app/core/live_driver.h"
#include "engine/live.h"
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"
#include "midi_io/midi_out.h"

#define SR 48000u
#define FRAMES 256u
#define BLOCKS 200u          /* 51200 samples = 1.0667 s */

static struct RILiveDriver s_drv;
static struct RILiveSession s_ses;
static struct RISongTrack s_tr;
static struct RIPatternBank s_bank;
static struct RIEvent s_ev[512];
static struct RIMidiOut s_out;
static int16_t s_pcm[FRAMES * 2u];
static float s_fl[FRAMES], s_fr[FRAMES];

static void fixture(void) {
    const struct RIPatternBank *b5[5];
    uint32_t q;
    ri_bank_init(&s_bank, 4u, RI_PATTERN_KIND_LEVI, 0u);
    for (q = 0u; q < 32u; q++)
        ri_pattern_set_length(&s_bank.pat[q], 16u);
    b5[0] = b5[1] = b5[2] = b5[3] = 0;
    b5[4] = &s_bank;
    ri_live_init(&s_ses, 96u, (float)SR, 120.0f, RI_ENGINE_SLEVI, s_ev, 512u);
    ri_live_set_banks(&s_ses, b5, &s_tr, 0u);
    ri_livedrv_init(&s_drv, &s_ses, FRAMES, 0);
}

static uint32_t drain_bytes(uint8_t *buf, uint32_t cap) {
    return midi_out_read(&s_out, buf, cap);
}

static uint32_t count_byte(const uint8_t *b, uint32_t n, uint8_t v) {
    uint32_t i, k = 0u;
    for (i = 0u; i < n; i++)
        if (b[i] == v)
            k++;
    return k;
}

int main(void) {
    static uint8_t buf[4096];
    uint32_t n, blocks;

    /* --- the NULL case: no producer, no change ------------------------- */
    fixture();
    s_drv.clk_out = 0;
    ri_live_play(&s_ses);
    for (blocks = 0u; blocks < BLOCKS; blocks++)
        ri_livedrv_render(&s_drv, s_pcm, s_fl, s_fr, FRAMES);
    RI_ASSERT(ri_atomic_load_acq(&s_drv.buffers) == BLOCKS, "all buffers rendered (%lu)",
        (unsigned long)ri_atomic_load_acq(&s_drv.buffers));
    RI_ASSERT(ri_atomic_load_acq(&s_drv.xruns) == 0u, "no xruns");

    /* --- a DISABLED producer receives nothing -------------------------- */
    fixture();
    midi_out_init(&s_out, SR, 120000u, 0u);      /* off by default */
    s_drv.clk_out = &s_out;
    ri_live_play(&s_ses);
    for (blocks = 0u; blocks < BLOCKS; blocks++)
        ri_livedrv_render(&s_drv, s_pcm, s_fl, s_fr, FRAMES);
    n = drain_bytes(buf, sizeof buf);
    RI_ASSERT(n == 0u, "a disabled producer got %u bytes", (unsigned)n);

    /* --- an ENABLED producer gets exactly the schedule ---------------- */
    fixture();
    midi_out_init(&s_out, SR, 120000u, 0u);
    midi_out_enable(&s_out, 1);
    s_drv.clk_out = &s_out;
    ri_live_play(&s_ses);
    /* Start the stream at the cursor the render is already at, so the
     * accounting is the driver's rather than a fiction. */
    midi_out_start(&s_out, s_ses.sample_cursor);
    n = drain_bytes(buf, sizeof buf);
    RI_ASSERT(n == 1u && buf[0] == 0xFAu, "start is FA (%u)", (unsigned)n);
    for (blocks = 0u; blocks < BLOCKS; blocks++)
        ri_livedrv_render(&s_drv, s_pcm, s_fl, s_fr, FRAMES);
    n = drain_bytes(buf, sizeof buf);
    /* 51200 samples at 120 BPM / 24 ppqn is 1000 samples per tick, so the
     * render owes 51 ticks -- 51 F8 bytes and nothing else. */
    RI_ASSERT(n == 51u, "51 ticks owed by 51200 samples, got %u",
        (unsigned)n);
    RI_ASSERT(count_byte(buf, n, 0xF8u) == n, "and every one is F8");
    /* The cursor really is where the accounting says it is. */
    RI_ASSERT(s_ses.sample_cursor >= (uint64_t)BLOCKS * FRAMES,
        "the session advanced past %u samples (%llu)",
        (unsigned)(BLOCKS * FRAMES),
        (unsigned long long)s_ses.sample_cursor);

    /* Rendering more produces exactly the extra ticks, not a re-send. */
    for (blocks = 0u; blocks < 100u; blocks++)
        ri_livedrv_render(&s_drv, s_pcm, s_fl, s_fr, FRAMES);
    n = drain_bytes(buf, sizeof buf);
    RI_ASSERT(n == 25u, "100 more blocks owe 25 more ticks, got %u",
        (unsigned)n);

    /* --- Stop closes the stream ---------------------------------------- */
    ri_atomic_store_rel(&s_drv.cmd, (uint32_t)RI_LIVE_CMD_STOP);
    ri_livedrv_render(&s_drv, s_pcm, s_fl, s_fr, FRAMES);
    n = drain_bytes(buf, sizeof buf);
    RI_ASSERT(n == 1u && buf[0] == 0xFCu, "stop is FC (%u/%02x)",
        (unsigned)n, n ? buf[0] : 0u);
    /* And no clock follows the stop. */
    for (blocks = 0u; blocks < 50u; blocks++)
        ri_livedrv_render(&s_drv, s_pcm, s_fl, s_fr, FRAMES);
    n = drain_bytes(buf, sizeof buf);
    RI_ASSERT(n == 0u, "no clock after stop (%u)", (unsigned)n);

    /* --- Play opens it again -------------------------------------------- */
    ri_atomic_store_rel(&s_drv.cmd, (uint32_t)RI_LIVE_CMD_PLAY);
    ri_livedrv_render(&s_drv, s_pcm, s_fl, s_fr, FRAMES);
    n = drain_bytes(buf, sizeof buf);
    RI_ASSERT(n == 1u && buf[0] == 0xFAu, "play reopens with FA (%u/%02x)",
        (unsigned)n, n ? buf[0] : 0u);

    /* --- and the audio is untouched by any of it ----------------------- */
    RI_ASSERT(ri_atomic_load_acq(&s_drv.xruns) == 0u, "still no xruns");

    RI_RESULT("live-driver-clkout");
}
