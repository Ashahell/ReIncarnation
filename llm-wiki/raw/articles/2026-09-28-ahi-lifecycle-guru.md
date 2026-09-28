# AHI lifecycle guru: teardown suspect, err-7 fallback confirmed in the field

- Source: owner evidence commit `3db1c92`, 2026-09-28 (Dell session 3, build 0f1e290)
- Collected: 2026-09-28
- Published: 2026-09-28
- Evidence: `docs/evidence/portability/ahi-lifecycle-guru.md` (13 lines, quoted below)

## Finding

Render-task guru (privilege violation, `LibNextTagItem+0x8`) found
parked after a normal close; the next open fell back clean `err 7`
(null backend, panel fully usable — degraded mode as designed).
Recovery: Suspend the requester, close, start again — sound back, NO
reboot needed.

## Suspect (open)

Teardown path (stop/FreeAudio/CloseDevice) wedging the driver for
the next open; caller tag lists are stack-valid on all paths, so a
fault inside `LibNextTagItem` with valid caller lists reads
driver-side. Exact faulting call unidentified from the pixels
available; harden only with attribution.

## Why this matters here

- First field confirmation of the bounded-open design: a dead render
  task surfaces as err 7 with a usable null panel instead of a wedge
  (`audio_ahi_live.c` handshake). The design held on real hardware.
- Narrows the remaining driver work: open/init faults are now
  fenced by err 7, but the *teardown* path (close → reopen) has its
  own suspect — same class (driver-side read), different call.
  Next step when owned: identify the exact faulting call (needs a
  readable More page at close time), then attribute before hardening.
- Recovery ladder for the field, cheapest first: Suspend + restart;
  reboot only if that fails.
