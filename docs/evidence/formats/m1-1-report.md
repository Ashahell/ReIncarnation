# M1.1 AHI measurement report (OPEN-09 / OPEN-06)

**Status:** TEMPLATE — fields marked UNMEASURED. Filled by running
`audio_io/probe_ahi` on the named reference box (procedure below).
Gate G5 closes only when every field is non-empty AND the spec OPEN-09
row is updated to "measured" in the same commit. Do NOT update the spec
row from this template.

**Probe:** `audio_io/probe_ahi.c` (Task 5), built via
`bash scripts/ri_build_aros.sh` → `/tmp/ri/aros/probe_ahi`.
Copy to the guest (see procedure), run `probe_ahi`, paste the
`RI_PROBE ...` lines verbatim into §Raw output, then fill the fields.

## Fields (all required by gate G5)

| Field | Value |
|-------|-------|
| Reference-box name (OPEN-06) | UNMEASURED — assigned by the project when the M1.1 machine is named (spec §10: "reference box stays unnamed until M1.1") |
| Machine (model / board / RAM) | UNMEASURED |
| CPU (model, clock, `Cpu`/`Avail` output) | UNMEASURED |
| Compiler flags (exact `ri_build_aros.sh` CFLAGS_AROS at run time) | UNMEASURED — record `grep CFLAGS_AROS scripts/ri_build_aros.sh` output + `x86_64-aros-gcc -dumpfullversion` |
| Low-level min buffer (frames @48 kHz) | UNMEASURED — `RI_PROBE low_min_frames=` |
| Device min buffer (frames @48 kHz) | UNMEASURED — `RI_PROBE dev_min_frames=` |
| Xrun notes (verify observed/expected/shortfall + any AHIE_* codes) | UNMEASURED — `RI_PROBE verify ...` + per-rung `err1/err2` |
| Chosen backend (low-level vs `ahi.device`, Task 6 input) | UNMEASURED — lower reliable min buffer wins; on a tie low-level wins (only path exposing MixFreq + player hook, spec §4.2) |
| AHI mode actually negotiated (id, freq, bits, stereo, hifi, maxch) | UNMEASURED — `RI_PROBE best_mode:` + `alloc_audio:` lines |

## Hypotheses under test (confirm or correct)

- Register entry 18: latency floor ≥ 20 ms via the shared-device shim
  (960 frames @48 kHz). If `dev_min_frames` > 960 while `low_min_frames`
  is smaller, the shim floor is confirmed and low-level is the Task 6 path.
- AHI dev doc: keep `AHIA_PlayerFreq` below 100–200 Hz (≥ 240 frames
  @48 kHz). Ladder rungs below 256 frames test whether the AROS driver
  honors over-suggestion frequencies or rejects them (`rc != 0`).
- Engine block is LOCKED at 64 frames; the device buffer must be an
  integer multiple of it (spec §2.3). Every ladder rung is ÷64-clean.

## Fill procedure (exact)

1. On the build host: `bash scripts/ri_build_aros.sh` → `/tmp/ri/aros/probe_ahi`.
2. Deliver `probe_ahi` to the reference box (CD ISO, `spike put`, or
   equivalent — record the method here when done).
3. On AROS, in a Shell: `probe_ahi` (no args, no audio hardware changes
   beforehand; record the AHI Prefs unit/mode selection in force).
4. Paste the COMPLETE serial/console output under §Raw output below.
5. Fill every table cell above from the pasted lines. No derived numbers:
   a number the probe printed is evidence; anything else is hypothesis.
6. Commit this report + the spec OPEN-09 row update together `[gate:G5]`.

## Why UNMEASURED on 2026-09-20 (Task 5 execution record)

- The only AROS guest in this environment (pid 787040, running since
  Sep 18) has NO sound hardware: its qemu command line carries no
  `-device intel-hda|ac97` and no `-audiodev`; `info qtree` via the
  monitor (port 4447) lists 43 devices, zero audio. AROS HDAudio has
  nothing to bind to, so no real AHI unit exists to measure.
- Its 54 MB serial log contains zero AHI/HDAudio driver lines
  (only base64 atcp payload noise matches "ahi").
- The HDD image is lock-held by the running guest; a second `-snapshot`
  boot fails with "Failed to get shared write lock". Booting a copy
  would still measure a driverless path — not the reference box.
- Reference box itself is unnamed (OPEN-06 open by design until M1.1).

## Raw output

(UNMEASURED — paste full `probe_ahi` console output here when run.)
