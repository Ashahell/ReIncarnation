/* t130_levi_algo_modes — Levi algorithm modes (fidelity plan P3, owner
 * 2026-09-30; manual pp. 58-61, clean-room: own 64 topologies).
 * Laws: 64 presets are loop-free with a carrier and all distinct, each
 * renders finite; Single = slot 1; Morph walks the 8-slot list (OFF
 * skipped, SILENCE silent, ends bit-exact with the pure presets); the
 * position key spans the live slots; Custom starts from what sounds and
 * takes up to 3 targets per oscillator, dropping loops; Mute drops an
 * op, Solo auditions one op (modulators too); fail-closed edges; the UI
 * algorithm pages, texts, keys and the "C" readout.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/seq/autolane.h"
#include "gui/sectlevi.h"

#define SR 48000.0f
#define N 4800u

static struct RILeviSet A, B;
static float oa[N], ob[N];

static void render(struct RILeviSet *s, float *o, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        o[i] = levi_voice_render(&s->v[0], 0, SR);
}

static float peak(const float *o, uint32_t n) {
    uint32_t i;
    float p = 0.0f;
    for (i = 0u; i < n; i++) {
        float a = o[i] < 0.0f ? -o[i] : o[i];
        if (!(a < 1e30f))
            return 1e30f;
        if (a > p)
            p = a;
    }
    return p;
}

int main(void) {
    struct RISectLevi u;
    uint32_t a, b, o, k;
    char t[32];
    uint16_t key;
    int val;

    /* ---- Presets: loop-free (feeds point down), op 1 a carrier, distinct. ---- */
    for (a = 0u; a < RI_LEVI_ALGO_N; a++) {
        RI_ASSERT(ri_levi_preset_feeds(a, 0u) == 0u, "algo %u op1 carrier", a + 1u);
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            RI_ASSERT((ri_levi_preset_feeds(a, o) >> o) == 0u, "algo %u op %u feeds down", a + 1u, o + 1u);
        for (b = 0u; b < a; b++) {
            int same = 1;
            for (o = 0u; o < RI_LEVI_NOPS; o++)
                same &= ri_levi_preset_feeds(a, o) == ri_levi_preset_feeds(b, o);
            RI_ASSERT(!same, "algo %u duplicates %u", a + 1u, b + 1u);
        }
        levi_init_set(&A);
        RI_ASSERT(levi_set_algo(&A, 0u, a) == 0, "set %u", a);
        levi_trigger(&A, 0u, 60u);
        render(&A, oa, N);
        RI_ASSERT(peak(oa, N) > 1e-4f && peak(oa, N) < 64.0f, "algo %u sounds finite %f", a + 1u, (double)peak(oa, N));
    }
    RI_ASSERT(ri_levi_preset_feeds(0u, 1u) == 0x01u, "algo 1 = the v1 pair (op2 -> op1)");

    /* ---- Single mode plays slot 1, bit-exact with set_algo. ---- */
    levi_init_set(&A);
    levi_init_set(&B);
    RI_ASSERT(levi_set_slot(&A, 0u, 0u, 17u) == 0, "slot1");
    RI_ASSERT(levi_set_algo(&B, 0u, 17u) == 0, "algo");
    levi_trigger(&A, 0u, 57u);
    levi_trigger(&B, 0u, 57u);
    render(&A, oa, N);
    render(&B, ob, N);
    RI_ASSERT(!memcmp(oa, ob, sizeof oa), "single == slot 1");

    /* ---- Morph: list walk, ends bit-exact, OFF skipped. ---- */
    levi_init_set(&A);
    levi_set_slot(&A, 0u, 0u, 3u);
    levi_set_slot(&A, 0u, 2u, 40u);            /* slot 2 OFF: skipped */
    RI_ASSERT(levi_set_amode(&A, 0u, RI_LEVI_AMODE_MORPH) == 0, "morph mode");
    RI_ASSERT(levi_morph_slots(&A, 0u) == 2u, "live slots %u", levi_morph_slots(&A, 0u));
    for (b = 0u; b < 2u; b++) {
        levi_init_set(&B);
        levi_set_algo(&B, 0u, b ? 40u : 3u);
        RI_ASSERT(levi_set_mpos(&A, 0u, b * 100u) == 0, "mpos");
        levi_trigger(&A, 0u, 64u);
        levi_trigger(&B, 0u, 64u);
        render(&A, oa, N);
        render(&B, ob, N);
        RI_ASSERT(!memcmp(oa, ob, sizeof oa), "morph end %u == pure preset", b);
    }
    levi_set_mpos(&A, 0u, 50u);
    levi_trigger(&A, 0u, 64u);
    render(&A, oa, N);
    RI_ASSERT(memcmp(oa, ob, sizeof oa) != 0 && peak(oa, N) < 64.0f, "midpoint blends");
    RI_ASSERT(levi_set_mpos(&A, 0u, 700u) == 0 && A.v[0].mpos == 100u, "position clamps to the list");
    /* SILENCE slot: the far end is silent. */
    levi_set_slot(&A, 0u, 1u, RI_LEVI_SLOT_SILENCE);
    RI_ASSERT(levi_morph_slots(&A, 0u) == 3u, "3 live slots");
    levi_set_mpos(&A, 0u, 100u);
    levi_trigger(&A, 0u, 64u);
    render(&A, oa, N);
    RI_ASSERT(peak(oa, N) == 0.0f, "silence slot silent %f", (double)peak(oa, N));
    /* The 7-bit key spans the live slots: 127 = the last. */
    RI_ASSERT(levi_set_param_ui(&A, 0u, RI_CTL_LEVI_MPOS & 0xFFu, 127u) == 0 && A.v[0].mpos == 200u,
        "key 127 = last slot, got %u", A.v[0].mpos);
    RI_ASSERT(levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SLOT0 & 0xFFu, RI_LEVI_SLOT_OFF) == 2,
        "slot 1 never off");
    RI_ASSERT(levi_set_slot(&A, 0u, 0u, RI_LEVI_SLOT_SILENCE) == 2 && levi_set_slot(&A, 0u, 8u, 1u) == 2 &&
        levi_set_slot(&A, 0u, 1u, 66u) == 2 && levi_set_amode(&A, 0u, 3u) == 2 &&
        levi_set_mpos(&A, 0u, 701u) == 2 && levi_set_slot(0, 0u, 1u, 1u) == 2, "fail closed");

    /* ---- Custom: starts from what sounds; targets; loops dropped. ---- */
    levi_init_set(&A);
    levi_init_set(&B);
    levi_set_algo(&A, 0u, 9u);
    levi_set_algo(&B, 0u, 9u);
    RI_ASSERT(levi_set_amode(&A, 0u, RI_LEVI_AMODE_CUSTOM) == 0 && levi_algo_get(&A, 0u) == (int)RI_LEVI_ALGO_CUSTOM,
        "custom mode");
    for (o = 0u; o < RI_LEVI_NOPS; o++)
        RI_ASSERT(A.v[0].cfeeds[o] == ri_levi_preset_feeds(9u, o), "custom copies op %u", o + 1u);
    levi_trigger(&A, 0u, 60u);
    levi_trigger(&B, 0u, 60u);
    render(&A, oa, N);
    render(&B, ob, N);
    RI_ASSERT(!memcmp(oa, ob, sizeof oa), "entering custom keeps the sound");
    levi_init_set(&A);
    levi_set_amode(&A, 0u, RI_LEVI_AMODE_CUSTOM);
    RI_ASSERT(levi_set_op_ui(&A, 0u, 2u, RI_LEVI_OP_TGT1, 1u) == 0 &&
        levi_set_op_ui(&A, 0u, 2u, RI_LEVI_OP_TGT2, 2u) == 0 && A.v[0].cfeeds[2] == 0x03u,
        "op3 -> op1 + op2: %02x", A.v[0].cfeeds[2]);
    RI_ASSERT(levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_TGT1, 3u) == 0 && A.v[0].cfeeds[0] == 0u,
        "loop target dropped");
    RI_ASSERT(levi_set_op_ui(&A, 0u, 3u, RI_LEVI_OP_TGT3, 4u) == 0 && A.v[0].cfeeds[3] == 0u,
        "self target dropped");
    RI_ASSERT(levi_set_feeds(&A, 0u, 0u, 0x04u) == 2 && levi_set_feeds(&A, 0u, 1u, 0x02u) == 2,
        "set_feeds refuses loops and self");

    /* ---- Mute drops an op; Solo auditions one op (a modulator too). ---- */
    levi_init_set(&A);                         /* DUO: op2 -> op1 */
    RI_ASSERT(levi_set_param_ui(&A, 0u, RI_CTL_LEVI_MUTELO & 0xFFu, 0x01u) == 0, "mute op1");
    levi_trigger(&A, 0u, 60u);
    render(&A, oa, N);
    RI_ASSERT(peak(oa, N) == 0.0f, "muted carrier silent");
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_MUTELO & 0xFFu, 0u);
    RI_ASSERT(levi_set_param_ui(&A, 0u, RI_CTL_LEVI_MUTEHI & 0xFFu, 1u) == 0 && A.v[0].mute == 0x80u, "mute op8");
    RI_ASSERT(levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SOLO & 0xFFu, 2u) == 0, "solo op2");
    levi_trigger(&A, 0u, 60u);
    render(&A, oa, N);
    levi_init_set(&B);
    levi_trigger(&B, 0u, 60u);
    render(&B, ob, N);
    RI_ASSERT(peak(oa, N) > 1e-4f && memcmp(oa, ob, sizeof oa) != 0, "solo hears the modulator");

    /* ---- Keys: the P3 section keys are automatable. ---- */
    for (k = RI_CTL_LEVI_AMODE; k <= RI_CTL_LEVI_MUTEHI; k++)
        RI_ASSERT(ri_auto_allowed((uint16_t)k), "key %04x allowed", k);

    /* ---- UI: five algorithm pages. ---- */
    ri_slevi_init(&u);
    RI_ASSERT(ri_slevi_set_value(&u, RI_SLEVI_MODULE, (int)RI_SLEVI_M_ALGO) == 1, "algo module");
    RI_ASSERT(ri_slevi_page_count(&u) == 5u, "5 pages");
    RI_ASSERT(!strcmp(ri_slevi_enc_name(&u, 0u), "MODE") && ri_slevi_ctl_idx(&u, RI_SLEVI_ENC0) == RI_SLEVI_AMODE,
        "page 1 mode");
    ri_slevi_enc_text(&u, 0u, t, sizeof t);
    RI_ASSERT(!strcmp(t, "SINGLE"), "mode text %s", t);
    RI_ASSERT(ri_slevi_set_value(&u, RI_SLEVI_ENC0, 127) == 1 && u.val[RI_SLEVI_AMODE] == 2, "mode custom");
    RI_ASSERT(ri_slevi_algo_display(&u) == 0, "custom readout C");
    u.val[RI_SLEVI_MUTELO] = 0x05;
    ri_slevi_enc_text(&u, 5u, t, sizeof t);
    RI_ASSERT(!strcmp(t, "1-3----"), "mute text %s", t);
    ri_slevi_press(&u, RI_SLEVI_PAGEDN);
    RI_ASSERT(u.page == 1u && ri_slevi_ctl_idx(&u, RI_SLEVI_ENC0 + 7u) == RI_SLEVI_SLOT0 + 7u, "page 2 slots");
    ri_slevi_enc_text(&u, 1u, t, sizeof t);
    RI_ASSERT(!strcmp(t, "OFF"), "slot 2 default off: %s", t);
    RI_ASSERT(ri_slevi_set_value(&u, RI_SLEVI_SLOT0, 12) == 1 && u.val[RI_SLEVI_ALGO] == 12 &&
        ri_slevi_set_value(&u, RI_SLEVI_ALGO, 30) == 1 && u.val[RI_SLEVI_SLOT0] == 30, "algo mirrors slot 1");
    ri_slevi_set_value(&u, RI_SLEVI_SLOT0 + 1u, (int)RI_LEVI_SLOT_SILENCE);
    ri_slevi_enc_text(&u, 1u, t, sizeof t);
    RI_ASSERT(!strcmp(t, "SILENCE"), "silence text %s", t);
    for (b = 0u; b < 3u; b++) {
        ri_slevi_press(&u, RI_SLEVI_PAGEDN);
        RI_ASSERT(u.page == 2u + b, "page %u", 3u + b);
        RI_ASSERT(ri_slevi_ctl_key(&u, RI_SLEVI_ENC0 + 5u, &key, &val) == 1 &&
            key == RI_LEVI_OPKEY(5u, RI_LEVI_OP_TGT1 + b), "grid column %u: osc 6 key %04x", b + 1u, key);
    }
    ri_slevi_set_value(&u, RI_SLEVI_ENC0 + 5u, 127);
    ri_slevi_enc_text(&u, 5u, t, sizeof t);
    RI_ASSERT(!strcmp(t, "OSC 8"), "target text %s", t);
    RI_ASSERT(ri_slevi_page_reaches(RI_SLEVI_SOLO) && ri_slevi_page_reaches(RI_SLEVI_SLOT0 + 7u) &&
        ri_slevi_legacy(RI_SLEVI_ALGOB) && ri_slevi_legacy(RI_SLEVI_MORPH), "reach / legacy");
    RI_RESULT("levi_algo_modes");
}
