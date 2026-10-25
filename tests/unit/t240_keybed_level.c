/* t240_keybed_level — the keybed plays one voice against the song's chordal
 * Levi part, and at parity per note it is 6 dB below the part it plays over.
 *
 * WHAT THE OWNER HEARD (2026-10-24, build 2f6d65f): "all piano key produce
 * sound now, but their volume is lower than the song being played."
 *
 * WHAT THE EVIDENCE SAYS, BEFORE THE RULE:
 *  - the keybed sends RI_CTL_LEVI_NOTE at velocity 127 (t238) and the live
 *    note path and the song path are the SAME fire: on the host, with the
 *    song's own patch and mix levels, peak(live 127) == peak(song note) to
 *    the bit (0.112997852). The keybed is not attenuated per note.
 *  - the song's Levi bank is chordal: pattern 1 row 4 carries notes
 *    46,73,34,61 on four lanes, so the part sums four voices at the same
 *    per-voice level. Uncorrelated voices sum as sqrt(n) in RMS, so one
 *    key is 1/4 of the part's peak and about 1/2 of its RMS.
 *  - measured on the host with the song's patch and mix: single key RMS
 *    0.073350373, song Levi part RMS 0.147373536, full mix RMS 0.1456.
 *    A single key is 6.05 dB below the part it plays over.
 *
 * THE RULE: the player's hands (the panel keybed and MIDI input, i.e.
 * levi_note_on/levi_note_vel) fire at RI_LEVI_LIVE_GAIN x the patch level,
 * so one key is as loud in RMS as the song's Levi part. The sequenced
 * paths stay at unity: levi_trigger (song lanes) and levi_note_strike
 * (the device sequencer and arp) are parts, not hands. A live gain is
 * stamped on the voice it fires and never leaks into a later fire.
 *
 * This pins a DECISION (the same shape as RI_LEVI_KEY_VEL 127 in t238):
 * the measurement above is the reason for 2.0, not a derivation of it.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"

#define SR 48000.0f
#define BS 256u
#define NBLK 40u

static struct RILeviSet A, B;

static void render_stereo(struct RILeviSet *s, uint32_t nblk,
    float *peak, float *rms) {
    static float l[BS], r[BS];
    uint32_t i, k;
    double sum = 0.0;
    float p = 0.0f;
    long n = 0;
    for (i = 0u; i < nblk; i++) {
        levi_voice_render_sum_stereo(s, l, r, BS, SR);
        for (k = 0u; k < BS; k++) {
            float a = l[k] < 0.0f ? -l[k] : l[k];
            float b = r[k] < 0.0f ? -r[k] : r[k];
            if (a > p)
                p = a;
            if (b > p)
                p = b;
            sum += (double)l[k] * (double)l[k] + (double)r[k] * (double)r[k];
            n += 2;
        }
    }
    *peak = p;
    if (rms)
        *rms = (float)__builtin_sqrt(sum / (double)n);
}

static float render_mono_peak(struct RILeviSet *s, uint32_t nblk) {
    static float o[BS];
    uint32_t i, k;
    float p = 0.0f;
    for (i = 0u; i < nblk; i++) {
        levi_voice_render_sum(s, o, BS, SR);
        for (k = 0u; k < BS; k++) {
            float a = o[k] < 0.0f ? -o[k] : o[k];
            if (a > p)
                p = a;
        }
    }
    return p;
}

static int feq(float a, float b) {
    float d = a > b ? a - b : b - a;
    float m = a > b ? a : b;
    m = m < 0.0f ? -m : m;
    return d <= m * 1e-5f + 1e-7f && d < 1e30f;
}

int main(void) {
    float song_peak, song_rms, live_peak, live_rms, live_mono, song_mono;
    float strike_peak;
    uint32_t v;

    /* The rule is a decision, not a derivation: one named constant. */
    RI_ASSERT(RI_LEVI_LIVE_GAIN > 1.0f, "live gain is a boost (%f)",
        (double)RI_LEVI_LIVE_GAIN);
    RI_ASSERT(feq(RI_LEVI_LIVE_GAIN, 2.0f), "live gain is +6 dB (%f)",
        (double)RI_LEVI_LIVE_GAIN);

    /* Defaults: nothing is boosted until a hand fires a note. */
    levi_init_set(&A);
    RI_ASSERT(feq(A.p_livegain, 1.0f), "pending live gain starts at unity");
    for (v = 0u; v < RI_LEVI_NVOICES; v++)
        RI_ASSERT(feq(A.v[v].livegain, 1.0f),
            "voice %u starts at unity (%f)", v, (double)A.v[v].livegain);

    /* The song path is the reference: one lane, one note, unity. */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    RI_ASSERT(feq(A.v[0].livegain, 1.0f), "the song path is at unity");
    render_stereo(&A, NBLK, &song_peak, &song_rms);
    RI_ASSERT(song_peak > 0.0f, "the reference note sounds");

    /* The player's hand: the same note at velocity 127 is the gain louder. */
    levi_init_set(&B);
    levi_note_vel(&B, 60u, 127u);
    RI_ASSERT(feq(B.v[0].livegain, RI_LEVI_LIVE_GAIN),
        "the live note carries the live gain (%f)", (double)B.v[0].livegain);
    RI_ASSERT(B.v[0].zgain == 1.0f,
        "the live gain is not folded into the zone gain (t148)");
    render_stereo(&B, NBLK, &live_peak, &live_rms);
    RI_ASSERT(feq(live_peak / song_peak, RI_LEVI_LIVE_GAIN),
        "live peak is the gain times the song note (%f vs %f)",
        (double)live_peak, (double)(song_peak * RI_LEVI_LIVE_GAIN));
    RI_ASSERT(feq(live_rms / song_rms, RI_LEVI_LIVE_GAIN),
        "live RMS is the gain times the song note (%f vs %f)",
        (double)live_rms, (double)(song_rms * RI_LEVI_LIVE_GAIN));

    /* Both render paths carry it: the mono sum is the same boost. */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    song_mono = render_mono_peak(&A, NBLK);
    levi_init_set(&B);
    levi_note_vel(&B, 60u, 127u);
    live_mono = render_mono_peak(&B, NBLK);
    RI_ASSERT(feq(live_mono / song_mono, RI_LEVI_LIVE_GAIN),
        "mono render carries the gain (%f vs %f)",
        (double)live_mono, (double)(song_mono * RI_LEVI_LIVE_GAIN));

    /* The plain live entry point is a hand too (chord path included). */
    levi_init_set(&B);
    levi_note_on(&B, 60u);
    RI_ASSERT(feq(B.v[0].livegain, RI_LEVI_LIVE_GAIN),
        "levi_note_on is a hand (%f)", (double)B.v[0].livegain);

    /* A retrigger stamps the gain once, not twice. */
    levi_note_vel(&B, 60u, 127u);
    RI_ASSERT(feq(B.v[0].livegain, RI_LEVI_LIVE_GAIN),
        "a retrigger does not square the gain (%f)", (double)B.v[0].livegain);

    /* The device sequencer is a part, not a hand: it stays at unity. */
    levi_init_set(&A);
    levi_note_strike(&A, 60u);
    RI_ASSERT(feq(A.v[0].livegain, 1.0f),
        "the device sequencer stays at unity (%f)", (double)A.v[0].livegain);
    render_stereo(&A, NBLK, &strike_peak, NULL);
    RI_ASSERT(feq(strike_peak, song_peak),
        "a struck note is the reference level (%f vs %f)",
        (double)strike_peak, (double)song_peak);

    /* A mono release re-fires the held key: it is still a hand. */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    levi_note_vel(&A, 60u, 127u);
    levi_note_vel(&A, 64u, 127u);
    levi_note_off(&A, 64u);
    RI_ASSERT(feq(A.v[0].livegain, RI_LEVI_LIVE_GAIN),
        "a mono re-fire of the held key keeps the hand gain (%f)",
        (double)A.v[0].livegain);

    /* No leak: the song's next fire on the same voice is at unity. */
    levi_init_set(&A);
    levi_note_vel(&A, 60u, 127u);
    levi_note_off(&A, 60u);
    levi_trigger(&A, 0u, 64u);
    RI_ASSERT(feq(A.v[0].livegain, 1.0f),
        "a song fire after a live note is at unity (%f)",
        (double)A.v[0].livegain);
    RI_ASSERT(feq(A.p_livegain, 1.0f),
        "the pending live gain is reset after the fire (%f)",
        (double)A.p_livegain);

    RI_RESULT("keybed_level");
    return 0;
}
