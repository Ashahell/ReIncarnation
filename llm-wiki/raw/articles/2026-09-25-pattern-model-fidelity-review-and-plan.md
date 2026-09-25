# 2026-09-25 — Pattern model: fidelity review, implementation plan, slide-direction finding

> Source: review of docs/superpowers/specs/2026-09-25-pattern-model-design.md against the ReBirth RB-338 2.0.1 Owner's Manual (E1) + code reading of engine/seq/sched.c; implementation plan authoring; git history of the §12.7a slice
> Collected: 2026-09-25
> Published: 2026-09-25
> Spec: [../../../docs/superpowers/specs/2026-09-25-pattern-model-design.md](../../../docs/superpowers/specs/2026-09-25-pattern-model-design.md) · Plan: [../../../docs/superpowers/plans/2026-09-25-pattern-model-implementation.md](../../../docs/superpowers/plans/2026-09-25-pattern-model-implementation.md)

## Disposition
New. Spec revised for hardware/ReBirth fidelity (owner re-approved, commit
`d10178e`), 11-task implementation plan written, and a slide-direction bug in
the walker's step semantics found before implementation.

## Spec revisions (spec §7)
1. 303 pitch stored as the 13-key button index (low C … high C) + Up/Down, not a
   MIDI note; MIDI derived at emit from `RI_303_BASE_NOTE` (E0 = 36, ledger row).
2. Drum rows `{on, high, flam, flags}`: 909 hit = off / low / high / flam per
   instrument (manual p. 30–31); global AC row as a flag (808 + 909); 808 holds
   on/off only. Dropped the per-lane accent mask and the row-level flam flag.
3. Canonical slot orders (p. 32): 808 `BD SD LT MT HT RS CP CB CY OH CH`,
   909 `BD SD LT MT HT RS CP CH OH CC RC`; 808 sound switches are panel state.
4. Edit ops per the Edit menu (p. 51–54): Shift and Alter act on all 16 steps
   regardless of length (“does not take Pattern length into consideration”);
   Shift/Random/Alter Drum per instrument; Random/Alter Pitches and
   Accents-etc. variants; Alter permutes existing data (“randomly shuffling the
   data in an existing Pattern”), empty stays empty; Transpose ±12 with octave
   fold; Clear = Pause + key 0 (C), length kept.
5. RBNG `BANK` records carry an explicit slot byte (sparse banks); v1.0 `PATT`
   converts with warnings.
6. Five OPEN items with closure tests: base note, shuffle scope (per pattern vs
   section), 909 flam level, transpose re-encoding, Random/Alter Pattern on
   rhythm sections.

## Slide-direction finding (verified in code)
The walker's `RI_STEP_SLIDE` means **slide into this step** (gate from the
previous note held, this note glides). ReBirth: “the selected step will be
tied to the next” (p. 154) — slide **from** this step. The model stores the
ReBirth meaning and the converter shifts it one step (last-row slide crosses
the loop seam via a carry); v1.0 files shift back on conversion. Without it,
every ReBirth pattern would play its slides one step late.

## Other plan facts
- 909 engine voice ids differ from panel lane order (`RB909_CH 2u` vs lane 7);
  plan adds `RI_LANE_TO_RB909_VOICE`; 808 lane == `rb808` slot (identity).
- Emit ends the rb909 `accent == 2 ⇒ flam` overload: flam bit → `RI_EV_FLAM`
  with the global Flam width; total accent → `RI_EV_ACCENT` on `RI_VOICE_ALL`.
- Struct sizes pinned: `RIPattern` 132 B, `RIPatternBank` 4228 B; `RISong`
  moves to static storage.

## Implementation status (another session, main)
`d10178e` spec + plan → `f6b958f` model types/validation/click cycle/lane
tables → `7f20572` edit ops → `4afcfc7` walker octave/total-accent/gate
constant/carry → `1fd0d38` 303 emit with ReBirth slide direction + seam →
`f25f2ec` drum emit → `e925f6e` RBNG v1.1 BANK + PATT conversion → `0961d60`
slide-into-Pause holds gate (first-light compat). Audit Phase 7b + byte-golden
sidecar still uncommitted at 13:44 (Task 9 in progress).
