# AHI honest rate: the "44100 for every rate" misread, the real off-list defect, and six driver fixes (2026-10-07)

> **Status update (2026-10-08):** the Dell proof is done. The patched HDAudio reports M = H within 0.1 % on all eight requests (50000 → 48000/48049, 100000 → 96000/96098). The RIAPP regression is clean (mix=48000, 0 xruns), and the original driver is restored. See [2026-10-08-ahi-rate-dell-proof-avail-flush-crash-and-ahi-v7-verdict.md](2026-10-08-ahi-rate-dell-proof-avail-flush-crash-and-ahi-v7-verdict.md).

- Source: ReIncarnation session, 2026-10-07 (opencode lane, Dell E6320 ABIv11 + upstream master `c99cd7a6a2`)
- Collected: 2026-10-07
- Published: 2026-10-07
- Raw: `docs/evidence/audio/ahi-rate/2026-10-07-h1-reproduce.md`, `2026-10-07-h2-fix.md`, `2026-10-07-h3-survey.md` (repo-relative)
- Related: [the Dell hands back 44100 for every rate](2026-10-03-dell-ahi-hands-back-44100-for-every-rate.md) (Outdated — the misread), [AC97 resamples 48 kHz to 44.1 kHz](2026-10-03-riqemu1-ac97-resamples-48k-to-44k1-so-playback-is-8-1-slow.md) (measurement stands; Dell rows corrected), [the AHI probe build contract](2026-10-03-the-ahi-probe-build-contract.md)
- Upstream: branch `ahi-report-selected-rate` (6 driver commits + 1 optional autodoc commit, unpushed), PR draft in the Vulkan4AROS tree (`docs/dev/dispatch/drafts/ahi-selected-rate-pr.md`), carriage diff `src/abi-patches/v1/aros/0072-ahi-report-selected-rate.diff`

## The misread

On 2026-10-03 `probe_rate.c` reported "the Dell hands back 44100 for
every rate". It read the rate with `AHI_GetAudioAttrsA(AHI_INVALID_ID,
actl, … AHIDB_Frequency …)` and no `AHIDB_FrequencyArg` — which per the
AHI autodoc (`Device/modeinfo.c` ~297–305) returns the frequency at index
`AHIDB_FrequencyArg`, default **0**. The HDAudio driver returns
`card->frequencies[argument].frequency` (`Drivers/HDAudio/main.c:431`;
index 0 on this codec is 44100). So "44100 for every rate" was "entry 0
of the rate list, every time". The correct query is
`AHI_ControlAudioA(actl, AHIC_MixFreq_Query, &f)` ("Get the current mixing
frequency", `Device/audioctrl.c:857`) — what the live backend uses, and
it logs `mix=48000 Hz` on the Dell. Pitch there always sounded right.

## The real defect

HDAudio's `_AHIsub_AllocAudio` (`Drivers/HDAudio/main.c:107-122`) clamps
below-floor requests honestly, then picks the **nearest** listed rate
into `selected_freq_index` and programs the converter/stream from that
index (`misc.c:1588-1590`) — but never writes `ahiac_MixFreq` back. NVHDMI
is the same shape. SB128/CMI8738/Envy24/Envy24HT accept anything at
`AllocAudio` and fall back in `_AHIsub_Start` (44100/44100/48000/48000)
without write-back. Measured on the Dell (new `probe_rate_hw.c`: silent
N = 4×M loop, `ReadEClock` stamps, H = N/mean over 8 loops; positive
control 48000 reads H = M within 0.06 %):

| req | M (mixer) | H (hardware) |
|---|---|---|
| 44100/48000/96000 | req | req |
| 50000/60000/100000 | req | 48000/48000/96000 — **4–20 % slow, nothing reports it** |
| 32000/22050 | 44100 | 44100 (clamp honest) |

## The fix

One write-back hunk per driver, placed so record gating still sees the
exact request (exact requests behavior-identical): HDAudio/NVHDMI write
back `frequencies[selected_freq_index]`; the other four write back
`Start`'s fallback when the request matches nothing. Toccata
(`toccata.c:235`) and the EMU10kx comment (`emu10kx-main.c:140-143`)
are the in-tree precedent/rule. No core change: `UpdateAudioCtrl` and
`RecalcBuff` both run after `AHIsub_AllocAudio`, so only the inaudible
anti-click length goes stale. All six relink for ABIv1 with no new
warnings; ABIv11 `hdaudio.audio` staged. Dell swap + rerun needs the
owner's "yes" (not given); HDAudio is hardware-measured, the other five
are compile-only.

## The survey result to remember

Honest: ac97 (forces 48000), VIA-AC97/EMU10kx/RPi×3/Void/Filesave
(hardware-or-file follows the request), Alsa/OSS/PulseAudio/WASAPI
(negotiate + write back), Toccata/Aura (round + write back). Unknown
(no C source in tree): Paula, SoundBlasterAWE, Wavetools.

## Method notes

- A diagnostic reporting a wrong number is worse than one reporting
  none — again: the first H1 run's 240 s watchdog fired mid-sweep and cut
  the 32000 row at 5 callbacks; kept verbatim beside the complete rerun
  so the two can be compared.
- The per-driver gen-dir `make` needs the tree's `collect-aros` on PATH;
  without it the link fails environmentally, not from the patch.
- Whole-file copies between the upstream worktree and the older local
  checkout silently re-encode latin-1 comment bytes (µ/© → U+FFFD);
  check `git diff` for non-hunk lines before committing upstream.
