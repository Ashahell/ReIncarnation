/* t58_transport — §12.9a transport state machine.
 * Task 1 first (RED: transport.h does not exist yet).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/transport.h"

int main(void) {
    struct RITransport t;
    uint64_t cur;
    int armed = 7;
    /* Law probe: after EVERY transition below that leaves STOPPED,
    * clicks must read 0. Checked inline at each site (not once at the
    * end) so a regression names its own transition. */
#define RI_T58_LAW(tt) RI_ASSERT((tt).state == RI_TR_STOPPED || (tt).clicks == 0u, "law")
    /* Play from STOPPED: continue, clicks 0. */
    t.state = RI_TR_STOPPED; t.clicks = 0; cur = 12345ULL;
    ri_tr_play(&t, &cur);
    RI_ASSERT(t.state == RI_TR_PLAYING, "play state");
    RI_ASSERT(cur == 12345ULL, "play cursor moved");
    RI_ASSERT(t.clicks == 0u, "play clicks");
    RI_T58_LAW(t);
    /* PLAYING + Play: no-op. */
    ri_tr_play(&t, &cur);
    RI_ASSERT(t.state == RI_TR_PLAYING && cur == 12345ULL, "play idempotent");
    RI_T58_LAW(t);
    /* RECORD + Stop: STOPPED click 1, cursor held. */
    t.state = RI_TR_RECORD; t.clicks = 0; cur = 999ULL;
    ri_tr_stop(&t, &cur, 0ULL, 0ULL);
    RI_ASSERT(t.state == RI_TR_STOPPED && t.clicks == 1u && cur == 999ULL, "rec stop");
    /* Stop law (ReBirth-inspired + deliberate extension — E1 ends at
    * the 3rd click; the 4th restarting as click 1 keeps every Stop press
    * meaningful and the machine total). The FIRST stop while STOPPED
    * (clicks==0) moves nothing — it only arms the next jump. */
    t.state = RI_TR_STOPPED; t.clicks = 1u; cur = 5000ULL;
    ri_tr_stop(&t, &cur, 777ULL, 0ULL);
    RI_ASSERT(cur == 777ULL && t.clicks == 2u, "stop2");
    ri_tr_stop(&t, &cur, 777ULL, 0ULL);
    RI_ASSERT(cur == 0ULL && t.clicks == 0u, "stop3");
    ri_tr_stop(&t, &cur, 777ULL, 0ULL);
    RI_ASSERT(cur == 0ULL && t.clicks == 1u, "stop4 restarts");
    /* First stop while STOPPED with clicks==0: arms only. */
    t.state = RI_TR_STOPPED; t.clicks = 0u; cur = 4242ULL;
    ri_tr_stop(&t, &cur, 777ULL, 0ULL);
    RI_ASSERT(cur == 4242ULL && t.clicks == 1u, "stop1 arms");
    /* Record button matrix. */
    t.state = RI_TR_STOPPED; t.clicks = 2u; cur = 11ULL;
    ri_tr_record(&t, &cur);
    RI_ASSERT(t.state == RI_TR_RECORD && cur == 11ULL && t.clicks == 0u, "rec from stop");
    RI_T58_LAW(t);
    t.state = RI_TR_PLAYING; cur = 11ULL;
    ri_tr_record(&t, &cur);
    RI_ASSERT(t.state == RI_TR_RECORD && cur == 11ULL, "punch in");
    RI_T58_LAW(t);
    ri_tr_record(&t, &cur);
    RI_ASSERT(t.state == RI_TR_PLAYING && cur == 11ULL, "punch out");
    RI_T58_LAW(t);
    /* Null-safe, armed untouched by every call above. Law holds on this
    * path too: the PLAYING exit zeroed clicks. */
    ri_tr_play(0, &cur); ri_tr_play(&t, 0); ri_tr_stop(0, 0, 0, 0); ri_tr_record(0, 0);
    RI_ASSERT(armed == 7, "armed touched");
#undef RI_T58_LAW
    RI_RESULT("transport");
}
