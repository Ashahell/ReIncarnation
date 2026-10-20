# CC out — design note for the owner (R12 O3)

**Status: design only. No code is in this change, and none should be written
until the choices below are decided.** The brief asks for CC out as R6's
second half: "knob moves and automation go out as CC, using the same
controller numbers as the G7 map (Appendix C), so a DAW can record
ReIncarnation's automation" (`docs/superpowers/specs/2026-09-29-interop-requirement.md`
R6).

The wire formatter already exists and has no caller. What is missing is not
the bytes; it is the answer to *which* changes become CC, *where* they are
taken from, and how the app avoids echoing a DAW back to itself.

## 1. What already exists

| Piece | Where | What it gives us |
|---|---|---|
| CC formatter | `midi_io/midi_devout.c` `ri_devout_cc()` | `0xB0 \| ch, cc, val`, clamped; gated by `RIDevOut.enabled` (so `RIAPP_MIDI_DEVOUT` off means **not one byte**); counts `emitted` / `refused` |
| Appendix C | `gui/ctlreg.c`, `midi_cc` column | **486** control rows, **102** with a controller number, **all 102 distinct**, **89** with a real parameter binding, **15** with a range other than 0..127 |
| Panel + inbound moves | `engine/seq/ctlplane.h` | one SPSC queue (GUI writer, render reader) of `{key, value, flags}`; overflow coalesces newest-per-key; drained into `RI_EV_AUTOMATION` in the render |
| Song automation | `engine/seq/autolane.c`, `engine/engine.c` `engine_automation()` | the same `RI_EV_AUTOMATION` event, applied in the render |
| Out ring | `midi_io/midi_out.h` | 256-byte SPSC ring; the **render is the only writer** (`livedrv_devout()`), the sender task drains it **64 bytes every 1 ms** |
| Inbound CC | `gui/midimap.c` `cc()`, one push site `app/riapp.c:1104` | a received CC is scaled to the control's range, written to the UI, and pushed to the same control plane |

Two facts decide most of the design:

1. **Both wanted sources already converge on `RI_EV_AUTOMATION`.** A panel move
   and a song automation event are the same event type by the time the engine
   applies them. A tap in `engine_automation()` therefore sees both, in the
   render task, with no new queue.
2. **An inbound CC is indistinguishable from a panel move once it reaches the
   control plane.** The only place that still knows it came from the wire is
   the one push site at `app/riapp.c:1104`. That is where the echo rule has to
   start.

## 2. Decision A — which changes produce a CC

| Option | Covers | Cost |
|---|---|---|
| A1 Song automation only | loops and recorded lanes | a live knob turn is not recordable; fails R6's "knob moves" |
| A2 Panel moves only | live gestures | recorded automation does not go out; a DAW cannot capture a song's lane |
| **A3 Both (recommended)** | R6 as written | one tap point already sees both; no extra queue |

**Recommend A3.** Inbound CC is **not** a source: it is applied, and it must
not be sent back (Decision E). With A3 the tap can be a single call in
`engine_automation()`, which already runs in the render and already receives
every value that reaches the engine.

## 3. Decision B — which controls

| Option | Result |
|---|---|
| Every row with a `midi_cc` | 102 rows; includes 13 rows with no registry binding (`bind == NONE`) that never reach the engine's automation path |
| **A row with a `midi_cc` and a parameter binding (`bind != NONE`) — recommended** | 89 rows: 303A/B 7 each, 808 28, 909 29, FX 14, mix 15, master 1. Their `(bind, engine_id, voice)` keys are unique, so one engine key resolves to exactly one controller |
| Automatable only | 101 rows; still includes the 13 unbound rows, so this is not the same filter |

