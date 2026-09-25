# Transport state machine (first §12.9 sub-slice) — design

**Date:** 2026-09-25. **Status:** owner-approved (§§1–5 in chat).
**Scope:** transport states/transitions, bar position mapping, loop
model, record/display state. Explicitly OUT: song track content,
pattern-change emission, automation capture, GUI widgets (sibling
lane), tempo editing.
**Fidelity order:** ReBirth RB-338 2.0.1 Owner's Manual (E1, pp.
145–146); unverified choices are ledger rows.

## 1. States + transitions (E1 pp. 145–146)

States: STOPPED / PLAYING / RECORD. Stored in `RISeq` beside its
clock/loop/snapshot logic (one position owner). All transitions are
pure functions on one cursor unit (ticks; bar helpers in §2 convert
for display/seek only). RECORD dominates PLAYING: `state == RECORD`
implies sounding on (RECORD is PLAYING + capture); there is no
separate PLAYING flag underneath. Ownership is strict:

- `RITransport` owns `state` + `clicks` only. `loop_on` lives in
  `RILoop` (song geometry, §3); `armed` lives with the record target
  (song-track slice, §4). Transport functions never read or write
  either; loop/armed arrive only as read-only inputs where named.
- **Stop-click law (invariant):** `clicks` is meaningful only while
  STOPPED. `clicks == 0` whenever `state != STOPPED`. Any call that
  leaves STOPPED zeros `clicks`; any seek zeros `clicks`.

- Play from STOPPED: continue at the held cursor (E1: Play = continue).
- Stop clicks: 1st → STOPPED, cursor held (`clicks = 1`); a further
  Stop while STOPPED with `clicks == 0` holds the cursor and arms
  (`clicks 0→1` — the visible jump comes on the 2nd press); 2nd
  → cursor to loop start (song start when loop is off); 3rd → song
  start (`clicks` back to 0). Any Play resets the click count.
  DELIBERATE EXTENSION (not E1 — the manual ends at the 3rd click):
  a 4th consecutive Stop restarts the sequence as click 1 (cursor held).
  Without it the 4th press is undefined; restarting keeps every press
  meaningful and the machine total.
- Rew/FF: ∓10 bars per press, clamped to valid starts [0, song_bars - 1] (empty song → tick 0; never the end boundary);
  held-key repeat lives GUI-side; the step is the fixed constant 10.
  Any seek (Rew/FF) zeroes the stop-click count (cursor intent
  replaces the stop sequence, per the stop-click law).
- Record button: from STOPPED → RECORD at the held cursor (plays and
  records); from PLAYING → punch in (`state = RECORD`, cursor
  unchanged); from RECORD → punch out (back to PLAYING). Stop from
  RECORD → STOPPED (click count consumed as §1; the law re-zeroes it
  on the STOPPED→PLAYING/RECORD exit, so no stale count survives).
  Arming (target select) never moves the cursor and never changes
  `state`; only the Record button changes punch state.

Full matrix (one place; `t` = `RITransport *`, `c` = cursor ticks;
loop start/end arrive as read-only tick inputs — never stored in `t`):

| From → input | To | Cursor | Clicks |
|--------------|----|--------|--------|
| STOPPED + Play | PLAYING | held | 0 |
| STOPPED + Record | RECORD | held | 0 |
| PLAYING + Stop | STOPPED | held (click 1) | 1 |
| RECORD + Stop | STOPPED | held (click 1) | 1 |
| PLAYING + Record | RECORD (punch in) | unchanged | 0 (law: not STOPPED) |
| RECORD + Record | PLAYING (punch out) | unchanged | 0 (law: not STOPPED) |
| STOPPED + Stop (clicks 0→1) | STOPPED | held | 1 |
| STOPPED + Stop (clicks 1→2) | STOPPED | loop start (song start if loop off) | 2 |
| STOPPED + Stop (clicks 2→strip) | STOPPED | song start | 0 (sequence restarts; a 4th Stop re-arms as click 1 — deliberate extension, not E1) |
| any + Rew/FF ±10 bars | state unchanged | clamped seek | 0 |

## 2. Position mapping (tempo-map aware)

Position lives in samples (existing clock) + ticks. Bar = ppq*4 ticks
(4/4 fixed — ReBirth has no time signature, E1). The ppq in every
bar query is the live `RISeq.ppq`, not a constant (defaults to 96,
P-20). Engine queries are 0-based and pure — they never see the
panel struct:

- `ri_seq_bar_at_tick` (tick → 0-based bar, floor: `tick / (4*ppq)`),
- `ri_seq_tick_of_bar` (bar → start tick, `bar * 4 * ppq`).

Display is a one-way projection, never engine input:
`ri_seq_bar_display` takes a tick and returns `{bar, beat,
sixteenth}` all 1-based for the panel only
(beat = `(tick_in_bar / ppq) + 1`, sixteenth =
`((tick_in_bar % ppq) / (ppq/4)) + 1`; ppq/4 is exact since ppq is a
multiple of 4 in every map the engine accepts). No engine path reads
`RIBarPos`. Segment-crossing
bars resolve in the tick domain; the single rounding
happens at sample conversion (clock convention). Loop bounds store as
bars and resolve through the same queries. Cursor unit rule: the
held cursor is ticks; samples come from `ri_map_tick` only.

## 3. Loop model

