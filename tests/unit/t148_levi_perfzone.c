/* t148_levi_perfzone — Levi P9c keyboard zones: the octave bias, Single/Multi,
 * Lower/Upper/Both, Dual/KeySplit and the layer balance.
 *
 * Laws: the fields default to Single at the centre octave and an explicit
 * default moves nothing (bit for bit); the octave bias shifts the note the
 * zone plays, so a biased note-on is the *exact twin* of the same note
 * played on the shifted key, and the clamp is at both ends; a release finds
 * its voice through the bias (a stuck note is the defect this guards), and so
 * does per-key aftertouch; the routing table is pinned at the balance extreme
 * where one layer is exactly silent (gain 0) and the other exactly 2, which
 * makes every branch an identity or a silence; the two layer gains sum to 2,
 * so DUAL is one layer louder than SINGLE at *any* balance; a muted layer
 * still takes its voice; strikes pass through the zones and the song path
 * (levi_trigger) passes through none of them; keys, registry rows, the page,
 * its slots, its texts and which slots are live in which mode.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_matrix.h"
#include "engine/seq/autolane.h"
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"

#define SR 48000.0f
#define BS 256u

static struct RILeviSet A, B;

static void run(struct RILeviSet *s, uint32_t nblocks) {
    static float l[BS], r[BS];
    uint32_t i;
    for (i = 0u; i < nblocks; i++)
        levi_voice_render_sum_stereo(s, l, r, BS, SR);
}

static int feq(float a, float b, float tol) {
    float d = a > b ? a - b : b - a;
    float m = a > b ? a : b;
    m = m < 0.0f ? -m : m;
    return d <= m * tol + 1e-6f;
}

/* 1 when the two sets' next stereo blocks differ (same-age twins). */
static int diff_block(struct RILeviSet *a, struct RILeviSet *b) {
    static float la[BS], lb[BS], oa[BS], ob[BS];
    uint32_t k;
    levi_voice_render_sum_stereo(a, la, lb, BS, SR);
    levi_voice_render_sum_stereo(b, oa, ob, BS, SR);
    for (k = 0u; k < BS; k++)
        if (la[k] != oa[k] || lb[k] != ob[k])
            return 1;
    return 0;
}

/* Peak of the next stereo block: a layer gain is a pure multiply, so the peak
 * ratio between twins is the gain itself. */
static float peak_of(struct RILeviSet *s) {
    static float l[BS], r[BS];
    uint32_t k;
    float p = 0.0f;
    levi_voice_render_sum_stereo(s, l, r, BS, SR);
    for (k = 0u; k < BS; k++) {
        float m = fabsf(l[k]);
        if (m > p)
            p = m;
        m = fabsf(r[k]);
        if (m > p)
            p = m;
    }
    return p;
}

static float peak_mono(struct RILeviSet *s) {
    static float o[BS];
    uint32_t k;
    float p = 0.0f;
    levi_voice_render_sum(s, o, BS, SR);
    for (k = 0u; k < BS; k++) {
        float m = fabsf(o[k]);
        if (m > p)
            p = m;
    }
    return p;
}

static uint32_t active_count(const struct RILeviSet *s) {
    uint32_t v, n = 0u;
    for (v = 0u; v < RI_LEVI_NVOICES; v++)
        n += s->v[v].active ? 1u : 0u;
    return n;
}

static int perf(struct RILeviSet *s, uint32_t field, int val) {
    return levi_perf_set(s, field, val);
}

/* The single-layer reference peak for this exact key: a layer gain is a pure
 * multiply, so the peak ratio against this is the gain itself. Different
 * keys are different signals, so every ratio law takes its own reference. */
static float ref_peak(uint8_t note) {
    levi_init_set(&B);
    RI_ASSERT(levi_note_vel(&B, note, 100u) == 1, "reference note %u", note);
    return peak_of(&B);
}

/* The single-layer reference peak of the mono sum (same key, same laws). */
static float ref_peak_mono(uint8_t note) {
    levi_init_set(&B);
    RI_ASSERT(levi_note_vel(&B, note, 100u) == 1, "reference note %u", note);
    return peak_mono(&B);
}

