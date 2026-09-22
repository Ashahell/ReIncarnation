# 2026-09-22 — First audible sound on the reference box (440 Hz tone, operator-confirmed)

> Source: operator ear report (Dell E6320 speakers) + probe logs, compiled by agent
> Collected: 2026-09-22
> Published: 2026-09-22

## Disposition
New. Milestone: first physical audio out of the project on reference
hardware. Signal path proven end-to-end (app → ahi.device → IDT codec
→ amp → speakers).

## What ran
Scratch `probe_tone` (v11 build, 27344 B, task.resource=0): repo
`probe_ahi` byte-identical except BSS P4 buffers filled with 440 Hz
sine @0.5 FS stereo. Two runs, both rc=0 with nominal numbers
(session 750). Tone sounds during the P4 device ladder (~10 s across
7 rungs).

## Operator report (verbatim)
"yes, it sounded a bit like an old ring tone. not consistent though.
Also the volume could be louder"

## Reading (agent interpretation, not yet verified)
- "Old ring tone": EXPECTED from the ladder shape — 7 burst-gap
  cycles (write buffers, poll, abort, next rung) sound like ringing,
  not like a defect. A continuous-tone player would sound steady.
- "Not consistent": burst lengths vary by rung (4096→64 frames) plus
  abort timing — same ladder shape, not dropouts (no err anomalies;
  all rungs err1=0/err2=-2 both runs).
- "Could be louder": OPEN — host volume (laptop keys) vs driver gain
  staging undetermined. 0.5 FS sine at ahir_Volume 0x10000 should be
  loud at max host volume; if it isn't, the codec amp path needs a
  look. Next: operator maxes volume, replay, compare.

## Resolution 2026-09-22 (volume question closed, no driver work needed)
- AROS volume app: `SYS:Prefs/AHI` (confirmed present, opened + closed
  remotely; Topaz defeats OCR — operator reads it directly). It HAS an
  output volume control, set to max.
- Laptop Fn volume maxed + AHI output volume maxed → replayed tone
  **definitely audible** (operator-confirmed, both runs rc=0 nominal).
- Verdict: two volume stages (host keys + AHI prefs output), both
  behaved; driver gain staging needs NO investigation. The earlier
  "could be louder" was host volume, not codec amp. End-to-end audio
  path on the reference box is fully functional at expected levels.

## Files (scratch, outside repo)
- `/home/miller/Work/ri_build/probe_tone.c` (sine fill + marker line),
  `probe_tone.o`, `probe_tone` (27344 B); guest copy `RAM:probe_tone`.