# AHI lifecycle guru (2026-09-27, Dell session 3, build 0f1e290)

Render-task guru (privilege violation, `LibNextTagItem+0x8`) found
parked after a normal close; the next open fell back clean `err 7`
(null backend, panel fully usable — degraded mode as designed).
Recovery: Suspend the requester, close, start again — sound back, NO
reboot needed.

Suspect: teardown path (stop/FreeAudio/CloseDevice) wedging the driver
for the next open; our tag lists are stack-valid on all paths, so a
fault inside `LibNextTagItem` with valid caller lists reads
driver-side. Open: exact faulting call unidentified from the pixels
available; harden only with attribution.