int main(void) {
    /* ---- Keys, field ids and the value law. ---- */
    RI_ASSERT(RI_CTL_LEVI_PFOCT == 0x0EC5u && RI_CTL_LEVI_PFMODE == 0x0EC6u &&
        RI_CTL_LEVI_PFSEL == 0x0EC7u && RI_CTL_LEVI_PFSPLIT == 0x0EC8u &&
        RI_CTL_LEVI_PFBAL == 0x0EC9u, "zone keys");
    RI_ASSERT(RI_LEVI_PF_OCT == 0u && RI_LEVI_PF_MODE == 1u && RI_LEVI_PF_SELECT == 2u &&
        RI_LEVI_PF_SPLITM == 3u && RI_LEVI_PF_BALANCE == 4u && RI_LEVI_PF_SPLITKEY == 5u &&
        RI_LEVI_PF_NFIELDS == 6u, "field ids");
    levi_init_set(&A);
    RI_ASSERT(A.p_mode == RI_LEVI_PF_SINGLE && A.p_oct == 0 && A.p_bal == 64u &&
        A.p_sel == RI_LEVI_PF_BOTH && A.p_split == RI_LEVI_PF_DUAL && A.p_splitkey == 60u,
        "zone defaults");
    RI_ASSERT(perf(&A, RI_LEVI_PF_OCT, 0) == 0 && A.p_oct == -2, "oct min %d", A.p_oct);
    RI_ASSERT(perf(&A, RI_LEVI_PF_OCT, 4) == 0 && A.p_oct == 2, "oct max %d", A.p_oct);
    RI_ASSERT(perf(&A, RI_LEVI_PF_OCT, 99) == 0 && A.p_oct == 2, "oct clamped high %d", A.p_oct);
    RI_ASSERT(perf(&A, RI_LEVI_PF_OCT, -7) == 0 && A.p_oct == -2, "oct clamped low %d", A.p_oct);
    RI_ASSERT(perf(&A, RI_LEVI_PF_OCT, 2) == 0 && A.p_oct == 0, "oct centre");
    RI_ASSERT(perf(&A, RI_LEVI_PF_MODE, 1) == 0 && A.p_mode == 1u, "mode multi");
    RI_ASSERT(perf(&A, RI_LEVI_PF_MODE, 9) == 0 && A.p_mode == 1u, "mode clamped");
    RI_ASSERT(perf(&A, RI_LEVI_PF_MODE, 0) == 0 && A.p_mode == 0u, "mode single");
    RI_ASSERT(perf(&A, RI_LEVI_PF_SELECT, 2) == 0 && A.p_sel == 2u, "select both");
    RI_ASSERT(perf(&A, RI_LEVI_PF_SELECT, 7) == 0 && A.p_sel == 2u, "select clamped");
    RI_ASSERT(perf(&A, RI_LEVI_PF_SPLITM, 1) == 0 && A.p_split == 1u, "keysplit");
    RI_ASSERT(perf(&A, RI_LEVI_PF_SPLITM, 5) == 0 && A.p_split == 1u, "split clamped");
    RI_ASSERT(perf(&A, RI_LEVI_PF_BALANCE, 127) == 0 && A.p_bal == 127u, "bal max");
    RI_ASSERT(perf(&A, RI_LEVI_PF_BALANCE, 999) == 0 && A.p_bal == 127u, "bal clamped high");
    RI_ASSERT(perf(&A, RI_LEVI_PF_BALANCE, -3) == 0 && A.p_bal == 0u, "bal clamped low");
    RI_ASSERT(perf(&A, RI_LEVI_PF_SPLITKEY, 200) == 0 && A.p_splitkey == 127u, "splitkey clamp");
    RI_ASSERT(perf(&A, RI_LEVI_PF_SPLITKEY, 126) == 0 && A.p_splitkey == 126u,
        "splitkey just under the ceiling %u", A.p_splitkey);
    RI_ASSERT(perf(&A, RI_LEVI_PF_SPLITKEY, 0) == 0 && A.p_splitkey == 0u, "splitkey min");
    RI_ASSERT(perf(&A, RI_LEVI_PF_NFIELDS, 64) == 2, "unknown field refused");
    RI_ASSERT(perf(0, RI_LEVI_PF_MODE, 1) == 2, "null refused");
    RI_ASSERT(perf(&A, RI_LEVI_PF_BALANCE, 64) == 0 && A.p_bal == 64u, "bal centre");
    RI_ASSERT(perf(&A, RI_LEVI_PF_SPLITKEY, 60) == 0, "splitkey default back");

    /* The panel path is the section-wide 0x0E loop, so any voice index may
     * carry a device-wide field. */
    RI_ASSERT(levi_set_param_ui(&A, 5u, RI_CTL_LEVI_PFMODE & 0xFFu, 1u) == 0 && A.p_mode == 1u,
        "perf row through the voice setter");
    RI_ASSERT(levi_set_param_ui(&A, 0u, RI_CTL_LEVI_PFBAL & 0xFFu, 32u) == 0 && A.p_bal == 32u,
        "balance row through the voice setter");
    levi_init_set(&A);

    /* ---- Defaults, and an explicit default that moves nothing. ---- */
    levi_init_set(&A);
    levi_init_set(&B);
    perf(&A, RI_LEVI_PF_OCT, 2);
    perf(&A, RI_LEVI_PF_MODE, 0);
    perf(&A, RI_LEVI_PF_SELECT, 2);
    perf(&A, RI_LEVI_PF_SPLITM, 0);
    perf(&A, RI_LEVI_PF_BALANCE, 64);
    perf(&A, RI_LEVI_PF_SPLITKEY, 60);
    RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 1, "explicit defaults note");
    RI_ASSERT(levi_note_vel(&B, 60u, 100u) == 1, "default note");
    RI_ASSERT(!diff_block(&A, &B), "explicit defaults are the default");
    RI_ASSERT(active_count(&A) == 1u && A.v[0].zgain == 1.0f, "one voice at unity");

    /* ---- The octave bias is the shifted key, exactly. ---- */
    {
        static const struct { int oct; uint8_t played; uint8_t heard; } TW[5] = {
            { 3, 60u, 72u }, { 4, 60u, 84u }, { 1, 60u, 48u },
            { 4, 110u, 127u }, { 0, 5u, 0u }
        };
        uint32_t i;
        for (i = 0u; i < 5u; i++) {
            levi_init_set(&A);
            perf(&A, RI_LEVI_PF_OCT, TW[i].oct);
            levi_init_set(&B);
            RI_ASSERT(levi_note_vel(&A, TW[i].played, 100u) == 1, "biased note %u", i);
            RI_ASSERT(levi_note_vel(&B, TW[i].heard, 100u) == 1, "heard note %u", i);
            RI_ASSERT(A.v[0].note == TW[i].heard, "voice holds the shifted note %u (%u)",
                i, A.v[0].note);
            /* The arp and the hold list see the played key, not the shift. */
            RI_ASSERT(A.an == 1u && A.anotes[0] == TW[i].played, "hold list keeps the played key");
            RI_ASSERT(!diff_block(&A, &B), "bias %d playing %u is the %u twin", TW[i].oct,
                TW[i].played, TW[i].heard);
        }
    }

    /* ---- A release finds its voice through the bias. ---- */
    levi_init_set(&A);
    perf(&A, RI_LEVI_PF_OCT, 3);
    RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 1, "biased note");
    run(&A, 2u);
    RI_ASSERT(A.v[0].menv[2].stage != RI_LEVI_SEG_R, "still sounding");
    RI_ASSERT(levi_note_off(&A, 60u) == 1, "release the played key");
    RI_ASSERT(A.v[0].menv[2].stage == RI_LEVI_SEG_R, "the shifted voice was released");
    /* The bias moved after the note-on: the release still lands. */
    levi_init_set(&A);
    RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 1, "plain note");
    perf(&A, RI_LEVI_PF_OCT, 4);
    run(&A, 1u);
    RI_ASSERT(levi_note_off(&A, 60u) == 1, "release after the bias moved");
    RI_ASSERT(A.v[0].menv[2].stage == RI_LEVI_SEG_R, "the unshifted match released it");
    /* Per-key aftertouch follows the played key too. */
    levi_init_set(&A);
    perf(&A, RI_LEVI_PF_OCT, 1);
    RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 1, "biased note for pressure");
    RI_ASSERT(levi_polyat(&A, 60u, 100u) == 0 && A.pat[0] == 100u, "pressure reached the voice");
    RI_ASSERT(levi_polyat(&A, 48u, 90u) == 0 && A.pat[0] == 90u,
        "pressure on the note the key plays also lands");
    RI_ASSERT(levi_polyat(&A, 72u, 0u) == 0 && A.pat[0] == 90u, "an unheld key leaves it alone");

    /* ---- The routing table, pinned at the balance extreme. ---- */
    /* Balance 0: the upper layer is exactly silent, the lower exactly 2. */
    levi_init_set(&B);
    RI_ASSERT(levi_note_vel(&B, 60u, 100u) == 1, "single twin note");
    {
        /* SINGLE: one voice at unity (and the same as the song path). */
        levi_init_set(&A);
        perf(&A, RI_LEVI_PF_MODE, 0);
        RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 1, "single note");
        RI_ASSERT(feq(A.v[0].zgain, 1.0f, 1e-6f), "single gain 1");
        levi_init_set(&A);
        levi_init_set(&B);
        RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 1, "single again");
        RI_ASSERT(levi_trigger(&B, 0u, 60u) == 0, "song lane trigger");
        RI_ASSERT(B.v[0].zgain == 1.0f, "the song path never zones");
        RI_ASSERT(!diff_block(&A, &B), "Single is the song path, bit for bit");
        /* The song path ignores the bias and the mode as well. */
        levi_init_set(&A);
        perf(&A, RI_LEVI_PF_OCT, 4);
        perf(&A, RI_LEVI_PF_MODE, 1);
        perf(&A, RI_LEVI_PF_BALANCE, 0);
        RI_ASSERT(levi_trigger(&A, 3u, 60u) == 0, "song lane under the zones");
        RI_ASSERT(A.v[3].note == 60u && A.v[3].zgain == 1.0f && active_count(&A) == 1u,
            "the song path is untouched by the zones");
    }
    /* The single-voice reference peak, re-measured for the ratios below. */
    levi_init_set(&A);
    levi_init_set(&B);
    RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 1, "reference note");
    RI_ASSERT(levi_note_vel(&B, 60u, 100u) == 1, "reference twin note");
    {
        float ref = ref_peak(60u);
        float dual;
        RI_ASSERT(ref > 0.0f, "the reference is audible");
        /* MULTI + DUAL: two voices, one layer louder at any balance. */
        levi_init_set(&A);
        perf(&A, RI_LEVI_PF_MODE, 1);
        perf(&A, RI_LEVI_PF_SPLITM, 0);
        RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 2, "dual fires two layers");
        RI_ASSERT(active_count(&A) == 2u, "dual takes two voices");
        RI_ASSERT(A.v[0].zgain == 1.0f && A.v[1].zgain == 1.0f, "centre gains");
        dual = peak_of(&A);
        RI_ASSERT(feq(dual / ref, 2.0f, 1e-6f), "dual is twice the single layer %g", dual / ref);
        /* The mono sum applies the same law (its own twin, its own first
         * block: the block index is part of the measurement). */
        {
            float mref = ref_peak_mono(60u);
            levi_init_set(&A);
            perf(&A, RI_LEVI_PF_MODE, 1);
            perf(&A, RI_LEVI_PF_SPLITM, 0);
            RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 2, "dual for the mono sum");
            RI_ASSERT(feq(peak_mono(&A) / mref, 2.0f, 1e-6f), "the mono sum doubles too %g",
                peak_mono(&A) / mref);
        }
        /* The same at the extreme: the two gains sum to 2, so the sum does
         * not move -- but the upper layer is now exactly silent. */
        levi_init_set(&A);
        perf(&A, RI_LEVI_PF_MODE, 1);
        perf(&A, RI_LEVI_PF_SPLITM, 0);
        perf(&A, RI_LEVI_PF_BALANCE, 0);
        RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 2, "dual at balance 0");
        RI_ASSERT(A.v[0].zgain == 2.0f && A.v[1].zgain == 0.0f,
            "lower 2, upper silent (%f/%f)", A.v[0].zgain, A.v[1].zgain);
        RI_ASSERT(feq(peak_of(&A) / ref, 2.0f, 1e-6f), "balance does not change the sum");
        /* A muted layer still holds its voice. */
        RI_ASSERT(active_count(&A) == 2u, "the silent layer is a voice");
        /* KEY SPLIT + BOTH on a low key is that same signal (one layer). */
        levi_init_set(&A);
        perf(&A, RI_LEVI_PF_MODE, 1);
        perf(&A, RI_LEVI_PF_SPLITM, 1);
        perf(&A, RI_LEVI_PF_SELECT, 2);
        perf(&A, RI_LEVI_PF_BALANCE, 0);
        RI_ASSERT(levi_note_vel(&A, 50u, 100u) == 1, "low key under key split");
        RI_ASSERT(A.v[0].zgain == 2.0f, "below the split is the lower layer");
        RI_ASSERT(feq(peak_of(&A) / ref_peak(50u), 2.0f, 1e-6f), "the low key is the lower layer");
        /* The split key itself belongs to the upper layer: exactly silent. */
        levi_init_set(&A);
        perf(&A, RI_LEVI_PF_MODE, 1);
        perf(&A, RI_LEVI_PF_SPLITM, 1);
        perf(&A, RI_LEVI_PF_SELECT, 2);
        perf(&A, RI_LEVI_PF_BALANCE, 0);
        RI_ASSERT(levi_note_vel(&A, 59u, 100u) == 1, "just below the split");
        RI_ASSERT(feq(peak_of(&A) / ref_peak(59u), 2.0f, 1e-6f), "59 is below the split");
        levi_init_set(&A);
        perf(&A, RI_LEVI_PF_MODE, 1);
        perf(&A, RI_LEVI_PF_SPLITM, 1);
        perf(&A, RI_LEVI_PF_SELECT, 2);
        perf(&A, RI_LEVI_PF_BALANCE, 0);
        RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 1, "the split key");
        RI_ASSERT(peak_of(&A) == 0.0f, "the split key is the upper layer");
        levi_init_set(&A);
        perf(&A, RI_LEVI_PF_MODE, 1);
        perf(&A, RI_LEVI_PF_SPLITM, 1);
        perf(&A, RI_LEVI_PF_SELECT, 2);
        perf(&A, RI_LEVI_PF_BALANCE, 0);
        RI_ASSERT(levi_note_vel(&A, 100u, 100u) == 1, "high key, split decides");
        RI_ASSERT(peak_of(&A) == 0.0f, "above the split is the upper layer");
        /* SELECT overrides the split key. */
        levi_init_set(&A);
        perf(&A, RI_LEVI_PF_MODE, 1);
        perf(&A, RI_LEVI_PF_SPLITM, 1);
        perf(&A, RI_LEVI_PF_SELECT, 0);
        perf(&A, RI_LEVI_PF_BALANCE, 0);
        RI_ASSERT(levi_note_vel(&A, 100u, 100u) == 1, "high key, lower selected");
        RI_ASSERT(A.v[0].zgain == 2.0f && feq(peak_of(&A) / ref_peak(100u), 2.0f, 1e-6f),
            "SELECT LOWER sends every key down");
        levi_init_set(&A);
        perf(&A, RI_LEVI_PF_MODE, 1);
        perf(&A, RI_LEVI_PF_SPLITM, 1);
        perf(&A, RI_LEVI_PF_SELECT, 1);
        perf(&A, RI_LEVI_PF_BALANCE, 0);
        RI_ASSERT(levi_note_vel(&A, 30u, 100u) == 1, "low key, upper selected");
        RI_ASSERT(A.v[0].zgain == 0.0f && peak_of(&A) == 0.0f, "SELECT UPPER sends it up");
        /* The balance's other end: exact powers of two on the ratio. */
        levi_init_set(&A);
        perf(&A, RI_LEVI_PF_MODE, 1);
        perf(&A, RI_LEVI_PF_SPLITM, 1);
        perf(&A, RI_LEVI_PF_SELECT, 2);
        perf(&A, RI_LEVI_PF_BALANCE, 127);
        RI_ASSERT(levi_note_vel(&A, 72u, 100u) == 1, "high key at balance 127");
        RI_ASSERT(feq(A.v[0].zgain, 127.0f / 64.0f, 1e-6f), "upper gain %f", A.v[0].zgain);
        RI_ASSERT(feq(peak_of(&A) / ref_peak(72u), 127.0f / 64.0f, 1e-6f),
            "the upper layer is louder");
        levi_init_set(&A);
        perf(&A, RI_LEVI_PF_MODE, 1);
        perf(&A, RI_LEVI_PF_SPLITM, 1);
        perf(&A, RI_LEVI_PF_SELECT, 2);
        perf(&A, RI_LEVI_PF_BALANCE, 127);
        RI_ASSERT(levi_note_vel(&A, 50u, 100u) == 1, "low key at balance 127");
        RI_ASSERT(feq(A.v[0].zgain, 1.0f / 64.0f, 1e-6f), "lower gain %f", A.v[0].zgain);
        RI_ASSERT(feq(peak_of(&A) / ref_peak(50u), 1.0f / 64.0f, 1e-6f),
            "the lower layer is nearly gone");
        /* A different split key moves the boundary. */
        levi_init_set(&A);
        perf(&A, RI_LEVI_PF_MODE, 1);
        perf(&A, RI_LEVI_PF_SPLITM, 1);
        perf(&A, RI_LEVI_PF_SELECT, 2);
        perf(&A, RI_LEVI_PF_SPLITKEY, 40);
        perf(&A, RI_LEVI_PF_BALANCE, 0);
        RI_ASSERT(levi_note_vel(&A, 40u, 100u) == 1, "the moved split key");
        RI_ASSERT(peak_of(&A) == 0.0f, "40 is the upper layer now");
        /* The release reaches both layers, and a silent layer still releases. */
        levi_init_set(&A);
        perf(&A, RI_LEVI_PF_MODE, 1);
        perf(&A, RI_LEVI_PF_SPLITM, 0);
        RI_ASSERT(levi_note_vel(&A, 60u, 100u) == 2, "dual for the release");
        run(&A, 1u);
        RI_ASSERT(levi_note_off(&A, 60u) == 1, "release both");
        RI_ASSERT(A.v[0].menv[2].stage == RI_LEVI_SEG_R && A.v[1].menv[2].stage == RI_LEVI_SEG_R,
            "both layers released");
        /* A layer gain never outlives its fire: a song lane triggered after
         * the zones have played is back at unity. */
        perf(&A, RI_LEVI_PF_BALANCE, 0);
        levi_note_vel(&A, 60u, 100u);
        RI_ASSERT(levi_trigger(&A, 4u, 60u) == 0, "song lane on the same set");
        RI_ASSERT(A.v[4].zgain == 1.0f, "no pending layer gain leaks into a song lane");
    }

    /* ---- Strikes pass through the zones, and never join the hold list. ---- */
    levi_init_set(&A);
    perf(&A, RI_LEVI_PF_MODE, 1);
    perf(&A, RI_LEVI_PF_SPLITM, 0);
    RI_ASSERT(levi_note_strike(&A, 60u) == 2, "a strike fires both layers");
    RI_ASSERT(active_count(&A) == 2u && A.an == 0u, "two voices, no held chord");
    RI_ASSERT(levi_note_strike(0, 60u) == -1, "null strike refused");

    /* ---- Keys, registry rows and the panel page. ---- */
    {
        uint32_t k;
        static const uint32_t KEY[5] = { RI_CTL_LEVI_PFOCT, RI_CTL_LEVI_PFMODE,
            RI_CTL_LEVI_PFSEL, RI_CTL_LEVI_PFSPLIT, RI_CTL_LEVI_PFBAL };
        static const char *const LEG[5] = { "Octave", "Mode", "Select", "Split", "Balance" };
        static const uint32_t MAXV[5] = { 4u, 1u, 2u, 1u, 127u };
        static const uint32_t DEFV[5] = { 2u, 0u, 2u, 0u, 64u };
        for (k = 0u; k < 5u; k++) {
            const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | (221u + k)));
            RI_ASSERT(d && d->engine_id == KEY[k], "row %u binds", 221u + k);
            RI_ASSERT(d && (uint32_t)d->max_v == MAXV[k] && (uint32_t)d->def_v == DEFV[k],
                "row %u range/default", 221u + k);
            RI_ASSERT(d && d->kind == (k == 0u || k == 4u ? RI_CK_KNOB : RI_CK_SELECTOR) &&
                d->automatable && d->bind == RI_BIND_LEVI, "row %u kind", 221u + k);
            RI_ASSERT(d && !strcmp(d->group, "Performance") && !strcmp(d->legend, LEG[k]),
                "row %u group/legend", 221u + k);
            RI_ASSERT(ri_auto_allowed((uint16_t)KEY[k]), "key %04x allowed", KEY[k]);
        }
        RI_ASSERT(ri_auto_allowed(0x0EC9u) && !ri_auto_allowed(0x0ECAu), "0x0ECA refused");
    }
    {
        struct RISectLevi lv;
        char tx[16];
        memset(&lv, 0, sizeof lv);
        ri_slevi_init(&lv);
        RI_ASSERT(RI_SLEVI_PFOCT == 221u && RI_SLEVI_PFBAL == 225u && RI_SLEVI_NCTL == 226u,
            "panel rows");
        RI_ASSERT(lv.val[RI_SLEVI_PFOCT] == 2 && lv.val[RI_SLEVI_PFMODE] == 0 &&
            lv.val[RI_SLEVI_PFSEL] == 2 && lv.val[RI_SLEVI_PFSPLIT] == 0 &&
            lv.val[RI_SLEVI_PFBAL] == 64, "panel defaults");
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_PERF) == 1, "perf page");
        RI_ASSERT(ri_slevi_page_count(&lv) == 1u, "performance is one page");
        RI_ASSERT(!strcmp(ri_slevi_page_title(&lv), "PERFORMANCE"), "page title %s",
            ri_slevi_page_title(&lv));
        /* Live slots follow the mode. Single is the default, and there the
         * zone knobs do nothing, so they draw dim with their names. */
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && !strcmp(ri_slevi_enc_name(&lv, 0u), "OCTAVE"),
            "slot 0");
        RI_ASSERT(ri_slevi_enc_live(&lv, 1u) && !strcmp(ri_slevi_enc_name(&lv, 1u), "MODE"),
            "slot 1");
        RI_ASSERT(!ri_slevi_enc_live(&lv, 2u) && !strcmp(ri_slevi_enc_name(&lv, 2u), "SELECT"),
            "single dims select");
        RI_ASSERT(!ri_slevi_enc_live(&lv, 3u) && !strcmp(ri_slevi_enc_name(&lv, 3u), "SPLIT"),
            "single dims split");
        RI_ASSERT(!ri_slevi_enc_live(&lv, 4u) && !strcmp(ri_slevi_enc_name(&lv, 4u), "BALANCE"),
            "single dims balance");
        RI_ASSERT(!ri_slevi_enc_live(&lv, 5u) && !ri_slevi_enc_live(&lv, 6u) &&
            !ri_slevi_enc_live(&lv, 7u), "three dead slots");
        /* Encoder travel is scaled across the row's range (the ribbon
         * law), so 0 and 127 are the ends and 64 the centre. */
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 1u, 127) == 1, "mode multi");
        RI_ASSERT(ri_slevi_enc_live(&lv, 3u) && ri_slevi_enc_live(&lv, 4u), "multi lights them");
        RI_ASSERT(!ri_slevi_enc_live(&lv, 2u), "dual (the default split) dims select");
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 3u, 127) == 1, "key split");
        RI_ASSERT(ri_slevi_enc_live(&lv, 2u) && !strcmp(ri_slevi_enc_name(&lv, 2u), "SELECT"),
            "key split lights select");
        lv.page = 0u;
        ri_slevi_enc_text(&lv, 0u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "0"), "octave centre text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 0u, 0) == 1, "octave min");
        ri_slevi_enc_text(&lv, 0u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "-2"), "octave min text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 0u, 127) == 1, "octave max");
        ri_slevi_enc_text(&lv, 0u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "+2"), "octave max text %s", tx);
        RI_ASSERT(ri_slevi_reset(&lv, RI_SLEVI_ENC0 + 0u) == 1 && lv.val[RI_SLEVI_PFOCT] == 2,
            "octave reset");
        ri_slevi_enc_text(&lv, 1u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "MULTI"), "mode text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 1u, 0) == 1, "mode single");
        ri_slevi_enc_text(&lv, 1u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "SINGLE"), "mode single text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 1u, 127) == 1, "mode multi");
        ri_slevi_enc_text(&lv, 2u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "BOTH"), "select text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 2u, 0) == 1, "select lower");
        ri_slevi_enc_text(&lv, 2u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "LOWER"), "select lower text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 2u, 64) == 1, "select upper");
        ri_slevi_enc_text(&lv, 2u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "UPPER"), "select upper text %s", tx);
        RI_ASSERT(ri_slevi_reset(&lv, RI_SLEVI_ENC0 + 2u) == 1, "select reset");
        ri_slevi_enc_text(&lv, 3u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "KEY SPLIT"), "split keysplit text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 3u, 0) == 1, "split dual");
        ri_slevi_enc_text(&lv, 3u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "DUAL"), "split dual text %s", tx);
        RI_ASSERT(!ri_slevi_enc_live(&lv, 2u), "back to dual, select dims again");
        ri_slevi_enc_text(&lv, 4u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "MID"), "balance centre text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 4u, 0) == 1, "balance min");
        ri_slevi_enc_text(&lv, 4u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "-64"), "balance min text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 4u, 127) == 1, "balance max");
        ri_slevi_enc_text(&lv, 4u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "+63"), "balance max text %s", tx);
        RI_ASSERT(ri_slevi_reset(&lv, RI_SLEVI_ENC0 + 4u) == 1 && lv.val[RI_SLEVI_PFBAL] == 64,
            "balance reset");
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 3u, 127) == 1, "key split again");
        RI_ASSERT(ri_slevi_reset(&lv, RI_SLEVI_ENC0 + 1u) == 1, "mode reset");
        RI_ASSERT(lv.val[RI_SLEVI_PFMODE] == 0 && !ri_slevi_enc_live(&lv, 4u),
            "single again dims balance");
    }

    /* ---- Null-safety and bounded extremes. ---- */
    RI_ASSERT(levi_note_on(0, 60u) == -1, "null note-on refused");
    {
        uint32_t i;
        static float l[BS], r[BS];
        for (i = 0u; i < 400u; i++) {
            uint32_t k, bad = 0u;
            float x, y;
            perf(&A, RI_LEVI_PF_OCT, (int)(i % 7u));
            perf(&A, RI_LEVI_PF_MODE, (int)(i % 3u));
            perf(&A, RI_LEVI_PF_SELECT, (int)(i % 4u));
            perf(&A, RI_LEVI_PF_SPLITM, (int)(i % 3u));
            perf(&A, RI_LEVI_PF_BALANCE, (int)(i * 5u));
            perf(&A, RI_LEVI_PF_SPLITKEY, (int)(i % 200u));
            if (i % 8u == 0u) {
                int n = levi_note_vel(&A, (uint8_t)(i % 128u), (uint8_t)(i * 37u));
                RI_ASSERT(n == 1 || n == 2, "note-on count %d at %u", n, i);
                levi_polyat(&A, (uint8_t)(i % 128u), (uint8_t)(i * 91u));
            }
            if (i % 37u == 0u && active_count(&A))
                levi_note_off(&A, (uint8_t)(i % 128u));
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            for (k = 0u; k < BS; k++) {
                x = l[k];
                y = r[k];
                if (!(x > -1e20f && x < 1e20f) || !(y > -1e20f && y < 1e20f))
                    bad = 1u;
            }
            RI_ASSERT(!bad, "extremes stay finite at %u", i);
            RI_ASSERT(active_count(&A) <= RI_LEVI_NVOICES, "voice budget at %u", i);
        }
    }
    RI_RESULT("levi_perfzone");
}
