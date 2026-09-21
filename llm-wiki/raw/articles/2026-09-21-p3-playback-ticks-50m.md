# 2026-09-21 — P3 playback started: 50,503,035 PlayerFunc ticks in 5 s

> Source: session evidence (guest serial PDBG markers, probe source), compiled by agent
> Collected: 2026-09-21
> Published: 2026-09-21

Follow-up to the device-as-library record, closing its open item (starting
playback at low level so PlayerFunc ticks fire).

## Mechanism (from audioctrl.c)

Low-level playback starts via `AHI_ControlAudio(actl, AHIC_Play, TRUE)`,
which routes through `AHIsub_Start` and spawns the driver's timing source.
Without it the mixer idles (observed earlier: 0 ticks over 5 s). Stopped
with `AHIC_Play, FALSE` after the window. Implemented in the repo probe
(`audio_io/probe_ahi.c`, uncommitted) with start/stop around the 5 s Delay.

## Measured on the codec-less guest (VOID fallback)

- P1 `BestAudioID=0x1f0002`, `AllocAudioA` non-NULL, P2 ladder all rc=0
  (unchanged from prior run).
- `P3 Delay done ticks=50503035` over 5 s at low_min=64, then stopped and
  `FreeAudio done`. Zero IRQ lines; no Guru.
- Expected at 64-frame blocks: 5 × 48000/64 = 3750 ticks. Observed
  50,503,035 ≈ 13,467× expected ≈ 10.1M ticks/s: the VOID slave spins
  unclocked (busy loop, no timer), so the verify shortfall is hugely
  negative BY CONSTRUCTION on VOID. Ticks prove the full low-level chain
  (negotiate → allocate → control → start → mixer → PlayerFunc hook →
  stop → free) executes; the COUNT characterizes VOID's fake timing, not
  audio quality.
- Wedge after P3 is the known P4 true-WaitIO(r2) hang (linked second
  request); P1–P3 themselves complete cleanly.

## Standing

- M1.1 low-level on codec-less, complete and honest: `low_min_frames=64`,
  `verify_obs=50503035`, `verify_exp=3750` (shortfall negative by design
  on VOID), zero faults.
- Combined with P4 (`dev_min_frames=64`, CloseDevice returns, rc=0), the
  whole probe is green except the linked-r2 natural completion already
  documented (bounded aborts keep it clean).