Excluded on purpose: the 13 unbound rows (the 808's instrument switches and
selection, the 909's accent level and two unbound controls, and the transport
row), plus every engine key with no registry row (Levi live-note/bend, matrix
and LFO keys). The former cannot be seen by an automation tap at all — their
`ri_panel_ctl_send()` returns "not bound" (t83 pins that) — and the latter are
counted as `unmapped` rather than guessed. Including the unbound rows would
need a separate panel-move tap, which is not worth a second path in v1.

Button-like controls that **are** bound (Waveform, FX on/off, mute) go out as
0 or 127, the same convention `gui/midimap.c` uses on the way in.

## 4. Decision C — channel

| Option | Consequence |
|---|---|
| **G7 remote channel (`settings.channel`) — recommended** | Appendix C is defined on this channel, and the inbound map listens here; one channel, 102 unique numbers, no collisions. A DAW that both records and controls uses one lane |
| Melodic note channel (`note_ch`) | notes and CC share the instrument; but the inbound CC map is on the remote channel, so record and control split across two channels |
| Per-device channels | needs about five settings (303A/B, 808, 909, FX/mix) and a channel per registry section; more setup than R6 asks for |

**Recommend the G7 remote channel for v1.** The cost to state plainly: the
303 notes may be on `note_ch` (default: the Levi channel) while the CCs are on
the remote channel, so a DAW records two channels of one performance. That is
normal for a control-surface lane and is reversible later; per-device
channels should be a separate decision, not a hidden default.

## 5. Decision D — value on the wire

The control's value is 0..127 for most rows, but 15 rows are not (Waveform
0..1, Tune 52..76, PCF pattern 0..53, delay steps 1..32, ...). Two choices:

| Option | Result |
|---|---|
| Send the raw value | Tune occupies 52..76 of the DAW lane; an inbound CC round trip through `gui/midimap.c` does not return the same value |
| **Inverse-scale into 0..127, using the input map's own formula — recommended** | the DAW lane is full-range and `out` then `in` returns the original value |

Use the same arithmetic as `gui/midimap.c` `cc()`:
`v = min + (val * span + 63) / 127` (knob/fader), selectors use
`min + val * (span + 1) / 128`, switches use `val >= 64`. For the inverse,
pick the CC that maps **back** exactly (there are more CC codes than control
values when `span < 127`; the 15 non-range rows need at most 128 candidates).
Build that inverse once per mapped row from the registry, not a second table
in the app.

A test for the implementation phase should walk all 89 rows and every value:
`out` then `in` must return the original. That is the same shape as the drum
channel test that caught the raw-nibble bug (t201): assert the literal wire
bytes, not a value derived from the code under test.

## 6. Decision E — the echo rule (the one that can bite)

A CC that **arrived** over MIDI must not be sent back out. A DAW with
"echo/through" enabled would otherwise bounce it between the two machines.

| Option | Why it does or does not work |
|---|---|
| Global window flag like `s_mmc_echo` | MMC's works because the apply is synchronous in the same task (`app/riapp.c:1168`). A CC push is queued in the main task and drained in the **render** task later, so a set/clear around the enqueue is already gone when the render reads it |
| Drop if the value equals the last value sent | a local move to the same value is wrongly suppressed; a wire echo that differs (after scaling) is not suppressed |
| **Tag the queued message at the one inbound push site — recommended** | the tag travels with the value, so it is correct no matter when the render drains it |

How the tag travels: `RIControlMsg` is an internal struct (no ABI), so give it
a `src` byte alongside `key`/`val`/`flags`, queued by a tagged variant of
`ri_ctl_send()`. `ri_ctl_drain()` puts it into **bit 7 of the low byte of
`RIEvent.flags`**, which is free for an automation event because the value is
7-bit and `engine_automation()` masks it with `127`. The high byte stays what
it is (the LEVI note/bend spare), so those events are untouched. The tap drops
records with that bit set. Only the one inbound push site
`app/riapp.c:1104` uses the tagged variant; every local knob and every song
automation event leaves the bit clear.

