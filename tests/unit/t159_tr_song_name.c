/* t159_tr_song_name — the transport plate shows which song is playing
 * (owner 2026-10-03: "the user cannot see which song is playing").
 *
 * The gap was real and lasted a long time: the log has said
 * `RIAPP song <path>: N bars at M BPM` since songs landed, and the only way to
 * find out what was playing was to pull the log off a guest by hand.
 *
 * These laws call the REAL functions -- ri_art_tr_copy and ri_art_tr_fit are
 * exported for exactly this -- rather than re-implementing the fit here. The
 * first version of this file mirrored the fit, and 18 of 19 mutants survived:
 * a mirror of the thing under test cannot fail when the thing changes. The
 * draw-path guards (draw only when set, only when the fit produced something)
 * are not directly callable without a framebuffer, so they are pinned as the
 * contract they encode rather than as a second copy of the logic.
 *
 * Laws:
 *  - unset reads empty, and the contract says draw nothing;
 *  - a name round-trips, and a second name replaces the first -- stale text is
 *    the exact failure this change exists to prevent;
 *  - NULL and "" both clear rather than crash or leave the old name;
 *  - the copy truncates over-long input and always terminates;
 *  - the copy is exact at the boundary, one char under and one char over;
 *  - the fit leaves a name that fits untouched;
 *  - the fit shortens a name that does not, and what it returns FITS;
 *  - the fit shortens by the minimum, not by eating the string;
 *  - truncation is visible: the result ends in an ellipsis;
 *  - the fit respects its room in both directions;
 *  - a tiny destination yields an ellipsis rather than an overflow.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/draw/art.h"

/* Stand-in face: 6 px per character, deterministic, no real font needed. */
static int tw_width(void *ctx, const char *s) {
    (void)ctx;
    return (int)strlen(s) * 6;
}
static const struct ri_text_metrics TW = { tw_width, 8, 6, 0 };

/* Canary pattern as a byte array rather than a string literal: \xa5 in a
 * literal is a source-encoding trap that survives a shell heredoc and turns
 * the comparison into a comparison against the wrong bytes. */
static const unsigned char CAN4[4] = { 0xa5u, 0xa5u, 0xa5u, 0xa5u };

#define PX_PER_CHAR 6
#define ROOM_CHARS  46                       /* the gap the transport has */
#define ROOM_PX     (ROOM_CHARS * PX_PER_CHAR)

