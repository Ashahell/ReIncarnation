# Live render task (G9.3) — evidence

## Host proof (2026-09-26, this session)

- `t80_ctlplane` PASS: FIFO order, coalescing law, wrap-around, refused
  keys, drained events render-identical to the setter path. Mutants killed:
  allow-check removal, coalesce-counter removal.
- `t81_live` PASS: the t81 fixture (303 pattern, 120 BPM, 48 kHz) renders
  bit-exact across device-buffer chunks 64/128/256/137, with and without
  an automation lane, and with a control move at sample 256 matching the
  same move as an offline `RI_EV_AUTOMATION` event. Mutants killed:
  control-drain removal, rebase removal. Meters from the engine
  (`sec_peak[0] > 0`), position from the transport, STOPPED renders
  silence.
- ABIv1 link: `RIAPP` 172792 bytes, 0 unresolved, 0 r12 moves
  (`bash scripts/ri_build_aros.sh riapp`).

## Dell measurement (OPEN — owner lane)

Pending on the Dell E6320 (ABIv11, 1366x768, real HDA):

- [ ] Negotiated device rate re-verified at G9.3 start (M1.1 said 44100 Hz
      / 16-bit, mode `0x003E0001`, 64 frames accepted).
- [ ] `RIAPP` plays the demo: owner presses Play, hears it, turns 303
      cutoff + 808 level + mixer fader, hears each within one buffer.
      Owner words recorded here.
- [ ] Buffer size run (64/128), xruns over 5 minutes, measured render time
      per buffer, CPU headroom.
- [ ] 5-minute soak with xrun count.

Low-level (`AHI_AllocAudioA` + PlayerFunc `Signal()`) vs `ahi.device`
`CMD_WRITE` (the `AuPlay` fallback, kept): measure which runs at 64/128
frames without xruns; ship the winner, keep the other documented.

## Dell first sound (2026-09-26, Claude session; owner listening)

The G9.3 backend in `f1bfb7d` was a placeholder: no `AHI_AllocAudioA`, the
hook was never registered, no render task existed, and RIAPP hard-coded the
null backend. It is replaced by a real low-level stream
(`audio_io/audio_ahi_live.c`):
- **Render task:** a render Process (priority 10) owns every AHI object.
  It opens `ahi.device` on `AHI_NO_UNIT` (base from `io_Device`) and picks
  `AHI_BestAudioID` for stereo HiFi at 48 kHz.
- **Stream:** one channel with two `AHIST_DYNAMICSAMPLE` `AHIST_S16S`
  sounds as the double buffer. The `SoundFunc` hook only counts and
  `Signal()`s (spec §4.2). The task renders the finished half through
  `ri_live_render` and queues it (`AHI_SetSound … AHISF_NONE`).
- **Under-runs:** a late buffer makes AHI loop the last half; it is counted
  as an xrun, never a hang.
- **Control:** the GUI posts transport requests (`au_live_request`) and
  knob keys (control plane) only. The task is the only caller of
  `ri_live_render/play/stop`.
- **Fallback:** a failed open means the null backend plus the documented
  message (with the failing step number).

Results on the Dell E6320 (ABIv11 build, `RIAPP 1024`):
- [x] Mode `0x003E0001`; AHI accepted a **48000 Hz** mix rate (the session
      runs at the queried mix rate, G9.0 E0).
- [x] Owner: "we hear both" (303 + 808), then after the demo-mix change
      "balance is good now, pan works". Cutoff (C/V), 303 level (L/K) and
      pan (P, now centre/left/right) are audible live.
- [x] First session: **5447 buffers × 1024 frames ≈ 116 s, 0 xruns**,
      clean close (`RIAPP closed: buffers=5447 xruns=0`).
- Demo mix: the 303 strip starts at 72 (−9.9 dB) and the 808 downbeats
  are accented (owner: the 808 should be louder). Host-measured RMS: 808
  −19.8 dBFS, 303A at 90 −17.3 dBFS. The voice calibration is unchanged.
- M only logs meters (no sound change, as designed). The first log's
  meter line was garbled: RawDoFmt `%lu` consumes packed 32-bit LONGs,
  not IPTRs. Fixed by building the args as ULONG.
- [ ] 64/128-frame runs, a 5-minute soak, render time per buffer: next.
