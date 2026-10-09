/* midi_host.c — host (CI) backend for ri_pal_midi (portability plan T5).
 * Deterministic script-file source for tests. Format (text, one event
 * per line): `<time_us> <hex bytes...>` e.g. `12000 90 3C 40`.
 * Lines starting with '#' are comments. ri_pal_midi_send appends to a
 * capture file (RI_MIDI_HOST_CAPTURE or /tmp/ri/midi_capture.txt).
 */
#include "platform/pal/ri_pal_midi.h"
#include "midi_io/midi_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static ri_midi_in s_cb;
static void *s_user;
static char s_script[512];
static char s_capture[512];
static struct RIMidiBridge s_bridge;
static int s_have_bridge;

void ri_pal_midi_poll(void);

static const char *cap_path(void) {
    const char *e = getenv("RI_MIDI_HOST_CAPTURE");
    return (e && e[0]) ? e : "/tmp/ri/midi_capture.txt";
}

int ri_pal_midi_open_in(const char *port, ri_midi_in cb, void *user) {
    FILE *f;
    char line[256];
    if (!port || !port[0])
        return 1;
    f = fopen(port, "r");
    if (!f)
        return 1;
    s_cb = cb;
    s_user = user;
    midi_bridge_init(&s_bridge);
    s_have_bridge = 1;
    snprintf(s_script, sizeof s_script, "%s", port);
    snprintf(s_capture, sizeof s_capture, "%s", cap_path());
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        uint64_t t = 0u;
        uint8_t msg[32];
        uint32_t n = 0u;
        if (line[0] == '#' || line[0] == '\n')
            continue;
        t = strtoull(p, &p, 10);
        while (*p && n < sizeof msg) {
            unsigned v = 0u;
            while (*p == ' ' || *p == '\t')
                p++;
            if (!*p || *p == '\n')
                break;
            v = (unsigned)strtoul(p, &p, 16);
            if (v > 255u)
                break;
            msg[n++] = (uint8_t)v;
        }
        if (n > 0u) {
            /* Script lines are whole CAMD messages (status + data). */
            midi_bridge_feed(&s_bridge, msg[0],
                n > 1u ? msg[1] : 0u, n > 2u ? msg[2] : 0u, t);
        }
    }
    fclose(f);
    /* Compatibility drain: historical callers observe the callback at
     * open (t89 pins it); queue-direct users read the bridge. */
    ri_pal_midi_poll();
    return 0;
}

int ri_pal_midi_send(const char *port, const uint8_t *msg, uint32_t len) {
    FILE *f;
    uint32_t i;
    const char *dst = (port && port[0]) ? port : cap_path();
    if (!msg || len == 0u)
        return 1;
    f = fopen(dst, "a");
    if (!f)
        return 1;
    for (i = 0u; i < len; i++)
        fprintf(f, "%02X%s", msg[i], i + 1u < len ? " " : "\n");
    fclose(f);
    return 0;
}

void ri_pal_midi_close(void) {
    s_cb = 0;
    s_user = 0;
    s_script[0] = 0;
    s_have_bridge = 0;
}

struct RIMidiBridge *ri_pal_midi_bridge(void) {
    return s_have_bridge ? &s_bridge : 0;
}

/* Host script backend delivers synchronously at open; the pump only
 * drains anything queued since (nothing, on host). */
void ri_pal_midi_poll(void) {
    struct RIMidiMsg msgs[32];
    uint32_t n, i;
    if (!s_have_bridge || !s_cb)
        return;
    n = midi_bridge_read_ch(&s_bridge, msgs, 32u);
    for (i = 0u; i < n; i++)
        s_cb(s_user, msgs[i].b, msgs[i].n, msgs[i].t_us);
}
