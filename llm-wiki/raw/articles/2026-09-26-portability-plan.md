# Portability plan: a platform layer so a Windows port becomes cheap (§12.12)

- Source: ReIncarnation session, 2026-09-26 (owner asked "how hard to port to Windows", then for a plan to make it cheaper)
- Collected: 2026-09-26
- Published: 2026-09-26
- Plan: `docs/superpowers/plans/2026-09-26-portability-plan.md` (commit `c05d49e`)

## Measured starting point (main at `4bf98ab`)
- **Portable core:** ~12.4k lines of pure C99, host-tested today — engine, sequencer, automation, formats (stdio), and GUI logic (`ctlreg`, `panelgeo`, `sect*`, `sectui`, `panelui`, `keymap`, `livestate`, `midimap`, `skin`).
- **AROS-only:** ~3.9k lines —
  - `gui/widgets/rsection.mcc.c` (1046 lines of Zune canvas and drawing helpers);
  - `gui/skin_aros.c` (datatypes);
  - `audio_io/audio_ahi*` (AHI);
  - CAMD in `app/sectproof.c`/`midisend.c`;
  - the Intuition app shells;
  - hard-coded `SYS:` and `RAM:` paths.
- **Unsafe off AROS:** the SPSC control plane, `RIAutoPub`, the `RISeq` pending/active pair and the `AuLive` fields use no atomics (plain or `volatile`). That is fine on single-core AROS x86-64, but not on multi-core or ARM64 Windows.
- **Key codes:** `keymap` is keyed by Amiga raw codes (positional), which needs a per-platform scancode table.
- **Determinism:** the D1 goldens rely on GCC float flags (`-ffp-contract=off -fno-unsafe-math-optimizations`); cross-compiler bit-identity is unproven.

## Answer given to the owner
Porting is moderate: the core ports as-is. Audio (WASAPI/ASIO), the panel canvas (Win32/Direct2D or SDL), skin image decode (WIC/stb), MIDI (WinMM) and the app shell need Windows backends — roughly a few weeks, plus audio-latency tuning. It is cheaper if a thin platform layer is introduced now.

## Plan (T1–T10 now, T11 Windows later)
- **T1:** acquire/release atomics for all cross-thread words (a correctness fix on its own), plus a TSan stress test.
- **T2:** all section art becomes a backend-free **display list** (`gui/draw/`); the AROS canvas becomes a replayer. A **host software rasterizer** gives pixel goldens of every section × zoom × skin on Linux CI, catching the kind of bugs the owner found by eye on the Dell. The AROS pixel-identity is proven before and after the move.
- **T3:** normalised input with a positional `RI_KEY_*` set; the canvas event logic moves to a pure core.
- **T4:** the live driver (double buffer, xruns, W capture, conversion) moves to a portable core; AHI becomes a thin backend.
- **T5:** MIDI split.
- **T6:** `ri_pal_path` / `ri_pal_list_dirs` / `ri_log` (core-side `vsnprintf`, which removes the RawDoFmt packing trap).
- **T7:** image decode split.
- **T8:** a portable app core plus a host `main_headless` that writes the demo WAV and panel PNGs in CI.
- **T9:** compiler/float flag matrix; D1 goldens bit-identical across GCC/Clang/mingw; explicit denormal policy.
- **T10:** a portable build description, a **mingw-w64 compile-only gate**, and grep gates (no AROS includes or drawing calls outside `platform/aros/`).
- **T11:** Windows backends (WASAPI, D2D/GDI or SDL, WinMM, WIC, `%APPDATA%`). After T1–T10 it is roughly 2–3k new lines (2–3 weeks, estimated).

## Owner decisions listed
- The third-party code policy (SDL2 zlib, miniaudio, stb public domain, vs clean-room).
- CMake vs Makefile.
- Denormal policy.
- The Windows audio API order.
- Windows data locations.
