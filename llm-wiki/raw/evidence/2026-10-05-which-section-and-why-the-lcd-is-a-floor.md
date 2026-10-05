# Verbatim evidence — 2026-10-05: `bsec` says TRANSPORT, and the LCD background is a floor

Source: ReIncarnation session, 2026-10-05 (opencode lane, Dell E6320, ABIv11, mixed build).
Collected 2026-10-05.

## 1. The lane after the cold reboot

The bridge unit had no `--bulk-port`, so it inherited the 9092 default that
`spike-v4.service` already claims; v4's journal reads:

```
Oct 05 09:54:47 frostmourne python3[204640]: OSError: [Errno 98] Address already in use
```

Now `--bulk-port 9192`, with a ufw rule from 192.168.1.60. 9092 was not in `ss -ltn`
afterwards, which is CORRECT: `_bulk_listener` binds per transfer, on demand.

Proven end to end on `RIPP-DISC` (build 9482915, 1016016 B, r12moves=42), over
BULK_MIN = 1_000_000:

```
[bulkget] Vk4aros:RIAPP.LOG -> /tmp/opencode/DISC1.LOG  34725/34725 B, 1318 ms  OK (sha verified)
```

## 2. bsec: 16 of 16 windows are the transport

`RIPP-DISC`, build 9482915, 1016016 B, r12moves=42, 16 quiet windows:

```
   n | part_avg build rpl blt gap | bsec bsecsum
    8 |      573    56  33 466   17 |   13      448
    8 |      176    55  32  69   18 |   13      447
    8 |      176    56  32  69   18 |   13      447
    8 |      175    55  33  68   18 |   13      447
    3 |      175    55  33  69   18 |   13      166
    0 |        0     0   0   0    0 |  255        0

QUIET WINDOWS (n=16)
  part_avg 175.9 | build 55.4 | replay 32.8 | blit 68.8 | gap 17.8

=== bsec: which SECTION owns the build ===
   {'TRANSPORT': 16}
   window: bsec=13  TRANSPORT  bsecsum=447  of build_avg=55  box_bar=['0/0/0', '176/177/8', '0/0/0', '0/0/0']

last hb: RIAPP hb: buffers=59152 xruns=0 render_max=4098 us wake_max=46 us wake_n=59150 prio=21 arm_us=0 load=3/1000 overloads=0 snd=0/0/0/0 pend=0/0/0/0
```

Partition residual per row (part - sum of the four means), all 17 rows:

```
  n=8   part=176   sum=175   residual=+1
  n=8   part=176   sum=174   residual=+2
  n=8   part=573   sum=572   residual=+1
  n=8   part=175   sum=174   residual=+1
max |residual| = 2
bsec values seen: [13]
```

## 3. The two art commits are worth nothing on this lane

```
                build_avg   part_avg
    RIPP-CULL       56.2       177.5
    RIPP-DISC       55.4       175.9
```

A 1.4 % move on a run-to-run spread of about 2 us.

## 4. bench_trbar against the REAL box

```
RI_STR_BAR bbox at zoom 3 -> rc=0  404,38..467,70  (64 x 33 px)
section is 632 x 78 px at this zoom; items 31
    unclipped n=829   rect=629   line=179  circ=0    text=21  image=0
    clipped   n=97    rect=74    line=21   circ=0    text=2   image=0

  unclipped     9.78 us over 829 commands  (11.8 ns/cmd)
  clipped       4.46 us -> 46.0 ns per SURVIVING command
  kept 97 of 829  (11.7% kept, 54.4% of the cost removed)
```

## 5. The value text is proportional -- the narrowed-box cut is dead

TEXT commands' own recorded bboxes, from `ri_dcmd_bbox`:

```
    "888"  w=15      three digits
    "100"  w=14      three digits
    "120"  w=14      three digits
    "  1"  w=12      two blanks + one digit
    "  4"  w=13      two blanks + one digit     <-- SAME STRUCTURE, 13 vs 12
```

`"  1"` and `"  4"` have identical structure and different widths, so no fixed-cell
split exists.

Two probe artefacts, both recorded because each nearly became a finding:

```
    measured value-text extent: x0=7 width=5 over 3 characters => cell 1 px
```
(compare against whatever text it saw first -- the transport's "0" legend at x=7)

and `ri_sui_set(&ui, RI_STR_BAR, v)` does not set the bar: `gui/secttr.c` routes it
around `ri_str_set_value` and reads `ri_seq_bar_display(s->cursor, s->ppq).bar`, so
the display showed `"  1"` at every value set.

## 6. The composition of the 97 survivors

`gui/draw/art_shared.c`:

```c
void ri_art_led_digits(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int v, int ndig) {
    ri_art_lcd_bg(dl, x0, y0, x1, y1);
    ri_art_text_c(dl, cx, cy, ndig == 3 ? "888" : "88", C_SEG_DIM);
    ...
    ri_art_text_c(dl, cx, cy, b, C_SEG);
}

void ri_art_lcd_bg(struct ri_dlist *dl, int x0, int y0, int x1, int y1) {
    for (k = y0; k <= y1; k++)
        ri_draw_rect(dl, x0, k, x1, k, ...);
```

So 74 rects + 21 lines is the LCD background (one rect per row) and 2 text commands
are the ghost and the value. The ghost is a literal `"888"` and never changes.

## 7. The platform contract: MUIM_Draw gets no rect

`src/abi/v11/AROS/workbench/libs/muimaster/mui_redraw.c:137`:

```c
    _flags(obj) = (_flags(obj) & ~MADF_DRAWFLAGS) | (flags & MADF_DRAWFLAGS);

    DoMethod(obj, MUIM_Draw, 0);
```

Lines 118-124 intersect the object's own bounds with `l->ClipRegion->bounds`. A
widget is never told WHAT changed, only that something did.

## 8. The decomposition, and the hoist refuted

```
    unclipped                    9.70 us   (everything)
    disjoint clip                3.62 us   (background + the items the cull cannot skip)
    the real box                 4.51 us   (that, plus the survivors)
    -> the item cull removes     6.08 us  (63% of the build)
    -> the SURVIVORS cost        0.89 us  (20% of the clipped build)
    -> unreachable floor         3.62 us  (80% of the clipped build)
```

Item population, by shape, with the enum read from `gui/panelgeo.h`
(`KNOB 0, RECT 1, LED 2, LEGEND 3, DIVIDER 4, OPTION 5, STEPPER 6`):

```
    shape      items    boxed   UNCULLED
    KNOB          14       14          0
    RECT           3        3          0
    LED            0        0          0
    LEGEND         5        0          5  <- drawn every repaint
    OPTION         8        8          0
    TOTAL          31       26          5
```

Absolute microseconds on this host are bimodal by about 25 %, so the bench prints a
within-process ratio:

```
PRE-HOIST   floor/unclipped = 0.3762, 0.3753, 0.3720
POST-HOIST  floor/unclipped = 0.4301, 0.3682, 0.3634, 0.4166
```

Pre-hoist is tighter and slightly better, so the hoist was reverted.
