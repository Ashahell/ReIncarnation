# riqemu1: wide modes proven; QEMU sound parked on AROS driver faults (owner 2026-09-30)

- Source: ReIncarnation session, 2026-09-30 (opencode lane; owner asked for QEMU sound + wider screen, then to park sound)
- Collected: 2026-09-30
- Published: 2026-09-30
- Prior: [2026-09-25-riqemu1-dh0-boot-v1j-rtl8139.md](2026-09-25-riqemu1-dh0-boot-v1j-rtl8139.md), [2026-09-26-riqemu1-sb128-open-hang.md](2026-09-26-riqemu1-sb128-open-hang.md) (sb128 movaps root cause; this session independently reproduces it)
- Lane: riqemu1 (private, ABIv1), spool `/tmp/spike_spool_priv`, serve `:9295` (started this session, background), monitor `:4477`, launcher `~/Work/vms/start_riqemu1.sh` (edited, see below)

> **SUPERSEDED ON AUDIO (2026-10-03), twice over.**
>
> 1. **AHI now works on this guest.** The `sb128.audio` / AC97 / open-hang findings
>    below were all downstream of one cause: the AROS ELF loader places sections
>    at 12 mod 16 despite `sh_addralign = 16`, so alignment-assuming SSE faults
>    on every AHI open. Fixed guest-side — see
>    [AHI on riqemu1](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md).
>    `probe_ahi` now passes all seven low-level sizes and PulseAudio carries a live
>    uncorked stream.
> 2. **`-device AC97` still cannot be made to run at 48 kHz.** It is fixed at
>    44100, so the guest's 48000 is resampled and everything sounds 8.1 % slow —
>    see [AC97 resamples to 44.1 kHz](2026-10-03-riqemu1-ac97-resamples-48k-to-44k1-so-playback-is-8-1-slow.md).
>    The "carries digital zeros" observation is therefore no longer a driver
>    fault: audio flows, at the wrong rate.
>
> The display work in this record — the mode probe, the 1920x1080 availability —
> is unaffected. So is the quarantine state it lists, though note that
> `DEVS:AudioModes` now holds only `ac97` and the drivers are patched.

## Wide screen: DONE (proven, owner picks the mode)

