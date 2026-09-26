# Live AHI stream: first sound on the Dell, buffer ladder, W capture, stuck-key trap (§12.11 G9)

- Source: ReIncarnation session, 2026-09-26 (owner listening on the Dell E6320, ABIv11)
- Collected: 2026-09-26
- Published: 2026-09-26
- Commits: `dd93ae7` (real low-level AHI stream + RIAPP first sound), `e884439` (render timing, small-buffer path, W capture), `8953cf6` (G9b multi-step opencode prompt)
- Evidence: `docs/evidence/audio/live-render-task.md`

## Review finding
opencode's G9.3 "live AHI backend structure" (`f1bfb7d`) was a placeholder:
- no `AHI_AllocAudioA`;
- the PlayerFunc hook was never registered;
- no render task;
- `au_live_start` rendered one buffer on the GUI task;
- RIAPP hard-coded the null backend.

## The real stream
- **Owner:** a render Process ("RIAPP render", priority 10) owns every AHI object: `ahi.device` on `AHI_NO_UNIT` (base from `io_Device`), `AHI_BestAudioID` for stereo HiFi at 48 kHz.
- **Rate:** the Dell gives mode `0x003E0001` and accepts a 48000 Hz mix rate; the session runs at the queried rate.
- **Double buffer:** one channel with two `AHIST_DYNAMICSAMPLE` `AHIST_S16S` sounds. The `SoundFunc` hook only counts and `Signal()`s. The task renders the finished half through `ri_live_render` and queues it (`AHISF_NONE`). A late half loops and is counted as an xrun.
- **GUI → task:** a transport request word plus the SPSC control plane only.
- **Owner by ear:** "we hear both" (303 + 808). After the demo-mix change (303 strip at 72, accented 808 downbeats; the 808 section was ~10 dB RMS under the 303): "balance is good now, pan works". Later builds: "sounded good again".
- **Numbers:**
  - first session: 5447 × 1024-frame buffers (~116 s), 0 xruns;
  - later sessions: 2573–2994 buffers, 0 xruns; one 9-minute run had 33 xruns.
- **Render time (EClock):** max 2.86 ms, mean ~1.8 ms per 21.33 ms period (~8 %).

## Small buffers
- **Without `AHIA_PlayerFreq`,** AHI mixes about 50 passes/s (~960 frames), so halves under that loop within a pass: 256/128/64 gave hundreds of xruns and only ~5 renders/s.
- **With `AHIA_PlayerFunc` + `AHIA_PlayerFreq` = rate/frames (Fixed 16.16),** the no-input runs gave 256 → 0 xruns, 128 → 0 and 64 → 1. Nobody has listened at those sizes yet.
- **At 1024 frames the owner heard the PlayerFreq build as "worse"** (0 xruns logged either way). The override is now used only below 1024 frames. **Open:** why (G9b Step 1).

## W capture
- The task copies each rendered half into a GUI-allocated buffer; the GUI writes `RAM:RIAPP.wav` (s16 stereo at the mix rate, ≤5 min). No IO in the render task.
- Q while recording also saves. The first owner take: 99328 frames (W pressed just before Q); a valid RIFF PCM 16-bit stereo 48 kHz file.

## Lane traps (cost two false "click bugs")
- **Agent `--ui-rawkey CODE,up` does not release the key.** It stays down and auto-repeats, so a stuck Q made RIAPP quit whenever its window was activated. Release with **`CODE|0x80`** (0x90 for Q). An IDCMP log proved it: `class=0x00200000 code=0x71` on the click.
- `Run >file` output stays empty; log by opening, appending and closing `RAM:RIAPP.LOG` per line.
- `RawDoFmt`/`VFPrintf` args are **packed 32-bit LONGs**; an IPTR array misaligns every field after the first.
- `--ui-close` does not match RIAPP's long title; quit with a click plus the Q press/release.

## Handoff
`docs/superpowers/plans/2026-09-26-g9b-opencode-prompt.md`, in steps:
0. Land (push, wikis).
1. Buffer size by ear and WAV comparison, plus a 5-minute soak.
2. The real panel in RIAPP, with a published meter snapshot.
3. 909 clean-room pack.
4. Automation from the panel, with bit-identical record/reload takes.
5. Recording UX.
6. Carry-overs, including the 808/303 calibration check.

New open decision: the shipped buffer default.
