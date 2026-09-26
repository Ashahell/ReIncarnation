/* log_aros.c — AROS backend for ri_pal_log (portability plan T6).
 * AROS-only. Core formats with vsnprintf (portable); the sink takes the
 * finished string, so RawDoFmt 32-bit packing rules never leak into callers.
 */
#ifndef __AROS__
#error "log_aros.c is AROS-only (portability plan T6)"
#endif

#include "platform/pal/ri_pal_log.h"

#include <exec/types.h>
#include <proto/dos.h>

#include <stdio.h>
#include <string.h>

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
        PutStr((STRPTR)s); /* no formatting: the string is already complete */
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
