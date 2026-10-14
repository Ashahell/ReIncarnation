/* t223_fx_lamp_repaint — the displaced FX lamp goes dark in the model, but
 * nobody ever repaints it.
 *
 * Owner ear proof, Dell lane, build e846eff, 2026-10-14:
 *
 *   "If Delay is turned on, and you then turn on Comp, comp makes an audible
 *    difference, but the green square on Delay remains lit."
 *
 * The model is provably right. t222 passes and the lamp draws from the
 * rack-derived selection:
 *
 *   int on = ri_sui_led(ui, idx, 0);        art_section.c:206
 *
 * so Delay's lamp reads dark the instant Comp claims the rack. The defect is
 * purely that those pixels are never redrawn. Pressing Comp dirties the COMP
 * canvas, so only COMP repaints; the FX meter loop refreshes the METER box of
 * each FX canvas and only when its level changed, and nothing anywhere
 * repaints the BYPASS LAMP of a canvas that was displaced as a side effect
 * of another canvas's press.
 *
 * So the fix has to be told WHERE to repaint, and that instruction has to be
 * portable to be testable: app/riapp.c is an AROS shell that cannot link on
 * the host. The rack therefore records who it displaced, and the shell reads
 * that and repaints exactly that canvas's lamp box.
 *
 * This is the same coupling noted in the P-FX3 ledger entry -- sync_values
 * pushes and repaints only the LAST-HIT canvas -- stated here as the bug it
 * actually causes.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/sectfx.h"
#include "gui/sectui.h"

/* Every transition the shell can be handed, and who must be repainted.
 * Takes RI_SFX_LIVE_NONE to release whatever is live. */
static void claim(struct RIFxRack *r, uint32_t slot) {
    r->displaced = (uint8_t)ri_sfx_rack_take(r, slot);
}

