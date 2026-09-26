/* t89_pal_midi — portability T5: host script-file MIDI backend.
 * Deterministic delivery (bytes + time_us, in file order) and send capture.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "tests/helpers/ri_assert.h"
#include "platform/pal/ri_pal_midi.h"

static uint8_t t_got[8][32];
static uint32_t t_len[8];
static uint64_t t_when[8];
static uint32_t t_n;

static void t_cb(void *user, const uint8_t *msg, uint32_t len, uint64_t time_us) {
    (void)user;
    if (t_n >= 8u || len > 32u)
        return;
    memcpy(t_got[t_n], msg, len);
    t_len[t_n] = len;
    t_when[t_n] = time_us;
    t_n++;
}

int main(void) {
    char script[256], cap[256];
    FILE *f;
    uint8_t out[3];
    snprintf(script, sizeof script, "/tmp/ri/t89-%d.txt", (int)getpid());
    snprintf(cap, sizeof cap, "/tmp/ri/t89cap-%d.txt", (int)getpid());
    unlink(cap);
    f = fopen(script, "w");
    RI_ASSERT(f != 0, "script open");
    fputs("# comment\n", f);
    fputs("12000 90 3C 40\n", f);
    fputs("24000 B0 07 64\n", f);
    fputs("36000 80 3C 00\n", f);
    fclose(f);
    t_n = 0u;
    RI_ASSERT(ri_pal_midi_open_in(script, t_cb, 0) == 0, "open");
    RI_ASSERT(t_n == 3u, "events %u", t_n);
    RI_ASSERT(t_len[0] == 3u && t_got[0][0] == 0x90u && t_got[0][1] == 0x3Cu && t_got[0][2] == 0x40u, "ev0");
    RI_ASSERT(t_when[0] == 12000u && t_when[1] == 24000u && t_when[2] == 36000u, "times");
    RI_ASSERT(t_len[1] == 3u && t_got[1][0] == 0xB0u, "ev1");
    RI_ASSERT(t_len[2] == 3u && t_got[2][0] == 0x80u, "ev2");
    RI_ASSERT(ri_pal_midi_open_in("/tmp/ri/t89-nope.txt", t_cb, 0) != 0, "miss");
    out[0] = 0x90u;
    out[1] = 0x40u;
    out[2] = 0x7Fu;
    RI_ASSERT(ri_pal_midi_send(cap, out, 3u) == 0, "send");
    RI_ASSERT(ri_pal_midi_send(cap, 0, 0u) != 0, "send empty");
    {
        char line[64];
        FILE *cf = fopen(cap, "r");
        RI_ASSERT(cf != 0, "cap open");
        RI_ASSERT(fgets(line, sizeof line, cf) != 0, "cap line");
        RI_ASSERT(strncmp(line, "90 40 7F", 8) == 0, "cap %s", line);
        fclose(cf);
    }
    ri_pal_midi_close();
    unlink(script);
    unlink(cap);
    RI_RESULT("pal_midi");
}
