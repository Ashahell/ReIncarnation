# M1.1 AHI measurement report (OPEN-09 / OPEN-06)

**Status:** PARTIAL FILL 2026-09-22 — real-hardware fields filled from
the reference-box runs (Appendix B); G5 still OPEN (this turn commits
nothing and touches no spec row — OPEN-09/OPEN-06 row updates ride the
gate commit per the rule below). Appendix A stays as the VOID baseline.

**Reference box (decision 2026-09-22):** Dell Latitude E6320, reference
for ITERATION; ultimate target ABIv1 (acceptance lane). Decision record:
`llm-wiki/raw/articles/2026-09-22-dell-reference-box-iteration-abiv1-target.md`.
Box identity pinned there (BIOS A19; stick sha256 `dbbc8fad…e522f`;
HDA 8086:1C20 rev 04 Dell-sub 0x0492; codec IDT 92HD90 hypothesis).

**Probe:** `audio_io/probe_ahi.c` (Task 5), built via
`bash scripts/ri_build_aros.sh` → `/tmp/ri/aros/probe_ahi`.
Copy to the guest (see procedure), run `probe_ahi`, paste the
`RI_PROBE ...` lines verbatim into §Raw output, then fill the fields.

## Fields (all required by gate G5)

| Field | Value |
|-------|-------|
| Reference-box name (OPEN-06) | Dell Latitude E6320 — named 2026-09-22 by project decision (iteration reference; ABIv1 ultimate target). Spec OPEN-06 row update rides the gate commit, NOT this edit |
| Machine (model / board / RAM) | Dell Latitude E6320, QM67 chipset, i5-2520M; RAM nodes per ShowConfig: 736.0M + 1.2G + 5.6M FAST, 2.0G + 15.0M + 635K CHIP (≈4 GB), 4.7M ROM |
| CPU (model, clock, `Cpu`/`Avail` output) | Intel(R) Core(TM) i5-2520M CPU @ 2.50GHz; Features: FPU MMX SSE SSE2 SSE3 SSSE3 SSE4.1 SSE4.2 AES AVX NoExecute 64Bit Hyperthreading (ShowConfig verbatim) |
| Compiler flags (exact `ri_build_aros.sh` CFLAGS_AROS at run time) | DEVIATION (documented): hardware runs did NOT use `ri_build_aros.sh` (v1 lane). v11 recipe: `x86_64-aros-gcc-16.1.0 -O2 -mcmodel=large -mno-red-zone -mno-ms-bitfields -fno-strict-aliasing -ffixed-r12` compile; link `$V11SDK/lib/startup.o … -nostartfiles -no-pie -L $V11SDK/lib -ldos -lexec`. Probe prints `built=Sep 22 2026 08:00:47` (canonical) / `10:04:29` (variants). `-O2` proven: rebuilt `.o` within 8 B of the recorded one |
| Low-level min buffer (frames @48 kHz) | 64 — `RI_PROBE low_min_frames=64` (ladder 4096..64 all rc=0) |
| Device min buffer (frames @48 kHz) | 0 — `RI_PROBE dev_min_frames=0` (metric counts natural completions only; r1 completes naturally err=0, r2 abort-clean err=-2 by the probe's bound, all rungs) |
| Xrun notes (verify observed/expected/shortfall + any AHIE_* codes) | frames=64: 55/3750 shortfall=3695; frames=128: 55/1875 shortfall=1820; frames=256: 55/935 shortfall=880 (all 5 s windows, rc=0, zero AHIE codes). 11 Hz verdict: observed exactly 55 at all three requests (750/375/187 Hz) → Player rate fixed ~11 Hz, `AHIA_PlayerFreq` accepted-but-not-honored. Full verbatim in Appendix B |
| Chosen backend (low-level vs `ahi.device`, Task 6 input) | INPUT (Task 6 decides): low-level reaches 64 frames with full MixFreq + player-hook exposure (spec §4.2); device path shows no natural-completion advantage (dev_min=0 by metric design, r1 err=0 / r2 err=-2 throughout). No xrun evidence favors the device path |
| AHI mode actually negotiated (id, freq, bits, stereo, hifi, maxch) | id `0x003E0001`, 44100 Hz, 16-bit, stereo, hifi=1, maxch=128 (`best_mode:` + `alloc_audio: OK` verbatim, Appendix B) |

## Hypotheses under test (confirm or correct)

- Register entry 18: latency floor ≥ 20 ms via the shared-device shim
  (960 frames @48 kHz). If `dev_min_frames` > 960 while `low_min_frames`
  is smaller, the shim floor is confirmed and low-level is the Task 6 path.
- AHI dev doc: keep `AHIA_PlayerFreq` below 100–200 Hz (≥ 240 frames
  @48 kHz). Ladder rungs below 256 frames test whether the AROS driver
  honors over-suggestion frequencies or rejects them (`rc != 0`).
- Engine block is LOCKED at 64 frames; the device buffer must be an
  integer multiple of it (spec §2.3). Every ladder rung is ÷64-clean.

**Verdicts 2026-09-22 (reference box, Appendix B):**
- Entry-18 shim floor: NOT confirmed as stated — `dev_min_frames=0`
  (metric counts natural completions only) with `low_min_frames=64`
  cannot satisfy the "dev_min > 960 while low_min smaller" condition;
  r1 completes naturally (err=0), r2 aborts cleanly (err=-2) by design.
  The shim-floor question needs a natural-completion metric, not this one.
- Over-suggestion: NEITHER honored NOR rejected — third outcome.
  Rungs 128/256/64 (375/187/750 Hz) all return rc=0 (accepted) while
  Player delivery stays fixed at exactly 55 ticks / 5 s (~11 Hz).
  `AHIA_PlayerFreq` is accepted-but-not-honored on this driver.
- ÷64-clean holds on hardware (all rungs, all three runs).

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

## Procedure as executed on the Dell lane (2026-09-22, deviation record)

Step 1 above describes the v1/QEMU lane. The reference-box runs used
the ABIv11 recipe instead (v1-SDK binaries fault on ABIv11 at
`exec_83_OpenResource` — see the wiki build-rule record):
1. Host: v11 `x86_64-aros-gcc` + v11 SDK (`-O2`, flags in the table
   above; AHI structs from the v11 source tree) → `probe_ahi_v11`
   (24,064 B, task.resource=0, UND=1). Variants `probe_ahi_{128,256}`
   differ by the ladder line only; repo `audio_io/probe_ahi.c` untouched.
2. `spike put` delivery to `RAM:`, plain run, agent alive; results back
   verbatim (Appendix B). Sessions 285 (pre-reboot), 641 (pinning),
   750 (repro + variants).
3. Characterization runs (128/256) need no audio-hardware changes
   between them; back-to-back in one submit is the proven shape.

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

(SUPERSEDED 2026-09-22 — the reference-box runs pasted here would have
gone in this section; they live verbatim in Appendix B instead, and the
fields above are filled from them. This placeholder is kept so the
original procedure reads intact.)

## Appendix A — Codec-less guest run, 2026-09-21 (NOT the reference box)

**Status note 2026-09-22:** the "fields remain UNMEASURED" reading below
was true at write time; the fields are now filled from Appendix B.
Appendix A stays byte-intact as the VOID baseline.

G5 stays open: the fields above remain UNMEASURED and the spec OPEN-09
row untouched. What follows is the same probe on the driverless QEMU
guest (no `-device intel-hda|ac97`, VOID fallback) with shadow
`ahi.device` + drivers, recorded so the numbers exist somewhere honest.
Method: `spike put` delivery, `PROGDIR:ahi.device` forced load,
`DEVS:` driver shadows, plain `probe_ahi` run, rc=0, agent alive, zero
`IRQHandle` lines. Verbatim `RI_PROBE` output:

```
RI_PROBE best_mode: id=0x001F0002
RI_PROBE alloc_audio: OK freq=5513 bits=32 stereo=1 hifi=1 maxch=128
RI_PROBE lowlevel frames=4096 playerfreq_hz=11 rc=0
RI_PROBE lowlevel frames=2048 playerfreq_hz=23 rc=0
RI_PROBE lowlevel frames=1024 playerfreq_hz=46 rc=0
RI_PROBE lowlevel frames=512 playerfreq_hz=93 rc=0
RI_PROBE lowlevel frames=256 playerfreq_hz=187 rc=0
RI_PROBE lowlevel frames=128 playerfreq_hz=375 rc=0
RI_PROBE lowlevel frames=64 playerfreq_hz=750 rc=0
RI_PROBE low_min_frames=64
RI_PROBE verify frames=64 window_s=5 observed=50231292 expected=3750 shortfall=-50227542
RI_PROBE device: unit 0 PRESENT
RI_PROBE device frames=4096 err1=0 err2=-2
RI_PROBE device frames=2048 err1=0 err2=-2
RI_PROBE device frames=1024 err1=0 err2=-2
RI_PROBE device frames=512 err1=0 err2=-2
RI_PROBE device frames=256 err1=0 err2=-2
RI_PROBE device frames=128 err1=0 err2=-2
RI_PROBE device frames=64 err1=0 err2=-2
RI_PROBE dev_min_frames=0
RI_PROBE SUMMARY low_min_frames=64 dev_min_frames=0 verify_obs=50231292 verify_exp=3750 dev_open_rc=0
```

Readings (codec-less VOID context, NOT reference values): low-level
ladder accepted to 64 frames; playback ticks 50,231,292 vs 3750 expected
(VOID spins unclocked); device writes accepted, r1 completes naturally,
r2 aborted cleanly by the probe's 1 s bound (dev_min counts natural
completions only, hence 0 here; 64 measured with true-WaitIO in
diagnostic runs). Hypotheses: entry-18 shim floor not testable without
hardware (no real unit exists); over-suggestion rungs accepted, not
rejected, by VOID; all rungs ÷64-clean per spec §2.3.

