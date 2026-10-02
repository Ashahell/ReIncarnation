/* t157_playlist_canon — RBPL entry paths are canonicalised textually.
 *
 * Why this exists (owner decision 2026-10-02): AROS does not resolve a ".."
 * component in a path, while the host PAL does. A playlist entry
 * "../demo/x.rbng" beside a playlist in ".../songs/local/" therefore resolved
 * on the host and failed on the Dell with `RIAPP song ...: open failed` and a
 * "Cannot load song" requester -- the bug that hid since 2026-09-30 because it
 * only fired on auto-advance past the last entry.
 *
 * The law (all in ri_playlist_join, the single choke point):
 *   - "." components are dropped
 *   - ".." pops the previous component
 *   - a ".." with nothing left to pop is DROPPED (clamped at the root, the
 *     way the kernel clamps at "/"); it is never emitted, because a leading
 *     ".." is exactly what AROS cannot resolve
 *   - a leading "/" and a volume ("Vk4aros:") are the root: never popped
 *   - empty components from "//" and a trailing slash are dropped
 *   - the result is idempotent
 */
#include <stdio.h>
#include <string.h>
#include "project/playlist.h"
#include "tests/helpers/ri_assert.h"

static void join(const char *dir, const char *rel, const char *want) {
    char out[RI_PLAYLIST_PATH];
    RI_ASSERT(ri_playlist_join(dir, rel, out, sizeof out) == 0, "join rc: %s + %s", dir, rel);
    RI_ASSERT(!strcmp(out, want), "join(%s, %s) = [%s], want [%s]", dir, rel, out, want);
}

int main(void) {
    char out[RI_PLAYLIST_PATH], again[RI_PLAYLIST_PATH];

    /* The bug, exactly as it was deployed. */
    join("Vk4aros:ReIncarnation/songs/local", "../demo/riapp-demo.rbng",
         "Vk4aros:ReIncarnation/songs/demo/riapp-demo.rbng");
    join("songs/local", "../demo/riapp-demo.rbng", "songs/demo/riapp-demo.rbng");
    join("songs/local/sub", "../../demo/x.rbng", "songs/demo/x.rbng");

    /* Plain names and inner dots are untouched by the fold. */
    join("Vk4aros:a/local", "the-knife/the-knife.rbng", "Vk4aros:a/local/the-knife/the-knife.rbng");
    join("Vk4aros:a", "x.rbng", "Vk4aros:a/x.rbng");
    join("x", "y/z.rbng", "x/y/z.rbng");

    /* "." goes, at the front and in the middle. */
    join("Vk4aros:a", "./x.rbng", "Vk4aros:a/x.rbng");
    join("Vk4aros:a/b", "./c/./d.rbng", "Vk4aros:a/b/c/d.rbng");
    join("Vk4aros:a/b/./", "c.rbng", "Vk4aros:a/b/c.rbng");

    /* Clamping at the root: surplus ".." is dropped, never emitted. */
    join("a", "../../x.rbng", "x.rbng");
    join("a", "../../../x.rbng", "x.rbng");
    join("Vk4aros:", "../x.rbng", "Vk4aros:x.rbng");
    join("Vk4aros:a", "../../../x.rbng", "Vk4aros:x.rbng");
    join("", "../x.rbng", "x.rbng");

    /* The root itself is never popped, and keeps its shape. */
    join("Vk4aros:", "x.rbng", "Vk4aros:x.rbng");
    join("Work:", "a.rbng", "Work:a.rbng");
    join("Vk4aros:", "", "Vk4aros:");

    /* A leading "/" is a root too (host-absolute entries pass through).
     * "/x/../y" is "/y" -- "x/.." is the root, exactly as POSIX and as the
     * host filesystem resolve it; the same holds for a directory on a volume. */
    join("Vk4aros:a", "/x/../y.rbng", "/y.rbng");
    join("Vk4aros:a", "/../y.rbng", "/y.rbng");
    join("x", "/abs/./q.rbng", "/abs/q.rbng");

    /* Absolute entries bypass the dir, as before, and are still canonical. */
    join("Vk4aros:a/b", "Work:z/../w.rbng", "Work:w.rbng");
    join("Vk4aros:a/b", "Work:z/./k/../w.rbng", "Work:z/w.rbng");

    /* Empty components and a trailing slash. */
    join("Vk4aros:a//b", "c.rbng", "Vk4aros:a/b/c.rbng");
    join("Vk4aros:a", "b/", "Vk4aros:a/b");
    join("Vk4aros:a/", "b.rbng", "Vk4aros:a/b.rbng");

    /* Idempotence: folding a folded path changes nothing. */
    RI_ASSERT(ri_playlist_join("Vk4aros:a/b", "../c/./d.rbng", out, sizeof out) == 0, "once");
    RI_ASSERT(ri_playlist_join("", out, again, sizeof again) == 0, "twice");
    RI_ASSERT(!strcmp(out, again), "not idempotent: [%s] then [%s]", out, again);

    /* Does not fit is still refused. A NULL dir is a passthrough (an entry
     * with no playlist directory to resolve against), as it always was. */
    RI_ASSERT(ri_playlist_join("Vk4aros:aaaaaaaaaa", "bbbbbbbbbb/cccccccccc/dddddddddd",
             out, 16u) == 2, "overflow must be refused");
    join(0, "x/../y.rbng", "y.rbng");
    RI_ASSERT(ri_playlist_join(0, 0, out, sizeof out) == 2, "null rel");
    RI_ASSERT(ri_playlist_join("a", "b", 0, 0u) == 2, "null out");

    /* A playlist parse end-to-end: the entry a user would have written. */
    {
        struct RIPlaylist pl;
        char err[160];
        const char *text =
            "RBPL 1\n"
            "TITLE t\n"
            "the-knife/the-knife.rbng | The Knife\n"
            "zombie-nation/zombie-nation.rbng | Zombie Nation\n"
            "../demo/riapp-demo.rbng | RIAPP demo\n";
        RI_ASSERT(ri_playlist_parse(text, "Vk4aros:ReIncarnation/songs/local", &pl,
            err, sizeof err) == 0, "parse: %s", err);
        RI_ASSERT(pl.n == 3u, "entries %lu", (unsigned long)pl.n);
        RI_ASSERT(!strcmp(pl.e[2].path, "Vk4aros:ReIncarnation/songs/demo/riapp-demo.rbng"),
            "entry 2 = [%s]", pl.e[2].path);
        RI_ASSERT(!strcmp(pl.e[0].path,
            "Vk4aros:ReIncarnation/songs/local/the-knife/the-knife.rbng"),
            "entry 0 = [%s]", pl.e[0].path);
    }

    RI_RESULT("playlist_canon");
}