MMC keeps its window: it is a different path, applied synchronously, and its
own regression test pins it.

## 7. Decision F — rate, coalescing, and the 64-byte pump

Normal rates are small: a knob drag is 60-125 events/s and one automation lane
is at most 60 values/s. The burst is a **preset change or a chase**, which can
touch all 89 mapped controls at once: 267 bytes, more than the 256-byte ring.

The 64-byte pump is **not** the limit: it runs every 1 ms, so it carries
64 KB/s, i.e. about 21 CCs per millisecond. The limit is the ring's 256 bytes.

**Recommend:**

1. A small **CC tap** (256 records of `{key, value, source}`, about 1 KB) that
   `engine_automation()` writes and the render drains in `livedrv_devout()`.
   Coalesce **newest per key in place** on overflow, exactly like the control
   plane, so a drag never grows the tap.
2. Skip a value that equals the last one already sent for that registry row
   (a 486-byte `last[]`, indexed by row). This turns a held knob and a repeated
   automation value into zero bytes.
3. On each drain, stop when
   `midi_out_pending() + 3 > RI_MIDIOUT_CAP`, leaving the remainder in the tap
   for the next block. The final value is therefore never the one dropped;
   the ring's own occupancy is the budget, so no magic per-block number.
4. Count what does not fit: tap overflow and ring refusal as `ccd`.

**No snapshot in v1.** A DAW that connects mid-song sees the changes after it
connects, not the current state. If the owner wants a snapshot, the cheap
route is a main-task sweep of the 89 rows pushed through the same control
plane on transport start; that is a follow-up decision, not part of this
design's core.

## 8. Decision G — which task writes the ring

| Option | Verdict |
|---|---|
| **Render writes through the existing `midi_out` ring (recommended)** | the render is already the ring's only writer; `engine_automation()` is already in the render; the note tap's drain point (`livedrv_devout()`) is already there. SPSC is preserved unchanged |
| Main task writes `midi_out` directly | breaks SPSC: a knob move could interleave with a clock byte the render is pushing. A lock would put the audio callback behind the GUI |
| A second ring/task for CC | a second consumer of the same CAMD link and a second place that can drop bytes; no benefit for 89 controls |

The render route means the main task's only job is the existing control-plane
enqueue; it never touches `midi_out`.

## 9. Shape of the change, if approved

- `midi_io` (new, pure, host-tested): `struct RICcTap` with
  `ri_ccout_put()` / `ri_ccout_read()` / `ri_ccout_dropped()`, coalescing and
  the `last[]` skip. No CAMD, no allocation.
- `engine/engine.c`: `engine_automation()` calls `ri_ccout_put()` after
  applying the value, with the wire-source bit from the event.
- `app/riapp.c`: set `RI_CTL_SRC_WIRE` at the one inbound push (line 1104)
  and add the `cco/ccs/ccd/ccu/ccw` counters to the `RIAPP clk:` line.
- `app/core/live_driver.c`: drain the tap in `livedrv_devout()`, resolve
  row -> controller and channel -> `ri_devout_cc()` -> `midi_out_put()`.
- `gui/ctlreg.c`: one reverse lookup (`key -> row`) built from the table, not
  a second CC map.
- Tests: mapping round trip for all 89 rows (literal wire bytes, `out` then
  `in`); echo (a wire-tagged value produces no byte); coalescing (a burst's
  final value survives); off by default (not one byte).

## 10. Open questions for the owner

1. **Channel:** the G7 remote channel (recommended) or the melodic channel?
2. **Snapshot on transport start:** follow-up, or wanted in the first cut?
3. **The 12 step-button CCs:** leave them out (recommended) or expose them as
   pattern automation?
4. **14-bit CC** for high-resolution knobs: not in v1 (recommended); the G7
   map is 7-bit.

No code, no spec change, and no test was written for this note. The next move
is the owner's answer to 1-4; then the implementation can be planned as its
own phase.