## Appendix B — Real-hardware reference-box runs, 2026-09-22 (REFERENCE)

Box: Dell Latitude E6320, BIOS A19, Kickstart 51.51 / AROS 41.3 /
Exec 51.8 64-bit (identity pinned in the wiki decision record).
Method: v11 build → `spike put` → `RAM:` run (procedure-as-executed
above). All three runs rc=0, agent alive, zero fault lines.

Run 1 — canonical M1.1 (session 285, pre-reboot; `built=Sep 22 2026
08:00:47`):
```
RI_PROBE program=probe_ahi m1.1 target=AROS-x86_64
RI_PROBE compiler=x86_64-aros-gcc-16.1.0 built=Sep 22 2026 08:00:47
RI_PROBE engine_block_frames=64 rate=48000
RI_PROBE ahi.session: OPEN unit=255 base=0x0000000100eed720 version=6
RI_PROBE best_mode: id=0x003E0001
RI_PROBE alloc_audio: OK freq=44100 bits=16 stereo=1 hifi=1 maxch=128
RI_PROBE lowlevel frames=4096 playerfreq_hz=11 rc=0
RI_PROBE lowlevel frames=2048 playerfreq_hz=23 rc=0
RI_PROBE lowlevel frames=1024 playerfreq_hz=46 rc=0
RI_PROBE lowlevel frames=512 playerfreq_hz=93 rc=0
RI_PROBE lowlevel frames=256 playerfreq_hz=187 rc=0
RI_PROBE lowlevel frames=128 playerfreq_hz=375 rc=0
RI_PROBE lowlevel frames=64 playerfreq_hz=750 rc=0
RI_PROBE low_min_frames=64
RI_PROBE verify frames=64 window_s=5 observed=55 expected=3750 shortfall=3695
RI_PROBE device: unit 0 PRESENT
RI_PROBE device frames=4096 err1=0 err2=-2
RI_PROBE device frames=2048 err1=0 err2=-2
RI_PROBE device frames=1024 err1=0 err2=-2
RI_PROBE device frames=512 err1=0 err2=-2
RI_PROBE device frames=256 err1=0 err2=-2
RI_PROBE device frames=128 err1=0 err2=-2
RI_PROBE device frames=64 err1=0 err2=-2
RI_PROBE dev_min_frames=0
RI_PROBE SUMMARY low_min_frames=64 dev_min_frames=0 verify_obs=55 verify_exp=3750 dev_open_rc=0
```

