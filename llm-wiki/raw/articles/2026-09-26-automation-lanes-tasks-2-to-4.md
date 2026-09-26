# Automation lanes (§12.9c): Tasks 2–4 landed — sweep/pass, edits, chase/emission, publish, ATRK, audit + mutants

- Source: ReIncarnation session, 2026-09-26
- Collected: 2026-09-26
- Published: 2026-09-26
- Commits: `7753178` (2a), `ceca27e` (2b), `bd56eab` (2c), `6a76c47` (3a),
  `4d34176` (3b), `fe96f72` (3c), `2820334` (Task 4 audit + pins).
  Superseded: `db13c14` (pre-review Task 2, replaced per R-REVIEWED-PLAN).
  Basis: spec r2 + reviewed plan `6ee2892`, Task 1 `2834f7d`.

## What landed

- **2a sweep + pass laws** (`7753178`): touch/copy set `RI_AUTO_EV_PASS`
  on the event they write; `ri_auto_sweep` erases unmarked-only in
  `[from, to)` and re-anchors held values at `to` (marked, no ppq —
  no quantize in sweep per reviewed plan); `ri_auto_pass_end` clears
  markers + both sets. RED: `t77_autolane.c:325` sweep rc (stub rc 2).
- **2b range edits** (`ceca27e`): init-song clear-all, clear
  end-exclusion, copy capacity/denied/NULL guards. RED: denied-ID copy
  stored (`ct denied refuses`) → allow-check in `copy_touched`
  precompute. Full audit 0/0.
- **2c measure edits** (`bd56eab`): R10 caller-owned clip
  (`base/span/n/cap` + storage, `sizeof < 64` static assert); cap refusal
  on copy/cut, corrupt-clip guard on paste; combined songtrack+lane cut
  bar-for-bar test. RED: `clip handle 262156` (inline 256 KB).
  R-CLIP-SPAN stands (span_ticks kept for the paste shift).
- **3a chase + emission** (`6a76c47`): chase emits latest-≤tick per
  control at the tick sample (punched suppressed); `emit_range` is
  lane-ordered (R8: player sorts); cap-drops keep the carry (songtrack
  R1), suppressed counts as consumed, carry resets on lane mutation
  (documented). RED: `t77_autolane.c:753` chase count 0.
- **3b render-safe publish** (`4d34176`): `RIAutoPub` front/back +
  staged request/apply (RISeq handshake shape), off-thread resync,
  tick re-index for carries; rate law (≤1 publish/block) + single-writer
  contract in the header; deterministic interleaving harness proves
  old/new purity + exactly-once resume. RED: `front swapped` on no-op
  request/apply (a NULL-returning stub crashed first, so pure accessors
  went in early; behavioral pins stayed stubbed).
- **3c ATRK codec (RBNG v1.2)** (`fe96f72`): caller-buffer
  `RISong.atrk/natrk/cap`; writer (minor iff present, AUTO/ATRK
  exclusive, validated); reader (9 reject classes, own err texts,
  two-pass validate-then-store); lane triples bridge
  (`ri_auto_load/store_triples`); master-spec ppq/24 Status blocks +
  `rbng.h` comment. RED: t59 minor-is-2 + atrk-count-0.
- **Task 4 audit wiring** (`2820334`): t77 line, autolane static-state +
  `autolane.h` layer guards, AROS compile entry. Full audit 0/0.

## Rulings (ledger `ledger-automation.md`)

- R-REVIEWED-PLAN: the owner-committed reviewed plan (R1–R10) is
  normative over the pre-review regen; Task 2 reworked in place.
- R-WRITE-BOTH: `nauto > 0` + `natrk > 0` refuses (fail-closed).
- R-ATRK-ORDER: SONG must precede ATRK (PATT precedent).
- R-READER-ATOMICITY: two-pass validate-then-store per STRK R7; no
  cross-chunk rollback per file convention (spliced AUTO+ATRK test
  documents the valid chunk staying stored).
- Probe detour: parse saves/restores the caller ATRK buffer across
  `rbng_song_init`; the count stays an OUT parameter (0 until an ATRK
  chunk parses fully). Test bugs fixed by evidence (missing count word
  in patch offsets; F1 searched instead of FB).

## Mutants — 10/10 killed

1. Sweep ignores the pass marker → later-count / touch-survives fail.
2. Erase skipped when the control does not move → step/wrap counts fail.
3. Allow-list accepts an unknown ID → 808/excluded/stamp-denied fail.
4. Forward quantize → nearest → nothing-before-the-line fails.
5. Chase removed at wrap → chase count/fields fail.
6. Paste shift omitted → **survived** value lookups (a missing shift is
   invisible to `ri_auto_value`); added tick-exact tail pin
   (`at1184 && !at800`, `lp.n == 4`), then killed.
7. Carry ignored → carry steps/counts/covers/drained fail.
8. Publish swaps mid-block → **survived** the request+apply pair (they
   compose to the same swap); added staged-request pin (request stages
   only + staged block has no C), then killed.
9. ATRK validate-then-store merged into one loop → 5 stored-count fails.
10. Value sentinel reintroduced → 5 not-found asserts fail.

## Open (Task 5, R1)

Delivery widening per block, each with a delivery test + allow-list
step: 5a FX (`0x0A0x` → `ri_engine_fx_set`), 5b 808/909 kit params
(`0x04xx`/`0x09xx` → `rb808/rb909_set_param`), 5c mixer strips (need a
new `0x0Bxx` ID block with the GUI registry owner; possibly an engine
level setter — owner call). The cross-check's expected-unbound list
only ever shrinks. E1 p. 72 "level changes" makes 5c the most
user-visible gap.
