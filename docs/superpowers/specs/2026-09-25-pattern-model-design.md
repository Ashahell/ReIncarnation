# Pattern model + RBNG v1.1 (first §12.7 sub-slice) — design

**Date:** 2026-09-25. **Status:** owner-approved (§§1–5 in chat).
**Scope:** per-instance pattern banks, full step flags, pure edit ops,
pattern emit, RBNG v1.1 codec. Explicitly OUT: scheduler streaming,
song track/transport (§12.9), engine 808/909 hosting, GUI step entry.

## 1. Data model (new `engine/seq/pattern.h`)

- `RI_PATTERN_STEPS_MAX 16`, `RI_PATTERN_BANK_PATTERNS 32` (4 banks × 8).
- Kinds: `RI_PATTERN_KIND_303 0`, `RI_PATTERN_KIND_DRUM 1`;
  `RI_PATTERN_PAYLOAD_VERSION 1`.
- `RI303Row {note:u8, flags:u8}` — flags are `RI_STEP_*` (§2).
- `RIDrumRow {hit:u16, accent:u16, flags:u8}` — lane bit = voice/slot
  index 0–15 (engine lane→sound mapping arrives with hosting; the
  model stores lanes opaquely); flags carry `RI_STEP_FLAM` (same bit).
- `RIPattern {kind, length 1–16, payload_ver, rows[16]}` (~100 B).
- `RIPatternBank {pat[32]}` (~3.2 KB) per device instance
  (0–3 = 303A/303B/808/909, matching event device ids and route
  sections — rack-aligned per D-k). All caller-owned, no allocation.

## 2. Step flags + emit

- `RI_STEP_UP 0x10`, `RI_STEP_DOWN 0x20`; both-on = neither
  (normalized in emit). `RI_RBNG_UP/DOWN` parallel in the codec.
- Octave in the shared step loop: Up → note+12, Down → note−12,
  `RI_EVFLAG_OCTAVE` set, result clamped 0–127 (closes the
  rb303.c:103-107 clamp row and the never-emitted-OCTAVE row together).
- `ri_sched_emit_pattern(pat, device, map, start_tick, ppq, opts, out,
  cap)`: 303 rows expand through the existing timed-emit loop
  (shuffle/legato/flam overlays unchanged); drum rows emit per-lane
  `NOTE_ON` (voice=value=lane, `ACCENT` from the accent mask, FLAM
  reuses second-hit emission); empty drum rows emit nothing. 303 rows
  (rests included) flow through the existing gate/slide table unchanged.
- Length: only rows `[0, length)` emit. Length 0 is rejected at the
  model boundary (setters clamp to 1–16).

## 3. Edit ops (pure, in `pattern.c`)

- Any kind: clear (rows to rest/empty, length kept), shift L/R
  (rotate within length), bank copy/paste/cut (slot to slot).
- 303-only: transpose (± semitones, non-rest rows, clamp),
  randomize (seeded LCG over an explicit `uint32_t` seed:
  pitches+accents for 303, hits+accents for drums), alter (seeded
  light variation of an existing pattern).
- Wrong-kind op returns 2, state unchanged. No RNG state, no time —
  same seed, same pattern.

## 4. RBNG v1.1 codec (`project/rbng.h` + codec)

- `RI_RBNG_MINOR 1` (major stays 1). New `BANK` chunk: u8 instance,
  u8 count (≤32), then per pattern u8 kind / u8 length /
  u8 payload_ver + fixed 16 rows (303: 16×{note,flags};
  drum: 16×{hit LE16, accent LE16, flags}).
- Compat: the v1.0 `PATT` reader is preserved byte-for-byte
  (→ instance 0, kind 303, length clamped to 16); the writer emits
  `BANK` only. The `BANK` reader is defensive: length clamps to 1–16,
  count to ≤32, unknown kinds/instances rejected (returns 2, nothing
  stored). t1_formats v1.0 pins stay green.
- `ri_song_to_steps` (v1.0 bridge) is untouched.

## 5. Testing

- New `t53_pattern`: model defaults/bounds, flag normalization,
  octave emit + clamp, drum lane events, every op incl. seed
  determinism + wrong-kind refusal, v1.1 round-trip, v1.0-compat read.
- Existing suites (t1_formats, sched/emit tests, engine tests) green
  unmodified — the shared step-loop change must not move them.
- Full `ri_audit.sh` 0/0 before commit.
