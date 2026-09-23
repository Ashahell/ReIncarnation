# 2026-09-23 — WBS 2.7 AHI backend: negotiate + playback on hardware (TDD)

> Source: session evidence (Dell run output ×3, audit ×3), compiled by agent
> Collected: 2026-09-23
> Published: 2026-09-23

## Disposition
Update. First engine-on-hardware code (prior hardware runs were probes);
BOTH the negotiate and playback slices are now green on the Dell (ABIv1
target; the E6320 is our reference box). The leak-wedge theory from the
09-22 session is POSTMORTEMED and FIXED: one leaked `AllocAudioA` wedges
the driver-global hardware reservation until reboot (blocks unit-0 AND
fresh NO_UNIT opens); the negotiator now releases everything by design.

## What
- `audio_io/audio_ahi.c|.h`: `au_ahi_negotiate()` — OpenDevice
  NO_UNIT, base from io_Device, BestAudioID (48k stereo HiFi),
  AllocAudioA (NO PlayerFunc hook, NO Min/Max clamp — minimal), attrs
  read-back. **Leak-free:** FreeAudio + CloseDevice + port/req delete
  — the 09-22 version leaked the open; that leak is the wedge.
- `audio_io/audio_ahi_play.c` (NEW, own TU): `AuPlay` — render the
  whole song to `RAM:auplay.wav` (engine's own `AuRenderToFile`),
  reopen `ahi.device` unit 0, stream the WAV in 16384-byte chunks via
  blocking CMD_WRITE, close + delete temp. `AHIST_M16S` (mono — the
  engine core renders mono; the 09-22 mono-as-stereo bug is on record).
  Split into its own TU so negotiate-only binaries (`ahi_neg`) never
  drag in the engine link.
- Audit: AROS-compile phase covers BOTH TUs (`-fasm` under `-std=c99`
  for v1 headers; `(STRPTR)` casts on every Printf literal — same
  pointer-sign class as the device-name fix, 6 sites).

## Proof (Dell runs, agent e6320, session 1, one boot)
- `ahi_neg` (leak-free) → `rc=0 mode=0x003E0001 freq=44100 bits=16
  maxch=128` — M1.1 numbers, REPEATED (two runs in one submit).
- `auplay` → `played 250286 bytes rc=0` (≈2.84 s mono 16-bit at
  44100), full engine path (AuStart → AuRenderToFile → AuPlay →
  open unit 0 → CMD_WRITE stream → close). Re-render byte-stable.
- **Leak theory proven by sequence:** negotiate → play → negotiate
  all rc=0 on the SAME boot. Pre-fix, one leaked AllocAudio wedged
  every later open (auplay `open unit 0 FAILED ioerr=0` on 09-22,
  fresh ahi_neg failed until reboot). Post-fix nothing wedges.
- Full `ri_audit.sh` 0/0.

## Audit robustness fix found en route
`ri_audit.sh` is now CWD-independent: `cd "$ROOT"` at startup. The
S909 gate (`--909pack`) died with a silent `|| exit 1` when the audit
was invoked from any non-repo CWD (the render tool resolves the pack
inventory CWD-relative at `tools/render.c:714`); the failure printed
NOTHING and only `bash -x` surfaced it. One-line fix, gate now passes
from any invocation directory (verified from Vulkan4Aros CWD).

## Files (uncommitted)
- `audio_io/audio_ahi.h`, `audio_io/audio_ahi.c`,
  `audio_io/audio_ahi_play.c` (new), `scripts/ri_audit.sh`
  (backend AROS-compile lines + CWD pin).
- Scratch: `/home/miller/Work/ri_build/{ahi_neg,auplay,<t>_main.c}`;
  guest `RAM:ahi_neg`, `RAM:auplay`.