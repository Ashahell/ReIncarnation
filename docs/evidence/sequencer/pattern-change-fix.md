# Pattern selection inaudible — root cause + fix (owner finding, 2026-09-27)

Owner: pattern-block changes (PAT/PATLEN/PATOFF/STEP, 60+ events in
`RIAPP-EV.LOG`) made no audible difference on the Dell.

## Root cause (two parts + one refinement)

1. **Dropped downbeats (engine).** `ri_player_block` STEP 1 skipped bar
   boundaries with `db <= tick_start || db >= tick_end`. Ticks are
   integers and render windows tile them, so a boundary coinciding with
   a window edge was skipped by BOTH adjacent windows — and with common
   buffer sizes (64/128/256 @48 kHz/120 bpm/96 ppq) EVERY boundary
   aligns: pattern selection could never take effect. Solved by
   `[tick_start, tick_end)` ownership with a split rule: pending
   sampling is edge-inclusive (audio must track) while change
   ANNOUNCEMENTS stay strictly interior (t74's R-DEFERRAL/song-end
   records hold byte-for-byte with zero test changes).
2. **One-shot grid writes (shell).** `sync_pat` captured the selection at
   the snapshot bar once; the playhead leaves the bar, so even with (1)
   only that bar would sound it. The panel now re-asserts the live
   selection every round via `ri_core_capture_sel` (no-op when stored,
   silent) — E1 pattern-mode stickiness. Arrangement-per-bar programming
   stays a song-mode feature (OPEN).

Refinement trail (recorded so the next reader trusts the shape): a plain
`<` fix broke 3 t74 event-count pins (cold sample-0 + song-end-jump
announcements); a carry-seeding alternative contradicted t74's explicit
`known == 0` pin; a carry-unknown skip would starve chained streaming
(every downbeat coincides with a block start under single-tick windows).
The split satisfies all three: audio flips, records hold, no new branches
beyond one `edge` flag.

Why it hid: all player/songtrack tests drive wide windows (boundaries
strictly inside) or static slots; t81's chunk test never changes slots;
the Step-2 owner proof exercised knobs, never pattern switches.

## Proof

- Host reproduction first: capture sel=3 at bar 2 → bars 2–7 byte-identical
  to the slot-0 control (broken); `pending_slot` frozen at 0 across 5 bars.
- `tests/unit/t96_pattern_change.c` (RED: `red-t96.txt`, 3 FAILs): unit
  edge-exact flip `[767,768)`→`[768,770)`; session one-shot sounds bar 3
  only; sticky re-capture persists (final sounding 3).
- GREEN: `PASS pattern_change`. Fix mutant == the RED run (revert → FAILs).
- Self-caught test bug: first t96 asserted the flip in the wrong bar
  (and used 4800-float arrays in an early draft) — corrected to
  bar-exact regions after tracing the wrap order.

## Verification on device: OPEN (owner)

Needs a Dell run with pattern interaction: select patterns, edit steps,
toggle off — all must be audible now. v11 rebuild + run on owner go.
