# riqemu1 quarantine reverted; sound proven on the riaudio lane (owner 2026-09-30)

- Source: ReIncarnation session, 2026-09-30 (opencode lane; owner reported the QEMU Software Failure + a host `t136_levi_stero` crash, asked for screenshot/analysis/fix)
- Collected: 2026-09-30
- Published: 2026-09-30
- Prior: [2026-09-30-riqemu1-wide-modes-parked-audio.md](2026-09-30-riqemu1-wide-modes-parked-audio.md) (quarantine + parking superseded below), [2026-09-28-audio-in-qemu.md](2026-09-28-audio-in-qemu.md) (proven riaudio lane + fixed-driver recipe)
- No repo code changed this slice (lane file ops only); P6c tree untouched.

## Host crash: intentional mutant, no action

- `t136_levi_stereo` SIGSEGV, PID 335621, 21:38:24 CEST, NULL deref (`segfault at 0`), 29 GiB RAM free (no OOM). Timestamp = the session's own null-guard mutant run (`if (0)` in place of the render_stereo NULL guard); source restored immediately after, suite green since. Same pattern as the 09-28 `t102`/`t106` mutant SEGVs in the core log. Nothing lost (regenerable test binary); will not recur.

## QEMU crash: empty AudioModes trips `ahi.device ReadConfig`

- Reaper (monitor screendump, fully readable): Task `RIAPP render`, Error `0x80000008` privilege violation, Module `ahi.device` Segment 4, Function `ReadConfig+0x5F`, `movaps (%rdx),%xmm0` unaligned-source shape; stack `Dev_OpenDevice → … → _AHI_LoadModeFile → driver _LibInit`.
- Mechanism: the quarantine from the prior slice left `DEVS:AudioModes/` EMPTY, and the device-open scan faults on the empty dir — one step further down the known ladder (`sb128 DriverInit+0x135` → `hdaudio card_init+0xF6A` → `ahi.device ReadConfig+0x6F`, per the 09-28 record). The quarantine traded hangs for crashes: reverted.
- Revert (all `Rename` back, byte-count verified): `sb128.audio`, `ac97.audio` + mode files `SB128`, `VIA-AC97`, `HDAUDIO`, `CMI8738`, `NVHDMI`, `ac97` — riqemu1 back to the err-7-hang-but-living state. Lesson: never leave the mode dir empty; quarantine drivers only, never the whole dir.

## Sound proven on riaudio (scratch lane, pre-existing)

- `~/Work/vms/riaudio_dh0.img` + `start_riaudio.sh` (HMP 4479, `-audiodev wav` + `intel-hda` + `hda-duplex`) booted instead of riqemu1 (spool conflict — one lane at a time); carries the 09-28 rebuilt drivers (sb128/hdaudio/device with vectorization off).
- P6c RIAPP (`953816`-byte ABIv11 build, reports `build=ff2772d` from the build worktree HEAD): window opens, no crash; `audio: AHI low-level mode=0x003e0001 mix=48000 Hz buffer=256 frames`; `buffers=5199 xruns=0`; evlog `TR PLAY/STOP` + `TAB page=2`; host wav peak 0.855, RMS 0.0385 — music, first P6c sound.
- riqemu1 parked again (quit via monitor; DH0 keeps grub native-default, ENV override, restored audio files). Serve `:9295` still up; either lane can come back.
- `hda-duplex` warns `Can not open 'adc'` (wav backend is playback-only) — harmless for playback.

## Open

- AC97 tick root cause on riqemu1 (hang with driver present) still open; HDA there still faults at load. Real fix remains AROS-driver-side (upstream) or riaudio-first for all sound proofs.
- VOICE page 3/4 pixels still unproven on-target (tab nav needs focus; 2 click misses on riqemu1).
- evlog `Vk4aros:` requester suppression in tree still unverified (ENV override proven instead).
