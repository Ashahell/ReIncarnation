# The keybed draws a keyboard it does not own: 19 of 32 visible keys had nothing behind them (2026-10-24)

- Source: ReIncarnation session (OpenCode lane), 2026-10-24. Commit `eb82fbb`, tag `[levi-midi]` (phase P-KEY3).
- Evidence: `tests/unit/t239_keybed_drawn_is_playable.c` (RED 78 → PASS), `tests/mutate_keybed_map.py` (8/8); `t238_levi_key_paint` and `t221_keybed` made map-driven; `t147`/`t148`/`t149` re-pinned to `RI_SLEVI_NCTL 252`; `t92_draw_hash` / `t93_raster_goldens` re-pinned on LEVI only; `gui/sectlevi.h`, `gui/sectlevi.c`, `gui/panelgeo.c`, `gui/ctlreg.c`, `gui/draw/art_levi.c`, `app/riapp.c`, `app/core/canvas_events.c`; `docs/evidence/midi/ledger.md` §P-KEY3; renders `docs/evidence/midi/pkey3-keybed-z{0,1}.png`.
- Collected: 2026-10-24
- Published: 2026-10-24
- Spec: `docs/superpowers/specs/2026-09-20-reincarnation-spec.md` (§12.10 GUI, keybed)
- Prior: `raw/articles/2026-10-23-the-keybed-painted-slots-it-does-not-own.md` (P-KEY2, the paint == hit rule); `raw/articles/2026-10-15-keybed-plays-and-step-record-gates-the-step-write.md` (P-KEY1, the edges). This article closes the half both left open: *which drawn keys have a control at all.*

## The report, verbatim

> "In the Levi panel, the 1st seven and the last seven white piano keys do not produce anything audible. The other white piano keys do produce a tone. Also, only the middle 5 black piano keys produce anything audible."

This is the first listening round **on the P-KEY2 build**, so it is also the ear evidence that build never had (the earlier "never heard" line is superseded — see the correction below).

## The arithmetic that explains it, from the picture

The art laid **22 white keys** (pitch 59, x 440..1738) and skipped its black keys only for k 7..13: **22 whites + 10 blacks = 32 visible keys**. The registry had **13** keys (ids 24..36) at cx 882..1295 — one octave in the middle. So:

- drawn-but-dead whites: 22 − 8 live whites = **14 = the first seven + the last seven**;
- drawn-but-dead blacks: **5 = the two outer groups**;
- live: the middle 8 whites and the middle 5 blacks.

That is exactly the report, group for group. No log line was needed: **the map and the picture disagree, and their disagreement has the same shape as the ear.**

## Why P-KEY2 could not see it — and the general rule

P-KEY2 asked *"does a control paint the box it is hit in?"* and answered it for the 13 controls that existed. It never asked *"does every drawn key have a control?"* — the art's 22 whites and 10 blacks were drawn by the **art**, not by the registry, so a decoration could look exactly like a key and nothing in the build compared the two lists. Adding black art keys back is safe from the *test* only because the raster pin sees the picture change; that is a gate, not an invariant.

The rule this phase adopts is the owner's: **every key the panel draws is a control, and every control on the bed is a key.** Decoration that looks playable is a defect, not styling. `t239` now drives it from the art's own band constants against the geometry and the registry, so a drawn slot without a control fails as `"white slot N has a control"`, and a key-shaped row the band does not draw fails the closed census.

## The bed is the whole drawn keyboard — the owner's choice

37 keys = **22 white + 15 black**, C4..C7, one semitone per key (5 blacks × 3 octaves = 15 is forced by the art, not picked for roundness). The alternative — dimming the outer keys and keeping 13 — was rejected by the owner: a picture that lies about what plays is the defect being removed.

## The ids are a MAP, not a range

The LEVI section's only free run is 228..255 (4..23 and 37..227 are other controls), so 37 keys cannot be contiguous:

