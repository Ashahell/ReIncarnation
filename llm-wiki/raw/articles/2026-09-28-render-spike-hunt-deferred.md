# Render-spike hunt: USB hypothesis falsified, deferred (2026-09-27/28)

- Source: ReIncarnation session, 2026-09-27/28 (host measurement + Dell E6320 matched A/B, owner-driven twiddling)
- Collected: 2026-09-28
- Published: 2026-09-28
- Commits: debug-only CF9 + audit gate (this round); evidence
  `docs/evidence/portability/render-spikes-deferred.md`
- Related: `docs/2026-09-24-improvement-opportunities.md` §2.3 (808
  deactivation — since fixed, verified in code) and §2.4 (soft-clip)

## Symptom
Occasional `render_max` ~3x the 5333 us device period (up to ~16 ms)
with a handful of xruns per long interactive Dell session.

## Ruled out (host-measured)
- Render cost bounded: everything-firing session flat ~840 us max over
  4000 buffers; voice scaling 1.4x silent→all; 808/909 deactivation
  paths present and working (no accumulation).
- `-O2` device builds: ~20% faster than `-O0` on host — far from the
  5–9x spike ratio. No toolchain gamble (r12 history stands).

## Falsified on device (matched pair)
- Round A, USB ev-log (204 CTL/STEP/PAT events, 37,202 buffers):
  2 xruns, max 3621 us.
- Round RAM:, `RIAPP_EVLOG=RAM:` override (25 events, 50,580 buffers):
  2 xruns, max 3554 us.
- 8x the USB writes, identical counts and ceilings → per-line USB
  flushes do not drive xruns. (Side finding: `Run`-detached processes
  do not inherit shell-local env on this AROS — foreground start
  required for the override; `RAM:` filesystem denies concurrent opens
  while the USB one allows them.)

## Standing suspects
Input-device event floods (interaction-correlated, unseparated);
long-session accumulation of unknown kind. Priority raise and bigger
buffers explicitly deferred with the hunt.

## Locked rules (owner 2026-09-28)
RIAPP debug builds only (`-O0`, audit-gated on CF9); ev-log defaults
to USB fresh-per-run (`Vk4aros:` + `MODE_NEWFILE`), `RIAPP_EVLOG=RAM:`
stays diagnostic. Resume on 3x-period spikes + xruns returning.
