# §12.7a pattern model + RBNG v1.1 (m63)

**Date:** 2026-09-25 (host lane; devices untouched)
**Scope:** review §12.7 first sub-slice (pattern banks/lengths + RBNG
chunk plan). OUT: streaming, song/transport, 808/909 hosting, GUI.
**Disposition:** New (architectural path: approach → 5 design sections
→ written spec → sibling fidelity review → plan; TDD per task)

## What landed

- `engine/seq/pattern.{h,c}`: `RIPattern` (kind/length 1–16/payload_ver
  + 16 rows: 303 `{key 0–12, flags}` or drum `{on,high,flam:16,
  flags}`), per-instance `RIPatternBank` (32 patterns), validation,
  909 click cycle, lane tables, key↔note helpers, full Edit-menu ops
  (clear/cut/copy/paste/shift/shift-lane/transpose ±12 with octave
  fold/seeded random/seeded alter-as-permutation).
- Walker (`sched.*`): Up/Down emit note±12 + OCTAVE (closes the
  never-emitted-OCTAVE and rb303-clamp rows — range is unrepresentable
  by construction), `RI_VOICE_ALL`, gate NUM/DEN constant, cyclic
  carry (`RI_STEP_TIE_OUT`, NULL-carries reproduce timed byte-for-byte).
- `pattern_emit.c`: 303 converter (ReBirth "tie to next" → walker
  "slide into", accent-on-Pause dropped, slide-into-Pause holds gate)
  + drum one-shots (per-lane NOTE_ON, 909 high→ACCENT, flam bit→FLAM,
  AC row→total accent, no NOTE_OFFs, fail-closed validation).
- `project/rbng.*`: v1.1 `BANK` chunk (instance/kind/class/count +
  slotted sparse records, exact-length + dup + combo rejects),
  `rbng_patt_to_bank` (slide shift-back, octave fold, warnings never
  silent), writer stays byte-identical v1.0 when nbanks == 0.
- 7 ledger rows under `docs/evidence/{sequencer,909}/`; byte golden
  `tests/golden/formats/bank-v11.rbng` (+sha); fuzz seeds
  `s11/s12-bank.rbng`; audit Phase 7b.

## Slide-direction finding (verbatim event lists, 140 BPM/ppq96/48 kHz)

Case 1, slide on row 0 (tie 0→1): `ON36@0, ON40+SLIDE@5143,
OFF40@10286` — no OFF between the notes.
Case 2, slide on row 1 (break): `ON36@0, OFF36@2571 (fractional),
ON40-plain@5143`.
Same 3-event count, opposite gate shape — the direction bit is real,
and the first-light compat case (25/26 golden events, OCTAVE masked)
pins the converter against production output.

## Test/gate record

- t53/t54/t55/t56 RED (missing headers/APIs) → GREEN; t55 also caught
  two real converter gaps (slide-into-Pause, rest-step shift-back) and
  the walker carried-fractional gap; t56 caught the union-garbage
  memcmp class (fixed by zeroing init).
- Mutation checks: drum voice+1 → 10 failures; grep gates proven
  against planted bad lines, then reverted.
- Neighbors green unmodified (t1_formats/sched/emit/engine suites).
- Full `ri_audit.sh` 0/0 (baseline 0/0 before, 0/0 after).

## Implementation deviations from the plan (all recorded)

- Writer stays byte-identical v1.0 when nbanks == 0 (plan: never write
  PATT) — keeps every legacy round-trip green.
- `BANK before VERS` rejected (minor must be known; PATT-before-SONG
  precedent) instead of deferred parsing.
- Case-8 comparison masks OCTAVE (new metadata, pitch/timing exact).
- Fuzz seeds are static committed files (no mksong regen).