| keys | ids | why |
|---|---|---|
| 0..12 (C4..C5) | 24..36 | unchanged: songs, skins and automation key on the ids, and a renumber would move every stored reference |
| 13..36 (C#5..C7) | 228..251 | the section's only free run |

`RI_SLEVI_NCTL` grows **228 → 252** so the extension keys' values have a home. Every `idx - RI_SLEVI_KEY0` site is gone: five call sites (step-record write, key LED, the latch in `canvas_events.c`, note-on and note-off in `riapp.c`) now use `ri_levi_key_id(n)` / `ri_levi_key_n(id)` / `ri_levi_key_note(n)` / `ri_levi_key_black(n)` / `ri_levi_key_slot(n)`, declared in `sectlevi.h` beside the band constants that the art, the geometry table and `t239` all read.

The **note** is the key number: `note = 60 + n`, so a press plays the pitch under the finger and step record writes that pitch. The classification is the piano's own (C D E F G A B white, the five semitones black), asked of the map rather than guessed from a box height.

## A correction inside this phase: the bed was never too wide

Planning had recorded *"22 keys at 59 overran the 1684-wide section"* and changed the pitch to 56. That is false: `ri_geo_section(RI_SEC_LEVI)` is **1756 Q** wide, the old art at pitch 59 ended at 1738, and `t239` now asserts the last white ends at **1735 < 1756**. So the bed keeps the art's own pitch 59, white width 55 and black width 36 — the change is *the whole drawn bed is live*, not a resize. Recorded because the wrong width was already being repeated as a reason, and because a fix that "needed" a resize would have hidden the real defect (missing controls) behind a cosmetic one.

## The art draws no black key at all now

All 15 blacks are controls, and a control paints its own box; an art black would be decoration by definition. What the art still owes them is the white they sit on: `ri_art_bg_levi` lays the bed 355..470 for all 22 whites uniformly (the old `active` special case is gone with its reason), and `ri_art_levi_item` classifies black/white with `ri_levi_key_black()` instead of `hh > PX(50)` — the drawing now asks the same map the geometry does.

## Tests: the map is driven, not restated

`t239_keybed_drawn_is_playable` was written first and was RED with **78 failures** on the old map (white slot 0: control at 855..909 against the drawn 442..494; white slots 8..21: `has a control (0)`; census 13 against 37). It computes the slot list **from the art's band constants**, not from `RI_GEO_LEVI`, and compares it box for box against the geometry and the registry. GREEN after the map:

1. the band is the piano: 22 whites + 15 blacks = 37, one pitch, and the last key is inside the panel;
2. every drawn slot has exactly one control **on that box** (white 470..550, black 354..470);
3. the pitch rises left to right, one semitone per key (sort the 37 rows by centre x; notes must be 60..96);
4. the census is closed both ways: 37 key-shaped geometry rows and 37 key-shaped registry rows;
5. the id map is the owner's (24..36 stable, 228..251 extension, bijective) and the note map is 60 + n;
6. the paint rule answers for all 37 and refuses a control that is not a key.

`t238` and `t221` are now driven by `ri_levi_key_id(k)` — the loops used `KEY0 + k`, which is only true for the first thirteen. The three tests that pin the panel row count moved 228 → 252, and the keybed legends are octave-qualified (`C4`…`C7`): "C" alone would appear three times in automation and help text.

## The pins moved where the panel moved, and nowhere else

`t92_draw_hash` and `t93_raster_goldens` each failed **exactly four** times — the four LEVI (sec 18) zooms — and nothing else, twice over. Re-pinned: display-list `f95e0ef2 / 965d4934 / 7ba7cb22 / b3701436`, raster `84be1317 / d00c813f / 54ae8c8c / 2e10198b`. Twenty other sections unmoved at four zooms is what confines the delta to the Levi panel.

## Harness: 8/8, and which gate carried each kill

`tests/mutate_keybed_map.py` runs five gates (t239, t238, t221, t149, t92), scores on **return codes**, keeps the marker rule (a marker never requires a token its mutant removes), and prints the killers:

| mutant | killed by |
|---|---|
| m1 the last white key's control deleted | t239 + t238 + t221 + t92 |
| m2 one black key moved 18 Q off its boundary | t239 + t92 |
| m3 an id collision (228 renumbered onto 229) | t239 + t238 + t221 + t92 |
| m4 the note map off by one semitone | **t239 only** |
| m5 the drawing stops using the paint rule | **t92 only** |
| m6 the art draws a black key with no control | **t92 only** |
| m7 `NCTL` left at 228 (extension values off the array) | **t149 only** |
| m8 the latch narrowed to the old octave | **t221 only** |

m5/m6 are t239's honest limit: it sees the map, not the painter. m7 says the row-count pin is load-bearing. `AUDIT 0/0 PASS` after the harness restored every edit.

## Build identity — the correction the earlier articles needed

The P-KEY2 article and the 2026-10-15 addendum named the build as **1237704 B / sha `27b5879dfa…`**. That binary is the **v1-line** shape (`ri_build_aros.sh riapp`). The Dell runs the **v11** shape (`ri_build_v11.sh "$ROOT" "$OUT/RIAPP.M4"`), and the binary actually staged and played was **1 217 912 B, sha256 `916961f022c8b191546d…`, `build=386abb8`**. Both numbers are true of different programs; a sha256 says which bytes ran, not what they are. `scripts/lane_proof.sh` exists because confusing the two deploys the wrong program.

## Status — what is proven, and what is not

Proven: every key the art lays out has exactly one control on exactly that box; the ids and notes are the owner's map; the bed fits the panel; the paint rule and the hit boxes coincide for all 37. **Not ear-proven:** the owner has not yet heard 37 keys. The next Dell round on **this** v11 build is the proof, and a listening launcher must set `RIAPP_MIDI_DEVOUT 0` and `RIAPP_MIDI_CLKOUT 0` — on this machine the camd wire is the cluster the app reads, so the app re-ingests its own output.

## The rule this leaves

A control surface is **two maps** — art and geometry — and nothing in the build makes them agree. P-KEY2's invariant covered *how* a control paints; this phase adds *whether it exists at all*: **the drawn set and the control set must be the same set**, and the census must be closed in both directions, or decoration will be mistaken for a control (and a control for decoration) until the owner's ear finds it.
