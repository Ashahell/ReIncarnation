# Clipping (Comp make-up) and tab-switch dropouts (render priority): diagnosis and fixes (2026-10-01)

- Source: ReIncarnation session, 2026-10-01 (Dell E6320 + private QEMU VM; changes in worktree `ReIncarnation-claude`, patch `riapp-comp-limiter-tabstall.patch`, not yet committed)
- Collected: 2026-10-01
- Published: 2026-10-01
- Related: [2026-10-01-dell-zn-telemetry-song-load-diagnosis.md](2026-10-01-dell-zn-telemetry-song-load-diagnosis.md), [2026-10-01-menu-hang-tab-artifacts-load-governor.md](2026-10-01-menu-hang-tab-artifacts-load-governor.md), [2026-09-28-render-spike-hunt-deferred.md](2026-09-28-render-spike-hunt-deferred.md)

## WAV capture from the Dell (`CAPTURE=<file>`)

- **Start option:** `RIAPP SONG=... CAPTURE=RAM:x.wav` records the live output from the next Play to the next Stop or pause. It then writes a 16-bit stereo WAV at the device rate. Maximum length is 600 s.
- **How it records:** the GUI-owned buffer is filled through the live driver's W capture. The capture holds rendered halves, so AHI xrun repeats are not in the file.
- **First pull:** `~/Music/zombie-nation-riapp-dell.wav`, 260.6 s, 50031660 bytes.
  - It had 6715 full-scale samples and RMS 0.156.
  - The host render of the same song peaks at 0.845 with no clipping.
- **Download behaviour:** downloads over 1 MiB fall back to chunked transfer, ~140 s for 50 MB.
  - The Dell rebooted during one 50 MB pull, cause unknown. A 27 MB pull later worked (120 s).
- **Stopping:** a click on the Stop button did not register remotely. The space key (`--ui-rawkey 0x40`) stopped the transport and finished the capture.

## Clipping: not a render bug

- **Bars 0–3 are sample-identical** between the Dell capture and the host render (`songplay`).
- **Small differences after that.** The divergence per bar is about 300–3600 counts.
- **Large differences only in specific windows:** bars 33–38, 45–49, 53–57, 61–65, 69–72 and 76–79.
- **The ev-log explains every window.** `Vk4aros:RIAPP-EV.LOG` records strip insert switches toggled during the capture. Each on/off window matches a divergent stretch exactly:
  - 909 Comp `0707` (bars 33–38);
  - Levi Comp `1407` (45–49);
  - 303A PCF `0406` (53–57);
  - 808 PCF `0606` (61–65);
  - 909 PCF `0706` (69–72);
  - 909 Dist `0705` (76–79);
  - 808 Comp `0607` (22–30, small).
- **Ruled out:**
  - The Dell's 909 pack is byte-identical to the repo pack (sha256 `4a635acb...`).
  - An older pack renders the same.
  - In the VM, a run with tab switches is sample-identical to a run without them.
  - Tab switches send no CTL/STEP/PAT events.
- **Cause of the clipping:** the auto make-up was full compensation, `MU_dB = -tdb*(1-1/R)`.
  - At the song's comp settings (Threshold 64 = -19.8 dB, Ratio 70 = 11.5:1) that is +18.1 dB (x8).
  - The 10 ms attack passes clap and Levi transients at that gain, and the mix hard-clips in the s16 conversion.
- **Fix:**
  - Auto make-up is halved to `-tdb*(1-1/R)/2`, the TASCAM-style half compensation found in research.
  - `ri_soft_limit` is added after the master fader:
    - exact below `RI_LIMIT_KNEE` 0.8912509 (-1 dBFS);
    - above it, the excess is tanh-shaped into the last 1 dB;
    - it is odd, monotonic and C1 at the knee, and stays below 1.0.
  - The master meters stay pre-limiter, so CLIP still shows.
