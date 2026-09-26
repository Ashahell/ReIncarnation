# Portability T1–T10 implementation: PAL lands, canvas moves, gates hold (§12.12)

- Source: ReIncarnation session, 2026-09-26 (opencode implementation run)
- Collected: 2026-09-26
- Published: 2026-09-26
- Plan: `docs/superpowers/plans/2026-09-26-portability-plan.md` (commit `c05d49e`)
- Commits: `0c14362` (T1) … `20d7c5b` (T3b); every commit RED→GREEN + mutant + `AUDIT 0/0 PASS`
- Evidence: `docs/evidence/portability/` (`red-t8*.txt`, `t1-atomics.md`, `t3-input.md`, `t3b-events.md`, `t4-audio.md`, `t5-midi.md`, `t6-paths-log.md`, `t7-image.md`, `t8-headless.md`, `t9-float.md`)

## What landed

- **T1 atomics** (`0c14362`, t84): `platform/pal/ri_pal_thread.h` (acquire/release over GCC/Clang `__atomic`, MSVC compiler barrier; ARM64-Windows fence is T11); migrated ctlplane head/tail, `RIAutoPub.front/staged`, RISeq snap/pending (pointer atomics), AuLive words, meter seqlock. TSan-clean; plain-store mutant flagged by TSan. Two live bugs fixed en route: `live_task` fail-path struct compares (`mygen == s_open_gen`) and `s_open_gen.v++` outside the API.
- **T3a keys** (`472052a`, t85): `RI_KEY_*` positional aliases (= Amiga raw); Win32 Set-1 + SDL maps, completeness-tested against all 68 keymap-used codes.
- **T3b events** (`20d7c5b`, t91): `rsection.mcc.c` HandleEvent ported branch-for-branch to `app/core/canvas_events.c`; the MCC maps IDCMP to kind ints (CHANGED/EAT never leak to MUI). `from_n` deleted.
- **T4 audio** (`e7656b0`, t88/t90): `app/core/live_driver.c` (policy: double buffer, s16 conv, xruns, transport word, W capture, injected clock); AHI keeps objects/hooks/task only; host null backend (sync deterministic WAV loop). t88 proves driver == offline bit-exact at 64/128/1024.
- **T5 MIDI** (`550a372`, t89): `platform/aros/midi_camd.c` (receiver/sender/poll/close, owns `CamdBase`); sectproof remote + midisend playback on PAL; SELFTEST stays raw-CAMD by design (it diagnoses the stack). Host script-file backend.
- **T6 paths/log** (`f8bb48b`, t86): `ri_pal_fs`/`ri_pal_log`; all 8 `SYS:`/`RAM:` literals moved (sectproof, riapp `rlog` now variadic via vsnprintf — RawDoFmt packing trap gone, auplay temp); literal gate in audit.
- **T7 image** (`8bcdb82`, t87): decode moved to `platform/aros/image_dt.c` (TEMP staging, documented); libpng host backend; all 808-RI PNGs cross-checked + mkskin spots (bg02 lacquer, knob body).
- **T8 headless, partial** (`1aa2c24`): `platform/host/main_headless.c` renders the demo fixture through driver+null backend (375 buffers, 0 xruns, deterministic sha256 `a9ec1715…f9ec`, peak 19168); `build/portable.mk`; audit determinism gate. `riapp_core` + panel PNGs need T2.
- **T9 evidence** (`t9-float.md`, uncommitted with T9): GCC 16.2.1 vs Clang 22.1.8 bit-identical on sched-check, first-light, 808×6, 909×6, PCF, mixer. mingw absent; MSVC deferred; denormal policy proposed (FTZ ON via `ri_pal_fpu_setup`, owner call).
- AROS stays first-class throughout: RISECT/RIAPP/MIDISEND re-link 0 UND, r12 gate holds; no behaviour changes (refactors only).

## Findings (portability-relevant)

