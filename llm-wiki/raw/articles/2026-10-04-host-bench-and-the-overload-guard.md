# The songs library lives at `<vol>/ReIncarnation/songs/local/` — and the tab-cycle "cost of -O2" was the overload guard

- Source: ReIncarnation session, 2026-10-04 (opencode lane, Dell E6320 + host)
- Collected: 2026-10-04
- Published: 2026-10-04
- Raw: [verbatim](../evidence/2026-10-04-host-bench-and-the-overload-guard.md)
- Related: [songs could not open](2026-10-04-songs-could-not-open.md), [the LFO path never runs](2026-10-04-the-lfo-path-never-runs.md), [the Dell deploy and USB stick layout](2026-10-01-dell-deploy-abiv11-usb-stick-layout.md)

Two things closed out here, one of which had been argued about for days and is
now measured.

## The song library path (owner asked for this in the wiki)

```
Vk4aros:ReIncarnation/songs/
    local/                         <- Zombie Nation and The Knife live HERE
        zombie-nation/zombie-nation.rbng
        the-knife/the-knife.rbng
        demos.rbpl
        knife.rbpl
    demo/
        riapp-demo.rbng
```

**Zombie Nation is at `Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng`**,
not at the `songs/` root and not on riqemu1. `Vk4aros:` is the **Dell's own
stick**; riqemu1's copy is at `SYS:Classes/ReIncarnation/Songs/zombie-nation.rbng`
(flat — no nesting). Proved by listing both lanes.

The `local/` + `demo/` split **must stay together**, because playlist entries are
relative and include `../demo/...`. That constraint was already recorded in the
Dell deploy article; what was missing is that the *loader* has to cope with the
nesting, which is why RIAPP now probes three shapes rather than joining root+leaf:

```
Vk4aros:ReIncarnation/songs/zombie-nation.rbng                          miss
Vk4aros:ReIncarnation/songs/local/zombie-nation.rbng                    miss
Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng      151 bars, loaded
```

## The tab cycle is not slower at -O2. The overload guard was.

The standing puzzle: `-O2` made the tab cycle look 2.4× slower, which read as a
compiler cost. It is not. It is the **load governor**, and it only fires at `-O0`.

`render_max` on the Dell, with the priority the render task actually held:

| build | governor | `prio` samples | overload trips | xruns |
|---|---|---|---|---|
| `-O0` | on | 21 ×117, **−1 ×7** | 37 | 5420 |
| `-O0` | forced off | 21 ×7, **never −1** | 17 | 2505 |
| `-O2` | on | 21 ×18, **never −1** | **0** | **0** |

At `-O0` a buffer overruns budget, the guard trips, and the render task is
dropped to `AU_LIVE_PRI_YIELD` (−1) — **below the GUI**, which hands the GUI free
CPU and makes repaints look cheap. At `-O2` no buffer ever overruns, the guard
never arms, the render holds priority 21 throughout, and the GUI competes for
real. So the "2.4× slower tabs at `-O2`" was `-O0`'s governor giving the GUI a
hand, and the `-O2` figure is the honest one.

Made falsifiable rather than arguable by `RIAPP_AUDIO_NOGOVERNOR` and
`RIAPP_AUDIO_PRI` (ENVARC, default to shipped behaviour). Forcing the guard off
at `-O0` removes the priority drop exactly as predicted, which is the proof.

**The `-O2` numbers across the board:** xruns 5420 → **0**, `render_max`
20405 µs → **4095 µs**, `wake_max` 5817 µs → **47 µs**. Owner switched tabs by
hand during the `-O2` run: no issues, song sounded correct.

**A separate, real problem this exposed, now attributed:** the
`-O0`+NOGOVERNOR run showed `part_max = 1364359 µs` — a **1.36 second** GUI stall
with 2505 xruns behind it. Not a governor artefact. The `box_*` split that exists
precisely to name an expensive partial's caller names it:

```
  part_max=1364359 part_avg=74648 n=25
  box_steps=0/0/0  box_bar=74648/1364359/25  box_other=0/0/0  box_none=0/0/0
  build_max=132813
```

`box_bar` is the **Song Position display** (`RI_RSEC_BOX_BAR`), and
`build_max ≈ part_max` (490523 vs 490609 in the row before it too) means the cost
is the **display-list build, not the blit**. A repaint blocked audio for 1.36 s
while rebuilding that box's display list.

**It is a burst, not a steady cost** — which is what makes it interesting rather
than merely slow. The same field in the quiet windows reads:

```
  part_max=204 part_avg=202 n=8  box_bar=202/204/8  build_max=116
```

