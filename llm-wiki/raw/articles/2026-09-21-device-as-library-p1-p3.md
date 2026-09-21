# 2026-09-21 — Device-as-library: P1–P3 run without LIBS:ahi.library

> Source: session evidence (guest serial PDBG markers, probe source), compiled by agent
> Collected: 2026-09-21
> Published: 2026-09-21

Correction to the ahi.library-absent no-go record: the premise was wrong.
Per the platform contract (classic AmigaOS/MorphOS/AROS identical): AHI is
provided by `ahi.device`; the supported entry is
`OpenDevice("ahi.device", AHI_NO_UNIT, req, 0)` and the library base is
taken from `req->ahir_Std.io_Device`. There is no `LIBS:ahi.library` BY
DESIGN — its absence on the CD is correct, not a gap. (Our own prior-art
register already described the device path; the probe was written against
the wrong entry.)

## Probe fix (`audio_io/probe_ahi.c`, uncommitted)

P1 now opens the device on `AHI_NO_UNIT` (255U) with a dedicated session
port/request kept alive through P3 (`CloseDevice` after `FreeAudio`),
taking `AHIBase` from `io_Device`. P4 keeps its own separate port/requests.

## Measured on the codec-less guest (VOID fallback, all shadows)

- `P1 BestAudioID=0x1f0002` — real mode ID from the database.
- `P1 AllocAudioA actl=0x000000004a39ef10` — non-NULL audio control.
- P2 ladder all 7 rungs (4096–64) rc=0 → low_min would be 64.
- P3 5 s Delay completes, then `FreeAudio done`; **ticks=0** —
  PlayerFunc never fired. Expected: nothing starts playback at low level
  here (no SetSound/start call in the probe; VOID Start runs on device
  CMD_WRITE, and the low-level path never issues one). The 5 s verify on
  VOID therefore measures "mixer idle", not timing.
- Zero IRQ lines; no Guru. The wedge after P3 is the known P4
  true-WaitIO(r2) hang (linked second request), unchanged.

## Standing

- P1–P3 are AVAILABLE and crash-free via the device entry; the no-go
  record's "unavailable" premise is superseded (annotated there).
- M1.1 low-level on codec-less VOID: negotiate Lars, allocate OK, ladder
  accepted, verify observes 0 ticks (playback never started — honest
  zero, not a failure). Starting playback at low level (AHI_SetSound or
  explicit start) is the open measurement item if PlayerFunc ticks are
  wanted.

> Status (2026-09-21): Resolved — playback started via `AHIC_Play, TRUE`
> (`AHIsub_Start` spawns the driver timing source); measured
> 50,503,035 ticks over 5 s at low_min=64 vs 3750 expected (≈13,467×,
> VOID spins unclocked). See
> `2026-09-21-p3-playback-ticks-50m.md`. The 0-ticks observation above
> stands as the pre-start baseline.
