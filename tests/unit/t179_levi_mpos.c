/* t179_levi_mpos — W3: Morph Pos survives slot and mode changes (bug 2).
 * levi_set_param_ui MPOS converted the 7-bit knob with the slot count at
 * the moment of the turn and stored only the internal mpos: setting MPOS
 * before filling slots (or changing slots/mode after) left the sound at
 * the wrong position, so automation/song replay order decided the sound.
 * The engine keeps the 7-bit knob per voice and re-derives mpos whenever
 * the slot list or mode changes. Each case below asserts the voice's
 * effective position equals what the same knob value gives when set last.
 * RED on HEAD: knob-first orders strand mpos at 0 (or the old count).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"

#define MPOS (RI_CTL_LEVI_MPOS & 0xFFu)
#define AMODE (RI_CTL_LEVI_AMODE & 0xFFu)
#define SLOT(i) ((uint32_t)((RI_CTL_LEVI_SLOT0 & 0xFFu) + (i)))

static uint32_t mpos_of(const struct RILeviSet *s) {
    return s->v[0].mpos;
}

static void ui(struct RILeviSet *s, uint32_t id, uint8_t val, const char *what) {
    RI_ASSERT(levi_set_param_ui(s, 0u, id, val) == 0, "%s", what);
}

int main(void) {
    static struct RILeviSet a, b;
    /* (i) MPOS set before the slots are filled. */
    levi_init_set(&a);
    levi_init_set(&b);
    ui(&a, AMODE, RI_LEVI_AMODE_MORPH, "a morph mode");
    ui(&a, MPOS, 127u, "a mpos first");
    ui(&a, SLOT(1), RI_LEVI_ALGO_DUO, "a slot 1");
    ui(&b, AMODE, RI_LEVI_AMODE_MORPH, "b morph mode");
    ui(&b, SLOT(1), RI_LEVI_ALGO_DUO, "b slot 1");
    ui(&b, MPOS, 127u, "b mpos last");
    RI_ASSERT(mpos_of(&b) == 100u, "knob 127 over 2 slots is 100, got %u", mpos_of(&b));
    RI_ASSERT(mpos_of(&a) == mpos_of(&b), "mpos-first == mpos-last (%u vs %u)",
        mpos_of(&a), mpos_of(&b));
    /* (ii) MPOS set, then a slot added, then removed. */
    levi_init_set(&a);
    levi_init_set(&b);
    ui(&a, AMODE, RI_LEVI_AMODE_MORPH, "a morph mode");
    ui(&a, SLOT(1), RI_LEVI_ALGO_DUO, "a slot 1");
    ui(&a, MPOS, 64u, "a mpos");
    ui(&a, SLOT(2), RI_LEVI_ALGO_DUO, "a slot 2 added");
    ui(&b, AMODE, RI_LEVI_AMODE_MORPH, "b morph mode");
    ui(&b, SLOT(1), RI_LEVI_ALGO_DUO, "b slot 1");
    ui(&b, SLOT(2), RI_LEVI_ALGO_DUO, "b slot 2");
    ui(&b, MPOS, 64u, "b mpos last");
    RI_ASSERT(mpos_of(&b) == 101u, "knob 64 over 3 slots is 101, got %u", mpos_of(&b));
    RI_ASSERT(mpos_of(&a) == mpos_of(&b), "add-slot keeps the knob (%u vs %u)",
        mpos_of(&a), mpos_of(&b));
    ui(&a, SLOT(2), RI_LEVI_SLOT_OFF, "a slot 2 removed");
    ui(&b, SLOT(2), RI_LEVI_SLOT_OFF, "b slot 2 removed");
    ui(&b, MPOS, 64u, "b mpos re-set");
    RI_ASSERT(mpos_of(&b) == 50u, "knob 64 over 2 slots is 50, got %u", mpos_of(&b));
    RI_ASSERT(mpos_of(&a) == mpos_of(&b), "remove-slot keeps the knob (%u vs %u)",
        mpos_of(&a), mpos_of(&b));
    /* (iii) MPOS set, then AMODE changed to MORPH. */
    levi_init_set(&a);
    levi_init_set(&b);
    ui(&a, MPOS, 127u, "a mpos in single");
    ui(&a, SLOT(1), RI_LEVI_ALGO_DUO, "a slot 1");
    ui(&a, AMODE, RI_LEVI_AMODE_MORPH, "a to morph");
    ui(&b, AMODE, RI_LEVI_AMODE_MORPH, "b morph mode");
    ui(&b, SLOT(1), RI_LEVI_ALGO_DUO, "b slot 1");
    ui(&b, MPOS, 127u, "b mpos last");
    RI_ASSERT(mpos_of(&a) == mpos_of(&b), "mode change keeps the knob (%u vs %u)",
        mpos_of(&a), mpos_of(&b));
    RI_ASSERT(mpos_of(&a) == 100u, "knob 127 lands at 100, got %u", mpos_of(&a));
    /* (iv) an internal slide does not move the knob: leaving MORPH and
     * coming back lands on the panel knob, not the slid position. */
    levi_init_set(&a);
    ui(&a, AMODE, RI_LEVI_AMODE_MORPH, "a morph mode");
    ui(&a, SLOT(1), RI_LEVI_ALGO_DUO, "a slot 1");
    ui(&a, MPOS, 127u, "a mpos");
    RI_ASSERT(levi_set_mpos(&a, 0u, 30u) == 0, "internal slide");
    RI_ASSERT(mpos_of(&a) == 30u, "slide lands, got %u", mpos_of(&a));
    ui(&a, AMODE, RI_LEVI_AMODE_SINGLE, "a to single");
    ui(&a, AMODE, RI_LEVI_AMODE_MORPH, "a back to morph");
    RI_ASSERT(mpos_of(&a) == 100u, "re-entry lands on the knob, got %u", mpos_of(&a));
    /* Fail-closed edges of the knob path. */
    RI_ASSERT(levi_set_param_ui(&a, 0u, MPOS, 200u) == 0, "knob clamps, not rejects");
    RI_ASSERT(mpos_of(&a) == 100u, "knob >127 clamps to full, got %u", mpos_of(&a));
    RI_RESULT("levimpos");
}
