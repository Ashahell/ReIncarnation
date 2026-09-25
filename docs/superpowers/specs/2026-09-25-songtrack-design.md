# Song track (second §12.9 sub-slice) — design

**Date:** 2026-09-25. **Status:** revised after E1 + transport-law review — needs owner re-approval before any plan.
**Scope:** dense pattern-selection track, downbeat capture, change emission at measure lines, measure edits, init-from-pattern, STRK codec, end-of-song rule. Explicitly OUT: automation lanes (32nd grid + exclusions noted for that slice), streaming/pattern-end changeover quantization (§12.9c), GUI song editor, tempo editing.
**Fidelity order:** ReBirth RB-338 Owner's Manual (E1; this repo's `/tmp/rb201-manual.pdf`, ReBirth 1.0 `pdf.book`, FrameMaker 5.5). Page numbers are the manual's printed numbers.

## 0. E1 evidence (verbatim, located by grep in the PDF text)

- Pattern-end effect (p. 20, "Pattern Changes take Effect on the Next Downbeat"): *"When you select a new Pattern, the change doesn't take effect until the end of the Pattern. ... this is true for each individual Pattern, regardless of its length."*
- Song authority (p. 72): *"you can switch Patterns manually, but when a Pattern change appears in the Song, this will be effectuated immediately."* — authority is immediate (the song overrules the hand); sounding follows the p. 20 rule.
- Downbeat capture (p. 84, "About Recording Pattern changes vs. Control Changes"): *"Pattern changes are only recorded on downbeats (the beginning of a measure) while control changes are recorded on each 32nd note."* Real-time flips quantize forward to the next bar (p. 76: *"You should hit the Pattern buttons before the downbeat. The program will then move it to the exact bar."*).
- Always-999 (p. 77, "Creating an 'End' for the Song"; p. 73): *"A song is actually always 999 bars ... Playback always continues up until bar 999, unless you stop it"* / *"All Songs can be up to 999 measures in Length and playback will continue up to that point, unless you stop it before."* Silence at the end is the user's job (silent patterns / deactivated sections), never the engine's.
- Loop repeats (p. 73): *"You can set up and use the Loop to repeat a section, infinitely."*
- Init commands (pp. 75–77, 83, 176–177): *"Initialize Song from Pattern mode"* (clears the song, then *"Events are inserted at the beginning of the Song so that it plays exactly like Pattern mode"* — song-level events, not just pattern slots) and *"Initialize Loop from Pattern Mode"* (*"all measures inside the Loop are filled with the settings currently made in Pattern mode"* — patterns AND knob/control settings; *"clears all the Pattern changes and knob recordings currently inside the Loop"*).
- Measure edits (p. 85): Cut/Copy Loop + Paste at Song Position / Paste Replace at Song Position (insert vs replace). Paste (insert) lengthens the song narrative; Replace overwrites in place.
- Switches-by-number (p. 84): *"what you record in the Song are switches between Pattern numbers, not the actual Patterns themselves"* — pattern edits propagate everywhere automatically.
- Automation exclusions (pp. 72–73): Tempo, Mixer Mutes, Master Level, Shuffle are not automated (noted for the automation slice).
- Step vs real-time writing (p. 76): step mode punches a list without playback; real-time writes while playing. Multi-pass per section is the documented workflow (record section 1, rewind, record section 2, ...).

## Song laws (normative — implementation, tests, and integration must preserve them)

### Ownership
* `RISongTrack` owns exactly the 999×4 slot grid (pattern NUMBERS, never pattern data). Pattern banks own the patterns. The song overrules the hand; neither owns the other.
* `transport.h` owns cursor/geometry/loop laws (`RI_SEQ_MAX_BARS`, `ri_ppq_or_default`, clamp, staging). This slice REUSES them — never redefines bars, counts, or the 999 ceiling.
* `songtrack.h` depends on `transport.h` for geometry constants only (one direction: songtrack → transport; transport never includes songtrack). No mutable static state in `songtrack.c`.
* The emitter owns NOTHING: it reads (track, loop, bar range, map) and returns events. Cursor ownership stays in transport; sounding changeover stays in the streaming slice.

