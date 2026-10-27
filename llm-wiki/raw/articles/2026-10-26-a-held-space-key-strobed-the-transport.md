# A held space key strobed the transport 1 493 800 times in 21 hours (2026-10-26)

- Source: ReIncarnation session (OpenCode lane), 2026-10-26. Commit `54d7c95`, tag `[levi-midi]` (phase P-TR1).
- Evidence: `docs/evidence/midi/transport-storm/` (log slices, stats, `List` samples, sha256); `tests/unit/t241_transport_toggle_edge.c` (RED 5 → PASS); `tests/mutate_transport_toggle.py` (5/5); `tests/unit/t71_panelui.c` (the pin that licensed the defect); `gui/panelui.c`, `gui/panelui.h`; `scripts/ri_audit.sh`; `docs/evidence/midi/ledger.md` §P-TR1.
- Collected: 2026-10-26
- Published: 2026-10-26
- Spec: `docs/superpowers/specs/2026-09-20-reincarnation-spec.md` (§12.10 GUI, keyboard transport)
- Prior: `raw/articles/2026-10-24-the-keybed-was-one-voice-against-four.md` (P-KEY4, the build that was staged when this was found).

## What was found, and how

Not by a listening round. The Dell lane had been quiet since the P-KEY4 re-stage, and the
check that should be routine — *is the instance I staged still the instance that is
running?* — turned up a 39.5 MB event log. It contained **1 493 800 `TR PLAY`/`TR STOP`
lines and nothing else** after the song had ended:

| | |
|---|---|
| app auto-play | `ev 23  15 TR PLAY` |
| song reached its end | `ev 24  48883 TR STOP` (48 868 buffers of a 48 866-buffer song) |
| the injected key-down | `ev 25  57591 TR PLAY`, 46 s later |
| first auto-repeat | `ev 26  57599 TR STOP` (8 buffers = 43 ms) |
| last toggle | `ev 1493824  14454370 TR STOP` |
| span | 14 396 779 buffers = 76 782.8 s = **21.33 h** |
| toggles | **1 493 800** lines, 19.45/s, 26.5 B each = 39.5 MB of `RAM:` *(corrected at the end: 746 900 presses, 9.73/s)* |

The buffer counter is a *render* counter: it advances whether or not the transport runs
(14.45 M buffers at 187.5/s = 21.4 h = the app's uptime), so the span can be read off the
log without a clock. The run ended the moment `--ui-rawkey 0x40,up` was injected. The proof
that it ended is not the job's `PASS` — it is that `List RAM:RIAPP-EV.LOG` returned the same
**39 582 889 B / mtime `Today 14:10:32`** at 06:41, 06:44:03, 06:44:18 and 06:45:49. The
last `TR` line *is* the last event.

## The mechanism: a toggle that reads its own lamp

`ri_key_decode` hands only a key-**down** to the transport (`gui/keymap.c:64`: a key-up is
`RI_KA_NONE`), and `RI_KT_STOPPLAY` is the one transport command that **toggles** — it reads
the PLAY LED and presses the other button (`gui/panelui.c:133`). `input.device` repeats a
held key's down (the log's 19.45 lines/s was two lines per press — see the correction
at the end; the key rate is 9.73/s), so:

> one key-down with no key-up = a transport strobe at the keyboard's repeat rate.

The trigger here was a harness injection (`--ui-rawkey 0x40`, key-down only; the release is
`0x40,up` and is a separate submit). The defect is not the injection: a finger resting on
the space bar, a stuck key, or a lost release event all do the same thing on real hardware.

## The test that licensed it

`t71_panelui` had this:

```c
RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE, 0) == 1 && !ri_sui_led(&tr, RI_STR_PLAY, 0), "space stops");
RI_ASSERT(ri_panel_key(&pu, RI_RAW_SPACE, 0) == 1 && ri_sui_led(&tr, RI_STR_PLAY, 0), "space plays");
```

Two presses with no release between them. A keyboard never sends that: the second "press"
was an auto-repeat. **The test that should have caught the strobe was the test that wrote it
down as a law.** This is the same class as a census driven from the map instead of from the
drawing: the pin was written from the code's behaviour rather than from the device's.

## The rule: a toggle acts on the press edge

The toggle key's code latches on the press, **that key's own** release clears it, and a
repeat is ignored:

- the guard is the **toggle command**, not `RI_KA_TRANSPORT`: holding `+` or `-` must still
  ramp the tempo, and holding `PLAY` must stay idempotent rather than being gated;
- **another** key's release must not re-arm a held space, or a held key would start toggling
  again the moment any other key came up;
- the release is not itself an action (m5 in the harness makes it one and dies).

The cost of the latch is the same cost `del_held` already pays: a lost release leaves the
key latched. That is accepted, recorded here, and is the reason the release is matched by
code rather than "any up".

## Tests and the harness

`t241_transport_toggle_edge` was written first: **RED 5**, and *exactly* the repeat pins —
the release, the fresh-panel and the tempo counter-pins were already green, so the RED was
the defect and not a scaffold. `t71` now sends the release between its two presses.

