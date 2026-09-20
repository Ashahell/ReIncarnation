# Hosted AROS SDL games black-screen + SIGILL-cascade under WSL2 — root cause is missing ALSA→PulseAudio bridge (NOT r12)

**Date:** 2026-07-16
**Status:** ✅ Fixed — MBX, xRick, SokobanGP2X all run after the WSL2 audio fix
**Fix lives in:** WSL host environment (not the repo, not the toolchain): `libasound2-plugins` + `~/.asoundrc`

## Symptom

On the hosted `linux-x86_64` (gcc-16) build under WSL2, three ABITEST-kit games — **MBX (Magical Broom eXtreme), xRick, SokobanGP2X** — opened a **black window** and crashed. Serial showed, in order:

```
SDL_SetError: Unable to open AHI device! Error code -1.
[KRN] Trap signal 4  … RIP=0x…9a5152  (hundreds of times, RSP -0x400 each)
[KRN] Trap signal 11 … RIP=0x…fd20af  (stack unwind)
… ending in the app binary at 0x6079xxxx
```

## The misdirection (and why it was wrong)

Deadwood's initial hypothesis was that this was the **r12 library-base miscompile** ([2026-07-16-gcc16-o2-r12-fixed-registers-miscompile]) manifesting "randomly." The decisive test refuted that: **the r12-fixed *release* hosted build (`core-linux-x86_64`, `-O2`, built with the FIXED toolchain) still reproduced the crash.** So it was not r12.

Reading the trap flood confirmed it:
- `Trap signal 4` = **SIGILL**, at a *fixed* `RIP` in the **hosted kernel/runtime**, `RSP` marching down exactly `0x400` per iteration = the trap handler **faulting on itself and re-entering** → stack overflow → the final `Trap signal 11` (SIGSEGV).
- `R12` during the flood held a scratch value (`= RAX`), **not** a library base (`RBX` = SysBase throughout). This is host-side trap dispatch, where the AROS r12 convention doesn't even apply.
- The games are **prebuilt** ABITEST binaries (reference toolchain), so they can't carry *our* gcc-16 r12 bug in their own code.

## Root cause

**WSLg provides PulseAudio, not ALSA.** Hosted AROS's AHI backend opens ALSA on the host; with no ALSA→Pulse bridge, the ALSA device open fails → `Unable to open AHI device! Error code -1`. The games' SDL audio init then took a **fatal path off the failed open** — the SIGILL cascade was a *symptom of the failed audio init*, not a codegen bug.

Confirmed state on the WSL box before the fix:
- `pactl info` → PulseServer works (`unix:/mnt/wslg/PulseServer`, RDPSink present) — **Pulse is fine**.
- `dpkg -l libasound2-plugins` → **not installed** (`un … <none>`) — no `pulse` ALSA plugin.
- no `~/.asoundrc` — ALSA `default` doesn't route to Pulse.

## The fix (WSL host, run as the AROS build user — here root)

From Deadwood's guide <https://arosnews.github.io/OWB-axrt-wsl2/> ("Problem with sound"):

```sh
apt-get install -y libasound2-plugins        # provides libasound_module_pcm_pulse.so
cat > ~/.asoundrc <<'EOF'
pcm.!default { type pulse }
ctl.!default { type pulse }
pcm.pulse    { type pulse }
ctl.pulse    { type pulse }
EOF
```

Notes / gotchas found in practice:
- The **inline `type pulse` form is more robust** than Deadwood's shorthand `pcm.default pulse`: on this box `/usr/share/alsa/pcm/pulse.conf` was **absent** (only `/usr/share/alsa/alsa.conf.d/50-pulseaudio.conf` defines `pcm.pulse`), so a bare `pcm.default pulse` reference could fail to resolve; `type pulse` calls the plugin `.so` directly and doesn't depend on that conf.
- **No WSL restart needed.** ALSA reads `.asoundrc` at app startup, so a fresh `AROSBootstrap` launch picks it up — and `wsl --shutdown` would kill the running dev session.
- `PULSE_SERVER=unix:/mnt/wslg/PulseServer` is already set by WSLg; root reaches the (uid-1000-owned) socket fine.
- `aplay`/alsa-utils aren't required (AROS links `libasound` directly); their absence just makes `aplay -L` empty — not a fault.

## Result

After the fix: **AHI opens, no `SDL_SetError`, and all three games run.** The SIGILL cascade was entirely downstream of the audio-open failure.

## Lessons

- **A `SDL_SetError: Unable to open AHI device` under WSL2 is not benign** — for SDL apps that don't gracefully handle a failed audio-open, it can cascade into a SIGILL→stack-overflow crash that *looks* like a codegen/ABI bug. Fix the host audio bridge first before chasing a miscompile.
- **The r12 fix does not explain everything.** A single decisive test (r12-fixed release build still crashing) saved a long dead-end. When a hypothesis predicts a fix and the fixed build still fails identically, the hypothesis is wrong — pivot.
- **Prebuilt reference-toolchain binaries can't carry our gcc-16 bugs in their own code** — only via the shared `.library`s they call. That narrows where a crash in such a binary can originate.
- **Environment vs repo:** this fix lives in the WSL host (`libasound2-plugins` + `~/.asoundrc`), not in the source tree or `gcc-16.1.0-aros.diff` — so there is **no audit-script gate** for it (the audit checks repo/toolchain state, which this doesn't touch). It's captured here and in the aros-development skill instead.

## Files

- WSL host: `~/.asoundrc`, apt package `libasound2-plugins`.
- Reference: <https://arosnews.github.io/OWB-axrt-wsl2/> (OWB on AxRT WSL2 install guide, "Problem with sound").
