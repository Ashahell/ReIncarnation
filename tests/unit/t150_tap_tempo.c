/* t150_tap_tempo — the transport TAP key (P9e). Tap tempo is a pure
 * millisecond estimator: it keeps up to four press intervals, averages
 * them and writes the tempo through the display's own 20..500 clamp. The
 * laws below are the arithmetic, read off the implementation:
 *   bpm = (60000 * n + sum/2) / sum      n = kept intervals, sum = their sum
 * No clock (ms == 0) is ignored rather than guessed, a pause longer than
 * 2 s starts a new measurement, and a tap never touches the transport. */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/secttr.h"
#include "gui/sectui.h"
#include "gui/ctlreg.h"
#include "engine/seq/autolane.h"

static void fresh(struct RISectTr *s) {
    RI_ASSERT(ri_str_init(s) == 0, "init");
}

int main(void) {
    struct RISectTr s;
    const struct RICtlDef *d;
    struct RITransport t0;
    uint64_t cur0, clicks0;

    /* --- the registry row: a plain button, no bind, no MIDI cc --------- */
    d = ri_ctlreg_find((uint16_t)((RI_SEC_TRANSPORT << 8) | RI_STR_TAP));
    RI_ASSERT(d != 0 && d->kind == RI_CK_BUTTON && !strcmp(d->legend, "Tap"),
        "transport TAP row is a BUTTON named Tap");
    RI_ASSERT(d->bind == RI_BIND_NONE && d->midi_cc == RI_MIDI_CC_NONE && d->engine_id == 0u,
        "TAP takes no engine binding and no MIDI cc");
    /* R4 added the clock-out lamp at index 15, so TAP is no longer the last
     * row. This assertion is the one that CAUGHT that change: a bare
     * constant bump would have left the registry and the panel disagreeing,
     * and nothing else in the suite would have said so. */
    RI_ASSERT(RI_STR_TAP == 14u && RI_STR_CLKOUT == 15u && RI_STR_NCTL == 16u,
        "TAP is 14, clock-out lamp is 15, NCTL 16");
    {
        uint32_t k, rows = 0u;
        for (k = 0u; k < ri_ctlreg_count(); k++) {
            const struct RICtlDef *dd = ri_ctlreg_at(k);
            if (dd && dd->section == RI_SEC_TRANSPORT) {
                rows++;
                if ((dd->reg_id & 0xFFu) == RI_STR_NCTL)
                    break;
            }
        }
        RI_ASSERT(k == ri_ctlreg_count() && rows == RI_STR_NCTL,
            "the transport has exactly NCTL rows and no index past it (%u)", rows);
    }

    /* --- a button, not a value: no press without a clock, no arrows ----- */
    fresh(&s);
    RI_ASSERT(ri_str_value(&s, RI_STR_TAP) == 0 && ri_str_led(&s, RI_STR_TAP, 0) == 0,
        "TAP reads 0 and its lamp is off (momentary)");
    RI_ASSERT(ri_str_press(&s, RI_STR_TAP) == 0, "a clockless press taps nothing");
    RI_ASSERT(ri_str_step(&s, RI_STR_TAP, 1) == 0 && ri_str_set_value(&s, RI_STR_TAP, 1) == 0 &&
        ri_str_reset(&s, RI_STR_TAP) == 0, "TAP has no value to step or set");

    /* --- one tap changes nothing -------------------------------------- */
    RI_ASSERT(ri_str_tap(&s, 100u) == 0 && ri_str_value(&s, RI_STR_TEMPO) == 120,
        "the first tap only starts the measurement");

    /* --- the estimator: average the intervals, then slide a 4-wide window */
    fresh(&s);
    RI_ASSERT(ri_str_tap(&s, 100u) == 0, "first tap");
    RI_ASSERT(ri_str_tap(&s, 1100u) == 1 && ri_str_value(&s, RI_STR_TEMPO) == 60,
        "1000 ms apart is 60 bpm");
    RI_ASSERT(ri_str_tap(&s, 1300u) == 1 && ri_str_value(&s, RI_STR_TEMPO) == 100,
        "two intervals average: 60000*2/1200 = 100");
    RI_ASSERT(ri_str_tap(&s, 1500u) == 1 && ri_str_value(&s, RI_STR_TEMPO) == 129,
        "three intervals: 60000*3/1400 rounds to 129");
    RI_ASSERT(ri_str_tap(&s, 1700u) == 1 && ri_str_value(&s, RI_STR_TEMPO) == 150,
        "four intervals: 60000*4/1600 rounds to 150");
    RI_ASSERT(ri_str_tap(&s, 1900u) == 1 && ri_str_value(&s, RI_STR_TEMPO) == 300,
        "the fifth interval drops the oldest: 60000*4/800 = 300");

    /* --- the 2 s pause boundary is part of the law --------------------- */
    fresh(&s);
    ri_str_tap(&s, 100u);
    RI_ASSERT(ri_str_tap(&s, 1100u) == 1 && ri_str_value(&s, RI_STR_TEMPO) == 60, "60 bpm");
    RI_ASSERT(ri_str_tap(&s, 3100u) == 1 && ri_str_value(&s, RI_STR_TEMPO) == 40,
        "a gap of exactly 2000 ms is kept: 60000*2/3000 = 40");
    fresh(&s);
    ri_str_tap(&s, 100u);
    ri_str_tap(&s, 1100u);
    RI_ASSERT(ri_str_tap(&s, 3102u) == 0 && ri_str_value(&s, RI_STR_TEMPO) == 60,
        "a gap of 2002 ms restarts and changes nothing");
    RI_ASSERT(ri_str_tap(&s, 3602u) == 1 && ri_str_value(&s, RI_STR_TEMPO) == 120,
        "after the restart the old interval is gone: 60000*1/500 = 120");

    /* --- no clock is ignored, and a clock that stops or goes backwards ---- */
    fresh(&s);
    RI_ASSERT(ri_str_tap(&s, 0u) == 0 && ri_str_tap(&s, 100u) == 0, "ms 0 is not a tap");
    RI_ASSERT(ri_str_tap(&s, 0u) == 0 && ri_str_tap(&s, 1100u) == 1 &&
        ri_str_value(&s, RI_STR_TEMPO) == 60, "the ignored tap never became the reference");
    RI_ASSERT(ri_str_tap(&s, 1100u) == 0 && ri_str_value(&s, RI_STR_TEMPO) == 60,
        "two taps in the same millisecond restart rather than measure a 0 ms interval");
    RI_ASSERT(ri_str_tap(&s, 1600u) == 1 && ri_str_value(&s, RI_STR_TEMPO) == 120,
        "and measure from the repeated stamp: 60000*1/500 = 120");
    fresh(&s);
    ri_str_tap(&s, 100u);
    ri_str_tap(&s, 1100u);
    RI_ASSERT(ri_str_tap(&s, 900u) == 0 && ri_str_value(&s, RI_STR_TEMPO) == 60,
        "a timestamp before the last tap restarts, it does not underflow");
    RI_ASSERT(ri_str_tap(&s, 1400u) == 1 && ri_str_value(&s, RI_STR_TEMPO) == 120,
        "the next tap measures from the restarted reference");

    /* --- the tempo clamp is the display's, and it latches -------------- */
    fresh(&s);
    ri_str_tap(&s, 1000u);
    RI_ASSERT(ri_str_tap(&s, 1001u) == 1 && ri_str_value(&s, RI_STR_TEMPO) == 500,
        "a 1 ms interval asks for 60000 and is clamped to 500");
    RI_ASSERT(ri_str_tap(&s, 1002u) == 0, "a second 1 ms tap changes nothing");
    fresh(&s);
    ri_str_tap(&s, 1000u);
    RI_ASSERT(ri_str_tap(&s, 3000u) == 1 && ri_str_value(&s, RI_STR_TEMPO) == 30,
        "the slowest reachable tempo is 30 bpm (four 2000 ms intervals)");
    RI_ASSERT(ri_str_tap(&s, 5000u) == 0 && ri_str_tap(&s, 7000u) == 0 &&
        ri_str_tap(&s, 9000u) == 0, "and it latches");

    /* --- a tap is a tempo edit and nothing else ----------------------- */
    fresh(&s);
    ri_str_press(&s, RI_STR_MODE);            /* song mode: taps still work */
    t0 = s.tr;
    cur0 = s.cursor;
    clicks0 = s.tr.clicks;
    ri_str_tap(&s, 100u);
    ri_str_tap(&s, 1100u);
    ri_str_tap(&s, 1300u);
    RI_ASSERT(s.tr.state == t0.state && s.tr.clicks == clicks0 && s.cursor == cur0,
        "three taps left the transport, the cursor and the click count alone");
    RI_ASSERT(ri_str_value(&s, RI_STR_TEMPO) == 100, "and the tempo moved");

    /* --- a manually set tempo is not an input: the next tap re-derives it --- */
    ri_str_set_value(&s, RI_STR_TEMPO, 137);
    RI_ASSERT(ri_str_tap(&s, 2300u) == 1 && ri_str_value(&s, RI_STR_TEMPO) == 82,
        "the window is the taps only (1000 + 200 + 1000 ms = 82 bpm), not the 137 on the display");

    /* --- the panel routes a clocked press to the key and only that key - */
    {
        struct RISectUI trui;
        RI_ASSERT(ri_sui_init(&trui, RI_SEC_TRANSPORT) == 0, "transport ui");
        ri_sui_tap(&trui, RI_STR_TAP, 100u);
        RI_ASSERT(ri_sui_tap(&trui, RI_STR_TAP, 1100u) == 1 &&
            ri_sui_value(&trui, RI_STR_TEMPO) == 60, "the routed tap moves the tempo");
        RI_ASSERT(ri_sui_tap(&trui, RI_STR_PLAY, 1200u) == 0 &&
            ri_sui_value(&trui, RI_STR_TEMPO) == 60,
            "a clocked press on another key does not tap");
        RI_ASSERT(ri_sui_tap(0, RI_STR_TAP, 1200u) == 0, "null-safe routing");
    }

    /* --- the key space is unchanged: 0x0ECC is still the first refusal -- */
    RI_ASSERT(!ri_auto_allowed(0x0ED1u), "0x0ED1 refused (past the live keys)");
    RI_ASSERT(ri_str_tap(0, 1100u) == 0, "null-safe");

    RI_RESULT("tap_tempo");
}