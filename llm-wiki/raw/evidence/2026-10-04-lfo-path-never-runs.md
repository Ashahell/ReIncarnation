# The LFO path never runs: `lfo_iters = 0` — verbatim, 2026-10-04

**Ingested:** 2026-10-04 into ReIncarnation `llm-wiki`
**Source:** Dell E6320, ABIv11, `-O0`, `RIAPP-vcount.v11`, song *The Knife*.
**Provenance:** verbatim log lines and stage tables; derived values show their
components.
**Recorded in:** [the LFO path never runs, and the cost is the voice render itself](../articles/2026-10-04-the-lfo-path-never-runs.md)

## 1. The build

```
HEAD now: 36a5502  (the waiting v11 binary was labelled 6541259 but built from a DIRTY tree)
AROS RIAPP v11 BUILD OK (/tmp/opencode/RIAPP-vcount.v11, 880608 bytes, build=36a5502, r12moves=286)
AROS RIAPP v11 BUILD OK (/tmp/opencode/RIAPP-vcount.v11, 1100264 bytes, build=36a5502, r12moves=41)
```

The first rebuild **omitted `RI_V11_OPT=-O0`** and came out at `-O2` — 880608 B,
`r12moves=286`. The lane record is explicit that the baseline stays `-O0` to match
what was proven to run, because *an `-O2` deploy changes CPU cost by ~24 % by size
alone and would confound the metrics being measured*. Rebuilt at `-O0`:
1100264 B, `r12moves=41`.

Also noted: the binary waiting from the previous session carried
`build=6541259` while being compiled from a **dirty** tree, so its embedded hash
understated its contents. The wiki already records that a deployed binary's
`build=` label is evidence of intent rather than provenance; this is another
instance.

## 2. The song that actually loaded

```
song: RIAPP song Vk4aros:ReIncarnation/songs/local/the-knife/the-knife.rbng: 104 bars a[t 140 BPM]
```

**Not Zombie Nation.** The run was launched as
`Run RAM:RIPP-VCOUNT SONG=RAM:zombie-nation.rbng`, and the explicit override did
**not** take effect — the demo path found *The Knife* on `Vk4aros:` instead.

That contradicts the documented precedence in `app/riapp.c`:

> an explicit `SONG=`/`PLAYLIST=` always wins, because an argument is a decision
> and this is a default

The likely cause is the guest Shell mangling an argument containing both `=` and
`:`, but that is not established. **It is a finding about the lane's argument
passing, not about the precedence rule.**

The Dell has no song library where riqemu1 does:

```
[exec] 'dir SYS:Classes/ReIncarnation/Songs' -> rc=20 (51 ms)
       Could not get information for SYS:Classes/ReIncarnation/Songs
       object not found

[put ] .../zombie-nation.rbng -> SYS:Classes/ReIncarnation/Songs/zombie-nation.rbng
[submit] error: ProtocolErr: put_begin failed: cannot open temp file
```

`put` cannot create the missing directory, and the guest Shell has no `mkdir`
(only `Run`, `SetEnv`, `echo`, `dir`, `status`, `wait`, `Break`, `Getenv`). The
song was therefore staged to `RAM:zombie-nation.rbng` instead.

## 3. The measurement — the answer

Work counters summed over every logged block:

```
=== WORK SHAPE inside levi_voice_render_sum_stereo ===
  blocks logged            : 28  (14 with any activity)
  voice_calls              : 13260
  voice_active             : 1844  -> 0.14 active voices per call (8 slots)
  lfo_samples              : 0
  lfo_iters                : 0   <-- THE ANSWER: the LFO path never executes
  fx_samples               : 0   (levi_fx_mod never runs)

  LFO iterations : voice calls = 0.000 : 1
```

Busiest single block:

```
  samples 64 | voice_calls 512 | voice_active 256 (4.00 active voices/sample)
  lfo_samples 0 (0% of samples) | lfo_iters 0 | fx_samples 0 (0%)
```

**Peak polyphony is 4 voices of 8 slots** (256 active of 512 calls).

## 4. The stage table

```
=== settled stage table (dstg n=197645) ===
  levi          597 us
  lev-arp         4 us
  lev-seq         4 us
  lev-voice     543 us
  lev-mix         6 us
  lev-tempo       4 us
  lev-probe       4 us
  lev-probe2      4 us
  block         1208 us
```

With this lane's floor at **4 µs** (both controls read 4):

```
  inner stages: 6 ; child tax = 6 x 4 = 24 us
  residual = 597 - 565 = 32 us          (5.4 % of levi)
  real levi  = 597 - 24 = 573 us -> 47.4 % of block   (reported 49.4 %)

  lev-arp     4 us   1 % of levi, 0 % above its own floor
  lev-seq     4 us   1 % of levi, 0 % above its own floor
  lev-voice 543 us  91 % of levi, 94 % above its own floor
  lev-mix     6 us   1 % of levi, 0 % above its own floor
  lev-tempo   4 us   1 % of levi, 0 % above its own floor
```

## 5. Cross-lane, both corrected for the child-count tax

| | riqemu1 (Zombie Nation) | Dell (The Knife) |
|---|---|---|
| block | 538 µs | 1208 µs |
| LEVI reported | 263 µs | 597 µs |
| LEVI corrected | 227 µs | **573 µs** |
| **LEVI share, corrected** | **42.2 %** | **47.4 %** |
| `lev-voice` share of LEVI | 81 % | **91 %** |
| control / pair cost | 6 µs | 4 µs |
| residual inside LEVI | 8 µs | 32 µs |

**The two lanes agree closely once the tax is removed** — 42.2 % and 47.4 % —
from two different songs on two different clock domains. And `lev-voice` is 81 %
and 91 % of LEVI: **`levi_voice_render_sum_stereo` is the cost on both.**

## 6. Two lane corrections this run produced

**`exec` does NOT die while RIAPP runs on the Dell.** With `RAM:RIPP-VCOUNT` live
as Process 8 and the panel up:

```
[exec] 'status' -> rc=0 (48 ms)
```

The "RIAPP running kills the guest exec channel" defect is **riqemu1-specific**,
not a general lane law. That widens the recorded scope of a defect that had been
stated generally.

**The `Software Failure!` requester clears on a VM restart, not a human click.**
riqemu1 was restarted (ACPI `system_powerdown`, then relaunch) and came back with
no requester:

```
  restarted: riqemu1 up, window mapped, P6 1280 1024 255
[exec] 'status' -> rc=0 (10 ms)
```

That refines the previous record's "needs a human click, or a guest reboot": a
**VM** restart suffices, and it needs nobody at the keyboard. The finding that
`sendkey` does not dismiss it stands unchanged.