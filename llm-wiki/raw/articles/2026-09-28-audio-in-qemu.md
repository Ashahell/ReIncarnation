# Audio in QEMU: scratch HDA lane it, full M1.1 green, music captured

- Source: ReIncarnation session, 2026-09-27/28 (new scratch lane, driver rebuilds, probe + RIAPP runs)
- Collected: 2026-09-28
- Published: 2026-09-28
- Evidence: host capture `/tmp/opencode/riapp-guest.wav` (31.6 s, 44.1 kHz stereo 16-bit, music from Play, −25 dBFS RMS)

## Lane

New scratch lane `riaudio` (not riqemu1, untouched): clone of
`riqemu1_dh0.img` → `riaudio_dh0.img`, launcher
`~/Work/vms/start_riaudio.sh`, HMP 4479, serial
`/tmp/riaudio_serial.log`, priv spool (sole agent only while
riqemu1 is down — hardcoded `10.0.2.2:9295` in the guest agent).
Audio: `-audiodev wav,id=snd,path=...` + `-device intel-hda` +
`-device hda-duplex` (PA backend also available; guest view
identical). QEMU PCI shows audio controller `8086:2668`.

## Driver rebuilds (all in gitignored gen/ objdirs; tree untouched)

- sb128 + 8 drivers: `-fno-vectorize -fno-slp-vectorize`.
- Per-TU `-mno-sse` where the TU allows: HDAudio `misc.c` et al
  (not `main.c`), CMI8738 (not `cmi8738hw.c`), device `device.c` —
  LVO entry TUs fail with "SSE register return", keep them.
- Gates: zero bare-register vector loads in sb128 `DriverInit`,
  hdaudio `card_init`, device `ReadConfig` (remaining hits are
  frame-relative spills or `__ieee754_log10`-internal).
- Deployed to lane `DEVS:AHI/` + `DEVS:ahi.device`, each with a
  `.orig` backup beside it.

## Fault ladder surfaced (same shape every time)

sb128 `DriverInit+0x135` → hdaudio `card_init+0xF6A` (RAX=2,
`movabs`-loaded table) → `ahi.device ReadConfig+0x6F`. Each fix
advanced the scan exactly one step. Remaining risk is further
sites of the same class, not new bugs.

## Proof

- Stock `probe_ahi` M1.1: open unit=255, `best_mode 0x003E0001`
  (the Dell's mode!), full ladder rc=0, no requester.
- RIAPP (panel, 256f): 20174 buffers (~86 s), **0 xruns**,
  render_max ~1.3 ms; host-captured 31.6 s stereo music starting
  exactly at Play. Realtime pacing (buffers ≈ wall time) rules
  out the VOID path.
- 3/3 clean boots green (before = 3/3 faulted on old binaries:
  before/after, not flakes).

## Findings for the sibling lane

- `evlog_vol()` probes `Vk4aros:` via `Lock()` → volume requester
  wedges every headless run. Workaround used: `SetEnv
  RIAPP_EVLOG RAM:`. Suggest `pr_WindowPtr=(APTR)-1` around the
  probe loop.
- v1 RIAPP binary grew 329 kB → 515 kB with no source change
  (toolchain/SDK moved underneath?); gates pass, flagged only.
- Captured rate is 44.1 kHz (codec) vs 48 kHz mix; heartbeat
  `snd=` counters stay 0 (their diag).
- Durable fix still theirs: flags into the AHI build
  (`USER_CFLAGS`/env), rebuild, redeploy. Binaries proven here.
