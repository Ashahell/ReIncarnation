/* ri_pal_log.h — logging (portability plan T6, §3.6).
 * Core formats with vsnprintf (portable); the backend sinks the string.
 * This kills the RawDoFmt packing trap (stays inside the AROS sink).
 * NOTE: uses <stdarg.h> for the varargs decl; the §2 include gate
 * allows stdarg.h for this header only.
 */
#ifndef RI_PAL_LOG_H
#define RI_PAL_LOG_H
#include <stdarg.h>
#include <stdint.h>

void ri_log(const char *fmt, ...);
/* Backend sink: already-formatted NUL-terminated line (no formatting here). */
void ri_pal_log_sink(const char *s);
/* Core-side formatter entry (portable vsnprintf wrapper, for tests). */
int ri_log_format(char *out, uint32_t cap, const char *fmt, va_list ap);

#endif
