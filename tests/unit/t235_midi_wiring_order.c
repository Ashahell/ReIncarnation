/* t235_midi_wiring_order — the MIDI outputs must be wired BEFORE the render
 * task starts.
 *
 * WHY THIS TEST EXISTS (2026-10-20, R12 O1 review). The outbound MIDI ring
 * is SINGLE-PRODUCER (the render) and the live driver's fields (clk_out,
 * dev_out, note_ch, prog_sent) are read by the render with no ordering and
 * no atomics. R12 O1 (272cf20) fixed the stale-settings defect by moving
 * the device-out wiring into midi_setup() — which runs AFTER au_live_run()
 * started the render task. So the main task, with the render already
 * running:
 *
 *   * wrote s_lv.drv.clk_out / dev_out / note_ch / prog_sent — the render's
 *     configuration, read without ordering;
 *   * called ri_livedrv_devout_attach(), which midi_out_put()s the programme
 *     change into the ring: a SECOND PRODUCER on a ring the render may be
 *     filling at that instant;
 *   * called midi_out_set_bpm() on the live ring.
 *
 * The old code had stale settings but ran single-threaded. The fix has to
 * keep both: correct settings AND everything wired before the render
 * starts.
 *
 * THE CHECK IS STRUCTURAL, NOT A NAME. The wiring must live in a function
 * whose CALL SITE precedes the render's call site. Renaming that function
 * does not hide the defect, and moving the call back after the render does
 * not hide it either.
 *
 * It also pins the assert that makes the order CHECKED rather than assumed:
 * s_midi_prepared is set by the preparing function and tested immediately
 * before the render starts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests/helpers/ri_assert.h"

#ifndef RI_REPO_ROOT
#define RI_REPO_ROOT "."
#endif

/* The statements that must not be reachable from the render's call site: each
 * is a write to the render's configuration, or a production into its ring. The
 * patterns are the CODE shapes, not the names: a comment that merely mentions
 * "au_live_run()" must not be mistaken for the call. */
static const char *k_wiring[] = {
    "s_lv.drv.clk_out =",
    "s_lv.drv.dev_out =",
    "s_lv.drv.note_ch =",
    "s_lv.drv.prog_sent =",
    "ri_livedrv_devout_attach(&s_lv.drv);",
    "midi_out_set_bpm(&s_mout",
    "midi_out_enable(&s_mout",
};

static char *slurp(const char *path, long *len_out) {
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
static long find_after(const char *hay, long hay_len, const char *needle,
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

/* The name of the function definition that encloses `off`, or "" if none.
 * A definition is "static void NAME(void) {" and ends at the next line that
 * is exactly "}". */
static void enclosing_function(const char *src, long len, long off,
                               char *name_out, size_t name_cap) {
    long i = 0;
    name_out[0] = 0;
    while (i < len) {
        long eol = find_after(src, len, "\n", i);
        long line_end = (eol < 0) ? len : eol + 1;
        if (line_end > off)
            break;
        if (strncmp(src + i, "static void ", 12) == 0 &&
            strstr(src + i, "(void) {") != NULL &&
            (src + i)[12] != '\0') {
            const char *p = src + i + 12;
            const char *q = p;
            while (q < src + line_end && *q != '(' && *q != ' ' && *q != '\n')
                q++;
            /* the definition's end: the next line that is exactly "}" */
            long j = line_end;
            long end = len;
            while (j < len) {
                long e2 = find_after(src, len, "\n", j);
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

/* Blank the COMMENTS, keeping every offset, so a comment cannot satisfy the
 * check. The ledger records why, from the 2026-10-12 cross-task crash:
 *
 *   "A STRUCTURAL CHECK THAT CAN BE SATISFIED BY A COMMENT IS NOT A
 *    STRUCTURAL CHECK."
 *
 * and the gate that got it wrong there (the sender-join check) "passed with
 * the fix deleted", because the fix's own explanatory comment contained the
 * very text the search looked for. This test searches for CODE shapes, so
 * blanking the comments first is the difference between a gate and a
 * sentence. */
static char *blank_comments(char *src, long len) {
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

int main(void) {
    char path[512];
    char *src;
    long len = 0;
    long at_render, at_assert;
    size_t i;
    /* The tree to read. The environment wins, so the test can be pointed at
     * another checkout (which is how the pre-fix source is checked). */
    const char *root = getenv("RI_REPO_ROOT");
    if (!root || !root[0])
        root = RI_REPO_ROOT;

    snprintf(path, sizeof path, "%s/app/riapp.c", root);
    src = slurp(path, &len);
    if (!src) {
        printf("FAIL: cannot read %s\n", path);
        RI_RESULT("midi-wiring-order");
    }
    /* Comments go first: a comment must not be able to satisfy this gate. */
    blank_comments(src, len);

    /* The render's call site. The pattern carries its argument, so a comment
     * that merely names au_live_run() is not mistaken for the call. */
    at_render = find_after(src, len, "au_live_run(&s_lv", 0);
    at_assert = find_after(src, len, "if (!s_midi_prepared)", 0);
    RI_ASSERT(at_render >= 0, "au_live_run() is never called");
    RI_ASSERT(at_assert >= 0, "the s_midi_prepared assert is missing");
    RI_ASSERT(at_assert < at_render,
        "the s_midi_prepared assert is at %ld, AFTER au_live_run() at %ld — "
        "the order must be checked before the render starts",
        at_assert, at_render);

    /* Every wiring statement must live in a function whose CALL SITE comes
     * before the render's call site. That is the invariant, and it is
     * independent of what that function is called. */
    for (i = 0; i < sizeof k_wiring / sizeof k_wiring[0]; i++) {
        const char *needle = k_wiring[i];
        long at = find_after(src, len, needle, 0);
        char fname[128];
        char call[160];
        long at_call;
        RI_ASSERT(at >= 0, "\"%s\" is not in the source at all", needle);
        if (at < 0)
            continue;
        enclosing_function(src, len, at, fname, sizeof fname);
        RI_ASSERT(fname[0] != 0,
            "\"%s\" is not inside a function definition", needle);
        if (fname[0] == 0)
            continue;
        snprintf(call, sizeof call, "%s();", fname);
        at_call = find_after(src, len, call, 0);
        RI_ASSERT(at_call >= 0, "%s() is never called", fname);
        if (at_call < 0)
            continue;
        RI_ASSERT(at_call < at_render,
            "\"%s\" lives in %s(), whose call site is at %ld — AFTER "
            "au_live_run() at %ld. The render is the ring's only producer and "
            "reads those fields without ordering.",
            needle, fname, at_call, at_render);
    }

    free(src);
    RI_RESULT("midi-wiring-order");
}
