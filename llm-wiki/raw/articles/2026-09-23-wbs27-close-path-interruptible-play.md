# 2026-09-23 — WBS 2.7 close path: interruptible playback on hardware (TDD)

> Source: session evidence (Dell run output, fresh audit run), compiled by agent
> Collected: 2026-09-23
> Published: 2026-09-23

## Disposition
New. The WBS 2.7 AHI backend now has its close path: playback is
interruptible (`AuPlayEx`), and a stop releases EVERYTHING (Close +
CloseDevice + DeleteIORequest + DeleteMsgPort + DeleteFile) — the
negotiate-slice leak lesson applied to the play side. Proven on the Dell
(E6320, the ABIv11 iteration reference — see m1-1; ABIv1 is the
eventual acceptance target) on ONE boot in sequence: negotiate →
stop-play → negotiate → full-play → negotiate, all rc=0, nothing
wedges.

## What
- `audio_io/audio_ahi.h`: `AuPlayEx(struct AudioObject *ao, const volatile
  int *stop)` — like `AuPlay`, but polls the caller-owned stop flag
  between CMD_WRITE chunks, releasing everything on stop and returning 1
  (evidence printed: `RI_AUPLAY stopped %lu bytes rc=1`). `AuPlay(ao)` is
  now a thin wrapper → `AuPlayEx(ao, NULL)`: the full-song path is
  unchanged by construction (same render, same chunks, same release tail).
- `audio_io/audio_ahi_play.c`: loop-top `if (stop && *stop) { stopped =
  1; break; }` between chunks — granularity ≤ 1 chunk (16384 B; ~171 ms
  at 48 kHz mono 16-bit, ~186 ms at the session's 44100 Hz) — then the
  stop-aware release tail. Because the poll sits at the loop top, there is
  never an in-flight IO at release time: the close is the autodoc's clean
  pattern (below), AbortIO-free by construction.
- `scripts/ri_audit.sh`: decl-grep for BOTH `AuPlay` and `AuPlayEx` in
  `audio_ahi.h` after the AHI TU compile gate (Phase 6 style). Genuine
  RED→GREEN: pre-API, the decl check fails; post-API it passes within
  the already-green compile chain.
- TDD: scratch `auplay_stop_main.c` RED first (`error: implicit
  declaration of function 'AuPlayEx'` against the pre-API header) →
  GREEN after the API landed. The stop driver is a spawned task
  (`NewCreateTask` + `TASKTAG_ARG1`, the x11.c:689-691 pattern, entry =
  plain-C fn receiving the params via ARG1) that waits on a
  `play_started` handshake set just before the call, `Delay(50)` (~1 s
  into the ~2.84 s song), then flips `*stop`. Stopped-as-designed maps to
  **exit 0** (any other AuPlayEx return → nonzero rc → job FAIL by
  design).

## Proof (Dell run, agent e6320, session 1, one boot, job
20260923-143658-211894-1)
- `ahi_neg` → rc=0 (160 ms): `RI_AHI_NEGOTIATE rc=0 mode=0x003E0001
  freq=44100 bits=16 maxch=128`
- `auplay_stop` → rc=0 (1222 ms):
  `RI_AUPLAY_STOP start rc=0 backend=null` → `RI_AUPLAY render...` →
  `RI_AUPLAY rendered ok` → **`RI_AUPLAY stopped 81920 bytes rc=1`** →
  `RI_AUPLAY_STOP AuPlayEx rc=1`. 81920 = 5×16384 B chunks ≈ 0.93 s of
  the ~2.84 s song; the dump lands exactly on a chunk boundary
  (≤1-chunk granularity confirmed).
- `ahi_neg` → rc=0 (180 ms): **no wedge after the stop** — the
  all-or-nothing release is why this line matters (a leaked release
  wedges ahi.device unit-0 until reboot).
- `auplay` → rc=0 (3085 ms): `RI_AUPLAY played 250286 bytes rc=0` —
  wrapper full-song path byte-for-byte unchanged.
- `ahi_neg` → rc=0 (180 ms).
- ABI gates re-checked on the three scratch binaries: `task.resource`
  refs 0; real UND 0 (only the `.symtab` null entry) on all three.
- Full `ri_audit.sh` 0/0 — **fresh run after implementation** (AUDIT 0/0
  PASS).

Note: `audio: AHI unavailable - null backend active (offline render
only)` prints on every run BY DESIGN — the engine's `AuStart` selects the
"null" live-sink (audio.c:350), identical to the wbs27 baseline; the
playback proof is the play TU's own ahi.device unit-0 open.

## Compliance note
No Khronos surface in this slice: the repo contains no SPIR-V/Vulkan
(`find` for `*.spv*`/`*.spvasm` = 0), so spirv-val is vacuous. The
API-conformance analog is the ahi.device contract as stated by the
driver's own autodoc (v11 tree
`workbench/devs/AHI/Docs/ahidev.texinfo`, "Writing To The Device"):
*"All I/O requests must be completed before `CloseDevice()`. Abort any
pending requests with `AbortIO()`."* Polling between chunks keeps the
close-time request table empty, so the stop path never needs AbortIO;
`io_Length` 16384 B is an even multiple of the 2-byte frame the autodoc
requires for `AHIST_M16S`; single request, `ahir_Link = NULL` (no double
buffering), blocking `DoIO` — all matching the documented single-buffer
form.

## Files
- `audio_io/audio_ahi.h`, `audio_io/audio_ahi_play.c`,
  `scripts/ri_audit.sh` (committed with this record, `[TC-2.7]`).
- Scratch (uncommitted by convention): `/home/miller/Work/ri_build/`
  `{auplay_stop,auplay,ahi_neg}` binaries + `auplay_stop_main.c`; guest
  `RAM:auplay_stop`, `RAM:auplay`.