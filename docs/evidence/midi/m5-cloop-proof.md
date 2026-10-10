# M5 clock-out on the wire: the first run that finished (2026-10-10)

Lane: **riqemu1** (ABIv1, no real-time audio pacing). Script:
`docs/evidence/midi/m5-clockout-cloop.run`. Raw artifact:
`docs/evidence/midi/m5-cloop-riqemu1.log`.

```
sent=1000        clocks=1000       received_f8=1000    lost=0
intervals=999    min_us=20025     max_us=40143        mean_us=33800
jitter_us=20118  verdict=2        other=0  badlen=0   backwards=0
slice_0=58 1=59 2=59 3=57 4=58 5=60 6=59 7=59 8=60 9=470
```

## What this proves

**The camd path carries a clock without losing, reordering or
misframing a single byte.** 1000 sent, 1000 received, `lost=0`,
`badlen=0`, `backwards=0`, and `intervals=999` — 1000 arrivals yield 999
intervals, which is the correct count for a stream whose first arrival has
no predecessor.

**And this is the first M5 proof run to complete on either lane.** MIDIRX
never finished one. That part is the deliverable.

## What this does NOT prove — and the numbers say so themselves

`verdict=2` is **JITTERY**, and I am not explaining that away. Two
independent reasons the interval figures are not evidence about a wire:

1. **riqemu1 has no real-time pacing.** The sender's own `Delay(1)` is
   whatever the emulation manages, so the *input* was not steady.
2. **The reader batches.** `max_us=40143` is almost exactly twice
   `min_us=20025`, and 470 of 999 intervals land in the final slice. That
   is a drain-cadence burst: messages queue and are delivered in clumps,
   so arrival stamps are quantised by the reader, not by the link.

So the honest reading is: **counts are proven, timing is not.** The
no-drift claim stays exactly where it was — pinned on the host by t185
(exactly 33600 ticks in ten minutes at 140 BPM, every interval 857 or 858)
— and the wire timing is still owed a real-time host. riqemu1 cannot
supply one.

The lesson is the same one that killed MIDIRX, arriving from the other
direction: **an instrument that timestamps its own draining reports the
draining.** The EClock stamps come from the M2 receiver task, not from a
poll loop — that part is right — but the task drains a batch at a time, so
on a lane without pacing the batch boundary is the loudest thing in the
data. On the Dell, where pacing is real, the same code measures the wire.

## How this was reached (the failures are the useful part)

| Attempt | Outcome | Cause |
|---|---|---|
| `LISTEN` + separate sender, sequential | `clocks=0` | one script at a time; the listener exits before the sender starts |
| same, with `cmd &` | `clocks=0`, script aborted | `&` does not background here |
| `REM` / `#` / `/*` comments | `object not found` per line | this shell honours **only `;`** (measured, all five candidates) |
| first `LISTEN` build | silent exit 5 | I wrote `argc < 6` and read `argv[5]` for a 5-argument command |
| one `system_reset` mid-session | `RAM:` wiped | a staged binary that had reported `sha_ok=True` was gone; re-stage after any recovery |

`CLOCKLOOP` exists because a lane runs one script at a time: both ends had
to become the two tasks of one process — sender on the main task, receiver
the M2 task — with a real camd link between them.

## Two lane rules learned the hard way

- **Do not use `--exec` for anything multi-step.** It is not a shell (a
  redirect returns rc=0 and writes nothing), and one blocked command there
  wedges the guest agent's entire command path until `system_reset`.
  `--run-script` with a `;`-commented file is the reliable form.
- **`RAM:` does not survive `system_reset`.** Re-stage after every recovery,
  and do not trust a `sha_ok=True` from before one.