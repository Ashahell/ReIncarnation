# H1: off-list MixFreq on HDAudio — mixer runs the request, hardware runs the nearest rate

**Date:** 2026-10-07. **Lane:** Dell E6320 (Sandy Bridge, Intel HDA), ABIv11.
**Probe:** `audio_io/probe_rate_hw.c` (RATEPROBE/HW), v11 build
`r12moves=38, und=0`. Mode `0x003E0001` for every rate (known-good mode id;
BestAudioID consulted once for 48000). Sound: N = 4 × M zeroed 16-bit mono
frames at sample freq M, gaplessly re-queued on SoundFunc; stamps via
ReadEClock (`efreq=1193180`). 10 callbacks → 9 loops, first dropped, mean
over 8. Silence still consumes hardware at H; volume full.

## Verbatim output, second run (600 s watchdog; complete)

```
RI_RATEHW probe=probe_rate_hw requests=8
RI_RATEHW version=6
RI_RATEHW eclock=1193180
RI_RATEHW mode=0x003E0001
RI_RATEHW req=44100 M=44100 H=44127 permille=1000 nloops=8 tickmean=4769740 tickmin=4769730 tickmax=4769769 efreq=1193180 nframes=176400
RI_RATEHW req=48000 M=48000 H=48028 permille=1000 nloops=8 tickmean=4769897 tickmin=4769722 tickmax=4770048 efreq=1193180 nframes=192000
RI_RATEHW req=50000 M=50000 H=48027 permille=960 nloops=8 tickmean=4968771 tickmin=4968755 tickmax=4968786 efreq=1193180 nframes=200000
RI_RATEHW req=60000 M=60000 H=48029 permille=800 nloops=8 tickmean=5962177 tickmin=5962148 tickmax=5962218 efreq=1193180 nframes=240000
RI_RATEHW req=96000 M=96000 H=96055 permille=1000 nloops=8 tickmean=4769974 tickmin=4769563 tickmax=4770636 efreq=1193180 nframes=384000
RI_RATEHW req=100000 M=100000 H=96054 permille=960 nloops=8 tickmean=4968747 tickmin=4967845 tickmax=4968899 efreq=1193180 nframes=400000
RI_RATEHW req=32000 M=44100 H=44124 permille=1000 nloops=8 tickmean=3461257 tickmin=3457748 tickmax=3482552 efreq=1193180 nframes=128000
RI_RATEHW req=22050 M=44100 H=44124 permille=1000 nloops=8 tickmean=2385046 tickmin=2385022 tickmax=2385148 efreq=1193180 nframes=88200
RI_RATEHW done
```

## Verbatim output, first run (240 s watchdog; 32000 row truncated by the probe's own watchdog)

```
RI_RATEHW probe=probe_rate_hw requests=8
RI_RATEHW version=6
RI_RATEHW eclock=1193180
RI_RATEHW mode=0x003E0001
RI_RATEHW req=44100 M=44100 H=44127 permille=1000 nloops=8 tickmean=4769741 tickmin=4769711 tickmax=4769768 efreq=1193180 nframes=176400
RI_RATEHW req=48000 M=48000 H=48027 permille=1000 nloops=8 tickmean=4769982 tickmin=4769760 tickmax=4770055 efreq=1193180 nframes=192000
RI_RATEHW req=50000 M=50000 H=48027 permille=960 nloops=8 tickmean=4968691 tickmin=4968466 tickmax=4968782 efreq=1193180 nframes=200000
RI_RATEHW req=60000 M=60000 H=48030 permille=800 nloops=8 tickmean=5962169 tickmin=5962156 tickmax=5962186 efreq=1193180 nframes=240000
RI_RATEHW req=96000 M=96000 H=96060 permille=1000 nloops=8 tickmean=4769725 tickmin=4769308 tickmax=4770400 efreq=1193180 nframes=384000
RI_RATEHW req=100000 M=100000 H=96059 permille=960 nloops=8 tickmean=4968491 tickmin=4967560 tickmax=4968636 efreq=1193180 nframes=400000
RI_RATEHW req=32000 TIMEOUT waiting callback 5/10
RI_RATEHW req=32000 mixq=44100 callbacks=5 SHORT
RI_RATEHW req=22050 M=44100 H=44127 permille=1000 nloops=8 tickmean=2384866 tickmin=2384851 tickmax=2384883 efreq=1193180 nframes=88200
RI_RATEHW done
```

The 240 s watchdog fired ~242 s into the sweep (rows before it: ~228 s of
loops), cutting the 32000 row at 5 callbacks. Its `mixq=44100` still stands
(the clamp); only its H was unmeasured there. Watchdog raised to 600 s, full
sweep re-run: every other row reproduces to ±3 Hz, and 32000 completes at
M = H = 44100. The truncated row is kept above so the two runs can be
compared; nothing in it contradicts the second run.

## Reading

| req | M (mixer) | H (hardware) | H/M | verdict |
|---|---|---|---|---|
| 44100 | 44100 | 44127 | 1.000 | listed: exact |
| 48000 | 48000 | 48028 | 1.000 | listed: exact (positive control, +0.058 %, within 0.1 %) |
| 50000 | 50000 | 48027 | 0.960 | **off-list: mixer 50000, hardware 48000 — ~4 % slow, nothing reports it** |
| 60000 | 60000 | 48029 | 0.800 | **off-list: mixer 60000, hardware 48000** |
| 96000 | 96000 | 96055 | 1.000 | listed: exact |
| 100000 | 100000 | 96054 | 0.960 | **off-list: mixer 100000, hardware 96000** |
| 32000 | 44100 | 44124 | 1.000 | below floor: clamp honest |
| 22050 | 44100 | 44124 | 1.000 | below floor: clamp honest |

- **Positive control passes:** at 48000, H = M within 0.1 % on both runs
  (48027 and 48028). The method is sound.
- **The predicted off-list split reproduces exactly as predicted:** M = req,
  H = nearest listed rate (50000→48000, 60000→48000, 100000→96000).
- **Common-mode +0.06 % on every listed rate** (44127/44100, 48028/48000,
  96055/96000 all read ~1.0006): EClock nominal vs actual PIT frequency, or
  the codec crystal — identical on all rows, an order below the 4–20 %
  off-list signal. Recorded, not corrected for.
- **Spread is hardware-tight:** tickmin..tickmax within ±0.01 % on 7 of 8
  rows (32000 shows one loop ~0.7 % long — scheduling jitter in the
  re-queue task; the mean over 8 still reads 44124).
- **QEMU HDA check skipped:** the only live QEMU lane (anon guest on the
  private spool) ran another session's jobs minutes before and after; per
  the lane rule it was left alone.

## Lane state left in

- `status` before: IPrefs, ConClip, Decorator, AROSTCP, ATCPBIN, Wanderer —
  no RIAPP, no AHI client. Probe binaries and logs deleted from `RAM:`
  after each run (`list RAM:` verified clean). No driver touched. No reboot.
  No `--ui-capture` used.