- **Tests:** t1_fx expects make-up 7.44 dB (was 14.88 at 4:1) and adds the limiter laws.
- **Goldens:** the `tests/golden/pcf/fx-chain.wav` golden and its sha256 were regenerated (the chain uses the comp). No other golden changed, because unclipped renders stay bit-identical under the limiter.
- **Dell verification:** all five strip Comps on, 139 s capture: peak 0.93, 0 full-scale samples.

## Tab-switch "unwanted sound" = xruns from priority pre-emption

- **Dell measurement** (Zombie Nation playing, tabs clicked through the spike agent):
  - xruns 0 → 94 in the first round of switches, then +72 in an identical second round.
  - Plain clicks without a tab change: +0.
  - render_max 84 ms.
- **Fixed instrumentation:**
  - rsection's `eclock_open` returned early when RIAPP's audio side had already opened timer.device, so `s_efreq` stayed 0 and every draw timing was dropped (`draw: n=0`). Now it reads the EClock rate in that case.
  - New diag fields `blit_max` and `alloc_n` appear in the `RIAPP draw:` heartbeat line.
  - The `TAB` ev-log line now carries `us=<switch time> xruns+<n>`.
- **Findings:**
  - The MIX switch took 101–121 ms (+10–14 xruns). One canvas `BltBitMapRastPort` measured 67–76 ms. render_max 62–83 ms means the render task was paused mid-render.
  - Splitting the blit into 48-row bands did not help (reverted).
  - **Experiment:** render task at priority 21 (above input.device at 20). Result: 0 xruns over startup and all switches, render_max 3.1 ms, `blit_max` dropped to ~13 ms.
- **Conclusion:** during page repaints on the Dell, Intuition's input handler (priority 20) does tens of ms of display work. That pre-empted the render task at priority 10.
  - This likely also explains the occasional 3x-period render spikes deferred in the 2026-09-28 spike hunt.
- **Not the cause** (AROS sources checked):
  - The IntelGMA driver takes `Forbid()` only in `AllocGfxMem`/`FreeGfxMem` (GATT mapping). The Dell's HD 3000 is probably on vesagfx anyway; `DEVS:Monitors` lists IntelGMA, NVidia, VMWare and Compositor.
  - fakegfxhidd's `Forbid()` is only a brief semaphore arbitration.
- **Fix:** `AU_LIVE_PRI 21` in `audio_io/audio_ahi_live.c`. The overload governor still drops the task to -1 under sustained overload.

## Governor robustness

- **Problem:** an external stall (84 ms) tripped the governor twice. It then moved the render task below the UI and made the glitching worse.
- **Fix:** each buffer's load sample is capped at `RI_LIVEDRV_LOAD_CAP_PM` 1200. A stall spanning a few renders cannot trip; about 8+ consecutive over-budget renders are needed.
- **Test:** a t88 case with one 84 ms render plus three 30 ms renders. It failed with the old 4000 cap and passes with 1200.

## Method notes

- **Scratch area:** the session scratchpad was wiped twice (host restarts or cleanup). Keep work in a persistent sibling worktree (`git worktree add --detach ../ReIncarnation-claude HEAD`). There `../Vulkan4Aros` resolves and the audit runs as-is.
- **Build-path isolation:** redirect `/tmp/ri` in the worktree's scripts and tests. Exclude those edits when extracting a patch (`git diff -- <own files>`). A sed with a `\b` pattern after `/tmp/ri/` doubles to `/tmp/ri-claude-claude`, which is harmless but messy.
- **VM recording:** QEMU `-audiodev wav,...,out.frequency=48000 -device intel-hda -device hda-output` records the guest's AHI output to a host WAV that keeps growing.
  - Its level is scaled (peaks ~1/3 of host), but runs compare sample-exactly against each other.
- **Locked logs:** `RAM:RIAPP-EV.LOG` is held open by RIAPP. Read it after quitting (shell `Break <process>`).
- **Keyboard tab switching:** Ctrl+1..5 sent as separate rawkeys through the spike agent did not switch tabs; no qualifier is carried.
