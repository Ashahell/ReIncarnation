# Render spikes: USB hypothesis falsified, deferred (2026-09-27/28)

## Symptom
Occasional `render_max` ~3x the 5333 us period (up to ~16 ms) with a
handful of xruns per long interactive Dell session. Heartbeat evidence
across builds; never in short runs.

## Ruled out (measured, host)
- Render cost is bounded: everything-firing session flat at ~840 us
  max over 4000 buffers (no accumulation; 808/909 deactivation works).
  Voice scaling only 1.4x silent→all.
- `-O2` device builds: only ~20% faster than `-O0` on host — nowhere
  near the 5–9x spike ratio. No toolchain gamble (r12 history).

## Falsified on device (matched A/B, owner-driven)
- Round A (USB ev-log, 204 events, 37k buffers): 2 xruns, max 3621 us.
- Round RAM: (`RIAPP_EVLOG=RAM:`, 25 events, 50k buffers): 2 xruns,
  max 3554 us.
- 8x the USB write events, identical counts and ceilings → per-line
  USB flushes do NOT drive xruns. Ceilings (~67%) rock-stable across
  runs; the 16 ms monsters appear only in 30+ minute heavy sessions.

## Still suspects (ranked)
1. Input-device event floods during twiddling (interaction-correlated,
   above render priority; unseparated — present in both rounds).
2. Long-session accumulation of unknown kind.
3. Priority: render task at 10; raising is standard practice but taken
   without a proven preemptor — deferred with the hunt.
4. Bigger buffers (latency tradeoff) stay the fallback; owner call.

## Rules locked by owner (2026-09-28)
- RIAPP ships debug builds only (`-O0`; audit-gated on the CF9 recipe).
- Ev-log defaults to the USB stick, fresh per run (`Vk4aros:` probe +
  `MODE_NEWFILE`); `RIAPP_EVLOG=RAM:` stays as a diagnostic override.
- Resume trigger: 3x-period spikes + xruns returning in interactive
  sessions ("another similar situation").
