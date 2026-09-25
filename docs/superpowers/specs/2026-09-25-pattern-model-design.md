# Pattern model + RBNG v1.1 (first §12.7 sub-slice) — design

**Date:** 2026-09-25. **Status:** owner-approved §§1–5 (chat); revised by
fidelity review 2026-09-25 (§7); **owner re-approved the §7 revisions
2026-09-25 (directed implementation of the plan in chat). Implemented in
`f6b958f` (model) + `7f20572` (edit ops) + `4afcfc7` (walker) +
`1fd0d38`/`f25f2ec` (303 emit) + `e925f6e`/`0961d60` (v1.1 codec);
open items §6 unchanged.**
**Scope:** per-instance pattern banks, full step data, pure edit ops,
pattern emit, RBNG v1.1 codec. Explicitly OUT: scheduler streaming,
song track/transport (§12.9), engine 808/909 hosting, GUI step entry.

**Fidelity order** (review doc §4): ReBirth RB-338 2.0.1 behaviour is the
target (E1 = Owner's Manual, page refs below); where ReBirth is silent,
the original TB-303 / TR-808 / TR-909 decides; anything unverified is a
ledger row, never a silent choice.

## 1. Data model (new `engine/seq/pattern.h`)

- `RI_PATTERN_STEPS 16` (storage is always 16 rows — ReBirth's Shift and
  Alter operate on all 16 steps regardless of length, p. 53–54, so rows
  beyond `length` are live data, not padding), `RI_PATTERN_BANK_PATTERNS
  32` (4 banks A–D × 8, p. 147).
- Kinds: `RI_PATTERN_KIND_303 0`, `RI_PATTERN_KIND_DRUM 1`;
  `RI_PATTERN_PAYLOAD_VERSION 1`.
- `RIPattern {kind, length 1–16, payload_ver, rows[16]}`. `length` is in
  16th-note steps only (ReBirth has no scale setting; the TR-808/909
  scale/triplet modes are a Power-mode extension, not Classic).

### 1.1 303 rows — ReBirth step model (p. 153–154)

`RI303Row {key:u8, flags:u8}`

- `key` = **pitch button 0–12** (low C … high C, the 303's 13-key
  keyboard), NOT a MIDI note. Clear sets key 0 (“Low C”). The MIDI note
  is derived only at emit: `note = RI_303_BASE_NOTE + key + 12·oct`,
  with `oct` from Up/Down. `RI_303_BASE_NOTE` is one ledger constant
  (HYPOTHESIS until measured against ReBirth — §6 P-row). The section
  Tune knob (±12 semitones) is voice/panel state, not pattern data.
- `flags` (`RI_STEP_*`): `REST` (= ReBirth Note/Pause set to Pause),
  `ACCENT`, `SLIDE`, `UP 0x10`, `DOWN 0x20`. Up+Down both set = neither
  (p. 154; normalized at emit, preserved in storage so a GUI toggle
  round-trips).
- Range is therefore exactly 3 octaves + 1 key (−12 … +24 from base) —
  every representable step is playable, and the old “clamp to 0–127”
  question disappears (closes the rb303.c clamp row).
- Hardware note (for import/MIDI, not stored): the TB-303 keeps pitch and
  time in separate sequences (time step = note / tie / rest; pitches are
  consumed by note steps only). ReBirth flattens this to one row per
  step. A 303 tie maps to ReBirth as *slide on the previous step + same
  key on this step* (legato, no retrigger, zero glide distance).

### 1.2 Drum rows — 808 and 909 (p. 28–35, 148–152)

`RIDrumRow {on:u16, high:u16, flam:u16, flags:u8}` — lane bit = slot.

- **Slot order is canonical per device class** (ReBirth tap-record /
  panel order, p. 32):
  - 808: `BD SD LT MT HT RS CP CB CY OH CH` (11 slots). The switch pairs
    LT/LC, MT/MC, HT/HC, RS/CL, CP/MA are **panel state**, not pattern
    data (“whatever is programmed for this instrument slot will be
    played by the sound selected at that time”, p. 149).
  - 909: `BD SD LT MT HT RS CP CH OH CC RC` (11 instruments).
  - Lanes 11–15 reserved (0 in Classic; available to rack device
    classes, D-k).
- **Per-lane step state:**
  - 808: `on` only. `high` and `flam` must be 0 (the 808 has no
    per-instrument level or flam).
  - 909: each hit is **off / low / high / flam** (p. 30–31: click cycles
    off → low → high → off; flam is its own hit type with a **global**
    width knob). Encoding: `on` bit set = hit; `high` bit = high level;
    `flam` bit = flam hit. Invalid combinations (`high` or `flam` without
    `on`, `high`+`flam` together) are rejected by setters and the codec.
    Whether a flam hit plays at low or high level is **OPEN** (§6).
- **Global accent (AC):** `flags` bit `RI_DRUM_AC` — the separate Accent
  “instrument” row whose level knob applies equally to all sounds on
  that step (p. 35, 148, 151). Both 808 and 909 have it; on the 909 it is
  independent of the per-hit low/high level.
- OH+CH on the same step is legal data; the voice layer applies the
  documented rules (808: very short OH; 909: OH heard, p. 35).
- The old `accent:u16` per-lane mask and the row-level `RI_STEP_FLAM`
  are dropped: they could not express the 909's per-instrument flam or
  low/high level, and gave the 808 a per-instrument accent it never had.

### 1.3 Banks and instances

- `RIPatternBank {pat[32]}` per **device instance**, caller-owned, no
  allocation. Instance ids come from the rack (D-k); the Classic rack is
  0–3 = 303A/303B/808/909 and matches event device ids and route
  sections. The model never hard-codes 4 instances.
- Section-level (not pattern-level) state stays outside this slice:
  selected bank/pattern, section on/off, Tune, sound switches.

## 2. Emit

- Octave: Up → +12, Down → −12, `RI_EVFLAG_OCTAVE` set when either
  applies; key → MIDI via `RI_303_BASE_NOTE` (§1.1).
- `ri_sched_emit_pattern(pat, device, map, start_tick, ppq, opts,
  carry_in, carry_out, out, cap)`: 303 rows expand through the existing timed-emit loop
  (shuffle/legato/flam overlays unchanged); rests (Pause) flow through
  the existing gate/slide table; Accent or Slide on a Pause step emits
  nothing for that step (slide from the *previous* note still holds the
  gate per the §8 table).
- Slide on the last step of the pattern ties into step 1 of the next
  loop (the pattern is cyclic; ReBirth p. 154: “tied to the next”).
- Slide direction (ledger `docs/evidence/sequencer/slide-direction.md`):
  rows store the ReBirth meaning (“tie to next”); the walker flag means
  “slide into”. The converter shifts by one step (walker SLIDE on step
  i ⇔ row i−1 is a sounding slide); the seam rides the carry channel,
  never a step-0 SLIDE. Verified by the Task 6 contrasting event lists
  (slide-on-row-0 ties, slide-on-row-1 breaks).
- **Gate length is not decided here.** The existing loop holds the gate
  to the next step boundary; the review (§4.1) flags that the 303 gate
  falls partway through a non-slide step. Emit must take the gate point
  from one parameter so the later measurement changes one constant.
- Drum rows: per set lane, `NOTE_ON` (voice = value = lane). Level:
  909 `high` → per-hit accent level; `RI_DRUM_AC` → a separate
  total-accent event on the step. **This replaces the current rb909
  `accent == 2 ⇒ flam` overload** — flam is emitted from the `flam` bit
  as the existing second-hit `RI_EV_FLAM` with the global Flam-knob
  width, and accent stays an accent. Empty rows emit nothing.
- Length: only rows `[0, length)` emit. Setters clamp length to 1–16;
  length 0 is unrepresentable.

## 3. Edit ops (pure, in `pattern.c`) — ReBirth Edit menu (p. 51–54)

All ops are deterministic: randomness takes an explicit `uint32_t`
seed (no RNG state, no time). Wrong-kind op returns 2, state unchanged.

| Op | Kinds | ReBirth semantics |
|----|-------|-------------------|
| Cut / Copy / Paste | any | pattern slot → slot, within or across instances of the same kind (p. 51) |
| Clear | any | 303: every step Pause, key 0 (C), no accent, no slide, no octave. Drum: every lane off, AC off. Length unchanged (p. 52) |
| Shift Pattern Left/Right | any | rotate by one step over **all 16 steps, ignoring length** (p. 53) |
| Shift Drum Left/Right | drum | same, one lane only (p. 53) |
| Transpose ±12 | 303 | semitone shift of every step's pitch; when the result leaves the representable range it is moved one octave toward it (“moved one octave in either direction so that they appear within the valid range”, p. 54). Re-encoding into key + Up/Down is §6 OPEN |
| Random Pattern | 303 | new pitches, Note/Pause, accents, slides, octaves (p. 53) |
| Random Pitches | 303 | keys only |
| Random Accents etc. | 303 | Note/Pause, Accent, Slide, Octave only |
| Random Drum | drum | one lane (909: over off/low/high/flam) |
| Alter Pattern / Pitches / Accents etc. | 303 | **permutes existing data** across the 16 steps (“randomly shuffling the data in an existing Pattern”); an empty pattern stays empty (p. 54) |
| Alter Drum | drum | permutes one lane's hits |

Pitch Mode (auto-advance entry) is GUI behaviour, not a model op.
ReBirth's random distributions are unknowable and are not a parity
target; only the *scope* of each op is.

## 4. RBNG v1.1 codec (`project/rbng.h` + codec)

- `RI_RBNG_MINOR 1` (major stays 1). New `BANK` chunk: u8 instance,
  u8 kind, u8 drum_class, u8 count (1–32), then per pattern
  **u8 slot (0–31)**, u8 kind, u8 length, u8 payload_ver (=1),
  fixed 16 rows:
  - 303: 16 × {key, flags}
  - drum: 16 × {on LE16, high LE16, flam LE16, flags}
- Per-pattern kind must equal the bank kind. Slots are explicit so an
  instance may store only non-empty patterns; a missing slot loads as
  a cleared pattern of the instance's kind. Duplicate slot → reject.
- Compat: the v1.0 `PATT` reader is preserved byte-for-byte (→ instance
  0, kind 303, slot 0, length clamped to 16). v1.0 notes are MIDI
  numbers: converted to key + Up/Down relative to `RI_303_BASE_NOTE`;
  notes outside the 3-octave range fold by octaves (same rule as
  Transpose) and the reader reports a warning, never a silent change.
  The writer emits `BANK` chunks (never `PATT`) when the song holds
  banks, and stays byte-identical v1.0 (minor 0 + `PATT`, no `BANK`)
  when nbanks == 0 — so legacy round-trips never change shape.
- The `BANK` reader is defensive: length clamps to 1–16, count ≤ 32,
  slot ≤ 31, unknown kinds/instances or invalid drum combinations
  (§1.2) rejected (returns 2, nothing stored). t1_formats v1.0 pins
  stay green.
- `ri_song_to_steps` (v1.0 bridge) is untouched.

## 5. Testing

- New `t53_pattern`: defaults/bounds; key range 0–12 and Up/Down
  normalization; octave emit incl. both-set = neither; 808 rejects
  `high`/`flam`; 909 off/low/high/flam encoding + invalid-combination
  refusal; AC flag emit; flam emitted as FLAM (not accent);
  Shift over 16 steps ignoring length (rows beyond length rotate in);
  Shift Drum touches one lane only; Clear sets key 0 + Pause; Transpose
  octave fold; Alter on an empty pattern stays empty and preserves the
  multiset of steps; Random* scope (e.g. Random Pitches leaves every flag
  unchanged); seed determinism; wrong-kind refusal; v1.1 round-trip incl.
  sparse slots; v1.0-compat read incl. out-of-range note warning.
- Existing suites (t1_formats, sched/emit tests, engine tests) green
  unmodified — the shared step-loop change must not move them.
- Full `ri_audit.sh` 0/0 before commit.

## 6. Open items (ledger rows before they lock)

| Item | Why open | How it closes |
|------|----------|---------------|
| `RI_303_BASE_NOTE` (MIDI note of key 0 with no octave flag) | not stated in the manual | ReBirth export of a key-0 step, pitch measured (E3) |
| Shuffle on/off: stored per pattern or per section | manual says “activate Shuffle for the desired sections” (p. 21, 147) but is silent on whether the switch follows pattern changes | ReBirth: set shuffle on A1, switch to A2, observe; decides whether the flag lives in `RIPattern` or section state |
| 909 flam hit level (low or high) | manual defines flam as a separate hit type | ReBirth export, peak of both hits vs low/high hits |
| Transpose re-encoding (which key/Up/Down encodes a shifted pitch; is key 12 or key 0 + Up used for high C) | manual only states the range fold | ReBirth: transpose a known pattern, inspect the step editor |
| Whether Random Pattern / Alter Pattern are enabled for rhythm sections | the menu “varies with the focus” (p. 52); only the Drum variants are documented for rhythm | ReBirth menu check with the rhythm section focused |

## 7. Review changes (2026-09-25, against ReBirth 2.0.1 manual + hardware)

1. 303 pitch stored as the 13-key button index + Up/Down, not a MIDI
   note (the 303 cannot hold arbitrary MIDI pitches; removes clamping).
2. Drum payload: per-instrument **low/high** level and **flam** as lane
   masks (909), a separate **global AC** flag (808 + 909); dropped the
   per-lane accent mask (808 has none) and the row-level flam flag
   (909 flam is per instrument).
3. Canonical slot orders for 808 and 909; 808 sound switches moved to
   panel state.
4. Shift and Alter operate on all 16 steps regardless of length (was:
   rotate within length). Added Shift Drum, Random/Alter Drum and the
   Pitches / Accents-etc. variants; Alter is a permutation of existing
   data, not a “light variation”. Fixed the §3 contradiction that listed
   randomize as 303-only while describing drum randomization.
5. Clear sets key 0 (Low C). Transpose limited to ±12 with octave fold.
6. RBNG `BANK` gains an explicit slot byte (sparse banks, duplicate
   detection) and the v1.0 MIDI-note conversion path.
7. Emit: flam comes from the flam bit (ends the rb909 `accent == 2`
   overload); gate point is one parameter pending the gate-length
   measurement; slide on the last step wraps to step 1.
8. Open items made explicit (§6) instead of implicit choices.
