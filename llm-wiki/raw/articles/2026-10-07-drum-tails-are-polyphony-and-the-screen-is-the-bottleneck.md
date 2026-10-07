# The 909/808 "spikes" are polyphony, and the tab switch is bound by screen write bandwidth (220 MB/s on the Dell's VESA framebuffer) (2026-10-07)

- Source: ReIncarnation session (advisor lane) and the opencode lane, 2026-10-07.
  - Opencode commits: `bbb9f8f`, `a7bcfe3`, `6536353` (reverted by `d0224b4`), `2522c79`, `94dcb3b`.
  - Advisor review: `6d85b08`.
  - Dispatch prompt: `docs/superpowers/plans/2026-10-07-drum-tails-and-tab-switch-opencode-prompt.md`.
- Evidence:
  - `docs/evidence/drum-tail/2026-10-07-a0-measure.md`;
  - `docs/evidence/drum-tail/2026-10-07-a1-tau-hoist.md`;
  - `docs/evidence/gui/tab-switch/2026-10-07-b0-split.md`, including the advisor addendum and the probe source.
- Collected: 2026-10-07
- Published: 2026-10-07
- Related:
  - [2026-10-02-five-sections-one-culprit-levi-is-32-percent.md](2026-10-02-five-sections-one-culprit-levi-is-32-percent.md), which opened the "909 tail" item;
  - [2026-10-05-tab-switch-bay-plate-cache-and-housekeeping.md](2026-10-05-tab-switch-bay-plate-cache-and-housekeeping.md);
  - [2026-10-05-rail-page-group-and-log-newlines.md](2026-10-05-rail-page-group-and-log-newlines.md);
  - [2026-10-03-the-o2-tab-cost-is-preemption-of-prebuilt-mui-code.md](2026-10-03-the-o2-tab-cost-is-preemption-of-prebuilt-mui-code.md).

## Part A: the drum tails

- **The item came from a `-O0` run on 2026-10-02:** 909 avg 174 µs, max
  316 µs per block.
- **At `-O2` on `521ab7e`** the 909 averages 31 µs with a 86–88 µs max on
  Zombie Nation, and 54/132 µs on The Knife. The worst block is about 2.5 %
  of the 5333 µs buffer period.
- **Instruments built:**
  - per-block active-voice-sample counters for the 909 and 808 (`vc_*`, the
    Levi discipline; the 909 flam second playhead counts separately);
  - `t175` laws: silence reads 0, k voices read k × 64, flam reads 128 in
    steady state;
  - `drum_bench` with a counter control on every row;
  - a Dell `(stage µs, active)` ring, sampled every 256th block and dumped
    at close under `RIAPP_DIAG`.
- **Dell regression** `µs = a + b × active`, 512 samples per song:

| Fit | a (µs) | b (µs/vs) | r |
|---|---|---|---|
| Zombie Nation 909 | 6.55 | 0.3018 | 0.9790 |
| The Knife 909 | 7.39 | 0.2931 | 0.9811 |
| Zombie Nation 808 | 7.03 | 0.3600 | 0.9990 |
| The Knife 808 | 4.98 | 0.3635 | 0.9952 |

- **Verdict: polyphony, not a defect.** Block cost tracks how many voices
  sound. The 808's larger max/avg (4.0×) at r = 0.999 is the cleanest proof.
- **The host outlier was priced and then dropped:**
  - crash and ride cost about 2.1× the median voice per voice-sample,
    because `decay_env` runs `ri_pow2` per sample (their tune-shortened
    decay, by design);
  - an exact tau-hoist (`6536353`) saved −27 % on the host;
  - but on the Dell A,B,B,A, no metric attributed an improvement to it
    (909 avg 31/31 against 31/32, maxima inside preemption noise), so it
    was **reverted** (`d0224b4`, owner-confirmed).
  - Lesson: **a host prize that does not show on the target is not a
    prize.**
- **Kept:** the counters, the bench, the ring and the `t175` laws. The
  engine's audio is unchanged, and the song hashes are unchanged.
- **Review note:** the engine-side counters and ring run in the shipping
  build (only the dump is gated), like Levi's `vc_*`. That is a small
  deviation from the "diagnostics cost nothing when off" rule, at
  negligible cost.

## Part B: the tab switch

- **The TAB probe now splits a switch's full draws** into build `fb`,
  replay `fr` and blit `fl`, plus `mui` = `page_us − (fb + fr + fl)`.
  - The `t176` partition law pins this.
  - `mui` was never negative.
  - A no-op control (clicking the already-active tab) reads all zero.
- **Dell result:** `fb + fr` ≤ 19.9 % of `page_us` and ≤ 4.8 ms on every
  tab, so the pre-written stop rule held. A replay skip would save at most
  4.8 ms at the cost of stale-pixel risk, and was not built.
- **But the blit `fl` alone is 4–15 ms per switch** (LEVI, one canvas,
  14993 µs). That prompted a direct throughput probe on the Dell (ABIv11,
  VESA, depth 24):

```
RAM->RAM  BltBitMap        484 us/frame  4332 MB/s
RAM->WIN  BltBitMapRastPort 9530 us/frame  220 MB/s
WIN       RectFill full    9290 us/frame  225 MB/s
```

- **What the probe shows:**
  - every write that reaches the screen runs at about **220 MB/s**, about
    20× slower than a memory copy;
  - even a solid `RectFill` of a 1024×512 window area costs 9.3 ms.
- **The likely mechanism** (from the AROS source, `rom/hidds/vesagfx`): the
  VESA driver draws into a RAM shadow, and `UpdateRect` then `CopyMem`s each
  changed rectangle to a `MapPCI` framebuffer mapping. Nothing in the driver
  sets up write-combining.
  - About 220 MB/s is the typical rate for uncached writes to a PCI
    framebuffer. The Dell's MTRR/PAT state has not been read yet, so
    "uncached" is inferred, not proven.
- **Consequences:**
  - `fl` and much of `mui` (backfill and rack art drawn into the window) are
    the slow path to the display, not MUI logic or our build.
  - The plate cache and the rail page group helped by removing work, but
    every changed pixel still crosses this path.
  - The same limit applies to every repaint on the Dell: meters, knob drags,
    chase lamps.
- **The levers,** in the todo:
  1. read the MTRR/PAT state to confirm;
  2. **upstream AROS:** map the VESA linear framebuffer write-combining,
     which helps every AROS app on VESA hardware (outward-facing, so the
     owner's call);
  3. app side: stop MUI backfilling under canvases on a page switch, so each
     canvas pixel crosses the slow path once instead of twice (not measured
     yet).

## Lane notes

- **`SONG=` must be a full path.** A bare leaf loads nothing and raises a
  modal requester that wedges the run.
- **The window title carries the song leaf**
  (`RIAPP live panel - <leaf>`). `--ui-close` matches by prefix, so close
  with `"RIAPP live panel"`.
- **`RIAPP.LOG` rolls at 256 KB.** A long diagnostic run needs `RIAPP.LOG.1`
  fetched too.
- **Concurrent host builds into the shared `/tmp/ri` tree corrupted it**
  (`/tmp/ri/portable`). Run audits in an isolated worktree with redirected
  paths.
- **One opencode run ended early (about 44 s, `d0224b4`)** with no sign in
  the log of what closed it. Watch for a repeat.
