# The song name was always in the log; the T: rule was always in the code (2026-10-03)

- Source: ReIncarnation session, 2026-10-03 (opencode lane, riqemu1)
- Collected: 2026-10-03
- Published: 2026-10-03
- Corrects: [the sticky-log location record](2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md) — its `fallback: RAM:` line was stale and was the direct cause of a false diagnosis
- Prior: [One requester, two instances, and a silent null backend](2026-10-02-one-requester-two-instances-and-a-silent-null-backend.md), [AHI on riqemu1](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md)

Three owner-reported defects. Each turned out to be one mistake, and two of the
three mistakes were mine, made hours earlier in the same lane.

## 1. "The user can't see which song is playing"

True, and true since songs landed. The log has said

```
RIAPP song SYS:Classes/ReIncarnation/Songs/zombie-nation.rbng: 151 bars at 140 BPM
```

for as long as songs have existed, so the only way to find out what was playing
was to pull the log off a guest by hand — which is exactly what I had been doing
to check playback.

**Fixed as a plate caption, beside `PATTERN` and `SONG MODE`.** Deliberately
**not** a ctlreg entry: a `RI_STR_*` control has to be automatable,
stepper-clampable and MIDI-mappable to satisfy ctlreg, and a read-only caption
wants none of those. It would also put a non-numeric value into a display that
renders numbers.

### Placement took three attempts, and only the guest could settle it

| attempt | where it went | what caught it |
|---|---|---|
| `PX(560)` wide, y `PX(196)` | the SYNC/MIDI legend row | reading the code |
| `PX(900)` wide, y `PX(197)` | inside the transport bar | reading the code |
| `PX(1560)`, y `PX(190)` | the free row at the plate foot | a **capture** |

The third one is right by arithmetic — the plate is 1684x208 and its own text
sits at 45 and 150, so 190 is the last row clear of the bar — and confirmed by
capture on riqemu1, where `zombie-nation...` is legible on the plate.

**The arithmetic was available before the first attempt.** `PX()` halves plate
units at zoom 1, and I checked that only after guessing twice. Measuring the
thing before touching it is the whole lesson, stated here because I did not
follow it.

## 2. "RAM: is a terrible place to store logs. We told your siblings multiple times to use T:, but somehow that never is documented"

**It was documented — in the code. That is the failure.**

`platform/pal/ri_pal_sticky.h` has carried the rule and a careful 40-line
rationale since 2026-10-02, including *"T: is RAM-backed on this guest, so this
is a better place to write, not a durable one."*

The **wiki** said:

```
Vk4aros:   USB0:   USB1:   UMSD0:   UMSD1:   USBDISK0:      fallback: RAM:
```

That line is stale and it is not a gap — it is an active hazard. It is what
manufactures the false diagnosis.

**I walked into it the same night.** When `RAM:RIAPP.LOG` looked missing I
"fixed" it with `setenv RIAPP_LOG RAM:` — the documented escape hatch, used for
exactly the wrong reason. `T:` was working the whole time and already held
`RIAPP.LOG` at 23159 B from the run before I touched anything.

A sibling following that wiki line lands on `RAM:`, finds the log gone after a
reboot, and concludes the fallback is broken. That is a loop, not a document.

**The rule now, where someone meets it:**

> never hand-place a log. `ri_pal_path(RI_PATH_TEMP, ...)` owns the decision and
> `app/riapp.c` asks it. `RIAPP_LOG=<vol>` is a deliberate escape hatch and is not
> the answer to "the log is missing."

### A real bug found on the way: `T:` was returned unprobed

```
case RI_PATH_TEMP:
    if (ri_pal_sticky_vol(out, cap) == 0) return 0;
    s = RI_PAL_STICKY_FALLBACK;      /* "T:" -- assumed, never checked */
```

A guest with no scratch disk was handed `T:` and the log went **nowhere** —
strictly worse than a RAM: log that at least dies at the next reboot. `T:` is now
the **last entry in the probe table**, so it is checked like everything else, and
`RAM:` is the fallback:

```
Vk4aros:  USB0:  USB1:  UMSD0:  UMSD1:  USBDISK0:  T:      fallback: RAM:
```