`{on, start_bar, len_bars}`, 0-based bars (`start_bar + len_bars ≤
999`, E1); `len_bars ≥ 1` (zero length = loop off, never an empty
region). The loop struct lives with the sequence (`RISeq`), not the
transport. Clamp law (`[0, 999]` bars ceiling on all public entry points): seeks and loop edits clamp into
`[0, song_bars]`; a loop edit whose `start_bar + len_bars` would pass
the song end (or 999) clamps `len_bars` down to fit — geometry never
exceeds the song. If the song shortens under a live loop, the loop
re-clamps the same way at the next bar line (§5). The resolved sample
region feeds the existing `RiSeqLoopPos` unchanged. Loop edits
quantize to bar starts.

## 4. Record + display

`armed` lives with the record target (song-track slice owns it), not
with the transport. Law: arming survives the stop-click sequence and
seeks — only the Record button sets or clears punch (`state`), never
`armed`. Pattern-change capture arrives with
the song-track slice — this slice stores `armed` and reads
`state == RECORD` (punch) as its capture gate. Display
= `{bar, beat, sixteenth}` struct (all 1-based projection for the
panel only; internal bar math stays 0-based — convert only at
display, §2). Past-end display law: the caller clamps the tick to the last valid bar start (`song_bars`, itself ≤ 999) — never blank, never 1000+.

## 5. Open items (ledger rows before they lock)

Decided (locked by this review — no longer open):

- Loop-edit timing while PLAYING: **quantize to the next bar line.**
  Immediate application races the audio buffer and glitches; ReBirth's
  spirit is musical. The pending edit stages and swaps at the next bar
  start (same mechanism as the song-shortening re-clamp in §3).
- Record-armed persistence: **armed survives stop-clicks and seeks**
  (§4 law). Only the Record button touches punch.
- Bar display past song end: **clamp to the last valid bar** (§4 law).

Still open:

| Item | Why open | How it closes |
|------|----------|---------------|
| Rew/FF held-repeat rate | manual says held = continuous (p. 146); rate lives GUI-side | ReBirth: hold Rew, count bars/s; pins GUI repeat timer only, engine step stays 10 |
| `RI_EV_TRANSPORT` emission on each transition | §8 reserves the type; this slice does not specify emission | Song-track slice (§12.9b) decides payload/placement; no emit here |

## 6. API sketch (executor-defined, not ABI)

```c
/* transport.h — pure, stdint.h only, no alloc, no IO.
 * Layers are strict: transport fns own the cursor and touch only
 * RITransport; bar helpers are free functions on (tick, ppq) that
 * never see any struct; the display projection never feeds back. */
enum RI_TRANSPORT { RI_TR_STOPPED = 0, RI_TR_PLAYING = 1, RI_TR_RECORD = 2 };
/* RECORD implies sounding + capture. loop_on/armed do NOT live here. */
struct RITransport { uint8_t state; uint8_t clicks; /* clicks valid only in STOPPED, 0..2 */ };
struct RILoop { uint8_t on; uint16_t start_bar; uint16_t len_bars; }; /* song geometry; lives in RISeq */
struct RIBarPos { uint16_t bar; uint8_t beat; uint8_t sixteenth; };    /* 1-based projection; display only */
#define RI_SEQ_MAX_BARS 999u
#define RI_PPQ_DEFAULT 96u
#define RI_PPQ_MIN 4u
static inline uint32_t ri_ppq_or_default(uint32_t ppq) {
    if (ppq == 0u || ppq < RI_PPQ_MIN) return RI_PPQ_DEFAULT;
    return ppq; /* one normalization: non-zero AND divisible-by-4-safe */
}

/* Transport (own cursor + clicks; loop bounds arrive as read-only tick inputs). */
void     ri_tr_play(struct RITransport *t, uint64_t *cursor_ticks); /* continue; clicks = 0 */
void     ri_tr_stop(struct RITransport *t, uint64_t *cursor_ticks,
                    uint64_t loop_start_tick, uint64_t song_start_tick); /* 3-stop sequence */
void     ri_tr_seek_bars(struct RITransport *t, uint64_t *cursor_ticks, uint32_t ppq,
                         int32_t delta_bars, uint64_t song_bars); /* +-10 path; zeroes clicks;
                          * caller owns the bar count (no tick re-derivation) */

/* Free bar helpers (no struct visibility). */
uint64_t ri_seq_tick_of_bar(uint32_t ppq, uint64_t bar);          /* bar * 4 * ppq */
uint64_t ri_seq_bar_at_tick(uint64_t tick, uint32_t ppq);         /* tick / (4 * ppq), 0-based */

/* Display projection (one-way; never engine input). Pure function of
 * (tick, ppq) — no clamp inside; the caller clamps the tick first
 * (seek path), then projects. Past-end ticks project raw bar math. */
struct RIBarPos ri_seq_bar_display(uint64_t tick, uint32_t ppq); /* pure; caller clamps first */
```

## 7. Testing

New `t58_transport`: full transition table (§1 matrix, incl. the
3-stop sequence and the stop-click law), ±10-bar seeks with
song-bound clamps and loop re-clamp (§3), armed-survives-stop/seek
(§4), next-bar loop-edit staging (§5),
bar/tick round-trips across a 2-segment tempo map, loop wrap
positions. Display pins: tick 0 → `1.1.1`;
end of bar 0 → `1.4.4`; bar rollover → `2.1.1`; `bar_at_tick` is
the floor (quantize-down) inverse of `tick_of_bar`; ppq = 0 falls
back to 96 (clock convention); past-end ticks display the last bar
(§4 law). Existing clock/seq suites green
unmodified. Full `ri_audit.sh` 0/0.