### Cursor
* The authoritative cursor is ticks (transport law). Bars enter this slice only as 0-based indices `0..998`; bar 999 is the END BOUNDARY, never a valid start — same law as transport seeks.
* Samples come only from `ri_map_tick()`. This slice never converts ticks↔samples itself; it hands measure-start ticks to the map.
* Capture never moves the cursor. Emission never moves the cursor. Display never moves the cursor.

### Geometry
* The song is ALWAYS 999 bars (E1 §0). There is no length field, no short song, no tail pointer. `RI_SONG_BARS === RI_SEQ_MAX_BARS` (single source in `transport.h`; both name the full-song count 999 — the `+1` sentinel form lives only in loop staging as `MAX_BARS + 1` and is never a bar count).
* Valid bar starts: `0..998`. `ri_song_ended(bar_now)` (`bar_now >= 999`) is the ONLY end test.
* Policy-vs-primitive split (transport law, reused verbatim): `selected/capture/emit` normalize indices BEFORE touching storage. No silent modulo, no wraparound reads.

### Capture
* Downbeat law: only downbeats are writable. The MODEL writes exactly the bar it is told (`ri_track_capture` is total on `0..998`); QUANTIZATION (real-time flip → next bar) is caller-side (GUI/record path), never inside the model. Test the model and the quantizer separately.
* Capture is per (bar, instance) — one slot per call. Multi-section passes are N calls, one per instance (E1 multi-pass workflow, p. 76).
* RECORD-gating lives OUTSIDE the model: the record path calls capture only while `state == RECORD`; the model itself is gate-blind and testable without a transport. A test asserts the pattern-mode path never calls capture (caller discipline, not model magic).
* Instance ≥ 4 is fail-closed (read slot 0, write ignored). Bar ≥ 999 is fail-closed (read slot 0, write ignored) — never wrap, never clamp-up.

### Emission
* Recorded truth, NOT sounding truth: the emitter marks which slot is SELECTED at each measure line. Which pattern is SOUNDING (pattern-end rule, p. 20) belongs to the streaming slice, which owns per-section loop phase.
* Range start always emits the four current slots (establishes downstream state, even with no change).
* Within a range, emit only REAL changes vs the previous measure's slots (steady silence emits nothing).
* Loop wrap: the loop `(on, start, len)` arrives as READ-ONLY inputs. Crossing the loop end wraps iteration to the loop start; change detection continues against the loop-end bar's slots. Loop ON means bar 999 is unreachable — end-of-song never fires mid-loop.
* Cap pressure drops LATER bars first (existing emitter convention), deterministic.
* The emitter is TWO layers, never one: `ri_track_emit_measure(t,
  bar, prev[4], force, ev, n)` emits only the slots that differ from
  `prev[]` (or all four when `force` is nonzero) — it knows no ranges,
  no loops, no cursor. The range walker owns the previous-slot cache
  and the loop arithmetic and calls it per bar. The hot path stays
  tiny and testable in isolation.
* Emission never moves the cursor and never reads transport state
  (cursor ownership stays in transport, stated absolutely).
* Emission ranges are left-closed and truncated before the end
  boundary: a request that would emit bar 999 is cut, and
  `ri_song_ended` is the ONLY stop signal (no phantom bar 999, no
  silent failure to stop).
* Slot 0 is a first-class pattern number, absolutely: neutral fill
  means "select pattern 0", never "leave the previous selection" or
  "emit nothing". Emission fires on a change to or from 0 exactly
  like any other change. No future optimization may treat 0 as a
  sentinel.

### Edits
* Init-song fills all 999 from the four live selections AND stamps the song-open event set (E1 p. 75: knob/control settings travel with the slots — the automation slice owns the knob half; this slice owns the slot half and documents the seam).
* Init-loop fills the loop bars, CLEARS the rest of the loop, leaves outside untouched (E1 pp. 83/176).
* Cut REMOVES (tail shifts left, freed end fills slot 0); copy leaves untouched; paste INSERTS (tail shifts right, overflow past 999 drops); paste-replace OVERWRITES in place. Nothing ever grows — always 999.
* Freed/initialized bars fill with slot 0 = NEUTRAL SELECTION ("keep playing whatever the bank holds at slot 0"), NOT silence. Silence is user data (silent patterns / muted sections); the engine never invents it.

