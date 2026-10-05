# Verbatim evidence — 2026-10-05: the bounded scan, the redundant clock read, and the ±1 line bbox

Source: ReIncarnation session, 2026-10-05 (opencode lane, Dell E6320, ABIv11, mixed build:
`engine/` at `-O2`, app+GUI at `-O0`). Collected 2026-10-05.

## 1. ri_ctlreg_find: the cost is positional

```
RI_CTLREG_N = 485  (ri_ctlreg_find is a LINEAR scan)
transport items 31 | resolved 31 | NOT found 0
scan depth: total 6949 over 31 items -> mean 224.2 entries (of 485)
  => a full transport build pays ~6949 comparisons before any drawing
```

Per-section index ranges and mean depth:

```
sec  first  last  count  contiguous  ascending-in-run
  0      0    29     30      yes        yes
  2     60   103     44      yes        yes
  3    104   149     46      yes        yes
 13    217   231     15      yes        yes
 18    252   479    228      yes        yes
 19    480   484      5      yes        yes
 20    182   189      8      yes        yes
```

(The "ascending" column is WRONG — see section 5.)

Timed inside one process, against the same section's disjoint-clip build:

```
31x ri_ctlreg_find()     1.971 us | disjoint-clip build 4.513 us  => 44%
31x ri_ctlreg_find()     2.017 us | disjoint-clip build 4.584 us  => 44%
31x ri_ctlreg_find()     2.015 us | disjoint-clip build 4.468 us  => 45%
31x ri_ctlreg_find()     1.947 us | disjoint-clip build 4.427 us  => 44%
```

After the bounded scan, same probe:

```
31x ri_ctlreg_find()     0.069 us | disjoint-clip build 2.014 us  => 3%
31x ri_ctlreg_find()     0.063 us | disjoint-clip build 2.031 us  => 3%
31x ri_ctlreg_find()     0.064 us | disjoint-clip build 2.005 us  => 3%
31x ri_ctlreg_find()     0.064 us | disjoint-clip build 2.009 us  => 3%
```

## 2. bench_trbar, the real box, before and after

```
RI_STR_BAR bbox at zoom 3 -> rc=0  404,38..467,70  (64 x 33 px)
section is 632 x 78 px at this zoom; items 31
    unclipped n=829   rect=629   line=179  circ=0    text=21  image=0
    clipped   n=97    rect=74    line=21   circ=0    text=2   image=0
  kept 97 of 829  (11.7% kept, 54.4% of the cost removed)
```

After the bounded scan and the panel trim:

```
  unclipped 7.89 us over 829 commands (9.5 ns/cmd)
  disjoint clip 2.01 us (background + the items the cull cannot skip)
  the real box 2.63 us (that, plus the survivors)
  RATIO floor/unclipped = 0.1936 survivors/unclipped = 0.1373
```

The floor ratio history on the same bench: 0.376 (pre-hoist) -> 0.257 (bounded
scan) -> 0.194 (panel trim).

## 3. The two binary searches are refuted

The ids do not ascend across the table:

```
first inversion at 190: 2048 after 5127
entries 485 | sorted by reg_id: NO | inversions 3 | duplicate reg_ids 0
```

And they do not ascend WITHIN a section either:

```
table[445] reg_id=4835 sec=18 idx=227  find=NULL
table[446] reg_id=4822 sec=18 idx=214  find=NULL
```

LEVI's run is ordered by sub-panel, each sub-panel ascending in reg_id, the
sub-panels themselves not in idx order.

The per-section range search, installed, failing:

```
FAIL t170_ctlreg_index.c:127: reg_id 4833: binary search gave NULL, linear scan gave an entry
```

## 4. ReadEClock is a PIT port read, and no cheaper clock exists

`src/abi/v11/AROS/arch/all-pc/timer/ticks.c`:

```c
void EClockUpdate(struct TimerBase *TimerBase)
{
    outb(CH0|ACCESS_LATCH, PIT_CONTROL);   /* Latch the current time value */
    time = ch_read(PIT_CH0);               /* Read out current 16-bit time */
```

and in `gui/widgets/rsection.mcc.c`, the duplicated pair:

```c
                if (timed) {
                    struct EClockVal te;
                    ULONG ur;
                    ReadEClock(&te);
                    ur = eclock_us(&tr, &te);
                    dp_ur_here = ur;
                    ...
                }
                if (timed)
                    ReadEClock(&tr);          /* start of blit -- SAME INSTANT */
```

