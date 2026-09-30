/* songscript.h — RBS song script: a line-based text source for RBNG
 * songs (songs & playlists, owner 2026-09-30). Authored by hand or by
 * tools/mid2rbs.py; tools/rbsc compiles it through the real RBNG codec,
 * so every rule of the codec (ranges, allow-listed automation) holds.
 *
 * Grammar (one statement per line; '#' starts a comment; tokens split
 * on blanks; numbers decimal unless 0x-prefixed):
 *   RBS 1                                 header, first statement
 *   TEMPO <30..300>                       default 140
 *   PPQ <24..960>                         default 96 (AUTO ticks use it)
 *   CPRG <text to end of line>            copyright / credit notice
 *   P303 <inst 0|1> <slot 0..31> <tok>..  1..16 steps, one token each:
 *        '-' rest, or <MIDI note>[a][s] (a accent, s slide); notes fold
 *        into the 303 range (36-12 .. 36+24) by octaves
 *   PDRUM <inst 2|3> <slot> <LANE> <chars> 1..16 chars: '.' off, 'x' low,
 *        'X' high, 'f' flam; lanes 808: BD SD LT MT HT RS CP CB CY OH CH,
 *        909: BD SD LT MT HT RS CP CH OH CC RC; the char count sets the
 *        pattern length
 *   PACC <inst 2|3> <slot> <chars>        accent row: '.' off, 'x' on
 *   PLEVI <slot> <len 1..16> <step> <note[,note..]>  up to 6 lanes; a
 *        later PLEVI for the same step replaces it
 *   TRACK <from> <to> <s0> <s1> <s2> <s3> <s4>  bars from..to inclusive
 *        select slots for 303A 303B 808 909 Levi; '-' leaves one as is
 *   AUTO <tick> <ctl> <val 0..127>        automation (song ppq ticks);
 *        the same (tick, ctl) twice: the later one wins
 *   SET <tick> <section.control> <val>    the same, by registry name:
 *        section = a registry token (levi, 808, 909, mix-303a, mix-levi,
 *        master, delay, dist, comp, pcf, ...) or 303a / 303b; control =
 *        its legend or group_legend, blanks as '_' ("303a.Cutoff",
 *        "909.BD_Tune", "levi.D_Type", "mix-levi.Level")
 * Unknown statements, bad numbers or ranges fail with "line N: reason".
 */
#ifndef RI_SONGSCRIPT_H
#define RI_SONGSCRIPT_H
#include <stdint.h>
#include "project/rbng.h"

/* Parse script text into s (rbng_song_init first; banks for all five
 * instances are set up, nbanks = 5). Automation lands in s->atrk, which
 * the caller provides (atrk/atrk_cap set before the call), sorted by
 * (tick, ctl). Returns 0 ok, 2 error with err filled. */
int ri_rbs_parse(const char *text, struct RISong *s, char *err, uint32_t errcap);

#endif
