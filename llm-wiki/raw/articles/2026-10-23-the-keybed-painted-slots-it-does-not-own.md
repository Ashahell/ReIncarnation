# The keybed painted the slots it does not own: five of thirteen keys had nothing behind them (2026-10-23)

- Source: ReIncarnation session (OpenCode lane), 2026-10-23. Commit `d9f2af3`, tag `[levi-midi]` (phase P-KEY2).
- Evidence: `tests/unit/t238_levi_key_paint.c` (RED 9 → PASS), `tests/mutate_levi_key_paint.py` (6/6); `t92_draw_hash` and `t93_raster_goldens` re-pinned on LEVI only; `gui/draw/art_levi.c`, `gui/draw/art.h`, `gui/sectlevi.h`, `gui/panelgeo.c`, `gui/ctlreg.c`; `docs/evidence/midi/ledger.md` §P-KEY2; scale-2 panel capture.
- Collected: 2026-10-23
- Published: 2026-10-23
- Spec: `docs/superpowers/specs/2026-09-20-reincarnation-spec.md` (§12.10 GUI, keybed)
- Prior: `raw/articles/2026-10-15-keybed-plays-and-step-record-gates-the-step-write.md` (P-KEY1, the edges); this article closes the *aim and the picture*, which that article never covered.

## The owner report, and the log that agrees with it

An ear round on the re-staged Dell said three things: *"of the 13 piano keys only a few produce something audible"*, *"the volume is low"*, *"one of the black keys (the 6th from the left) is only rendered half"*.

The presses came from the owner's hands, so the lane logged them: **18 presses, every one a WHITE-key note** (`LEVI key 60/62/64/65/67/69/71/72` = C D E F G A B C+), **zero lines** for any of the five black keys (idx 25,27,30,32,34 = notes 61,63,66,68,70). Six presses logged **E**.

That log alone cannot distinguish *"the black keys are dead"* from *"the aim and the picture disagree"* — both produce zero black-key lines. **The picture can, and it does not need the log.** That is the whole reason the picture is measured before the sound here.

## What is NOT at fault — and the pins that stay GREEN say so

The map is a correct piano. Driven off `RI_GEO_LEVI` and `gui/ctlreg.c`:

```
white rows  cx 882,941,1000,1059,1118,1177,1236,1295   cy 510, w 55, h 80   -> own y 470..550
black rows  cx 912,971,1089,1148,1207                 cy 412, w 36, h 116  -> own y 354..470
art band    LEVI_KEY_X0 440 / W 59 / N 22 / Y0 355 / Y1 550 / BKEY_Y1 470, bk[7] = {1,1,0,1,1,1,0}
```

A press at **each of the 13 rows' centres hit-tests to that row at every zoom** (0..3); every key has exactly one geometry row and a `BUTTON` registry row; the black rows sit centred on the white boundaries the art uses — `(882+941)/2 = 911.5` against 912, `(941+1000)/2 = 970.5` against 971, **a gap at 1029.5 (E/F — correctly no black key)**, 1088.5 against 1089, 1147.5 against 1148, 1206.5 against 1207.

Geometry, registry and latch were never the fault. **One line of paint is.**

## The arithmetic, and the pixel that agrees with it

`gui/draw/art_levi.c`'s live-white branch re-centred every white's paint over the **whole band** (y 355..550) instead of the 470..550 its hit box owns, and the geometry table lays **blacks before whites** (rows 292–296 before 297–304), so every white repaints the blacks beside it:

- white C paints x 855..909 at full height; black C# spans 894..930, so the white covers 894..909;
- white D paints 914..968 and covers the rest, 914..930;
- **what survives of a 36 Q black key is 910..913, about 4 Q** — under 2 image px at the lane's scale-2 calibration (22 image px per 59 Q).

The measured capture agrees exactly: the live black slots sat as **2 image px** hairlines (the gap between whites, not a key), full art blacks measured **14 px**, and the key the owner counted measured **8 px**.

The half key, found two ways: the art skips its blacks for k 7..13, so from the left the black keys the owner can SEE are the art octave's five at x 499, 558, 676, 735, 794 — then the art black at x **1325, the sixth**, which live white C+ (idx 36, painting over it to 1322) covers from the left, leaving 1322..1343 = **about 21 Q = 8 image px**. Same key, same number, from the code and from the pixels.

**And that is why the presses missed.** The full-width black keys the owner aimed at are art decoration at x < 853 and x > 1325 — the art band carries **22** whites, the live bed **13** (slots k 7..14, x 853..1325) — and there is **no control behind them**. Every black-key press therefore landed on a live **white** box, which is exactly the 18-for-8 log.

## The invariant, and which half of it was already GREEN

`t238_levi_key_paint` drives the map and the drawing's **own rule**, `ri_levi_key_paint_q` (declared in `gui/draw/art.h`, defined in `gui/draw/art_levi.c`, and `ri_art_levi_item` derives its rect from that call) — the shape that makes "where does a key paint?" a fact about the product rather than a second copy of a rule that could disagree with it. The invariant:

