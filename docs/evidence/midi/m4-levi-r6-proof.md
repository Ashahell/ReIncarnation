# M4 + R6 combined proof — and the clock-out lamp (2026-10-10)

**Staged for the owner to run. Nothing here is a claim until the owner's
report and the logs are in.**

## Status: BLOCKED ON THE LANE, not on the work

Both lanes were pinged for this run and **both timed out** — riqemu1
(`spike_spool_priv`) and the Dell (`spike_spool_laptop`), 30 s and 45 s
waits, jobs left queued. Nothing is deployed yet. When a lane answers, this
is a `--put` and a `--run-script`, and the owner runs the ear part.

## What is staged (built, NOT yet deployed — the lane is down)

Built at `a0542d3` + this slice. **ABIv11**, because the Dell is the v11
lane — the two ABIs are not interchangeable and a v1 binary on the Dell has
been the wrong-binary trap three times now, so the hashes are recorded here
to be checked before any PUT.

| file | build | bytes | sha256 (20) |
| --- | --- | --- | --- |
| `RIAPP.M4` | ABIv11, current | 1183344 | `3c028b811a689cc33163` |
| `MIDISEND` | unchanged since M0 | 66904 | `811878fb5f0c3d6b20c6` |
| `m4-levi.mid.txt` | unchanged since 2026-10-09 | — | — |
| `m4-levi-r6.run` | new | — | — |

## One session, three questions

They share a boot, the same two processes, and the same channel, and two of
the three are only answerable by eye.

| | question | who answers |
| --- | --- | --- |
| **A** | M4 — the Leviasynth on its own channel, **inbound** | owner + ev-log |
| **B** | R6 — melodic note output on the Leviasynth channel, **outbound** | owner + listener log |
| **C** | the clock-out lamp at (253, 40) | owner's eye |

### A. M4 inbound — unchanged from the 2026-10-09 staging

`m4-levi.mid.txt` sends on **channel 2** unless a line says otherwise, and
covers: a held chord then a scale with rising velocity; CC 55 and CC 74
(digital and analog filter, dark then bright); CC 1 mod wheel; pitch bend;
channel aftertouch; and **the no-collision law from both sides** — CC 7 on
channel 2 must move nothing, CC 55 on channel 1 must not open the
Leviasynth's filter; then a channel-1 note, which must be silent.

### B. R6 outbound — new, and this is the first run that puts R6d on a wire

Owner decision 2026-10-10: **"Leviasynth for melodies."** With no explicit
`RIAPP_MIDI_NOTECH`, the melodic output **follows the Leviasynth channel**
(`midi_settings_note_ch()`, pinned by t202). So RIAPP's own 303 patterns go
out on **channel 2** — the same channel M4 receives on, in the other
direction.

**To hear it, a Leviasynth patch must be loaded on channel 2 in whatever is
receiving.** RIAPP sending a bass line and a Leviasynth patch playing it is
the point of the decision, and it is worth saying plainly that the R6
melodic source is the **303**, not the Leviasynth: `RIEvent.device` 0/1 is
303A/303B. What lands on channel 2 is 303 content played by whatever patch
you have there.

**Listen for:** notes on channel 2 that line up with the pattern RIAPP is
playing, drums landing on **channel 10** (GM percussion, and not the melodic
channel — a drum note there would be a bug), and **one** program change when
note output is enabled and not one per note.

### C. The clock-out lamp

`{ TR(15), RI_GEO_RECT, 0, 253, 40, 12, 12 }` in `gui/panelgeo.c`, with the
`R(TRANSPORT, 15, LED, "", "Clock Out", ...)` row in `gui/ctlreg.c`.

- **Placed by arithmetic, never seen.** It sits between Sync (188, 40) and
  MIDI In (318, 40) on the row the manual puts the other two indicator lamps
  (p. 144–145); x=253 splits that span and the y=40 row is otherwise empty
  between them.
- **The question to answer: does it read as "the third lamp", or as
  clutter?** Either answer moves it. If it reads as clutter, the alternatives
  are the empty row under the transport displays or a line of three with
  tighter spacing.
- **It lights only while bytes actually flow** —
  `ri_str_clkout_set(s, sending, sent_total)` is `(sending && sent_total >
  0)`, deliberately unreachable from `ri_str_indicator_set()` so no caller
  can switch it on by hand. **So B is what makes it light:** look at it while
  R6 output is running, and confirm it is *dark* before the sender starts.
  A lamp that is lit when nothing is being sent is worse than no lamp.

## How to run it

From a shell on the Dell, once the files are in `RAM:`:

```
setenv RAM:RIAPP_MIDI_IN=RIAPP
setenv RAM:RIAPP_MIDI_DEVOUT=1
setenv RAM:RIAPP_MIDI_CLKOUT=1
setenv RAM:RIAPP_MIDI_LATMS=8
RIAPP_DEMO=0 RAM:RIAPP.M4
```

Then in a second shell:

```
RAM:MIDISEND riapp RAM:m4-levi.mid.txt
```

(`RIAPP_MIDI_NOTECH` is deliberately **unset** — that is what makes the
melodic output follow the Leviasynth channel. Setting it explicitly would
test the other branch.)

## The counters I will read

- `LEVI chan remote=1 levi=2 devices=2` — the table bound as E0 says.
- `LEVI note <n> on/off vel <v>` per note, matching on/off counts.
- `LEVI perf <k> val <v> hi <h>` per bend/AT/wheel; `LEVI param <key>=<val>` per CC.
- session `xr=` — must stay 0.
- **`DEV out=1 ch=1 prog=38`** — the new startup line. `ch=1` is the
  Leviasynth's channel 2 in 0-based terms; if it reads `ch=0` the Leviasynth
  channel is 1 and the two devices would share a channel, which the E0 rack
  rule forbids.
- `RAM:R6LISTEN.LOG` — what R6 actually put on the wire: channel-2 notes,
  channel-10 drums, and **how many program changes**. Exactly one expected.

## Known limits, stated before it runs

- **No USB-MIDI device on the Dell**, so this is the camd cluster and not a
  cable. The physical wire remains a separate owner proof.
- **Scripted, not played.** Latency and feel are not measured.
- **The lamp needs a light on it.** If the room is bright or the lamp is
  small on screen, "dark before, lit during" may be hard to judge and a
  screenshot of the transport section at the right moment is better.
- **`midi_drain()` still has no host test** — it was invisible to t184 until
  the fix in `2a17e90`. A green ev-log is necessary, not sufficient.
- **R6's melodic channel was changed this slice** and its first wire run is
  B. If the listener log shows melodic notes on a channel other than 2,
  that is a real finding and the counters will show it.