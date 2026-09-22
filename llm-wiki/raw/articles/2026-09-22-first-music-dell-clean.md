# 2026-09-22 — First music on the reference box (first-light 3 loops, operator: clean)

> Source: operator ear report + probe logs + host file analysis, compiled by agent
> Collected: 2026-09-22
> Published: 2026-09-22

## Disposition
New. Milestone: first musical playback (not test tones) on reference
hardware, human-confirmed clean.

## What ran
Scratch `dell_player` (v11 build, 20896 B): opens ahi.device unit 0,
CMD_WRITE-loops a WAV file skipping the 44-byte header (DoIO per
16 KB chunk, err-checked), N loops, rc=0. Played
`RAM:firstlight.wav` (t6 render, 250330 B, 2.6 s, 3 loops ≈ 8 s).

## The mono/stereo bug (found by ear, fixed in one line)
First playback: "heard sounds, but really low quality". Host analysis
of the exact bytes exonerated the file (peak 74% FS, 0 clipped, crest
10.4 dB — clean). Root cause was the player: mono file declared as
`AHIST_S16S` (stereo), so the driver dealt consecutive samples to
L/R — each ear got a half-rate aliased stream. (Why the 440 Hz tone
sounded fine: those buffers genuinely were stereo.) Fix:
`AHIST_M16S`. Replay: operator reports **clean**.

## Operator report (verbatim)
- Tone runs: "sounded a bit like an old ring tone... volume could be louder" → resolved (host + AHI prefs stages, both maxed).
- Music, first play: "we heard sounds, but they were really low quality".
- Music, fixed replay: "sounded clean".

## Standing
- Scratch player + WAV live on the guest (`RAM:player`,
  `RAM:firstlight.wav`); sources under `/home/miller/Work/ri_build/`
  (`dell_player.c/.o`, player binary). Promotion to a kept
  audible-self-test is a deliberate future step, not taken here.
- 48 kHz file into the 44100-negotiated mode played cleanly — no
  resampling complaint from the ear check (pitch accuracy untested;
  musicians would notice 8%).

## Files (scratch, outside repo)
- `/home/miller/Work/ri_build/dell_player.c` (+ `.o`, player binary).