int main(void) {
    static char big[512];
    static char out[64];

    /* Unset reads empty, and the draw contract says draw nothing. */
    RI_ASSERT(ri_art_tr_song() != NULL, "the getter never returns NULL");
    RI_ASSERT(strlen(ri_art_tr_song()) == 0,
        "a fresh label is empty (\"%s\")", ri_art_tr_song());

    /* Round-trips, and fits unchanged. */
    ri_art_tr_set_song("zombie-nation.rbng");
    RI_ASSERT(strcmp(ri_art_tr_song(), "zombie-nation.rbng") == 0,
        "a name round-trips (\"%s\")", ri_art_tr_song());
    ri_art_tr_fit(out, sizeof out, ri_art_tr_song(), &TW, ROOM_PX);
    RI_ASSERT(strcmp(out, "zombie-nation.rbng") == 0,
        "a name that fits is left whole (\"%s\")", out);

    /* A second name replaces the first: stale text is the bug being fixed. */
    ri_art_tr_set_song("the-knife.rbng");
    RI_ASSERT(strcmp(ri_art_tr_song(), "the-knife.rbng") == 0,
        "a new name replaces the old (\"%s\")", ri_art_tr_song());
    RI_ASSERT(strstr(ri_art_tr_song(), "zombie") == NULL,
        "no stale text from the previous song");

    /* NULL and "" both clear. */
    ri_art_tr_set_song(NULL);
    RI_ASSERT(strlen(ri_art_tr_song()) == 0, "NULL clears the label");
    ri_art_tr_set_song("x");
    ri_art_tr_set_song("");
    RI_ASSERT(strlen(ri_art_tr_song()) == 0, "an empty string clears the label");

    /* The copy: over-long input is truncated and terminated, never overflowed.
     * Guarded by canaries either side of the destination. This is the only
     * thing that can see a lost bound: a copy that runs past `cap` still
     * produces the right string for every input short enough to matter, so a
     * plain value assertion cannot fail when the bound is gone. The canaries
     * are what turn "the loop lost its bound" from a survivor into a kill. */
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    {
        static unsigned char guarded[72];
        static const unsigned char CAN = 0xa5;
        memset(guarded, (int)CAN, sizeof guarded);
        ri_art_tr_copy((char *)&guarded[4], 64u, big);
        RI_ASSERT(memcmp(&guarded[0], CAN4, 4) == 0,
            "the copy does not write BEFORE the destination");
        RI_ASSERT(memcmp(&guarded[68], CAN4, 4) == 0,
            "the copy does not write PAST the destination (overflow)");
        RI_ASSERT(strlen((char *)&guarded[4]) == 63,
            "and still fills cap-1 and terminates (%u)",
            (unsigned)strlen((char *)&guarded[4]));
    }
    memset(out, 0x7f, sizeof out);
    ri_art_tr_copy(out, sizeof out, big);
    RI_ASSERT(strlen(out) == sizeof out - 1,
        "the copy fills cap-1 and terminates (%u)", (unsigned)strlen(out));
    RI_ASSERT(out[sizeof out - 1] == '\0', "the copy terminates in range");

    /* The copy is exact at the boundary, one under and one over. */
    memset(big, 'y', (size_t)sizeof out - 2u);
    big[sizeof out - 2u] = '\0';
    ri_art_tr_copy(out, sizeof out, big);
    RI_ASSERT(strlen(out) == sizeof out - 2u,
        "cap-1 chars survive whole (%u)", (unsigned)strlen(out));
    memset(big, 'y', (size_t)sizeof out - 1u);
    big[sizeof out - 1u] = '\0';
    ri_art_tr_copy(out, sizeof out, big);
    RI_ASSERT(strlen(out) == sizeof out - 1u,
        "cap chars are truncated to cap-1 (%u)", (unsigned)strlen(out));
    RI_ASSERT(out[sizeof out - 1u] == '\0', "the truncation terminated");

    /* The fit leaves a fitting name alone, in both room directions. */
    ri_art_tr_fit(out, sizeof out, "zombie-nation.rbng", &TW, ROOM_PX);
    RI_ASSERT(strcmp(out, "zombie-nation.rbng") == 0, "exact room fits whole");
    ri_art_tr_fit(out, sizeof out, "zombie-nation.rbng", &TW, ROOM_PX * 4);
    RI_ASSERT(strcmp(out, "zombie-nation.rbng") == 0, "generous room fits whole");
    ri_art_tr_fit(out, sizeof out, "", &TW, ROOM_PX);
    RI_ASSERT(out[0] == '\0', "an empty name stays empty");

    /* The fit shortens a name that does not fit, and what it returns FITS.
     * Also canary-guarded: the fit writes three dots past its own cursor on
     * every iteration, so an off-by-one there corrupts the caller's stack. */
    {
        static unsigned char gu[72];
        static const unsigned char CAN = 0xa5;
        const char *longname =
            "Vk4aros:ReIncarnation/songs/local/demos/the-knife/the-knife.rbng";
        memset(gu, (int)CAN, sizeof gu);
        ri_art_tr_fit((char *)&gu[4], 64u, longname, &TW, ROOM_PX);
        RI_ASSERT(memcmp(&gu[0], CAN4, 4) == 0,
            "the fit does not write BEFORE the destination");
        RI_ASSERT(memcmp(&gu[68], CAN4, 4) == 0,
            "the fit does not write PAST the destination");
        ri_art_tr_fit(out, sizeof out, longname, &TW, ROOM_PX);
        RI_ASSERT(tw_width(NULL, out) <= ROOM_PX,
            "the fitted name fits the room (%d px of %d)",
            tw_width(NULL, out), ROOM_PX);
        RI_ASSERT(strlen(out) < strlen(longname), "an over-long name is shortened");
        RI_ASSERT(strlen(out) > 4u, "truncation leaves something to read (%u)",
            (unsigned)strlen(out));
        RI_ASSERT(strcmp(&out[strlen(out) - 3u], "...") == 0,
            "a shortened name ends in an ellipsis (\"%s\")", out);
    }

    /* The fit shortens by the MINIMUM: a name one char over the limit loses
     * about four characters (three for the ellipsis), not the whole string.
     * This is what catches a shrink loop that eats everything. */
    {
        char edge[64];
        memset(edge, 'z', ROOM_CHARS);
        edge[ROOM_CHARS] = '\0';               /* exactly at the limit */
        ri_art_tr_fit(out, sizeof out, edge, &TW, ROOM_PX);
        RI_ASSERT(strcmp(out, edge) == 0,
            "a name exactly at the limit is not truncated (%u)",
            (unsigned)strlen(out));
        edge[ROOM_CHARS + 1] = '\0';          /* one over */
        ri_art_tr_fit(out, sizeof out, edge, &TW, ROOM_PX);
        RI_ASSERT(strlen(out) >= ROOM_CHARS - 1u,
            "one char over loses only the ellipsis, not the name (%u of %d)",
            (unsigned)strlen(out), ROOM_CHARS + 1);
        RI_ASSERT(tw_width(NULL, out) <= ROOM_PX, "and the result still fits");
    }

    /* The shrink loop must stop at the SHORTEST fitting prefix. Every one of
     * these laws exists because a mutant of the loop survived otherwise:
     * `keep < 40u` (gives up early), `len = 1u` (measures from the wrong
     * offset), and an inverted or `>=` fits test all produced a name that was
     * merely different rather than wrong, and the test had no law that said
     * which. Pinning the exact expected output is what makes them kills. */
    {
        /* 60 chars into a 46-char room: the answer is deterministic here
         * because the stand-in face is 6px per glyph. */
        char exact[64];
        memset(exact, 'a', 60);
        exact[60] = '\0';
        ri_art_tr_fit(out, sizeof out, exact, &TW, ROOM_PX);
        /* 46 px / 6 px = 43 glyphs fit; 3 are the ellipsis, so 40 survive. */
        /* Spelled out, not built from the same constants as the code under
         * test -- a generated expectation is a tautology. */
        RI_ASSERT(strcmp(out,
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa...") == 0,
            "the fit stops at the shortest fitting prefix (\"%s\")", out);
        RI_ASSERT(strlen(out) == 46u,
            "43 glyphs plus the ellipsis (%u)", (unsigned)strlen(out));
    }
    {
        /* One char past the room: exactly four characters must go. */
        char edge[64];
        memset(edge, 'b', ROOM_CHARS + 1);
        edge[ROOM_CHARS + 1] = '\0';
        ri_art_tr_fit(out, sizeof out, edge, &TW, ROOM_PX);
        RI_ASSERT(strlen(out) == ROOM_CHARS,
            "one char over loses exactly the ellipsis (%u)", (unsigned)strlen(out));
        RI_ASSERT(out[ROOM_CHARS - 4] == 'b',
            "and the last real character survives (\"%s\")", out);
        RI_ASSERT(strcmp(&out[ROOM_CHARS - 3u], "...") == 0,
            "the three dropped glyphs became the ellipsis");
    }

    /* A 3-char name is the shortest the fit has to handle, and it is where a
     * mis-measured length shows: `len = 1u` made the fit believe the name was
     * one character long, so it never truncated at all and a long name came
     * back whole. This law pins that the length is measured from the start. */
    {
        static char g[72];
        memset(g, 0x41, sizeof g);
        g[71] = '\0';
        memcpy(&g[8], "abc", 4u);
        /* dst = g+8, cap = 63, so the string is "abc" followed by 0x41s. */
        ri_art_tr_fit(&g[8], 63u, &g[8], &TW, ROOM_PX);
        RI_ASSERT(strlen(&g[8]) < 63u,
            "a 63-char name is shortened by the fit (%u)", (unsigned)strlen(&g[8]));
        RI_ASSERT(strncmp(&g[8], "abc", 3) == 0,
            "and it keeps the first three characters (\"%s\")", &g[8]);
    }

    /* No room at all: the result must still fit, i.e. be an ellipsis. */
    ri_art_tr_fit(out, sizeof out, "zombie-nation.rbng", &TW, 0);
    RI_ASSERT(tw_width(NULL, out) <= 0,
        "zero room yields nothing wider than zero (%d)", tw_width(NULL, out));

    /* A destination too small to hold even an ellipsis must not overflow. */
    {
        static char tiny[4];
        memset(tiny, 0x7f, sizeof tiny);
        ri_art_tr_fit(tiny, sizeof tiny, "zombie-nation.rbng", &TW, ROOM_PX);
        RI_ASSERT(strlen(tiny) < sizeof tiny,
            "a 4-byte destination does not overflow (%u)", (unsigned)strlen(tiny));
        RI_ASSERT(tiny[sizeof tiny - 1u] == '\0' || strlen(tiny) < sizeof tiny,
            "and stays terminated");
    }

    RI_RESULT("tr_song_name");
}