202 µs normally, 74.6 ms on average across a 25-refresh burst. Twenty-five Song
Position rebuilds in one 30 s window, when a tick-driven BAR refresh is otherwise
cheap. This lands on the **existing** `mut_fixM9` / `t93_raster_goldens`
transport display-list track rather than opening a new one: the transport display
list is already the known-expensive one, and that track's missing assertion is
exactly the one that would pin this.

## The host bench: what it settled, and what it killed

Levi voice cost is deterministic DSP, so it needs no AROS lane. On the host a
clock read is ~20 ns instead of 4–6 µs, and the counts can be **exactly
controlled** instead of sampled: render a fixed patch with exactly k held voices,
and the step between adjacent rows *is* the marginal cost.

| build | µs per voice-sample | 8 voices × 64 samples |
|---|---|---|
| `-O2` | **0.307** | 159 µs/block |
| `-O0` | 1.022 | 525 µs/block |

Cross-checked against the Dell with the same `vus` pairing:

| | Dell | host | ratio |
|---|---|---|---|
| `-O0` → `-O2` | 3.07× | 3.33× | **same within 8 %** |
| Dell ÷ host at `-O2` | — | — | **6.42×** |
| Dell ÷ host at `-O0` | — | — | 5.91× |

The `-O0`/`-O2` factor is a property of the **code**, not the machine, and the
Dell is uniformly ~6× the host. The Dell's own regression against the paired
counter (`r = 0.9983`) gives **1.97 µs per voice-sample** and **13 µs fixed
overhead per block** — so on the Dell, as on the host, **the cost is
overwhelmingly per sounding voice and almost nothing is fixed.**

### Two proposed cuts, refuted by measurement

**"Skip silent or unrouted operators" is already shipped.** `voice_pass` opens
with `if (i >= RI_LEVI_NOPS || !live[i]) { opout[..] = 0; continue; }`, and the
operator sweep proves it: 8 operators and 1 operator both cost ~20.8 µs. **The
flat line is not a measurement gap — it is the existing skip.** A cut here would
have reimplemented working code.

**"Morph renders two banks, skip the unused one" is false.** morph 0, 50 and 100
all measure ~79.8 µs. The mid-crossfade is not the expensive path, so **there is
no 2× to reclaim.** This came from reading the source and assuming rather than
measuring, and measuring is what caught it.

So of the bit-identical list: operator skip exists, idle voices already early out,
morph skip has no measurable prize. **That list is empty.** Control-rate envelope
and LFO updates remain, and they are *not* bit-identical — which is where the
owner's ledger should start.

## Corrections to my own earlier claims

- **Closing RIAPP works.** I recorded that "a crashed RIAPP survives
  `--ui-close`". It does not: `--ui-close "#0"` returns `ok` and the process is
  gone. My earlier failures were a **window-indexing error** — with requesters
  up, `#0` was not the live panel. Target the panel window, not index 0.
- **Do not ask for reboots.** The Dell was closed, not rebooted, and it came
  back clean. Ask for a close first.
- **A song-load miss can wedge startup** because `EasyRequestArgs` is modal; the
  probe is now quiet and every shape it tried is logged, readable over atcpbin.

## Open

- **The 1.36 s GUI stall is now attributed** to the Song Position display-list
  build (`box_bar`, `build_max ≈ part_max`), in a 25-refresh burst — but *why* it
  bursts, when the same refresh costs 202 µs quietly, is not established.
- **Control-rate updates** — the only remaining meaningful cut, and it changes
  the sound. Owner's call.
- **A mixed build** (engine/DSP at `-O2`, GUI/app at `-O0`) is untested. The
  measurements say `-O2` is worth ~3× on the DSP with no audible cost; whether
  the debuggability tradeoff is acceptable is the owner's 2026-09-28 rule.
- **`t93_raster_goldens`** still carries 6 `mut_fixM9` survivors awaiting a
  transport display-list assertion.

## Method

- **Measure the thing before cutting it.** Two of four proposed cuts were
  already implemented or had no prize, and only the bench showed that.
- **An A/B where both arms are identical is still a result.** Forcing the guard
  off at `-O2` changed nothing *because the guard is inert there* — that is the
  finding, not a failed experiment.
- **Bring a mechanism to the lane as a switch, not an argument.** A hypothesis
  about scheduling cannot be settled by reading code; it needs a knob that makes
  the two arms differ.

## See Also

- [songs could not open](2026-10-04-songs-could-not-open.md) — the three stacked defects behind "riapp fails to open songs"
- [the LFO path never runs](2026-10-04-the-lfo-path-never-runs.md) — why the cost tracks sounding voices