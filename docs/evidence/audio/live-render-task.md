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