int main(void) {
    struct RIFxRack rack;
    struct RISectUI pcf, dly, dst, cmp;
    uint32_t PCF = RI_SEC_PCF - RI_SEC_PCF;
    uint32_t DLY = RI_SEC_DELAY - RI_SEC_PCF;
    uint32_t DST = RI_SEC_DIST - RI_SEC_PCF;
    uint32_t CMP = RI_SEC_COMP - RI_SEC_PCF;

    /* Slots are canvas order, and must be the four distinct values the
     * shell indexes its canvas table with. */
    RI_ASSERT(PCF == 0u && DLY == 1u && DST == 2u && CMP == 3u,
        "canvas slots are the RI_SEC_* offsets (%u %u %u %u)", PCF, DLY, DST, CMP);

    ri_sfx_rack_init(&rack);
    RI_ASSERT(rack.live == RI_SFX_LIVE_NONE && rack.displaced == RI_SFX_LIVE_NONE,
        "nothing live and nothing owed a repaint at rest");

    /* First claim: nothing to displace, so nothing to repaint. This matters
     * -- if "displaced" defaulted to the claiming slot the shell would
     * repaint the canvas the user just pressed, which is already repainting,
     * and would leave the real victim unpainted. */
    claim(&rack, PCF);
    RI_ASSERT(rack.live == PCF && rack.displaced == RI_SFX_LIVE_NONE,
        "first claim displaces nobody");

    /* The reported case: Delay live, Comp claims it. */
    claim(&rack, DLY);
    RI_ASSERT(rack.live == DLY && rack.displaced == PCF, "DLY displaces PCF");
    claim(&rack, CMP);
    RI_ASSERT(rack.live == CMP && rack.displaced == DLY,
        "COMP displaces DLY -- the owner's green square");

    /* Claiming the SAME slot again displaces nobody: it is already the live
     * one, so the shell must not be told to repaint it. */
    claim(&rack, CMP);
    RI_ASSERT(rack.displaced == RI_SFX_LIVE_NONE,
        "re-claiming the live slot displaces nobody");

    /* OFF is asymmetric with ON (t222) and so is the bookkeeping: clearing
     * the live slot repaints nobody, because no OTHER canvas changed. */
    /* Added AFTER implementation, so these two were not watched RED. The
     * mutation harness (m3, m5) is the compensating evidence that they have
     * teeth -- do not treat them as if they had been earned the usual way. */
    RI_ASSERT(ri_sfx_rack_take(&rack, RI_SFX_LIVE_NONE) == RI_SFX_LIVE_NONE,
        "clearing the rack displaces nobody");
    RI_ASSERT(rack.live == RI_SFX_LIVE_NONE, "rack cleared");
    claim(&rack, DST);
    RI_ASSERT(rack.displaced == RI_SFX_LIVE_NONE, "claim after clear displaces nobody");

    /* The shell indexes its canvas table with whatever this returns, so an
     * out-of-range slot must be refused with the state left alone rather
     * than repainting some unrelated canvas. */
    {
        uint8_t before = rack.live;
        RI_ASSERT(ri_sfx_rack_take(&rack, RI_SFX_RACK_SLOTS) == RI_SFX_LIVE_NONE,
            "slot past the last canvas is refused");
        RI_ASSERT(rack.live == before, "a refused claim changes nothing");
        RI_ASSERT(ri_sfx_rack_take(0, 0u) == RI_SFX_LIVE_NONE, "null rack");
    }

    /* A displaced slot must always be a real canvas, never a stale or
     * out-of-range index: the shell uses it to index its canvas table. */
    claim(&rack, PCF);
    RI_ASSERT(rack.displaced == DST && rack.displaced < 4u,
        "displaced is an in-range canvas index (%u)", rack.displaced);

    /* End-to-end through the real panel API, so this is not a test of a
     * bookkeeping struct that the press path does not actually use. */
    ri_sfx_rack_init(&rack);
    ri_sui_init(&pcf, RI_SEC_PCF);
    ri_sui_init(&dly, RI_SEC_DELAY);
    ri_sui_init(&dst, RI_SEC_DIST);
    ri_sui_init(&cmp, RI_SEC_COMP);
    ri_sui_bind_fx_rack(&pcf, &rack);
    ri_sui_bind_fx_rack(&dly, &rack);
    ri_sui_bind_fx_rack(&dst, &rack);
    ri_sui_bind_fx_rack(&cmp, &rack);
    ri_sui_press(&dly, RI_SFX_ONOFF);
    RI_ASSERT(rack.live == DLY && rack.displaced == RI_SFX_LIVE_NONE, "Delay on");
    ri_sui_press(&cmp, RI_SFX_ONOFF);
    RI_ASSERT(rack.live == CMP && rack.displaced == DLY,
        "pressing Comp through the panel records Delay as displaced");
    RI_ASSERT(ri_sui_led(&dly, RI_SFX_ONOFF, 0) == 0,
        "and Delay's lamp really is dark in the model");
    ri_sui_press(&dly, RI_SFX_ONOFF);
    RI_ASSERT(rack.live == DLY && rack.displaced == CMP,
        "pressing Delay back displaces Comp");

    /* The repaint TARGET, which is what the shell actually consumes. This was
     * in app/riapp.c first and could not be tested at all: mutate_fx_lamp m6
     * ("the shell repaints the live canvas instead of the displaced one")
     * survived, because riapp.c is an AROS shell that never links on the host.
     * Extracted here, the same mistake is now a one-token mutation. */
    ri_sfx_rack_init(&rack);
    ri_sui_press(&dly, RI_SFX_ONOFF);
    RI_ASSERT(ri_sfx_rack_stale_section(&rack) == RI_SFX_LIVE_NONE,
        "nothing is owed after a first claim");
    ri_sui_press(&cmp, RI_SFX_ONOFF);
    RI_ASSERT(ri_sfx_rack_stale_section(&rack) == RI_SEC_DELAY,
        "the STALE one is Delay -- not Comp, which is being repainted anyway");
    RI_ASSERT(ri_sfx_rack_stale_section(&rack) == RI_SFX_LIVE_NONE,
        "reading clears it, so the repaint happens once not every frame");
    /* Corruption must not become a canvas index: the shell adds an offset to
     * whatever comes back. */
    rack.displaced = RI_SFX_RACK_SLOTS;
    RI_ASSERT(ri_sfx_rack_stale_section(&rack) == RI_SFX_LIVE_NONE,
        "an out-of-range displaced slot is refused");
    RI_ASSERT(ri_sfx_rack_stale_section(0) == RI_SFX_LIVE_NONE, "null rack");

    RI_RESULT("fx_lamp_repaint");
    return 0;
}