`tests/mutate_transport_toggle.py`, scored only on gate return codes, prints the killers:

| mutant | killed by |
|---|---|
| m1 no repeat guard (the defect) | **t241 only** |
| m2 guard the whole transport class | t241, t71 |
| m3 no clear on release | t241, t71 |
| m4 any release clears | **t241 only** |
| m5 the release treated as the action | t241, t71 |

`m1 → t241 only` is the measured statement that t71 could never have caught this: a test
that presses without releasing cannot distinguish a press from a repeat.

## What it cost, and the standing lesson

- The staged P-KEY4 instance was unusable for the owner's volume ear round for 21 h: the
  transport never stayed in either state.
- 39.5 MB of `RAM:` in one log (plus 30 MB of `RIAPP.LOG`) on a 768 MB machine, and every
  pull of it is a 39 MB transfer — the bulk protocol has no ranged read.
- **Harness lesson, standing:** `--ui-rawkey CODE` is key-down only. A paired press is two
  submits, and the proof that the release landed is the log *ceasing to grow*, never the
  job's `PASS`.

## Lane state after

The strobed instance was closed by exact title (`ui-close`, 57 ms) and verified gone by
`--ui-windows` (2 windows, no RIAPP); the storm log was renamed to `RAM:STORM-EV.LOG` (same
size) so the guest keeps its own copy; `RAM:LEVIPROBE.RUN` was re-run (`rc=0`, 15 078 ms).
New instance: exactly one RIAPP window, `build=afa8551`, `SONG load ... bars=151 bpm=140`,
`TR PLAY`, ev log **998 B / 23 lines** with no `TR` line after it, buffers 2919 → 5856 →
8811, `xruns=0`, `overloads=0`.

The toggle fix is committed but **not staged**: the P-KEY4 ear round should hear the build
the ledger names (`afa8551`), and a re-stage would change the build identity under the
owner's ears for a change that cannot affect the keybed's level.

## Correction (2026-10-26 07:20): one press made two lines — and there were two defects

The first version of this article counted the log's `TR` **lines** and called them
toggles: *1 493 800 lines at 19.45/s*. That is wrong by a factor of two, and it puts
the wrong cause on the `STOP` half.

**What the log is.** 1 493 800 `TR` lines, strictly alternating `PLAY`, `STOP`, …
(0 non-alternating adjacent pairs in 1 493 801 gaps) — so **746 900 pairs**, and a pair
is **one key-down**. Interval medians in render buffers: PLAY→STOP **8** (43 ms),
STOP→PLAY **11** (59 ms), PLAY→PLAY **19** (101 ms). A repeat stream has **one**
period; two tight, different distributions is "press, then something 43 ms later".
The direct proof is not statistical: a single **paired** press (`0x40` down,
`0x40,up`) on a transport parked at the song's end produced exactly `TR PLAY` then
`TR STOP` five buffers later and nothing else (`ev-replay.log` ev 73/74).

So the press rate is **9.73/s** (~101 ms, 19 buffers), the log cost **53 B per
press**, and "AROS repeats a held key at ~19/s" was the *line* rate, not the key rate.

**Why the pair.** The transport was stopped at the song's **end** and could not be
moved off it: each press started playback, and `song_poll_end` stopped it a median
8 buffers later. That is a **second defect, P-TR2** — `sync_transport()` early-returned
on an unchanged *state*, and `ri_tr_stop()`'s clicks law changes no state when the
transport is already stopped, so the panel's cursor moved and the engine's did not.
The song played exactly once per launch: two Stop presses and Rewind+Play all still
stopped, and those presses wrote **no `TR` line at all** (`ev-replay3.log`,
`ev-replay4.log` — the Play that followed was stopped in the **same** render buffer).

**The fix.** The propagation decision moved into a pure helper,
`ri_core_transport_sync(panel_state, panel_clicks, &last_state, &last_clicks)`
(`app/core/riapp_core.c`, host-built), which acts when **either** the state or the
stop-click count differs; `sync_transport()` keeps an `s_tr_clicks` shadow beside
`s_tr_state` and still drives MMC out and the engine from one place. A held Stop is
one press as well: `p->tr_stop` latches on the press edge and that key's own release
clears it, separate from `p->tr_toggle` so holding one key cannot silence the other.

**Tests.** `t242_transport_rewind` RED **8** (3 propagation pins, 5 panel pins) →
GREEN; harness **6/6** (m1 state-only sync → **t242 only**; m6 guard-the-class →
t241+t71). `AUDIT 0/0 PASS`.

**The lesson worth keeping.** A sync shadow that compares a widget's *state* is only
correct if the state is the whole of its meaning. `ri_tr_stop()` has memory (the click
count); the shadow had none, so the two drifted by one press, the display looked
right, and the drift only showed when the cursor's position mattered. This is the
same shape as the P-TR1 mistake — a test that recorded a repeat as a press — one
level down: **the thing you compare has to carry the meaning you act on.**
