/* t230_mute_automatable — SETTLE the mixer mute vs manual p. 56.
 *
 * The decision, stated once so it is recorded rather than incidental:
 *
 *   KEEP the mixer On/Off Song-automatable (`automatable = 1`), which
 *   REVERSES the parity article's manual-sourced line "Mixer on/off is the
 *   mute and is not Song-automated (p. 56)".
 *
 * WHY, in the order the evidence actually arrived:
 *
 *  1. **It is inert today.** Every shipped song is a text scaffold and NONE
 *     automates a mixer control -- Level, Pan, Send or the mute. So granting
 *     the capability changes nothing about existing content. This is the
 *     fact that decided it, and the census below is the tripwire that keeps
 *     it true: the moment a song automates a mixer control, the deviation
 *     stops being free and this test fails and asks for the call again.
 *  2. **Reverting would create a NEW asymmetry.** Level, Pan and Send on the
 *     same strip are automatable. A functional mute that cannot be shown
 *     when a song moves it is a worse defect than the fidelity deviation.
 *  3. **The manual cannot be checked here.** No manual in the repository, and
 *     web search is unavailable. The p. 56 claim reaches us second-hand
 *     through the parity article, so it is a weaker input than the three
 *     measured facts above -- not disregarded, outranked.
 *
 * WHAT THIS DOES NOT CLAIM. It does not claim the manual says otherwise; it
 * records that we could not read it and chose on evidence. One line
 * (`automatable = 1` -> `0`, plus the allow-list keys and the t60 pin)
 * reverses the whole decision, and that is deliberate: a fidelity call should
 * be cheap to undo, not expensive.
 *
 * THIS TEST WAS NEVER RED, and saying so is part of its contract. It is a PIN
 * over a decision that P-MIX1 had already implemented, so it was green the
 * moment it was written. A test that was never red proves nothing by itself,
 * which is why `tests/mutate_mute_automatable.py` exists: it breaks each claim
 * this pin makes -- the flag, the allow-list half, and the tripwire's ability
 * to fire -- and requires the gate to notice. 3/3.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/sectmix.h"
#include "engine/seq/autolane.h"

static const char *SECT[5] = { "SYNTH1", "SYNTH2", "808", "909", "LEVI" };

int main(void) {
    uint32_t s;

    /* ---- the decision itself, pinned on all five strips ---- */
    for (s = 0; s < 5u; s++) {
        uint32_t sec = (s == 0u ? RI_SEC_MIX_SYNTH1 : s == 1u ? RI_SEC_MIX_SYNTH2
            : s == 2u ? RI_SEC_MIX_808 : s == 3u ? RI_SEC_MIX_909 : RI_SEC_MIX_LEVI);
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((sec << 8) | 0u));
        uint32_t strip = s == 4u ? RI_AUTO_STRIP_LEVI : s;
        uint16_t key = (uint16_t)RI_AUTO_ID_MIX(strip, RI_AUTO_MIX_MUTE);
        RI_ASSERT(d != 0, "%s On/Off exists", SECT[s]);
        RI_ASSERT(d && d->kind == RI_CK_SWITCH, "%s On/Off is a switch", SECT[s]);
        RI_ASSERT(d && d->bind == RI_BIND_MUTE, "%s On/Off is bound", SECT[s]);
        RI_ASSERT(d && d->automatable == 1u,
            "%s On/Off is Song-automatable -- SETTLED 2026-10-15, reversing "
            "the p. 56 line in the parity article (see the test header)", SECT[s]);
        /* Both halves of the capability: recording is gated by the allow
         * list, panel readback by `automatable`. Granting one without the
         * other is how a control ends up half-automatable. */
        RI_ASSERT(ri_auto_allowed(key) == 1,
            "%s mute key %04x is recordable", SECT[s], key);
        RI_ASSERT(d && ri_ctlreg_auto_id(d) == key,
            "%s mute key is %04x", SECT[s], key);
    }

    /* ---- the tripwire ----
     * The decision rests on no shipped song automating a mixer control. The
     * goldens are TEXT scaffolds, so a plain substring scan is honest here;
     * if the real RBNG codec lands this test must be revisited, because the
     * corpus stops being greppable and, more importantly, starts containing
     * keys this decision never considered. */
    {
        static const char *CORPUS[] = {
            "tests/golden/songs/corpus/s01.rbng", "tests/golden/songs/corpus/s02.rbng",
            "tests/golden/songs/corpus/s03.rbng", "tests/golden/songs/corpus/s04.rbng",
            "tests/golden/songs/corpus/s05.rbng", "tests/golden/songs/corpus/s06.rbng",
            "tests/golden/songs/corpus/s07.rbng", "tests/golden/songs/corpus/s08.rbng",
            "tests/golden/songs/corpus/s09.rbng", "tests/golden/songs/corpus/s10.rbng",
            "tests/golden/songs/corpus/s11-bank.rbng", "tests/golden/songs/corpus/s12-bank.rbng",
            "tests/golden/songs/corpus/zombie-nation.rbng", "tests/golden/303/first-light.rbng",
        };
        int checked = 0, with_mix = 0, with_mute = 0;
        for (s = 0; s < sizeof CORPUS / sizeof CORPUS[0]; s++) {
            FILE *f = fopen(CORPUS[s], "rb");
            char buf[65536];
            size_t n;
            if (!f)
                continue;          /* corpus not built here; census is advisory */
            checked++;
            n = fread(buf, 1u, sizeof buf - 1u, f);
            fclose(f);
            buf[n] = '\0';
            /* Any MIX-block key at all (0x0Bxx) means the corpus has started
             * automating the mixer, which is the moment this decision stops
             * being free. */
            if (strstr(buf, "0x0B") || strstr(buf, "0x0b"))
                with_mix++;
            if (strstr(buf, "MIX(") || strstr(buf, "mix="))
                with_mute++;
        }
        RI_ASSERT(checked >= 10, "corpus readable (%d of %u files)", checked,
            (int)(sizeof CORPUS / sizeof CORPUS[0]));
        RI_ASSERT(with_mix == 0 && with_mute == 0,
            "TRIPWIRE: %d song(s) now automate a mixer control and %d mention a "
            "mute. The p. 56 deviation is NO LONGER FREE -- revisit the call.",
            with_mix, with_mute);
    }

    RI_RESULT("mute_automatable");
    return 0;
}