1. **a key paints THE SLOT IT CAN HIT** — paint box == hit box, compared **in Q units**;
2. **no live key's paint covers another live key's centre** — all 13 × 12 pairs, driven from the map, not the two the report happened to name.

Honest about which assertion carried the finding: **the width assertion and the centre-coverage assertion were GREEN on the old code** — the whites' x extents stop at the boundaries, so no white reaches a black's *centre*. **The height assertion is the finding** (8 whites painting `355..550` against a hit band of `470..550`). RED 9 = those 8 + 1 velocity, GREEN after, `t220`/`t221` unchanged.

## The method trap this phase caught in itself

The rule's first version classified keys with `it->h > 50` in **raw Q** while the drawing classifies on **half-heights** (`hh > PX(50)`). In raw Q that puts the whites (h = 80) in the **black** branch, so the rule returned the hit box for every key, the invariant went **GREEN with no finding at all**, and the test looked like a passing test. The threshold is `it->h / 2 > 50`.

Two standing lessons, both applied rather than restated: a comparison that mixes units passes at one zoom and fails at another while the product is unchanged; and **a GREEN from an edit that never landed is indistinguishable from a real GREEN without `grep -c`.**

## The fix, as the owner ruled: keep the geometry, fix the paint

- `ri_levi_key_paint_q` paints **the slot a key can hit** — `cx ± w/2`, `cy ± h/2`. The stretched version is gone; the fix is the same function, not a second one.
- `ri_art_bg_levi` now **lays the white bed 355..470 for the live slots** (k 7..14) instead of skipping them. That is why confining the paint does **not** shorten the white keys: the art underlay paints 355..470, the control paints 470..550 — and it is what gives the black keys **white to be black on** instead of panel-dark. The art still skips the live black slots, because the controls paint those at 354..470 on top.
- `RI_LEVI_KEY_VEL` **100 → 127**. **A reversal** of the owner's 2026-10-13 fixed-100 ruling, recorded where it is read (`gui/sectlevi.h`, the `app/riapp.c` comment, the ledger). **Fixedness stands** — the hardware keybed is not velocity-sensitive — only the value, at the note path's ceiling (`levi_note_vel` clamps 0..127; `t146` owns that clamp and the `vel01` → level route, `t238` pins `vel01 == 1.0f` rather than 100/127).

## The harness, and the blind spot it names instead of hiding

`tests/mutate_levi_key_paint.py`, **6/6**, scored on return codes with the marker rule (a mutant's marker never requires a token it removes). The gate is **two tests on purpose**, and the harness prints which gate killed each mutant:

| mutant | killed by |
|---|---|
| m1 the white band stretched again (the exact regression) | t238 + t92 |
| m2 the box widened to the full pitch | t238 + t92 |
| m3 the box shifted by 4 Q | t238 + t92 |
| m4 the velocity lowered back to 100 | t238 + t92 |
| m5 the DRAWING re-stretches the live whites itself | **t92 only** |
| m6 the art's bed for the live slots not laid | **t92 only** |

**m5 is the honest limit of t238**: it drives the rule, so a drawing that stops using the rule's answer is invisible to it *by construction* — testing a rule nothing uses tests nothing. Only the hash pin sees m5/m6, and the harness says so rather than scoring a quiet 6/6.

## The pins that had to move, and what they confirmed

`t92_draw_hash` and `t93_raster_goldens` each failed **exactly 4** — the four LEVI (sec 18) pins — and nothing else. LEVI display-list hashes re-pinned to `1e034651 / a8def2a7 / d3afa1c7 / 6cde42a7`; LEVI raster goldens to `13485eab / ded94c13 / 1de18940 / 6e785975`. **Twenty other sections untouched at all four zooms, twice over, is what proves the delta is confined to the Levi panel** — the check working, not a re-pin habit.

`AUDIT 0/0 PASS`. AROS build **1237704 bytes, sha256 `27b5879dfa8c50a54c97`** (the count moved 20 328 from `c382b38`'s 1217072; this build has never run on the Dell).

## Status — and what this does NOT claim

**Nothing here is ear-proven.** Proven: the picture and the hit boxes coincide (the invariant, in Q); the art's black keys coincide with the live black rows; the keybed's fixed velocity is the path's ceiling. NOT claimed: that the owner now hears all 13 keys, that every white-key note is audible, or that the low-volume complaint is closed at the mixer. The next Dell listening round **on this build** is the ear proof, and a listening launcher must set `RIAPP_MIDI_DEVOUT 0` and `RIAPP_MIDI_CLKOUT 0` — on this machine the camd wire IS the cluster the app reads, so the app re-ingests its own output.

## The rule this leaves for the next panel, not just this one

A control surface is **two maps** — art and geometry — and nothing in the build makes them agree. The invariant that generalises: **for every hittable slot, the painted slot coincides with the hit box, and no live control's paint covers another live control's centre**, driven off the registry/geometry map rather than off the symptom. Here the mismatch was 22 art whites against 13 live keys, and the mismatch was invisible to every test that only asks whether a press latches — which it did, always.