Owner: *"RAM: can be a fallback in case T: isn't available."*

Same lesson as the `movaps` patch earlier the same day: **count what is emitted,
not what is meant.** `t153` now pins `T:` in the table exactly once and last.

## 3. Zombie Nation is the default demo song

Loaded from `SYS:Classes/ReIncarnation/Songs/` — where `RI_PATH_SONGS` already
points, so no new location was invented.

**Only when the command line chose nothing.** An explicit `SONG=` or
`PLAYLIST=` is a decision; this is a default, and a default does not overrule a
decision.

**A miss logs one line and carries on.** riqemu1 has no song library deployed,
and that must not raise a requester — the built-in demo pattern is the correct
state there. `RIAPP_DEMO_SONG=<file>` names a different arrangement and
`RIAPP_DEMO=0` turns the attempt off.

Verified on riqemu1:

```
RIAPP song SYS:Classes/ReIncarnation/Songs/zombie-nation.rbng: 151 bars at 140 BPM
audio: AHI low-level mode=0x00390004 mix=48000 Hz buffer=256 frames period=5333 us
```

## The test found two bugs in my own code

`t159_tr_song_name` is new and gated. It exercises the real functions, and it
caught:

1. **The fit dropped its ellipsis.** It tested the bare prefix against the room
   and returned it, so a name that had been cut off came out looking complete —
   the worst of both, because nothing on screen says "this is truncated".
2. **It could emit an ellipsis wider than the room.** Now measured before it is
   written.

### The test-shape lesson

The first version of `t159` re-implemented the fit **inside the test**.
**18 of 19 mutants survived.** A mirror of the thing under test cannot fail when
the thing changes, so `ri_art_tr_copy` and `ri_art_tr_fit` are now exported and
the test calls them.

The same applies to bounds. A copy that loses its bound still returns the right
string for every input short enough to matter, so **no value assertion can see
it** — the first mutation run survived 14 of 15 for exactly this reason. The
destinations are now **canary-guarded**, which is what turned "the copy loses its
bound" from a survivor into a kill.

One more trap, worth recording because it cost a round: `\xa5` written through a
shell heredoc became a literal `\xa5` in the C source, so the canary compared
against the wrong bytes and failed on correct code. The canary is a byte array
now. **A test that fails on correct code is a broken test, and the first
instinct to distrust it is right.**

### Honest tally

- `mut_fixM8` (sticky list): **7/7 killed**.
- `mut_fixM9` (song name): **6/17 killed**. The survivors are the draw-path guards
  in `ri_art_bg_tr` — "draw only when a name is set", "draw only when the fit
  produced something", "left-aligned" — which are **not reachable from a host
  test** because they need a framebuffer. They are pinned as the contract they
  encode rather than as a second copy of the logic, and they are recorded as
  survivors rather than dropped from the set. Closing them properly needs a
  display-list assertion on the transport section, which `t93`'s raster goldens
  are the right place for and which this change did not add.

## Still open, unchanged

- **Playback is 8.1 % slow and it is not a performance problem.** QEMU's AC97 is
  fixed at 44100 Hz and the guest runs 48000, so everything is resampled down:
  `44100/48000 = 0.91875`. The guest's own counters rule out load —
  `xruns=0`, `render_max` 3113 us against a 5333 us period (58 %), `load=3/1000`,
  and a pacing ratio of exactly 1.00. Running AHI at 44100 removes the conversion.
- **Injected clicks do not reach the guest on riqemu1.** `--ui-click` does not
  switch a tab or press Play; `ui-capture` and `ui-windows` work. Switching the
  active pointer to PS/2 (`mouse_set 2`) made no difference. A click had to be
  done by hand to confirm audible audio.
- **RIAPP running kills the guest's `exec` channel** (`rc=1`, no output);
  closing it restores `exec` immediately. This silently invalidates any harness
  that reads a log while playing.

## See Also

- [the sticky-log record, now corrected](2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md)
- [AHI on riqemu1: the loader ignores sh_addralign](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md) — the other same-day correction, and the same shape of mistake: a plausible mechanism and a confident conclusion that the arithmetic did not support