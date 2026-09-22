# 2026-09-22 — Task 6 reconciled with M1.1: latency floor 64 (was 256 hypothesis) + test-harness exit fix

> Source: session evidence (test output, audit), compiled by agent
> Collected: 2026-09-22
> Published: 2026-09-22

## Disposition
New. TDD work unit (RED watched, GREEN verified, audit 0/0).

## Backend choice (from measurement, recorded)
LOW-LEVEL path, confirmed (not just defaulted): ladder accepted to 64
on Dell + ABIv1, driver exposes MixFreq + Player hook (spec §4.2), no
device-path advantage measured. `ahi.device` stays the fallback.

## Change (minimal)
- `audio_io/audio.h:44`: `RI_DEVICE_FRAMES` 256u → 64u (floor =
  max(engine-block lock 64, measured ladder min 64)); header + code
  comments flipped HYPOTHESIS → MEASURED with date/lanes.
- `tests/unit/t6_w1backend.c`: assert `lat == 64` (was 256). RED
  watched first: `FAIL ... latency 256, want 64` (correct reason).
- `scripts/ri_audit.sh` Phase 6 gate: pins 64u (was 256u).
- `scripts/ri_build_host.sh` test mode: propagate test exit
  (`"$OUT/$2" || exit 1`) — the trailing `BUILD OK` echo masked ALL
  test failures from the audit's `|| FAIL` (same defect class as the
  old `build_host_icd.sh` exits-0; verified: exit=1 while RED, 0 GREEN).

## Proof
- GREEN: `PASS w1backend` — one-renderer SHA proof intact at 64-frame
  chunks (file vs live-stub byte-identical); fallback string pinned;
  xruns 0.
- Full `ri_audit.sh`: 0/0 PASS (all phases incl. G6 gate + AROS
  compile + hygiene).
- Buffer safety at 64: `fbuf[RI_DEVICE_FRAMES]` shrinks (fine); chunk
  guard unchanged in shape; no other 256-dependents (grep clean:
  remaining 256s are a `char line[256]`, the define's own history
  note, and the probe ladder itself).

## Files (uncommitted)
- `audio_io/audio.h`, `audio_io/audio.c`, `tests/unit/t6_w1backend.c`,
  `scripts/ri_audit.sh`, `scripts/ri_build_host.sh`.