- **setjmp/volatile (T7):** locals modified between `setjmp` and libpng's `longjmp` must be `volatile` — observed exit 139 under `-O2`. Any backend using setjmp error paths (PNG, JPEG, audio codecs) needs this.
- **Stale-object trap (T3a/T6):** `ri_build_host.sh test` links existing `OUT/*.o` without rebuilding — after editing a backend TU you must rebuild its module first, or the test re-runs the old object (140 FAILs against fixed source). Same class as the golden re-render trap (WBS 2.3 record).
- **Vacuous-coverage catch (T3b):** steppers exist only in FX/transport layouts; the first t91 scanned SYNTH1, silently skipped repeat coverage, and SURVIVED the mutant. Moved to transport; mutant killed. Echoes the t76 drag-law lesson.
- **New-host-TU link tax (T7):** any TU added to `OUT/*.o` needs `-lpng` (now: libpng) on all five tool link lines across `ri_audit.sh`, `ri_fuzz.sh`, `ri_soak.sh` — audit caught it at the render build.
- **CAMD specifics (T5):** `MidiMsg.mm_Data` is a SysEx pointer, not packed bytes — status comes from `mm_Status/Data1/Data2` (sectproof already did this); camd inline calls need one `CamdBase` definition per link (backend owns it, apps `extern`).
- **Host/device divergence pinned (T7):** host PNG cap 8192 vs AROS 4096 — knob strips (45×4480) decode on CI but fall back to Classic on device. Owner call to unify.
- **Remote MIDI Wait (T5):** the loop no longer waits on the CAMD signal bit (PAL exports no signal) — remote drains per event-loop iteration until T8 clock work.
- **Research (subagent):** C11 atomics vs `__atomic` vs MSVC `_Interlocked*` (+`_acq`/`_rel` ARM-only suffixes); WASAPI event-shared init/pull sequence; WinMM `midiInOpen` callback rules; stb_image vs libpng vs clean-room tradeoffs; FTZ/DAZ (MXCSR/FPCR) + `/fp:strict` vs `-ffp-contract=off`; mingw C99 pitfalls (VLAs, `%llu`/UCRT, `long double`, struct packing, text-mode `fopen`).

## Status 2026-09-26 (later): T2, T9 remainder, T10 landed

- **T2 canvas** (`6345908`, t92/t93): `gui/draw/` art layer, AROS replayer
  (pixel-identical by construction), host rasterizer + goldens; T8 PNG half
  closed by the same rasterizer. Device pixel-identity still open.
- **T9 remainder:** VLA-clean (`-Wvla -Werror`); `%llu` confined to host
  dumps (mingw handling noted); `fwrite` field-by-field; GCC/Clang
  bit-identical (see `t9-float.md`).
- **T10:** confinement gates (AROS-include + drawing-call, portable set),
  portable-build gate (`make -f build/portable.mk test headless`),
  mingw-SKIP recorded. `scripts/` stays at 5 files.
- Evidence: `t2-canvas.md`, `red-t92.txt`, `red-t93.txt`,
  `docs/evidence/gui/host-raster/`.

## Open (not started or lane-bound)

- **T8 remainder:** `app/core/riapp_core.c` session move — deferred to a
  lane-available turn (touches Dell-proven RIAPP code; links prove nothing
  about sound).

- **T2 display list** (biggest remainder): `gui/draw/canvas.*` + `ri_pal_draw.h` drafted; `bg_*`/painters transliteration, AROS replayer, host rasterizer + goldens, device pixel-identity proof.
- **T8 remainder:** `app/core/riapp_core.c` session move; headless panel PNGs.
- **T9 remainder:** denormal policy, VLA/`%llu`/struct-`fwrite` sweep, mingw compile-only gate (toolchain absent here), MSVC proof (T11).
- **T10:** AROS-include gate, drawing-call gate (needs T2), portable-build gate, tracker §12.12.
- **Device proofs (owner lanes):** Dell by-ear T4 (0 xruns), riqemu1 G7 remote re-proof, AROS pixel-identity T2, Dell click/drag feel T3b.
- **Owner decisions (§8, undecided):** third-party policy (libpng-host vs stb vs clean-room for Windows), CMake vs Makefile, denormals, WASAPI order, Windows data locations.
- **Cross-post candidates (Vulkan4AROS wiki):** setjmp/volatile rule, stale-object trap, `-lpng` link tax, CAMD `CamdBase`/`mm_Data` notes.

## Resolutions 2026-09-26 (owner, plan §9)

System APIs only; Makefile kept; FTZ ON via `ri_pal_fpu_setup` (t94,
wired at render-task entry); WASAPI shared→exclusive; `%APPDATA%`.
Next priority: device proof round (owner lanes).
