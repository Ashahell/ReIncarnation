/* ri_tmp.h — the ONE build root, as a string literal.
 *
 * Every build artefact lives under ONE root: the objects, the run
 * scaffolds, the probe artefacts, the portable build. Two lanes on one
 * machine therefore share nothing, and a lane sets its own folder and
 * nothing else moves.
 *
 * RI_TMP_ROOT is that root, and it is a STRING LITERAL so that a source can
 * build a path by concatenating it:
 *
 *     RI_TMP_ROOT "/run/t6/song.rbng"
 *
 * The build scripts pass -DRI_TMP_ROOT="<the lane's folder>", which
 * OVERRIDES the default here. Without that flag this default applies, so a
 * hand-run compile of a source that writes a scratch file still builds, and
 * still writes where the default scripts would put it — the same place, not
 * a second one.
 *
 * 2026-10-20: 22 sources take their scratch path from this macro and only
 * the scripts defined it, so every other compile of them failed with
 * "RI_TMP_ROOT undeclared" — a hand-run gcc, the usage comments in
 * levi_bench.c and drum_bench.c, and any future build system.
 */
#ifndef RI_TMP_ROOT
#define RI_TMP_ROOT "/tmp/ri"
#endif
