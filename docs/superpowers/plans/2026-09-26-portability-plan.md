# Portability plan — a platform layer so a Windows (or any) port becomes cheap

> **Status:** implementation plan, 2026-09-26. The owner asked for "everything needed to make portability cheaper". AROS stays the primary, first-class target; nothing here may regress it (audit 0/0, Dell and QEMU lanes).
> **For agentic workers:** superpowers:executing-plans, TDD with a behavioural RED, mutation proofs, and a full `scripts/ri_audit.sh` before every commit. Same rules as the G9 prompts (`docs/superpowers/plans/2026-09-26-g9-opencode-prompt.md` §3).

## 0. Goal, principle, non-goals

**Goal:** a Windows build becomes "write about six backends against small, tested interfaces", instead of "untangle AROS calls from application logic". The measurable target:
- after this plan, the **AROS-specific code is confined to `platform/aros/`**;
- **the same application core links against a second, non-AROS backend in CI** (Linux host: null + a software/SDL canvas);
- a Windows build is then **only new files under `platform/win32/`** and a build entry.

**Principle — a thin platform layer (PAL), pure-C core:**
- The engine, sequencer, formats and GUI logic are already pure C99 and host-tested (~12.4k lines).
- The platform-bound code is ~3.9k lines. Each area becomes one small C interface, `ri_pal_*`, with:
  - one AROS implementation (today's code, moved and adapted);
  - one host implementation used by CI;
  - later, a Windows implementation.
- The core calls only `ri_pal_*`; backends never call the core except through the callbacks the interfaces define.

**Non-goals (now):**
- no Windows code in this plan's scope beyond a **mingw-w64 compile-only gate** (Task T10);
- no new third-party dependencies without an owner decision (§8);
- no behaviour or look changes — every task is a refactor with bit-exact or pixel-exact evidence.

## 1. Inventory — where the platform is coupled today (verified 2026-09-26, `main` at `4bf98ab`)

| Area | Files (AROS-only today) | What they use | Portable core they serve |
|---|---|---|---|
| Panel drawing | `gui/widgets/rsection.mcc.c` (1046 lines): colour helpers `pen/fill_rect/fill_circle/line/text_c/bevel`, plus `draw_knob/key_909/meter/fader/note_glyph/led_digits/gr_row/tr_key` and per-section `bg_*` | `RectFill`, `Move`/`Draw`, `Text`/`TextLength`, `SetAPen`/`SetDrMd`/`SetFont`, `ObtainBestPen`/`ReleasePen`, `AllocBitMap`/`BltBitMapRastPort` (double buffer) | `panelgeo`, `sectui`, `sect*`, `ctlreg` |
| Widget/window glue | `rsection.mcc.c` MUI methods: `Setup/Cleanup/AskMinMax/Show/Hide/Draw/HandleEvent/CreateShortHelp`; older `rknb/rstp/rlbl/rfdr/rlvl.mcc.c`, `knob_blit.c` | Zune MCC, IDCMP (mouse buttons/move/rawkey/intuiticks), MUI bubble help | `panelui`, `keymap`, `livestate` |
| Skins (images) | `gui/skin_aros.c` | `datatypes.library` / `picture.class` decode (`PDTA_BitMapHeader`, `PDTM_READPIXELARRAY`), `WritePixelArrayAlpha`, `ExAll` dir scan (in `app/sectproof.c`) | `gui/skin.c` (format 1, pure) |
| Live audio | `audio_io/audio_ahi_live.{h,c}`, `audio_ahi.{h,c}`, `audio_ahi_play.c`, `probe_ahi.c` | `ahi.device` low-level: `AHI_AllocAudioA`, `SoundFunc`/`PlayerFunc` hooks, `AHI_LoadSound`/`SetSound`; render Process (`CreateNewProcTags`); `Signal`/`Wait`; EClock (`ReadEClock`) | `engine/live.c`, `engine/seq/ctlplane.c` |
| MIDI | `app/sectproof.c` (remote mode), `app/midisend.c` | `camd.library` (`CreateMidi`, links, `GetMidi`) | `gui/midimap.c` |
| App shells | `app/riapp.c`, `app/sectproof.c`, `app/main.c`, `panel909/stepproof/knobproof.c` | Intuition windows, `Printf`/`VFPrintf`, `RAM:` logs, `Delay`, `AllocVec` | everything |
| Paths / files | `SYS:Classes/ReIncarnation/Mods/` (skins), `RAM:RIAPP.LOG`, `RAM:RIAPP.wav`, `RAM:RISECT.*` | DOS `Open/Read/Write/Seek`, `Lock`, `ExAll` | `project/rbng.c`, `rbnm.c`, `engine/fx/pcf.c` use **stdio `fopen`** (already portable) |
| OS extras (out of scope, stay AROS-only) | `project/arexx_aros.c`, `project/datatypes/rbng.datatype.c`, `rbnm.datatype.c` | ARexx port, datatype classes | — |

**Concurrency assumptions that are unsafe off AROS:**
- `engine/seq/ctlplane.c` (SPSC ring) has **no atomics or barriers**.
- `RIAutoPub` uses plain `volatile`.
- `RISeq` pending/active are documented as "no atomics, no locks".
- `AuLive` fields are `volatile`.
- This works on single-core AROS x86-64, but it is **not correct on a multi-core Windows machine** (compiler and CPU reordering; ARM64 Windows is weakly ordered).

**Key codes:** `gui/keymap` is keyed by **Amiga raw key codes** (positional; key-up = `code|0x80`). That is correct for AROS but needs a platform translation elsewhere.

**Determinism:** host `CFLAGS` are `-std=c99 -O2 -Wall -Wextra -Werror -pedantic -ffp-contract=off -fno-unsafe-math-optimizations -ftrapv`. The D1 goldens are bit-exact on GCC; other compilers need equivalent float settings (T9).

## 2. Target layout

```
engine/ project/ gui/(pure)      ← unchanged portable core (C99, host-tested)
app/core/                        ← NEW portable application core (from riapp.c / sectproof.c logic)
gui/draw/                        ← NEW portable canvas: display-list builder (all section art)
platform/pal/*.h                 ← NEW interfaces (ri_pal_audio, _draw, _input, _midi, _fs, _thread, _image, _log)
platform/aros/                   ← AROS backends (moved: rsection glue, skin_aros, audio_ahi_live, camd, Intuition shell)
platform/host/                   ← CI backends: null audio, software rasterizer canvas, file-based MIDI script, stdio fs
platform/win32/                  ← later: WASAPI, Win32/Direct2D (or SDL), WinMM, %APPDATA%
```

**Audit gates** (added in T10):
- no `<exec/…>`, `<dos/…>`, `<proto/…>`, `<intuition/…>`, `<graphics/…>`, `<libraries/…>`, `<devices/…>` include outside `platform/aros/`, `project/datatypes/`, `project/arexx_aros.c` and the legacy proof apps (listed explicitly);
- no drawing API call outside `platform/*/`;
- `platform/pal/*.h` include only `<stdint.h>` / `<stddef.h>`.

## 3. Interfaces (sketches — the plan fixes the signatures in each task's RED step)

All are C99, pointer + length, no allocation inside hot paths, and return `0` on success or a small error code. Every interface gets a host test through the `platform/host` backend.

### 3.1 `ri_pal_thread.h` — atomics, threads, time (T1)
```c
typedef struct { volatile uint32_t v; } ri_atomic_u32;
uint32_t ri_atomic_load_acq(const ri_atomic_u32 *a);
void     ri_atomic_store_rel(ri_atomic_u32 *a, uint32_t v);
/* C11 <stdatomic.h> when __STDC_VERSION__ >= 201112L && !__STDC_NO_ATOMICS__,
 * MSVC _InterlockedExchange/_ReadWriteBarrier, AROS: compiler barrier +
 * x86 TSO. One header, per-compiler blocks, host-tested under threads. */
typedef struct ri_thread ri_thread;          /* opaque, backend-defined */
int  ri_thread_start(ri_thread **t, void (*fn)(void *), void *arg, int prio_hint);
void ri_thread_join(ri_thread *t);
typedef struct ri_event ri_event;            /* auto-reset wake: hook -> render thread */
int  ri_event_create(ri_event **e); void ri_event_signal(ri_event *e);   /* callable from audio callback */
int  ri_event_wait(ri_event *e, uint32_t timeout_ms);                    /* 0 signalled, 1 timeout */
uint64_t ri_time_us(void);                   /* monotonic; AROS EClock, host clock_gettime, Win QPC */
```

### 3.2 `ri_pal_audio.h` — device I/O only; policy moves to the core (T4)
The double-buffer policy, xrun accounting, transport request word and W capture move to `app/core/live_driver.c` (portable). A backend only:
```c
struct ri_audio_cfg { uint32_t want_rate, frames, channels; };
struct ri_audio_info { uint32_t rate, frames, format; /* RI_FMT_S16, RI_FMT_F32 */ char name[32]; };
/* The backend calls pull() on ITS realtime thread (or signals the core's
 * render thread, backend's choice) once per device buffer. */
typedef void (*ri_audio_pull)(void *user, void *out, uint32_t frames);
int  ri_pal_audio_open(const struct ri_audio_cfg *c, ri_audio_pull pull, void *user, struct ri_audio_info *got);
int  ri_pal_audio_start(void); void ri_pal_audio_stop(void); void ri_pal_audio_close(void);
uint32_t ri_pal_audio_late(void);            /* backend-observed under-runs */
```
- **AROS:** today's AHI render Process and SoundFunc double buffer; `pull()` runs in the render Process (hook → `ri_event_signal`).
- **Host CI:** a null backend with a deterministic pull cadence, writing WAV.
- **Windows (later):** WASAPI event-driven shared mode (or miniaudio, §8).

### 3.3 `ri_pal_draw.h` + `gui/draw/` — display list (T2, the biggest win)
All section art becomes **backend-free**: `gui/draw/canvas.c` records primitives into a caller-owned display list; the backend replays it.
```c
enum ri_dop { RI_D_RECT, RI_D_LINE, RI_D_CIRCLE, RI_D_TEXT, RI_D_IMAGE, RI_D_CLIP };
struct ri_dcmd { uint8_t op, align, pad[2]; int16_t x0, y0, x1, y1; uint32_t rgb; uint16_t img, frame; const char *text; };
struct ri_dlist { struct ri_dcmd *cmd; uint32_t n, cap; };
/* text metrics come from the backend (fonts differ per platform): */
struct ri_text_metrics { int (*width)(void *ctx, const char *s); int height, baseline; void *ctx; };
void ri_draw_section(struct ri_dlist *out, const struct RISectUI *ui, uint8_t section, int zoom,
                     int ox, int oy, const struct ri_text_metrics *tm, const struct RISkin *skin);
/* backend: */ void ri_pal_draw_replay(void *surface, const struct ri_dlist *dl);
```
- Colours become RGB tokens (today's `RI_RSECT_RGB` table). Pen obtaining becomes a backend concern: AROS keeps `ObtainBestPen` per screen, and true-colour backends use RGB directly.
- Double buffering is a backend concern (AROS keeps its friend bitmap and single blit).
- **Host software rasterizer** (`platform/host/raster.c`, clean-room, no fonts beyond a built-in 5x7 bitmap face for tests): replays a list into RGBA. This enables **pixel goldens of every section, zoom and skin on Linux CI**, which catches the kind of layout bugs the owner found by eye on the Dell (303 rim, 909 white labels) before a device run.
- The tests also pin a **display-list hash per (section, zoom, skin)**, so any drawing change is deliberate.

### 3.4 `ri_pal_input.h` — normalised events (T3)
```c
enum ri_ev_kind { RI_IN_MOUSE_DOWN, RI_IN_MOUSE_UP, RI_IN_MOUSE_MOVE, RI_IN_WHEEL, RI_IN_KEY_DOWN, RI_IN_KEY_UP, RI_IN_TICK };
struct ri_input { uint8_t kind, button; uint16_t key; /* RI_KEY_* positional */ uint16_t qual; int16_t x, y, dx, dy; };
```
- `RI_KEY_*` is a **positional** set matching today's Amiga raw codes (keymap stays unchanged).
- Each backend maps its native scancodes to `RI_KEY_*` with one table: AROS identity, Windows scan codes set 1, SDL scancodes. Test the tables for completeness against Appendix E's key list.
- The canvas event logic in `rsection.mcc.c` (hit test, drag accumulation, 150-px law, repeat on ticks) moves to `app/core/canvas_events.c` (pure, host-tested with synthetic events).

### 3.5 `ri_pal_midi.h` (T5)
```c
typedef void (*ri_midi_in)(void *user, const uint8_t *msg, uint32_t len, uint64_t time_us);
int ri_pal_midi_open_in(const char *port, ri_midi_in cb, void *user);  /* AROS CAMD cluster, Win WinMM, host: script file */
int ri_pal_midi_send(const char *port, const uint8_t *msg, uint32_t len);
void ri_pal_midi_close(void);
```
Messages go into an SPSC queue that the GUI thread drains into `midimap` (today's G7 logic, unchanged).

### 3.6 `ri_pal_fs.h` + `ri_pal_log.h` (T6)
```c
enum ri_path { RI_PATH_MODS, RI_PATH_SONGS, RI_PATH_TEMP, RI_PATH_PREFS };
int ri_pal_path(enum ri_path p, char *out, uint32_t cap);   /* AROS: SYS:Classes/ReIncarnation/Mods/, RAM:, ENVARC:…; Win: %APPDATA%\ReIncarnation\… */
int ri_pal_path_join(char *out, uint32_t cap, const char *dir, const char *leaf);   /* ':' vs '\\' */
int ri_pal_list_dirs(const char *dir, int (*cb)(void *u, const char *name), void *u); /* replaces ExAll scan */
int ri_pal_read_file(const char *path, void *buf, uint32_t cap, uint32_t *got);
int ri_pal_write_file(const char *path, const void *buf, uint32_t n);
void ri_log(const char *fmt, ...);  /* core-side formatter (portable vsnprintf), backend sink */
```
- Kill the platform-specific formatting trap: the core formats with `vsnprintf`, so `RawDoFmt` packing rules stay inside the AROS sink.
- The stdio users (`rbng.c`, `rbnm.c`, `pcf.c`) stay on stdio; only their **path construction** moves to `ri_pal_path`.

### 3.7 `ri_pal_image.h` (T7)
```c
int ri_pal_image_decode(const void *file, uint32_t len, uint32_t **rgba, uint32_t *w, uint32_t *h); /* caller frees via ri_pal_image_free */
```
- **AROS:** datatypes, with the size from `PDTA_BitMapHeader` (lesson learned).
- **Host / Windows:** a portable PNG decoder (§8 decision: a clean-room minimal decoder in `tools/`-grade C, or a vetted public-domain one).
- Skin **blitting** becomes `RI_D_IMAGE` in the display list; per-backend alpha blit.

## 4. Tasks (in order; each is its own RED→GREEN commit with audit 0/0)

### T1 — Atomics and SPSC hardening (do first; it is a correctness fix even on AROS SMP)
- Add `platform/pal/ri_pal_thread.h` atomics (header-only) and switch these to acquire/release: `ctlplane` head/tail, `RIAutoPub.front/staged`, `RISeq` pending/active pointers, and `AuLive` cmd/state/cap fields.
- **Host test (new t-number):** a pthread producer/consumer stress on the ring (1e7 messages, order + no loss + coalesce law), run under `-fsanitize=thread` in a separate audit line (skip if the toolchain lacks TSan; say so).
- **Mutant:** replace the release store with a plain store; TSan must flag it (record it).
- AROS: build gates unchanged; a Dell RIAPP run with 0 xruns.

### T2 — Display list: move all art out of `rsection.mcc.c`
1. Create `gui/draw/canvas.c` + `gui/draw/art_*.c` (one per section group) by **moving** the helper functions and `bg_*`, with `RastPort` calls replaced by display-list recording.
2. `rsection.mcc.c` becomes the AROS replayer: pens, friend bitmap, `WritePixelArrayAlpha` for images.
3. **Equivalence proof on AROS:** before the move, capture every section × zoom × {Classic, 808-RI} on riqemu1 (monitor screendump, full resolution) and on the Dell (half-scale agent capture). After the move, the captures must be pixel-identical. Record the hashes.
4. **Host goldens:** the software rasterizer renders the same matrix; commit the hashes plus a few PNGs to `docs/evidence/gui/host-raster/`.
5. The host test pins a display-list hash per (section, zoom, skin).

### T3 — Input normalisation
- `RI_KEY_*` table (positional); the AROS mapping is the identity; a completeness test against the keymap's used codes.
- Move the canvas event logic to `app/core/canvas_events.c`, with a host test using synthetic events: click, drag with the 150-px law at every zoom, arrow repeat, focus rules — mirroring today's t70/t71 behaviours.

### T4 — Audio split: portable live driver + thin AHI backend
- `app/core/live_driver.c` owns:
  - the double buffer and f32 → s16/s24 conversion;
  - xrun accounting;
  - the transport request word;
  - W capture (numbered takes);
  - render timing via `ri_time_us`.
- `platform/aros/audio_ahi.c` keeps only the AHI objects and the SoundFunc/PlayerFunc → `ri_event_signal`.
- **Host backend:** `platform/host/audio_null.c` pulls on a deterministic cadence and writes a WAV.
- **Test:** host live-driver output == offline render (t81 law), bit-exact, at 64/128/1024-frame buffers.
- **Dell check:** RIAPP sounds as before (owner by ear) with 0 xruns.

### T5 — MIDI split
- CAMD code moves to `platform/aros/midi_camd.c`.
- Host backend: a script-file MIDI source, deterministic, for tests.
- The G7 remote proof re-runs on riqemu1 through MIDISEND with the same traces.

### T6 — Paths, dir scan, logging
- All hard-coded `SYS:` and `RAM:` strings go through `ri_pal_path`; `ExAll` goes behind `ri_pal_list_dirs` (keeping the fixed walk: `ED_TYPE`, `ed_Next`, `ExAllEnd`).
- `ri_log` uses core-side `vsnprintf`.
- Grep gate: no `"SYS:"`, `"RAM:"`, `"ENV:"` literals outside `platform/aros/`.

### T7 — Image decode split
- `skin_aros.c` decode goes to `platform/aros/image_dt.c`.
- Host decoder, per the §8 decision.
- **Tests:** the host decodes every shipped skin PNG, and its pixels match `tools/mkskin.c`'s known spot pixels (mkskin already verifies these).

### T8 — Portable application core
- `app/core/riapp_core.c` takes from `riapp.c` and `sectproof.c` everything that is not window or OS glue: session wiring, demo song, key bindings (`RI_KEY_*`), recording, meter publishing and the section layout.
- `platform/aros/main_riapp.c` / `main_risect.c` shrink to window creation plus the event pump.
- Host: `platform/host/main_headless.c` runs the core with scripted input and writes a WAV plus a PNG of the panel. This is **the CI proof that the core runs without AROS**.

### T9 — Compiler and float portability
- **Flag matrix:**
  - GCC/Clang: `-std=c99 -ffp-contract=off -fno-unsafe-math-optimizations -fno-fast-math`, SSE2 math (x86-64 default);
  - MSVC: `/std:c11 /fp:strict /arch:SSE2`-equivalent (x64 default), `/W4 /WX`.
- **D1 cross-compiler proof:** render the golden set with GCC, Clang and mingw-w64 (and later MSVC); all must be **bit-identical**. List every golden that differs and why before "fixing" anything.
- **Denormals:** decide one policy (flush-to-zero on or off) and set it explicitly at render-thread start through a PAL call (`ri_pal_fpu_setup`). Today it is implicit per OS.
- **Remove C99 constructs MSVC rejects**, if any: VLAs; `static inline` in headers is fine; `%llu` formatting goes through `ri_log`. Also check struct packing assumptions in the formats (the IFF codecs must write bytes explicitly; verify `rbng.c`/`rbnm.c` never `fwrite` a struct).

### T10 — Build and CI
- **Build system:** keep the 5-script rule for the AROS lane. Add **one** portable build description (CMake or a plain Makefile) that builds the core, the host PAL, the tests and `main_headless`. Owner call: CMake is conventional on Windows; a Makefile keeps zero dependencies.
- **Audit additions:**
  - host PAL build + `main_headless` run (WAV hash + PNG hash);
  - **mingw-w64 compile-only** of the core + `platform/pal` + a stub `platform/win32` (every function returns "unsupported"), which proves the core compiles for Windows from day one;
  - the §2 grep gates.
- Keep `scripts/` at exactly 5 files (the gate): put the portable build under `build/`, not `scripts/`.

### T11 — Windows backends (separate milestone, after T1–T10)
Only after the owner says go:
- `platform/win32/audio_wasapi.c` (event-driven shared mode first; exclusive mode and ASIO later);
- `canvas_d2d.c` or `canvas_gdi.c` (or SDL2 for both window and canvas);
- `input_win32.c` (scancode table → `RI_KEY_*`);
- `midi_winmm.c`, `fs_win32.c` (`%APPDATA%\ReIncarnation\Mods`, songs in Documents), `image_wic.c`, `main_win32.c`.

Acceptance: the same demo, the same WAV hash as the host headless run, and an owner listening test on a Windows PC.

## 5. What already makes this cheap (keep it that way)

- The engine has no allocation, IO or OS calls; everything is fed by events and caller-owned buffers.
- Formats use stdio and explicit parsing; skins are format 1 with named keys (portable file layout).
- The GUI behaviour (`sect*`, `sectui`, `panelui`, `keymap`, `livestate`, `midimap`, `panelgeo`, `ctlreg`, `skin`) is pure and tested.
- One control path (lane keys → control plane → engine) serves knobs, MIDI and automation alike.

**Rule going forward (add it to the G9b prompt and CLAUDE-style notes):** new code goes into the core or behind a `ri_pal_*` interface. **Never add a new AROS call outside `platform/aros/`** once T2–T6 have landed.

## 6. Sizing (rough, for planning)

| Task | Moves / new lines (est.) | Risk |
|---|---|---|
| T1 atomics | +150, touches 4 modules | low; high value |
| T2 display list + host rasterizer | move ~800, new ~700 | medium (pixel-identity proof on AROS) |
| T3 input | move ~250, new ~200 | low |
| T4 audio split | move ~300, new ~250 | medium (Dell by ear) |
| T5 MIDI | move ~300, new ~150 | low |
| T6 paths/log | ~200 | low |
| T7 image | ~150 + decoder | low/medium (decoder choice) |
| T8 app core | move ~900, new ~300 | medium |
| T9 compiler/float | ~100 + investigation | medium (D1 goldens) |
| T10 build/CI | ~200 | low |
| T11 Windows | ~2–3k new | separate milestone |

After T1–T10, a Windows port is essentially T11. The rough estimate for T11 is 2–3 weeks, plus Windows audio-latency tuning.

## 7. Acceptance for this plan (T1–T10)

- The AROS lanes are unchanged: Dell RIAPP by ear with 0 xruns, and RISECT pixel-identical captures for every section × zoom × skin.
- `main_headless` on Linux produces the demo WAV (hash pinned, equal to the offline render) and panel PNGs (hashes pinned).
- The mingw-w64 compile-only gate passes, and the §2 grep gates pass.
- D1 goldens are bit-identical across GCC and Clang, and mingw is at least compile-clean.
- Evidence file `docs/evidence/portability/` + an llm-wiki record + a tracker section "§12.12 Portability layer".

## 8. Owner decisions (list them in each report; do not decide)

1. **Third-party code policy for non-AROS backends**, each with a license record:
   - SDL2 (zlib);
   - miniaudio (public domain / MIT-0);
   - stb_image (public domain);

   or clean-room only.
2. CMake vs a plain Makefile for the portable build.
3. Denormal policy (flush-to-zero on/off) as a locked D1 rule.
4. The Windows audio API order: WASAPI shared → exclusive → ASIO (ASIO SDK licensing).
5. Windows data locations (`%APPDATA%` vs portable-folder mode).