## 5. The probe that lied, and the ±1 that explained everything

The probe that "proved" every section ascends:

```c
                if (!first && d->reg_id <= prev) asc = 0;
                first = 0; prev = i;          /* <-- prev = i: the INDEX */
```

`ri_dcmd_bbox`, the reason a hairline is kept one row outside the box:

```c
    case RI_D_LINE:
        /* 1-px strokes on both backends; grow one for raster rounding. */
        *x0 = (c->x0 < c->x1 ? c->x0 : c->x1) - 1;
        *y0 = (c->y0 < c->y1 ? c->y0 : c->y1) - 1;
        *x1 = (c->x0 < c->x1 ? c->x1 : c->x0) + 1;
        *y1 = (c->y0 < c->y1 ? c->y1 : c->y0) + 1;
```

Both streams for SYNTH1, box 143,22..209,88, with the wrong clamp:

```
unclipped 3009 | clipped 237
ref[0] op=1 0,0..731,229
ref[1] op=2 0,21..731,21      <-- one row ABOVE the box, kept
ref[2] op=2 0,23..731,23
got[0] op=1 0,0..731,229
got[1] op=2 0,23..731,23      <-- y=21 missing
```

The same box on the tree WITHOUT the panel change:

```
unclipped 3009 | real clip keeps 239
   op   bbox-ok   hits_box
  RECT     2628        195
  LINE      362         44
  TEXT       19          0
  expect_count equivalent = 239   (bbox errors: 0)
```

And a full brushed panel on its own:

```
SYNTH1 brushed panel alone: 119 commands (section 732x230)
```

## 6. ri_dlist_set_clip does not reset n

```
SYNTH1 unclipped 3009 | clipped to 143,22..209,88 -> 3248 kept
```

3248 = 3009 + 239: setting a clip on a populated list appends.

## 7. On target

`RIPP-SCAN` (bounded scan), 32 quiet windows:

```
  part_avg 167.0 | build 45.9 | replay 33.1 | blit 68.9 | gap 18.0
  bsec set: [13] | box_bar 163/170/3
  RIPP-DISC (linear scan)   : part_avg 175.9 | build 55.4
  partition: rows 34 | mismatches beyond truncation 0
```

`RIPP-CLK` (one redundant clock read removed), 47 quiet windows:

```
  part_avg 162.8 | build 43.0 | replay 33.2 | blit 68.9 | gap 16.7
  bsec set: [13] | box_bar 161/165/3
  RIPP-SCAN (8 reads) : part_avg 167.0 | build 45.9 | gap 18.0
  partition: rows 51 | mismatches beyond truncation 0
  last hb: RIAPP hb: buffers=56314 xruns=0 render_max=4101 us wake_max=46 us ...
```

`RIPP-PAN` (panel trim), 15 quiet windows:

```
  part_avg 149.5 | build 32.7 | replay 32.9 | blit 69.0 | gap 14.1
  bsec set: [13] | box_bar 152/159/3
  RIPP-CLK : part_avg 162.8 | build 43.0 | gap 16.7
  partition: rows 17 | mismatches beyond truncation 0
  last hb: RIAPP hb: buffers=59149 xruns=0 render_max=4116 us wake_max=46 us ...
```

## 8. The blit's call path, traced

`src/abi/v11/AROS/rom/graphics/bltbitmaprastport.c`:

```c
    brd.srcbm_obj = OBTAIN_HIDD_BM(srcBitMap);
    ...
    do_render_with_gc(destRP, &src, &rr, bitmap_render, &brd, gc, TRUE, TRUE, GfxBase);
    RELEASE_HIDD_BM(brd.srcbm_obj, srcBitMap);
```

`src/abi/v11/AROS/rom/graphics/gfxfuncsupport.c`:

```c
    if (NULL == L) { ... GetRPClipRectangleForBitMap ... }
    else
    {
        LockLayerRom(L);
        have_rp_cliprectangle = GetRPClipRectangleForLayer(rp, L, &rp_clip_rectangle, GfxBase);
        ...
        for (;NULL != CR; CR = CR->Next) { if (_AndRectRect(&CR->bounds, &torender, &intersect)) ... }
```

blt_avg across every window of every run of the session: 68.8, 68.9, 69.0.