- Root cause: GRUB pinned `vesa=1280x1024x32` **plus `nomonitors`** (both in the default entry's `ARGS:` per `ShowConfig`); `nomonitors` keeps every monitor driver out, so the mode walk finds exactly **1 mode (1280x1024x24)**. DEVS:Monitors ships VMWare/NVidia/IntelGMA/ATI but none can load.
- Fix (lane, reversible): `DH0:Boot/grub/grub.cfg` default → `"AROS64 with native Gfx"` (backup `grub.cfg.mine`; one-line diff, byte-verified round trip). Native boot ARGS: `ATA=32bit debug=serial`.
- Result: 23 modes, all avail, incl. 1366x768, 1600x900, 1680x1050, **1920x1080 (0x00101000)**, 1920x1200, 2560x1600/1440 (probed with a scratch `BestModeID`/`FindDisplayInfo` tool over the agent channel; `BestModeID`-only probing returns INVALID throughout — walk the IDs).
- Pixel proof: `OPEN 0x00101000 OK 1920x1080`, capture 960x540 (= 1920x1080 screen), clean close.
- Owner step: pick the mode in `Prefs/ScreenMode` (persists in ENVARC). Deliberately NOT forced at boot (a `vesa=1920x1080` single-mode entry needs unproven VBE support — bricking risk, SFS unwritable from host).
- Side lesson: guest `argc/argv` did not arrive in a minimal `-nostartfiles` tool (fell back to a `SetEnv` trigger); AmigaShell rejects double redirects (`too many levels`); `Run >file prog` is the form.

## Sound: PARKED on AROS audio-driver faults (not ReIncarnation code)

- Host + QEMU side are fine: PulseAudio/PipeWire present, `pa`/`pipewire` audiodevs available, QEMU `pa0` stream exists uncorked at 44100 Hz — but carries digital zeros.
- `sb128.audio` DriverInit faults (`movaps (%rdx),%xmm0`, unaligned source) when AHI's `_AHI_LoadModeFile` scan loads it — independent repro of the 09-21/09-26/09-27 findings (same module, same function; stack: probe → `ahi.device` → `_AHI_LoadModeFile` → `_LibInit` → `DriverInit`). Quarantined (reversible): `DEVS:AHI/sb128.audio` → `.bak`, `DEVS:AudioModes/SB128` → `SYS:Storage/SB128.bak`.
- `hdaudio.audio` Segment 4 illegal-address fault with real HDA hardware (`intel-hda` + `hda-output` and `hda-duplex` codecs; `msi=off` tried) — same driver-init class, Reaper-confirmed.
- AC97 (`-device AC97`, `ac97.audio` present): loads, never ticks. Still silent with a single `ac97` mode file left, and still hanging with the driver removed too — the stall is in `OpenDevice("ahi.device")` / the mode scan, not one file. RIAPP's bounded open reports `err 7` → null fallback (the design works as intended: app lives, silent).
- Transport proven working (`sendkey spc` → evlog `TR PLAY/STOP`; 22 s PLAY recorded zeros on the host monitor → silent path, not a control problem).
- evlog volume requester: `evlog_vol()` Locks `Vk4aros:` et al. with requesters enabled — modal block on volume-less machines (seen on riqemu1). A `pr_WindowPtr = -1` suppression is in the uncommitted P6b tree but **unverified**; the working lane instead persists `RIAPP_EVLOG=RAM:` in ENVARC (proven: clean unattended boots since).
- Dell cross-evidence: P6a, P6b and a clean-tree P5b rebuild all take Software Failure at startup on the post-reboot Dell — lane state, not the diff (host audit green, ASan/UBSan clean, Dell builds 0 UND). Owner investigating on their side ("yet again").
- Next (unblocks sound): AROS-side audio driver work (upstream scope) or Dell-first; AC97 tick root cause still open. `probe_ahi` wedges unkillably in a hung driver — always prefer RIAPP's 10 s bounded open as the audio probe.

## Lane state parked (2026-09-30)

> **Status: the GRUB line of this lane state no longer holds (2026-10-03).** The
> live lane has been moved back to the pinned `vesa=1280x1024x32` entry, which is
> the state this record had deliberately moved away from — **so the trade-off
> described above is now live in the opposite direction.** Full record and
> reasoning: [the display mode is a GRUB kernel argument](2026-10-03-the-display-mode-is-a-grub-kernel-argument.md).
>
> What changed, and why it matters for anyone reading the original:
>
> | | pinned `vesa=1280x1024x32` + `nomonitors` | `native` |
> |---|---|---|
> | resolution | deterministic 1280x1024 | varies per boot |
> | modes in the walk | **1** | **23**, incl. 1920x1080 |
>
> The pinned state was chosen to satisfy "riqemu1 always starts at 1280x". The
> cost is the 23-mode capability this record exists to document, so **the wide-mode
> work above can no longer be repeated on the lane as it now stands.**
>
> Also now true, and worth knowing before re-reading the parked state:
> `grub.cfg` and `grub.cfg.mine` were made **byte-identical** (7148 B each), the
> pinned default lives in both, and the malformed stray line
> `'set default AROS64 with native Gfx '` that lived only in `.mine` was removed.
> So `.mine` is **no longer a backup of the superseded state** — the distinction
> this record relied on has been erased.
>
> **Not confirmed:** depth. The entry requests `x32`, this record observed the
> single VBE mode as **1280x1024x24**, and `screendump` emits RGB regardless, so
> the discrepancy is still open.

- GRUB default = native (wide-capable); backup `grub.cfg.mine` on DH0.
- Quarantined on DH0: `sb128.audio`→`.bak`, mode files `SB128`/`VIA-AC97`/`HDAUDIO`/`CMI8738`/`NVHDMI`→`SYS:Storage/*.bak`, `ac97.audio`→`.bak` (only `DEVS:AudioModes/ac97` left; restore order is the reverse).
- `ENVARC:RIAPP_EVLOG=RAM:` persists; `start_riqemu1.sh` now `-vga vmware` + `-audiodev pa` + `-device intel-hda,msi=off -device hda-duplex` (AC97 stanza superseded; HDA still silent — see above).
- Scratch guest tools (in `/tmp`, not the repo): `MODES` (list + `SetEnv MODES_OPEN` screen proof), `BESTMODE`, `AHANG` (ahidevice hang pinpointer, unkillable when wedged — reboot to clear).
- Disclosures: a GrimReaper cleanup click aimed at Kill hit Reboot instead (only the session's own crashed process present; apologized); a two-instance AHI-contention freeze followed (never run two RIAPPs — known freeze).

## Open owner items

- Dell crash (theirs to diagnose; offer: compare Reaper Module line against `hdaudio.audio` here).
- Pick the wide Workbench mode in ScreenMode prefs (lane ready).
- Voice count 8 + stereo approved 2026-09-30 (P6c/P6d continue on host; P6b held uncommitted for Dell proof).
