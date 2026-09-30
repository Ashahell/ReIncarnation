/* playlist.h — RBPL song playlists (songs & playlists, owner 2026-09-30).
 * A playlist is a small text file listing songs to load in order:
 *   RBPL 1                         header, first statement
 *   TITLE <text>                   optional playlist name
 *   <path> [| <title>]             one song per line; a relative path is
 *                                  relative to the playlist's directory
 * '#' starts a comment line; blank lines are skipped. Paths may be host
 * ("/x/y.rbng", "songs/a.rbng") or AROS ("Vk4aros:songs/a.rbng",
 * "Work:a.rbng"); anything with a ':' or a leading '/' is absolute.
 * Pure (no IO): the caller reads the file and passes its text and
 * directory. */
#ifndef RI_PLAYLIST_H
#define RI_PLAYLIST_H
#include <stdint.h>

#define RI_PLAYLIST_MAX 64u
#define RI_PLAYLIST_PATH 256u
#define RI_PLAYLIST_NAME 64u

struct RIPlaylistEntry {
    char path[RI_PLAYLIST_PATH];   /* resolved (directory applied) */
    char name[RI_PLAYLIST_NAME];   /* display title ("" = use the file name) */
};

struct RIPlaylist {
    uint32_t n;
    char title[RI_PLAYLIST_NAME];
    struct RIPlaylistEntry e[RI_PLAYLIST_MAX];
};

/* Parse text; dir is the playlist's directory ("" when unknown). Returns 0
 * ok (n >= 1), 2 error with "line N: reason" in err. */
int ri_playlist_parse(const char *text, const char *dir, struct RIPlaylist *pl, char *err, uint32_t errcap);
/* Directory part of a path ("songs/x.rbpl" -> "songs", "Work:x" -> "Work:",
 * "x" -> ""). out always NUL-terminated. */
void ri_playlist_dirname(const char *path, char *out, uint32_t cap);
/* Join dir and a relative path (absolute paths pass through). Returns 0
 * ok, 2 when it does not fit. */
int ri_playlist_join(const char *dir, const char *rel, char *out, uint32_t cap);
/* Next / previous entry with wrap-around (0 when empty). */
uint32_t ri_playlist_next(const struct RIPlaylist *pl, uint32_t cur);
uint32_t ri_playlist_prev(const struct RIPlaylist *pl, uint32_t cur);
/* Display name of entry i: its title, else the file name without the
 * directory and extension (written to out). */
void ri_playlist_name(const struct RIPlaylist *pl, uint32_t i, char *out, uint32_t cap);

#endif