Run 2 — post-reboot reproduction (session 750; `built=Sep 22 2026
10:04:29`): every number identical except lib base
`0x0000000101376c80` (placement varies per boot, always above 4 GB).
Reference numbers are boot-stable.

Run 3 — characterization variants (session 750, same boot as Run 2;
ladder-line-only builds, full verbatim):
```
RI_PROBE program=probe_ahi m1.1 target=AROS-x86_64
RI_PROBE compiler=x86_64-aros-gcc-16.1.0 built=Sep 22 2026 10:04:29
RI_PROBE engine_block_frames=64 rate=48000
RI_PROBE ahi.session: OPEN unit=255 base=0x0000000101376c80 version=6
RI_PROBE best_mode: id=0x003E0001
RI_PROBE alloc_audio: OK freq=44100 bits=16 stereo=1 hifi=1 maxch=128
RI_PROBE lowlevel frames=4096 playerfreq_hz=11 rc=0
RI_PROBE lowlevel frames=2048 playerfreq_hz=23 rc=0
RI_PROBE lowlevel frames=1024 playerfreq_hz=46 rc=0
RI_PROBE lowlevel frames=512 playerfreq_hz=93 rc=0
RI_PROBE lowlevel frames=256 playerfreq_hz=187 rc=0
RI_PROBE lowlevel frames=128 playerfreq_hz=375 rc=0
RI_PROBE low_min_frames=128
RI_PROBE verify frames=128 window_s=5 observed=55 expected=1875 shortfall=1820
RI_PROBE device: unit 0 PRESENT
RI_PROBE device frames=4096 err1=0 err2=-2
RI_PROBE device frames=2048 err1=0 err2=-2
RI_PROBE device frames=1024 err1=0 err2=-2
RI_PROBE device frames=512 err1=0 err2=-2
RI_PROBE device frames=256 err1=0 err2=-2
RI_PROBE device frames=128 err1=0 err2=-2
RI_PROBE dev_min_frames=0
RI_PROBE SUMMARY low_min_frames=128 dev_min_frames=0 verify_obs=55 verify_exp=1875 dev_open_rc=0
```
256-variant: ladder 4096..256, `low_min_frames=256`,
`verify frames=256 window_s=5 observed=55 expected=935 shortfall=880`,
identical P1/P4 lines, `SUMMARY low_min_frames=256 dev_min_frames=0
verify_obs=55 verify_exp=935 dev_open_rc=0`.

Readings (reference context): low-level ladder accepted to 64 frames;
Player delivery fixed at exactly 55 ticks / 5 s (~11 Hz) at all three
requests (750/375/187 Hz) — `AHIA_PlayerFreq` accepted-but-not-honored
(11 Hz verdict, wiki record); device writes accepted, r1 natural err=0,
r2 abort-clean err=-2 by design; negotiated mode 0x003E0001 at
44100/16-bit/stereo.
