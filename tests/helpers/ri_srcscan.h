/* ri_srcscan.h — helpers for the SOURCE-STRUCTURE gates.
 *
 * WHY THESE EXIST. Two defects in this tree (the 2026-10-12 sender-join
 * crash and the 2026-10-20 init-order defect) cannot be host-tested: they
 * need two tasks and a real link, or a task and a render. So they are pinned
 * as source structure. The ledger records the lesson that came with the
 * first of those gates:
 *
 *   "A STRUCTURAL CHECK THAT CAN BE SATISFIED BY A COMMENT IS NOT A
 *    STRUCTURAL CHECK."
 *
 * — the first version of that gate PASSED WITH THE FIX DELETED, because the
 * fix's own explanatory comment contained the text the search looked for.
 * So the comments are blanked before any search, the offsets are preserved
 * (so a failure can name a line), and the checks are written against CODE
 * shapes rather than names.
 *
 * The helpers are `static inline` so that a test which does not use one of
 * them does not trip -Wunused-function.
 */
#ifndef RI_SRCSCAN_H
#define RI_SRCSCAN_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read a whole file. Returns NULL if it cannot be read. */
static inline char *ri_src_slurp(const char *path, long *len_out) {
    FILE *f = fopen(path, "rb");
    char *buf;
    long n;
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return 0; }
    buf = (char *)malloc((size_t)n + 1u);
    if (!buf) { fclose(f); return 0; }
    if (fread(buf, 1u, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return 0; }
    buf[n] = 0;
    fclose(f);
    if (len_out)
        *len_out = n;
    return buf;
}

/* The offset of the first occurrence of `needle` at or after `from`, or -1. */
static inline long ri_src_find(const char *hay, long hay_len, const char *needle,
                               long from) {
    long nl = (long)strlen(needle);
    long i;
    if (from < 0)
        from = 0;
    for (i = from; i + nl <= hay_len; i++) {
        if (memcmp(hay + i, needle, (size_t)nl) == 0)
            return i;
    }
    return -1;
}

/* Blank the COMMENTS, keeping every offset, so a comment cannot satisfy a
 * structural check. String and character literals are left alone. */
static inline char *ri_src_blank_comments(char *src, long len) {
    long i = 0;
    int in_block = 0, in_line = 0, in_str = 0, in_chr = 0;
    for (i = 0; i < len; i++) {
        char c = src[i];
        char n = (i + 1 < len) ? src[i + 1] : '\0';
        if (in_block) {
            if (c == '*' && n == '/') { src[i] = ' '; src[i + 1] = ' '; i++; in_block = 0; continue; }
            if (c != '\n') src[i] = ' ';
            continue;
        }
        if (in_line) {
            if (c == '\n') { in_line = 0; continue; }
            src[i] = ' ';
            continue;
        }
        if (in_str) {
            if (c == '\\') { i++; continue; }
            if (c == '"') in_str = 0;
            continue;
        }
        if (in_chr) {
            if (c == '\\') { i++; continue; }
            if (c == '\'') in_chr = 0;
            continue;
        }
        if (c == '/' && n == '*') { src[i] = ' '; src[i + 1] = ' '; i++; in_block = 1; continue; }
        if (c == '/' && n == '/') { src[i] = ' '; in_line = 1; continue; }
        if (c == '"') { in_str = 1; continue; }
        if (c == '\'') { in_chr = 1; continue; }
    }
    return src;
}

/* The range of the function definition `name` ("static <type> name(...)"),
 * as offsets into the source. A definition ends at the next line that is
 * exactly "}". Returns 0 on success. */
static inline int ri_src_function_range(const char *src, long len,
                                        const char *name,
                                        long *start_out, long *end_out) {
    char pat[128];
    long at;
    snprintf(pat, sizeof pat, "%s(", name);
    at = ri_src_find(src, len, pat, 0);
    if (at < 0)
        return 1;
    /* the end: the next line that is exactly "}" */
    {
        long j = at;
        long end = len;
        while (j < len) {
            long e2 = ri_src_find(src, len, "\n", j);
            long le = (e2 < 0) ? len : e2 + 1;
            if (le - j == 2 && src[j] == '}' && src[j + 1] == '\n') {
                end = j;
                break;
            }
            j = le;
        }
        if (start_out) *start_out = at;
        if (end_out) *end_out = end;
    }
    return 0;
}

/* The name of the function definition that encloses `off`, or "" if none. */
static inline void ri_src_enclosing_function(const char *src, long len, long off,
                                             char *name_out, size_t name_cap) {
    long i = 0;
    name_out[0] = 0;
    while (i < len) {
        long eol = ri_src_find(src, len, "\n", i);
        long line_end = (eol < 0) ? len : eol + 1;
        if (line_end > off)
            break;
        if (strncmp(src + i, "static void ", 12) == 0 &&
            strstr(src + i, "(void) {") != NULL &&
            (src + i)[12] != '\0') {
            const char *p = src + i + 12;
            const char *q = p;
            long j, end = len;
            while (q < src + line_end && *q != '(' && *q != ' ' && *q != '\n')
                q++;
            j = line_end;
            while (j < len) {
                long e2 = ri_src_find(src, len, "\n", j);
                long le = (e2 < 0) ? len : e2 + 1;
                if (le - j == 2 && src[j] == '}' && src[j + 1] == '\n') {
                    end = j;
                    break;
                }
                j = le;
            }
            if (off < end) {
                size_t n = (size_t)(q - p);
                if (n >= name_cap)
                    n = name_cap - 1u;
                memcpy(name_out, p, n);
                name_out[n] = 0;
                return;
            }
        }
        i = line_end;
    }
}

#endif /* RI_SRCSCAN_H */
