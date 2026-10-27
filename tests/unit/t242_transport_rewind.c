/* t242_transport_rewind — a stop press reaches the engine even when the
 * transport is already stopped, and a held stop key is ONE press.
 *
 * Why this exists (P-TR2). On 2026-10-26 the Dell's song could be played
 * exactly once per launch: after it ended, every Play was stopped again a few
 * buffers later. `song_poll_end` stops when the published cursor is past
 * `song_bars + 1`, and the cursor never came back, because `sync_transport()`
 * acted only on a *state* change — and the documented rewind (`ri_tr_stop`'s
 * clicks law: stop holds the cursor, stop again goes to the loop start, a
 * third to the song start) changes no state at all when the transport is
 * already stopped. The panel's own cursor moved; the engine's did not.
 *
 * The pair half of the 21-hour strobe (docs/evidence/midi/transport-storm/)
 * is this defect: each key-down started playback and the end guard stopped it
 * again, which is why one press wrote TWO log lines.
 *
 * Two halves, both pinned here:
 *   1. ri_core_transport_sync — the propagation decision, which must look at
 *      the panel's stop-click count as well as its state (host-built, in
 *      app/core, so the rule is testable without an AROS app);
 *   2. the front panel — a held STOP must rewind ONCE, not alternate the
 *      cursor between the loop start and the song start at the key repeat
 *      rate (the same press-edge rule t241 pins for the toggle).
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "app/core/riapp_core.h"
#include "gui/panelui.h"
#include "gui/keymap.h"
#include "gui/secttr.h"
#include "engine/seq/transport.h"

int main(void) {
    static struct RISectUI tr;
    struct RIPanelUI pu;
    uint64_t loop_start, end;
    int last_state;
    uint32_t last_clicks;

    /* ---- 1. the propagation decision ---------------------------------- */

    /* A stopped transport whose click count has NOT moved is not an event:
     * sync_transport() runs every round and must stay quiet. */
    last_state = RI_TR_STOPPED;
    last_clicks = 0u;
    RI_ASSERT(ri_core_transport_sync(RI_TR_STOPPED, 0u, &last_state, &last_clicks)
        == RI_TR_SYNC_NONE, "idle stopped: nothing to do");

    /* THE DEFECT: a stop press while already stopped changes no state, but it
     * is the rewind. Dropping it parks the engine's cursor at the song's end
     * and every later Play is stopped by song_poll_end. */
    last_state = RI_TR_STOPPED;
    last_clicks = 0u;
    RI_ASSERT(ri_core_transport_sync(RI_TR_STOPPED, 1u, &last_state, &last_clicks)
        == RI_TR_SYNC_STOP, "rewind press reaches the engine");
    RI_ASSERT(last_clicks == 1u, "and the acted-on count is recorded");

    /* The third press (clicks wrap to 0) is the song start, and it is also a
     * stop request. */
    RI_ASSERT(ri_core_transport_sync(RI_TR_STOPPED, 0u, &last_state, &last_clicks)
        == RI_TR_SYNC_STOP, "third press (song start) reaches the engine");
    RI_ASSERT(last_clicks == 0u, "and is recorded");

    /* A state change still acts, and PLAY/RECORD are the playing side. */
    last_state = RI_TR_STOPPED;
    last_clicks = 0u;
    RI_ASSERT(ri_core_transport_sync(RI_TR_PLAYING, 0u, &last_state, &last_clicks)
        == RI_TR_SYNC_PLAY, "play reaches the engine");
    last_state = RI_TR_PLAYING;
    RI_ASSERT(ri_core_transport_sync(RI_TR_RECORD, 0u, &last_state, &last_clicks)
        == RI_TR_SYNC_PLAY, "record is the playing side");
    RI_ASSERT(ri_core_transport_sync(RI_TR_STOPPED, 0u, &last_state, &last_clicks)
        == RI_TR_SYNC_STOP, "stop from playing reaches the engine");
    RI_ASSERT(ri_core_transport_sync(RI_TR_STOPPED, 0u, 0, &last_clicks)
        == RI_TR_SYNC_NONE, "null state is not a crash");
    RI_ASSERT(ri_core_transport_sync(RI_TR_STOPPED, 0u, &last_state, 0)
        == RI_TR_SYNC_NONE, "null clicks is not a crash");

    /* ---- 2. the front panel: a held STOP is one press ------------------ */

    ri_sui_init(&tr, RI_SEC_TRANSPORT);
    ri_panel_init(&pu);
    pu.tr = &tr;
    tr.u.tr.loop.start_bar = 4u;                     /* a real loop start */
    loop_start = ri_seq_tick_of_bar(tr.u.tr.ppq, 4u);
    end = ri_seq_tick_of_bar(tr.u.tr.ppq, 100u);     /* parked at the song end */
    tr.u.tr.tr.state = RI_TR_STOPPED;
    tr.u.tr.tr.clicks = 0u;
    tr.u.tr.cursor = end;

    /* One press: the first stop while stopped goes to the loop start. */
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KP0, 0) == 1, "stop press acts");
    RI_ASSERT(tr.u.tr.cursor == loop_start, "first stop -> loop start");
    RI_ASSERT(tr.u.tr.tr.clicks == 1u, "clicks 0 -> 1");

    /* The repeats that follow a held key are not presses. Before the guard
     * each repeat stepped the cursor again (loop start, song start, ...). */
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KP0, 0) == 0, "repeat ignored");
    RI_ASSERT(tr.u.tr.cursor == loop_start, "repeat leaves the cursor");
    RI_ASSERT(tr.u.tr.tr.clicks == 1u, "repeat leaves the count");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KP0, 0) == 0, "second repeat ignored");
    RI_ASSERT(tr.u.tr.cursor == loop_start && tr.u.tr.tr.clicks == 1u, "still held");

    /* Another key's release must not re-arm the held STOP (the same rule t241
     * pins for the toggle): a held key would start stepping again the moment
     * any other key came up. */
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KPPLUS | RI_RAW_UPFLAG, 0) == 0, "other key up");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KP0, 0) == 0, "repeat after other up ignored");
    RI_ASSERT(tr.u.tr.cursor == loop_start && tr.u.tr.tr.clicks == 1u, "still held after other up");

    /* The release re-arms it, and the next real press is the song start. */
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KP0 | RI_RAW_UPFLAG, 0) == 0, "release no action");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KP0, 0) == 1, "second press acts");
    RI_ASSERT(tr.u.tr.cursor == 0u, "second stop -> song start");
    RI_ASSERT(tr.u.tr.tr.clicks == 0u, "clicks 1 -> 0");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KP0 | RI_RAW_UPFLAG, 0) == 0, "stop released");

    /* The toggle key and the stop key keep separate latches: holding one must
     * not silence the other. */
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE, 0) == 1, "space plays");
    RI_ASSERT(ri_sui_led(&tr, RI_STR_PLAY, 0), "playing");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_KP0, 0) == 1, "stop while space held");
    RI_ASSERT(!ri_sui_led(&tr, RI_STR_PLAY, 0), "stopped");
    RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE, 0) == 0, "held space still ignored");
    RI_ASSERT(!ri_sui_led(&tr, RI_STR_PLAY, 0), "and does not play");

    RI_RESULT("transport_rewind");
    return 0;
}
