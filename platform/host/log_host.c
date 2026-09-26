/* log_host.c — host (CI) backend for ri_pal_log (portability plan T6).
 * Core-side vsnprintf formatter (portable idiom); sink = stderr.
 */
#include "platform/pal/ri_pal_log.h"
#include <stdio.h>

int ri_log_format(char *out, uint32_t cap, const char *fmt, va_list ap) {
    int n;
    if (!out || cap == 0u || !fmt)
        return -1;
    n = vsnprintf(out, (size_t)cap, fmt, ap);
    out[cap - 1u] = 0;
    return n;
}

void ri_pal_log_sink(const char *s) {
    if (s)
        fputs(s, stderr);
}

void ri_log(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    if (!fmt)
        return;
    va_start(ap, fmt);
    ri_log_format(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ri_pal_log_sink(buf);
}
