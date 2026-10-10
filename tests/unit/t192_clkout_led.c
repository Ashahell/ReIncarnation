/* t192_clkout_led — the clock-out LED (R4: "add a clock-out LED").
 *
 * THE LAW IS "SENDING", NOT "ENABLED". A lamp that lights whenever the
 * feature is switched on is a lie in the one case that matters: the
 * feature is ON and nothing is coming out. That is precisely the state a
 * clock-out failure produces -- RIAPP_MIDI_CLKOUT=1, the sender task
 * refuses to start, and `midi_out_enable(o, 0)` fails it closed (see
 * app/riapp.c). A lamp that showed "enabled" would read green through
 * exactly the failure it exists to reveal.
 *
 * So the LED follows BYTES: it lights when ticks have actually left the
 * producer since the last clock, and it goes out when they stop, however
 * long that takes. A timeout is a lie in the other direction -- a lamp
 * that stays lit for 300 ms after the last tick is showing the timeout,
 * not the wire.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "gui/secttr.h"

int main(void) {
    static struct RISectTr s;

    /* --- off until told otherwise ------------------------------------- */
    ri_str_init(&s);
    RI_ASSERT(ri_str_value(&s, RI_STR_CLKOUT) == 0, "clock-out LED starts off");

    /* --- idle: configured but nothing sent is NOT lit ---------------- */
    ri_str_clkout_set(&s, 1, 0u);      /* enabled, zero ticks so far */
    RI_ASSERT(ri_str_value(&s, RI_STR_CLKOUT) == 0,
        "enabled with nothing sent is still dark -- this is the failure case");
    ri_str_clkout_set(&s, 1, 100u);
    RI_ASSERT(ri_str_value(&s, RI_STR_CLKOUT) == 1, "bytes sent lights it");

    /* --- stopping the clock puts it out, with no timeout to wait out -- */
    ri_str_clkout_set(&s, 0, 0u);
    RI_ASSERT(ri_str_value(&s, RI_STR_CLKOUT) == 0,
        "no bytes since last poll is dark immediately, not after a timeout");
    ri_str_clkout_set(&s, 0, 42u);
    RI_ASSERT(ri_str_value(&s, RI_STR_CLKOUT) == 0,
        "and it stays dark even if the count is non-zero but not sending");

    /* --- the value the UI reads is 0/1, never a raw byte count -------- */
    ri_str_clkout_set(&s, 1, 65535u);
    RI_ASSERT(ri_str_value(&s, RI_STR_CLKOUT) == 1,
        "a large count reads as lit, not as a number");
    ri_str_clkout_set(&s, 1, 1u);
    RI_ASSERT(ri_str_value(&s, RI_STR_CLKOUT) == 1, "one byte is enough");

    /* --- it does not disturb the MIDI or Sync lamps ------------------- */
    ri_str_init(&s);
    ri_str_indicator_set(&s, RI_STR_MIDI, 1);
    ri_str_clkout_set(&s, 1, 5u);
    RI_ASSERT(ri_str_value(&s, RI_STR_MIDI) == 1, "MIDI lamp unaffected");
    RI_ASSERT(ri_str_value(&s, RI_STR_SYNC) == 0, "Sync lamp unaffected");

    /* --- the generic setter must NOT be able to drive it: the LED is
     *     derived from bytes, and a caller setting it by hand would be
     *     asserting something the code never observed. -------------- */
    ri_str_init(&s);
    ri_str_indicator_set(&s, RI_STR_CLKOUT, 1);
    RI_ASSERT(ri_str_value(&s, RI_STR_CLKOUT) == 0,
        "the generic setter cannot fake a lit clock-out lamp");

    /* --- nulls are inert ------------------------------------------------ */
    ri_str_clkout_set(0, 1, 5u);
    ri_str_init(0);

    RI_RESULT("clkout-led");
}