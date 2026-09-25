# Song track (second §12.9 sub-slice) — design

**Date:** 2026-09-25. **Status:** owner-approved §§1–4 in chat;
**substantially revised after E1 pass (manual quotes below) — needs
owner re-approval before any plan.**
**Scope:** dense pattern-selection track, downbeat capture, change
emission at measure lines, measure edits, init-from-pattern, STRK
codec, end-of-song rule. Explicitly OUT: automation lanes (32nd grid +
exclusions noted for that slice), streaming/pattern-end changeover
quantization, GUI song editor, tempo editing.
**Fidelity order:** ReBirth RB-338 Owner's Manual (E1; this repo's
`/tmp/rb201-manual.pdf`, ReBirth 1.0 `pdf.book`, FrameMaker 5.5).
Page numbers are the manual's printed numbers.

## 0. E1 evidence (verbatim, located by grep in the PDF text)

- Pattern-end effect (p. 20, "Pattern Changes take Effect on the Next
  Downbeat"): *"When you select a new Pattern, the change doesn't take
  effect until the end of the Pattern. ... this is true for each
  individual Pattern, regardless of its length."*
- Song authority (p. 72): *"you can switch Patterns manually, but when
  a Pattern change appears in the Song, this will be effectuated
  immediately."* — authority is immediate (the song overrules the
  hand); sounding follows the p. 20 rule.
- Downbeat capture (p. 84, "About Recording Pattern changes vs.
  Control Changes"): *"Pattern changes are only recorded on downbeats
  (the beginning of a measure) while control changes are recorded on
  each 32nd note."*
- Always-999 (p. 77, "Creating an 'End' for the Song"): *"A song is
  actually always 999 bars ... Playback always continues up until bar
  999, unless you stop it."* Silence at the end is the user's job
  (silent patterns / deactivated sections), never the engine's.
- Loop repeats (p. 73): *"You can set up and use the Loop to repeat a
  section, infinitely."*
- Init commands (pp. 77, 83): *"Initialize Song from Pattern mode"*
  (clears the song) and *"Initialize Loop from Pattern Mode"*
  (fills the loop bars, clears the rest of the loop).
- Measure edits (p. 85): Cut/Copy Loop + Paste at Song Position /
  Paste Replace at Song Position (insert vs replace).
- Switches-by-number (p. 84): *"what you record in the Song are
  switches between Pattern numbers, not the actual Patterns
  themselves"* — pattern edits propagate everywhere automatically.
- Automation exclusions (pp. 72–73): Tempo, Mixer Mutes, Master Level,
  Shuffle are not automated (noted for the automation slice).

## 1. Data model (new `engine/seq/songtrack.h`)

- `RISongTrack {slot[999][4]}` — slot 0..31 per instance (0–3 =
  303A/303B/808/909). Fixed 999 bars (§0 always-999): there is NO
  length field, no short songs. Caller-owned, no allocation (~4 KB).
- Init fills slot 0 everywhere (= single pattern looping = today's
  playback, bit-identical).
- `selected(bar, instance)`: bar clamps into `[0, 998]`; instance ≥ 4
  returns slot 0 (fail-closed).
- Capture writes only through `ri_track_capture(track, bar, instance,
  slot)` with the same clamps. Downbeat law (§0): the CALLER passes
  the downbeat (real-time flips quantize GUI-side); the model writes
  exactly the given bar, total and testable.
- Transport seeks take `song_bars = 999` (constant
  `RI_SONG_BARS`); the track owns no length because there is none.

## 2. Emission (recorded truth, not sounding truth)

- At each measure line crossed in a render window, emit
  PATTERN_CHANGE per instance whose slot differs from the previous
  measure (device = instance, value = slot, sample = measure-start
  tick through `ri_map_tick`). Range start always emits the four
  current slots (establishes downstream state).
- Authority is immediate (§0 p. 72: the song overrules manual
  selection at once); SOUNDING changeover at pattern end (p. 20)
  belongs to the streaming slice, which owns per-section loop phase.
  This emitter marks downbeat selections — never the sounding switch.
- Pure function over (track, banks, bar range, map) → events;
  cap-guarded like existing emitters (later bars drop first).
- Loop wrap (E1 p. 73: loop repeats infinitely): the emitter takes
  the loop `(on, start_bar, len_bars)` as read-only inputs. When a
  bar range crosses the loop end, iteration wraps to the loop start
  and change detection continues against the loop-end bar's slots
  (a wrap that lands on identical slots emits nothing extra). Loop
  OFF (or a range that never touches the loop) behaves exactly as
  the unwrapped path. Loop ON means bar 999 is unreachable, so
  end-of-song never fires mid-loop.
- End of song: `ri_song_ended(bar_now)` (`bar_now >= 999`) tells the
  player to stop (E1: playback continues to 999 unless stopped). The
  engine never invents silence — silent tails are user data (§0).

## 3. Track edits (pure, E1 pp. 77–85)

- `ri_track_init_song(track, slots4)` — fill all 999 bars from the
  four current pattern-mode selections (clears everything first).
- `ri_track_init_loop(track, slots4, start_bar, len_bars)` — fill the
  loop bars from selections, CLEAR the rest of the loop (E1 p. 83:
  "The rest of the loop is cleared completely"), leave bars outside
  the loop untouched.
- Measure clipboard (caller-owned `RITrackClip {len, slot[len][4]}`,
  len ≤ 999): `ri_track_cut/copy(track, start, len, clip)` — cut
  REMOVES the range (E1 p. 85 "Removing Measures"): the tail shifts
  left to close the gap and the freed end fills with slot 0 (neutral;
  E1's silence practice stays user data). Copy leaves the track
  untouched. `ri_track_paste(track, at, clip)` inserts (shifts the
  tail right, drops overflow past 999); `ri_track_paste_replace`
  overwrites in place. All clamp to the song; nothing ever grows —
  the song is always 999.

## 4. Codec (`STRK`, v1.1)

- Body: exactly 999×4 = 3996 slot bytes, row-major (bar, then
  instance). Exact-length check (≠ 3996 rejects); any slot > 31
  rejects. Minor-0 files never carry STRK (BANK rule precedent).
- v1.0 files (no STRK): slot 0 everywhere — playback identical to
  today by construction.

## 5. Testing

- New `t59_songtrack`: selection (tail = last selection, clamp,
  instance ≥ 4), capture writes-exactly + pattern-mode path never
  calls capture (gating is caller-side; test asserts the model writes
  only what it is told), emission at measure lines with exact
  samples + range-start establishment + steady silence, loop wrap
  (crossing the loop end re-emits only real changes vs the loop-end
  bar; loop ON never reaches 999), init
  song/loop (incl. rest-of-loop cleared, outside untouched),
  cut/copy/paste(-replace) incl. close-gap shift, slot-0 tail fill,
  and insert-overflow truncation,
  end-of-song (bar 999 stops, 998 plays), STRK round-trip + each
  reject + v1.0 default, 999 boundary everywhere.
- Existing suites green unmodified. Full `ri_audit.sh` 0/0 (t59 wired
  beside t58).
