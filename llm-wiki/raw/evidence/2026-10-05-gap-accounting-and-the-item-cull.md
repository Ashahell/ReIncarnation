# Verbatim evidence — 2026-10-05: the GAP counter, the item cull, and the attribution that followed

Source: ReIncarnation session, 2026-10-05 (opencode lane, Dell E6320, ABIv11, mixed build:
`engine/` at `-O2`, app+GUI at `-O0`). Collected 2026-10-05.

## 1. The GAP counter was `max` where the arithmetic said `sum`

`gui/widgets/rsection.mcc.c`, before the fix:

```c
                        ULONG acct = us_build;
                        if (dp_ur_here > acct) acct = dp_ur_here;
                        if (dp_ub_here > acct) acct = dp_ub_here;
                        if (acct < us) {
```

The phases are disjoint intervals, so `us - max` equals the other two phases plus
the true residue. Every mean on the `RIAPP draw:` line shares one denominator, so:

```
  buffered      233 == 113 + 34 + 70 + gap  =>  gap = 16     (logged: 121)
  direct-paint  238 == 117 + 105 + 0 + gap  =>  gap = 16     (logged: 119)
```

`121 - 16 = 105 = 34 + 70`, exactly, in both configurations.

Corrected on target (11 quiet windows), `RIPP-SUM`, mixed build 1013880 B, r12moves=42:

```
   n | part_avg build rpl blt gap_avg gap_max
    8 |      583   112 377  72      20       41
    8 |      234   112  33  69      19       28
    8 |      234   112  33  69      19       28
    3 |      718   596  33  69      19       20
```

```
  quiet windows: part_avg 232.6 | gap_avg 17.6 us  (was reported as 121)
  === the partition invariant: build+rpl+blt+gap == part ===
  rows checked: 11 | mismatches: 3
```

The three "mismatches" are 2 us each: four independent `floor(sum/n)` averages.

## 2. Direct-paint partials: measured, net zero

Quiet windows, 13 of them, on `RIPP-DP2` (mixed build 1013328 B, r12moves=42):

```
DIRECT-PAINT quiet windows (n=13):
  part_avg 238.2 | build 117.3 | replay 105.4 | BLIT 0.0 | gap 119.2

  BEFORE (through the buffer): part_avg 232-234 | build 113 | replay 34 | blit 70 | gap 121
```

The blit is gone (0, by construction) and the replay absorbed exactly what it cost:
34 -> 105 us, +71 against the -70 removed.

First direct-paint build, `RIPP-DP` (mixed build 1013592 B, r12moves=42) — the dead
timing block, showing `blt` duplicating `rpl`:

```
    8 |      240      230 |   113 109 113 |     127 | 230/240/8
    8 |      230      228 |   113  99 103 |     117 | 228/230/8
```

## 3. bench_build: the item loop is 70-93 % of a build, and text is not the cost

`tests/unit/bench_build.c`, host, 21 sections, 64x16 damage box at the section centre:

```
section      items   built     bg  items   wasted  txt_b  txt_k  us_built   us_clip
SYNTH1          69    3009    566   2443     2957     19      0     46.27      9.86
808             95    3605    174   3431     3591     51      0     51.85      7.99
909             87    4967    364   4603     4920     67      0     75.87     16.20
LEVI           108    3031    916   2115     2912    212      0     69.11     43.28
TOTAL items 685 | built 26766 | kept 544 (2.0% kept) | us whole 413.5 | us clipped 155.6
```

`us_clip` after the item cull. Before the cull the same bench read `us clipped 490.4`
for the 21 sections, with every section flagged "98% discarded".

Background op split, added later in the session (LEVI): 761 rect, 58 line, 97 image,
97 text -- so the cost is plain rectangles, not font measurement.

## 4. The item cull on target: build 112 -> 56 us

`RIPP-CULL`, mixed build 1014432 B, r12moves=42, 11 quiet windows:

```
ITEM-CULL quiet windows (n=11):
  part_avg 177.5 | build 56.2 | replay 34.3 | blit 68.9 | gap 17.6
  BEFORE (2026-10-05, buffered+sum): part_avg 232.6 | build 112 | replay 33 | blit 69 | gap 17.6
```

```
   n | part_avg build rpl blt gap_avg gap_max
    8 |      176    56  34  69      17       19
    8 |      178    56  34  69      19       28
    8 |      519    56  34 411      17       19
```

Partition invariant: `rows 12 | beyond-truncation mismatches 0`.

```
last hb: RIAPP hb: buffers=56357 xruns=0 render_max=4122 us wake_max=47 us wake_n=56355 prio=21 arm_us=0 load=3/1000 overloads=0 snd=0/0/0/0 pend=0/0/0/0
```

## 5. t169 mutant results

```
  off-by-one <= for <                KILLED (kept 67, expected 70)
  "no damage box" -> skip            KILLED
  RI_GEO_BBOX_MARGIN dropped         KILLED by t169 AND t112
  ox/oy dropped from the cull's test  SURVIVED, then KILLED after a third arm
  cull removed entirely              SURVIVED by design
  M-seek-off-by-one (303)            SURVIVED -- seeks one strip EARLIER
  dy0 +1 (disc_grad rows)            KILLED
  dy1 -1 (disc_grad rows)            KILLED
  dy0 -1 (disc_grad rows)            SURVIVED -- safe direction
```

Two mutation attempts reported SURVIVED because `str.replace` matched an 8-space
indent where the line had 12, so no mutation was written at all:

```
  +1 top row (lossy)  [mutation verified in file]  KILLED
  -1 bottom row       [mutation verified in file]  KILLED
  -1 dy0 (SAFE dir)                                 SURVIVED
```

## 6. The 303 strip seek, caught by t169

First version sought the first strip whose TOP is below the box:

```
FAIL t169_item_cull_parity.c:152: sec=0 item=23 box=100,175..122,211: culled 67 commands, the clip alone would keep 70
```

The fix seeks the first strip whose BOTTOM reaches the box (`need = cy0 - st + 1`).

The same idea applied to `ri_art_panel` was rejected and is still rejected:

```
FAIL t169_item_cull_parity.c:152: sec=0 item=3 box=143,22..209,88: culled 237 commands, the clip alone would keep 23
```

## 7. disc_grad rows: SYNTH1 8.00 -> 6.34, LEVI 42.01 -> 30.38

```
                303 strip seek only    + disc_grad rows
    SYNTH1                        8.00                6.34
    808                           8.16                6.56
    909                          16.53               13.23
    LEVI                         42.01               30.38
    21 sections                 152.1                127.6
```

`kept 544` is identical to the unclipped reference.
