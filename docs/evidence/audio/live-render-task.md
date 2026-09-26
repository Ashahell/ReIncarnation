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

## Buffer ladder, timing, capture (2026-09-26, later)

- **EClock render timing** (render task). At 1024 frames: max 2.86 ms,
  mean about 1.8 ms per 21.33 ms period (~8 %). Owner-played sessions: 0
  xruns over 2573–2994 buffers.
- **Without `AHIA_PlayerFreq`, halves under ~960 frames loop inside AHI's
  default mixing pass** (about 50 passes/s). 256/128/64 gave hundreds of
  xruns and only ~5 renders/s.
- **With `AHIA_PlayerFunc` + `AHIA_PlayerFreq` = rate/frames (Fixed
  16.16),** the no-input runs gave 256 → 0 xruns, 128 → 0 xruns, 64 → 1
  xrun. The owner has not listened at those sizes yet.
- **At 1024 frames the PlayerFreq build "sounds worse"** (owner), with 0
  xruns logged. The override is now used only below 1024 frames.
  **Open:** explain it — a listening test plus a WAV comparison at each
  size.
- **"The window disappears on click" was NOT an audio bug.** Agent
  `--ui-rawkey CODE,up` does not release the key, so a stuck Q key
  auto-repeated 'q' into whichever window became active. Release a key by
  injecting `CODE|0x80` (e.g. `0x90` for Q). IDCMP logging proved it:
  `class=0x00200000 code=0x71` on the click.
- **W records the live output** to `RAM:RIAPP.wav` (s16 stereo at the mix
  rate, up to 5 min). The task copies each half into a GUI-allocated
  buffer; the GUI writes the file (no IO in the render task). Q while
  recording also saves. The first owner take held 99328 frames (W was
  pressed just before Q); the file was verified as RIFF PCM 16-bit stereo
  48000 Hz.

## Buffer ladder, scripted takes + soak (2026-09-26, opencode G9b Step 1)

Same agent-driven script at every size (ABIv11 `RIAPP <frames>`: click
(100,60), Space, W, three C presses, Q; adjacent press/release pairs, no
holds — a 1 s hold auto-repeats, proven by a 12x "RIAPP play" storm on a
separated-pair run). Each take verified from `RAM:RIAPP.LOG` (one play,
one record-start, cutoffs 64→48→32→16, wrote-on-quit). WAVs fetched and
compared on the host (all RIFF PCM 16-bit stereo 48000 Hz):

| frames | buffers | xruns | render max | render mean | WAV frames | peak | RMS    | adj-repeat blocks |
|-------:|--------:|------:|-----------:|------------:|-----------:|-----:|--------|-------------------|
|   1024 |     612 |     0 |    2830 us | ~1878 us    |     416768 | 17286 | −17.9 dBFS | 0 |
|    512 |    1212 |     0 |    1455 us | ~960 us     |     413184 | 17286 | −17.9 dBFS | 0 |
|    256 |    2441 |     0 |     766 us | ~501 us     |     416256 | 17286 | −17.9 dBFS | 0 |
|    128 |    4879 |     0 |     410 us | ~269 us     |     416256 | 17286 | −17.9 dBFS | 0 |
|     64 |    9767 |     2 |     251 us | ~154 us     |     416256 | 17286 | −17.9 dBFS | 0 |

Headroom (max/period): 13 % @1024, 14 % @512, 14 % @256, 15 % @128,
19 % @64. Mean load ~9–12 % at every size.

- **Render is identical at every size**: peak 17286 and RMS −17.9 dBFS in
  all five takes, zero adjacent repeated half-blocks. (Honesty note: the W
  capture is task-side, pre-device — it proves the render, not what AHI
  delivered. Device loops are counted as xruns, not visible in the WAV.
  True live==offline bit-exactness per take needs buffer-index logging of
  each control drain; the task does not log it yet. Host t81 already proves
  live==offline bit-exact for the same control history.)
- **"Worse at 1024 with PlayerFreq" hypothesis** (not yet proven): with the
  pass length equal to the half length, the `AHI_SetSound(…, AHISF_NONE)`
  queue races the pass start — a pass-start race replays with an offset or
  clicks without tripping the xrun counter (the hook counts starts, and 0
  xruns were logged on the "worse" build). Proposed test: a build forcing
  PlayerFreq on at 1024 for an owner A/B; keep the `<1024` gate until then.
- **5-minute soak at 256**: 59038 buffers (~315 s incl. stopped head),
  **0 xruns**, render_max 773 us, mean ~662 us (12.4 % of the 5333 us
  period). Caveat: 9 unexplained 303-level moves (up to 127, back to 79)
  appear mid-log with no matching agent job in the spool — the take was not
  input-clean (possibly the owner at the keyboard, or a stuck-key echo).
  The stability numbers stand; re-run input-clean before closing Step 1.
- **Recommendation (owner confirms by ear)**: ship **256 frames**
  (5.33 ms latency, 0 xruns, 14 % worst-case load). 128 is equally clean
  and halves latency again (2.67 ms). 64 is marginal (1–2 xruns per ~10k
  buffers). `RIAPP_DEV_FRAMES` stays 1024 until the owner listens at
  256/128 — nobody has listened below 1024 yet.
