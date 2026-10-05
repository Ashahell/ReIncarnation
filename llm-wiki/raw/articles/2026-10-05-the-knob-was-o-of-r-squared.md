# The knob was O(r²): `ri_art_disc_grad` emits one rect per row (2026-10-05)

- Source: ReIncarnation session, 2026-10-05 (opencode lane, host benches, ABIv11)
- Collected: 2026-10-05
- Published: 2026-10-05
- Raw: [verbatim](../evidence/2026-10-05-gap-accounting-and-the-item-cull.md)
- Related: [the item cull that threw away 98 %](2026-10-05-the-item-cull-that-threw-away-98-percent.md), [which section owns the build](2026-10-05-which-section-owns-the-build-and-why-the-lcd-is-a-floor.md)

`bench_build` reported the Levi background at **24 ns/command against SYNTH1's 8**,
and the earlier reasoning had been entirely count-based — so it had no way to explain
a per-command cost that differed by 3× between two sections emitting the same kinds of
command.

## The cost was an iteration count, not a command count

```c
static int art_hw(int r, int dy) {
    int dx = r;
    while (dx > 0 && dx * dx + dy * dy > r * r) dx--;
    return dx;
}

void ri_art_disc_grad(struct ri_dlist *dl, int cx, int cy, int r, ...) {
    for (dy = -r; dy <= r; dy++) {
        int w = art_hw(r, dy);
        ...
```

**One disc is O(r²)** — `art_hw` walks `dx` down from `r` per row. And
`ri_art_knob` is **11 tick discs plus 6 gradient discs**, so a single knob is a few
hundred commands *and* a few hundred iterations. The Levi's `DKNOB` table is 108
knobs.

**Nothing in the command count could have found this. The command count was never the
problem; the per-command iteration count was.**

## The cut, and why it is simpler than the one before it

Clamp a disc's rows to the damage box. Each row is a **one-pixel-tall** rect, so a row
intersects the box iff its `y` lies inside it — **no straddling case, and therefore no
seek arithmetic here to get wrong**, which is exactly the arithmetic that was wrong
the first time it was written (the 303 strip seek).

```
                303 strip seek only    + disc_grad rows
    SYNTH1                        8.00                6.34
    808                           8.16                6.56
    909                          16.53               13.23
    LEVI                         42.01               30.38
    21 sections                 152.1                127.6
```

`kept 544` — identical to the unclipped reference. With no clip set the loop is the
original one, so the goldens do not move.

## Mutants: 3 run, 2 killed, 1 correct survivor

`dy0 +1` **KILLED**, `dy1 -1` **KILLED**, `dy0 -1` **SURVIVED** — the third seeks one
row *earlier*, draws slightly more, and lets the push-time clip reject it. **The
parity test pins the lossy direction, so a mutation in the direction the design
already backstops is invisible by construction.**

## The same idea on `ri_art_panel` is rejected, and the anomaly is unexplained

The panel is one big rectangle of horizontal strips, the row clamp should apply, and
it measured a win. `t169` rejected it:

```
culled 237 commands, the clip alone would keep 23
```

Both counts use the same production predicate on the same box, so the two builds must
be emitting **different coordinates** — and neither the row clamp nor the hairline
seek moves a coordinate. **Not understood, therefore not shipped.**

**And the bisect that was supposed to answer it made things worse first:** reconstructing
"the original" by string surgery **dropped two of the panel's four edge lines**, which
turned the next `t93` failure into a mystery about the goldens instead of about the
panel. **A revert performed by retyping the function is not a revert.**

## The general lesson, stated as a preference

*Prefer the primitive that cannot be wrong over the clever one.* The disc row clamp is
a **one-pixel row range** — no formula to get wrong. The 303 strip seek was a
**formula over a variable stride**, and I got it wrong the first time. The panel
attempt was a **formula over a two-pixel stride**, and it produced an anomaly I still
cannot explain. **The complexity of the arithmetic is the risk.**