### Codec
* STRK body is exactly `RI_RBNG_STRK_BYTES` = 999×4 = 3996 bytes (row-major bar→instance, derived from the track constants so it cannot drift). `≠3996` rejects; slot `>31` rejects. Minor-0 files never carry STRK (BANK precedent); a missing STRK means slot 0 everywhere (today's playback, bit-identical).
* Writer rule: the chunk is written only when the track is NOT all-zero (an empty track keeps the legacy byte-identical shape), and the file minor becomes 1 when banks OR a non-empty track are present — a track without banks written as minor 0 would be unreadable by its own reader.

### Proof discipline
Every invariant has (1) one named enforcement point, (2) executable tests, (3) at least one boundary test, (4) at least one mutation test.

## 1. Data model (new `engine/seq/songtrack.h`)

- `RISongTrack {slot[999][4]}` — slot 0..31 per instance (0–3 = 303A/303B/808/909). Fixed 999 bars (§0 always-999): NO length field, no short songs. Caller-owned, no allocation (~4 KB). Depends on `transport.h` for `RI_SEQ_MAX_BARS` only.
- Init fills slot 0 everywhere (= single pattern looping = today's playback, bit-identical).
- `selected(bar, instance)`: bar `>= 999` fail-closed → slot 0; instance ≥ 4 fail-closed → slot 0. Normalizes BEFORE storage (policy-vs-primitive law).
- Capture writes only through `ri_track_capture(track, bar, instance, slot)` with the same fail-closed clamps. Downbeat law (§0 p. 84 + p. 76): the CALLER passes the downbeat (real-time flips quantize to the NEXT bar GUI-side — `next_bar = bar_at_tick(cursor) + 1`, clamped to 998); the model writes exactly the given bar, total and testable.
- Transport seeks take `song_bars = RI_SONG_BARS` (=== `RI_SEQ_MAX_BARS`); the track owns no length because there is none.

## 2. Emission (recorded truth, not sounding truth)

- Measure emission (pure and dumb):
  `ri_track_emit_measure(t, bar, prev[4], force, ev, n)` emits
  PATTERN_CHANGE (device = instance, value = slot, sample =
  measure-start tick through `ri_map_tick`) only for slots that
  differ from `prev[]`, or all four when `force` is nonzero. It knows
  no ranges, no loops, no cursor.
- Range walker (owns the cache and the arithmetic): holds the
  previous-slot cache, calls the measure function per bar, performs
  loop wrap (continuing change detection against the loop-end bar's
  slots), and forces the range-start establishment. Loop ON never
  reaches 999, so end-of-song never fires mid-loop.
- Authority is immediate (§0 p. 72: the song overrules manual selection at once); SOUNDING changeover at pattern end (p. 20) belongs to the streaming slice, which owns per-section loop phase. This emitter marks downbeat selections — never the sounding switch.
- Pure function over (track, banks, bar range, map) → events; cap-guarded like existing emitters (later bars drop first).
- Loop wrap (E1 p. 73: loop repeats infinitely): the emitter takes the loop `(on, start_bar, len_bars)` as read-only inputs. When a bar range crosses the loop end, iteration wraps to the loop start and change detection continues against the loop-end bar's slots (a wrap that lands on identical slots emits nothing extra). Loop OFF (or a range that never touches the loop) behaves exactly as the unwrapped path. Loop ON means bar 999 is unreachable, so end-of-song never fires mid-loop.
- End of song: `ri_song_ended(bar_now)` (`bar_now >= 999`) tells the player to stop (E1: playback continues to 999 unless stopped). The engine never invents silence — silent tails are user data (§0).

## 3. Track edits (pure, E1 pp. 75–85, 176–177)

- `ri_track_init_song(track, slots4)` — fill all 999 bars from the four current pattern-mode selections (clears everything first). Documents the seam: the automation slice stamps the knob/control half of the E1 p. 75 promise; this function owns the slot half.
- Read-only run view (never stored; computed on demand for the GUI
  and future phrase-level editing — the seam that keeps regions from
  ever becoming a linked list): `RITrackRun {start_bar, len_bars,
  slots[4]}` + `ri_track_runs(t, out[], cap)` run-length-encodes the
  dense grid. All mutation stays on the dense array; the view is a
  projection and cannot disagree with it (pinned by the replay
  property in §6).
- `ri_track_init_loop(track, slots4, start_bar, len_bars)` — fill the loop bars from selections, CLEAR the rest of the loop (E1 pp. 83/176: rest-of-loop cleared INCLUDING knob recordings — the automation slice clears its lane range in the same call path), leave bars outside the loop untouched.
- Measure clipboard (caller-owned `RITrackClip {len, slot[len][4]}`, len ≤ 999): `ri_track_cut/copy(track, start, len, clip)` — cut REMOVES the range (E1 p. 85 "Removing Measures"): the tail shifts left to close the gap and the freed end fills with slot 0 (neutral selection, NOT silence — §laws). Copy leaves the track untouched. `ri_track_paste(track, at, clip)` inserts (shifts the tail right, drops overflow past 999); `ri_track_paste_replace` overwrites in place. All clamp to the song; nothing ever grows — the song is always 999.

## 4. Codec (`STRK`, v1.1)

- Body: exactly `RI_RBNG_STRK_BYTES` = 999×4 = 3996 slot bytes, row-major (bar, then instance), no trailing pad (3996 is even). Exact-length check (≠ 3996 rejects); any slot > 31 rejects. Minor-0 files never carry STRK (BANK rule precedent). Writer omits the chunk when the track is all-zero and raises the minor when banks or a non-empty track exist (§laws); `ri_track_is_empty` is the single enforcement point of "all-zero".
- v1.0 files (no STRK): slot 0 everywhere — playback identical to today by construction.

## 5. Open items (ledger rows before they lock)

| Item | Why open | How it closes |
|------|----------|---------------|
| Init-song knob/control stamp (E1 p. 75 second half) | this slice owns slots; knobs belong to automation | Automation slice defines the control-event stamp; this doc records the seam (§3) |
| Init-loop knob clearing (E1 p. 176: clears knob recordings in-loop) | same seam, loop-scoped | Same slice, loop-scoped clear |
| Sounding changeover at pattern end (p. 20) | needs per-section loop phase, not owned here | Streaming slice (§12.9c) owns it; this emitter marks selections only |
| `RI_EV_TRANSPORT` emission on track transitions | §8 reserves the type; transport slice defers here | This slice emits PATTERN_CHANGE only; TRANSPORT decisions ride the streaming slice |

## 6. Testing

- New `t59_songtrack`: selection (fail-closed reads: bar 999/5000 → slot 0, instance 4/255 → slot 0; writes ignored, neighboring bars untouched), capture writes-exactly + quantizer test (flip at bar 5 mid-measure → bar 6; flip exactly on the downbeat → that bar; flip at bar 998 → 998, never 999) + pattern-mode path never calls capture, emission at measure lines with exact samples + range-start establishment + steady silence, loop wrap (crossing the loop end re-emits only real changes vs the loop-end bar; loop ON never reaches 999), init song/loop (incl. rest-of-loop cleared, outside untouched, knob-seam documented), cut/copy/paste(-replace) incl. close-gap shift, slot-0 tail fill, and insert-overflow truncation, end-of-song (bar 999 stops, 998 plays), STRK round-trip + each reject + v1.0 default, 999 boundary everywhere.
- Property loops: all-999 round-trip (write slot `b % 32` per bar, read back); emission sweep (alternating slots every bar over 0..999 emits exactly the change bars + range start).
- Replay property: for any sequence of captures and edits, the dense
  array is the only source of truth — replaying the emission events
  (apply each PATTERN_CHANGE to a 4-slot cursor starting from the
  range-start establishment) reproduces `selected()` at every bar.
- View property: the run list re-expands to the dense grid exactly;
  adjacent runs always differ in at least one slot.
- Mutation proofs (3, recorded with failure lines): (a) `≠3996` → `>=3996` must FAIL the reject case; (b) replace the wrap formula with `bar = loop->start_bar` must FAIL the phase test (count 8 → 10 events, buggy → 6); (c) `>= 999` → `> 999` in `selected` must FAIL the bar-999 case.
- Existing suites green unmodified. Full `ri_audit.sh` 0/0 (t59 wired beside t58).
