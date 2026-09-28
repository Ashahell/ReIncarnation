# Prompt for OpenCode: GUI round 3 (seven improvements), §12.11 G9

> Written 2026-09-28 by the Claude session that built the GUI hardware look (`adc51ee`), the Mix/FX rack bay (`8a61acf`) and rack pass 2 (`48adb49`): brushed bays, rack rails with screws, module seams, a mixer strip per device, a power-button device rail, and a rail that follows the tab. All of it is pushed to `origin/main`.
>
> This prompt has **one preparation step and seven work steps**. Each work step can be finished on its own: finish one, prove it, commit, report, then continue. If you run short of context, stop at a step boundary and write the handoff line (§8).
>
> The most important section is **§4, "How to build GUI here"**. It is the exact working method that produced the current look. Follow it step by step; every shortcut listed there has already cost a day.

---

## 0. Context in one paragraph

ReIncarnation re-creates Propellerhead ReBirth RB-338 2.0.1 for AROS x86-64 as a clean-room instrument rack. The live app `RIAPP` (`app/riapp.c`) plays on a Dell E6320 (ABIv11, 1366x768, real HDA audio).

Its window, top to bottom:
1. the **transport** canvas;
2. a **device rail** of power buttons (one per device, the glyph is the LED; it shows only the active tab's devices, and all four on Mix/FX);
3. a Zune **Register** with four pages:
   - **Synths:** 303A/303B rows, pattern section + voice;
   - **Drums:** 808/909 rows;
   - **Mix:** four mixer strips (TB-303 A, TB-303 B, TR-808, TR-909) sharing one board, in a brushed rack bay;
   - **FX:** PCF, Delay, Dist, Comp in the same kind of bay.

The owner asked for hardware realism: "look as modern as possible", "resemble the original hardware", "feel like working with real hardware", "be extremely critical". The owner judges by eye on the Dell. You cannot see the Dell screen except through half-scale captures.

The seven improvements, in the owner-approved order:

| Step | Improvement |
|---|---|
| S1 | Remove the remaining MUI grey: the transport surround, the window background, and the stock Zune tab bar. |
| S2 | Real lettering: a clean-room condensed sans bitmap face for panel legends. |
| S3 | Faster redraws: dirty-rectangle redraw for knob drags, meters and chase lights. |
| S4 | A master fader on the Mix tab, after the engine accepts master level as a (non-automatable) control. |
| S5 | Window size: auto-fit zoom plus a zoom choice, so the panel uses the screen. |
| S6 | Close the testing gap: make lane clicks reach the app, so the GUI can be verified without the owner. |
| S7 | Per-section skins: the designed but unbuilt selection side. |

**S6 may be done first** if you want the click proof for S1–S5. It touches the lane tooling in the Vulkan4Aros repo and has its own safety rules (§7 S6).

---

## 1. Read first (in this order)

1. `docs/evidence/gui/hw-look/README.md`: the whole hardware-look record, including the three rack passes, what was done and what is open.
2. `llm-wiki/index.md`: the sections "Live app (§12.11)" and "GUI (§12.10)". Then read these raw articles:
   - `2026-09-28-gui-hardware-look-pass.md`
   - `2026-09-28-mix-fx-rack-bay.md`
   - `2026-09-27-rail-led-bitmap-saga.md` (why Bitmap.mui is banned for this job)
3. In the Vulkan4AROS wiki (`/home/miller/Work/projects/Vulkan4Aros/llm-wiki/raw/articles/`):
   - `2026-09-28-aros-exact-rgb-fillpixelarray.md` (**RPTAG_FgColor is ignored on this AROS**);
   - `2026-09-28-zune-rgb-background-group-alignment.md`;
   - `2026-09-27-zune-bitmap-transparency-mask.md`.
4. `docs/superpowers/plans/2026-09-26-g9-opencode-prompt.md` §3 (hard rules) and §4 (lanes). **They still apply**; this file adds to them and corrects the v11 build flags (see §4.6).
5. `docs/superpowers/specs/2026-09-26-skins-design.md`: the whole file, and for S7 especially "Per-module skin choice".
6. `docs/superpowers/specs/2026-09-20-reincarnation-spec.md`:
   - §1 clean-room;
   - §4 realtime (LOCKED);
   - §5 one renderer;
   - §8 owner decisions.
7. The code (§2 says what each part does):
   - `gui/draw/canvas.{h,c}`, `gui/draw/art.h`, `gui/draw/art_shared.c`, `gui/draw/art_*.c`, `gui/draw/art_section.c`;
   - `platform/pal/ri_pal_draw.h`, `platform/host/raster.{h,c}`;
   - `gui/widgets/rsection.{h,mcc.c}`, `gui/widgets/rsection_replay.inc`;
   - `app/riapp.c`;
   - `tests/unit/t92_draw_hash.c`, `tests/unit/t93_raster_goldens.c`, `tests/unit/t61_panelgeo.c`, `tests/unit/t76_zoom.c`, `tests/unit/t75_skin.c`, `tests/unit/t70_keymap.c`.

---

## 2. What exists (do not rebuild it)

### 2.1 The drawing architecture (portability plan T2)

```
section state (gui/sect*.c, pure) ──► ri_draw_section() in gui/draw/  ──►  display list (struct ri_dlist)
                                                                              │
                         ┌────────────────────────────────────────────────────┴───────────────┐
                         ▼                                                                    ▼
    AROS: rsection_replay.inc replay_dl()                            host: platform/host/raster.c
    (FillPixelArray exact RGB on truecolor,                          ri_raster_replay() → RGBA buffer
     pens for text/diagonals, nearest pen on palette)                → PNG, pixel hashes (t93)
```

- **`struct ri_dcmd`** (`platform/pal/ri_pal_draw.h`) has these fields:
  - `op`: `RI_D_RECT`, `RI_D_LINE`, `RI_D_CIRCLE` (filled disc), `RI_D_TEXT` (centred at `x0`), `RI_D_IMAGE` (skin frame) or `RI_D_CLIP` (no-op today);
  - `align`, `pad[2]`;
  - `int16 x0,y0,x1,y1`, `uint32 rgb`, `img`, `frame`, `text`.
- A list is caller-owned and bounded: `ri_dlist_init(dl, backing, cap, spool, spcap)`. TEXT bytes are copied into the spool.
- The **AROS canvas** (`rsection.mcc.c`) paints the whole section into an off-screen friend bitmap and blits it once per draw, so there is no flicker. Its display-list backing is `s_dl_back[24576]`; the 909 at 2x emits about 8.2k commands.
- **Exact colour on AROS:**
  - `rfill_rgb()` calls cybergraphics `FillPixelArray(rp, x, y, w, h, 0xFF000000|rgb)` through a private base `s_rcyber` (`#define __CYBERGRAPHICS_LIBBASE s_rcyber`), opened in `pens_obtain()`.
  - Text and diagonal lines use `s_pens[]`: the `C_*` palette, `ObtainBestPen … PRECISION_EXACT`.
  - `ri_art_index(rgb)` maps an RGB to a palette index; unknown colours use the nearest pen.
- **Public replay for app-side art:** `ri_rsection_replay(struct RastPort *, const struct ri_dlist *)` (`rsection.h`). It needs a section canvas already set up on the same screen, because that is where the pens and `s_direct_rgb` come from. RIAPP always has the transport canvas.

### 2.2 Shared art vocabulary (`gui/draw/art.h`)

**Colour math:**
- `ri_art_mix(a, b, t256)`, `ri_art_shade(c, pct)` (+ toward white, − toward black), `ri_art_luma`.

**Primitives:**
- `ri_art_disc_grad(cx, cy, r, top, bot)`: graded disc as spans;
- `ri_art_panel(x0, y0, x1, y1, base, brushed)`: satin or brushed plate with a rolled edge;
- `ri_art_screw`, `ri_art_led(…, lit, panel)`, `ri_art_bevel` (moulded key);
- `ri_art_knob(…, deg, panel)`, `ri_art_fader`, `ri_art_meter`, `ri_art_lcd_bg`, `ri_art_led_digits`;
- `ri_art_text_c`: centred, upper-cased legend.

**Rack furniture (2026-09-28):**
- `ri_art_bay` (brushed dark plate, grain keyed to its own box);
- `ri_art_rack_rail` (vertical grain, 1U holes, screws in the first and last hole);
- `ri_art_seam` (module edge; two neighbours make a groove);
- `ri_art_power` (moulded cap; the power glyph is the LED; the label dims when off; the cap sinks while pressed).

**Constants:** `RI_ART_BAY_BASE 0x2A2B2F`, `RI_ART_RAIL_W 18`, `RI_ART_SEAM_W 2`, `RI_ART_POWER_H 26`.

### 2.3 App-side MUI classes (`app/riapp.c`)

- **`RArt`** (`MUIC_Area` subclass):
  - `MUIA_RArt_Kind`: `RART_RAIL` / `RART_SEAM_L` / `RART_SEAM_R` / `RART_POWER`; `MUIA_RArt_On`, `MUIA_RArt_Label`.
  - `MUIM_AskMinMax` gives a fixed width, and an unbounded height for rails and seams. Power is text-measured with `_font(obj)` via a local `InitRastPort`.
  - `MUIM_Draw` first lets Area paint the parent background (`MUIA_FillArea TRUE`), then builds the art list into `s_art_back[16384]` and calls `ri_rsection_replay`.
  - The power kind uses `MUIA_InputMode RelVerify`, `MUIA_ShowSelState FALSE`, and redraws on `MUIA_Selected` so the cap sinks while held. It notifies `MUIA_Pressed FALSE → ReturnID`.
- **`RBay`** (`MUIC_Group` subclass): overrides **`MUIM_DrawBackground`**. Zune calls it for the group itself (when it has no `MUIA_Background`) and, through `MUIM_DrawParentBackground`, for every child without a background.
  - It builds `ri_art_bay` over the bay's own `_left.._right, _top.._bottom` (so the grain is stable), then `art_clip()`s the list to the requested box. `art_clip` keeps only rects and axis lines, clipped, which is all the bay emits.
  - It tracks Show/Hide and paints nothing when hidden.
- **Fallbacks** when a class is missing: a flat bay via the penspec background `"2:r1A1A1A1A,1B1B1B1B,1E1E1E1E"`, plain `MUIO_Button`s and plain rectangles.
- **Page builders:**
  - `rack_page(mods, n, slots)`: rail | spacer | column | spacer | rail on an RBay.
  - The column is `{spacer, row, spacer}`, and the row has **`MUIA_Weight 1`**, so it stays at the tallest module and is centred.
  - Each module sits in `rack_slot(mod)` = `VGroup{ HGroup{seamL, mod, seamR}, Rectangle }`, which top-aligns it; Zune groups centre fixed children and have no align attribute.
- **Device rail:**
  - `tab_rail()` builds an RBay HGroup of RArt power buttons plus a filler.
  - `rail_for_tab()` sets `MUIA_ShowMe` per device from `MUIA_Group_ActivePage` of `s_reg`, via the Register notify → `RIAPP_ID_TAB`.
  - `dev_tab(d)` uses `ri_tab_devices` with an all-visible set.
- **Mixer strips:**
  - canvases `C_MXA`, `C_MXB`, `C_MIX` (808, owns the board) and `C_MX9`, in device order `c_mix_canvas[4]`;
  - `ri_sui_bind_board` shares the board;
  - `s_mixslot[d]` is hidden with the device (`dev_visibility_toggle`);
  - `meter_round()` feeds all four strips from `ri_core_meters` (303A/808) and `ri_live_meters_read` `sec_peak[1]`/`[3]` (303B/909).
- **Startup log line** in `RAM:RIAPP.LOG`: `RIAPP panel: tabbed Synths/Drums/Mix/FX + transport, rack bay (open=%ld rack=%ld)`. `open` is `MUIA_Window_Open`; `rack` means both classes were created.

### 2.4 Tests that guard the GUI

- **t92 `draw_hash`:** a display-list FNV hash per (section, zoom); 72 pins.
- **t93 `raster_goldens`:**
  - a host-raster pixel hash per (section, zoom) plus the skinned-808 pin;
  - **rack-furniture property checks** (`rack_checks()`): the bay is dark, fully covered and brushed; the rail has holes and a screw; the power glyph is green only when on, the label dims when off, and the pressed state differs. Mutation-proven.
  - It also writes `docs/evidence/gui/host-raster/*.png` snapshots.
- **t61:** panel geometry. **t76:** zoom. **t75:** skin. **t70:** keymap.
- The audit (`scripts/ri_audit.sh`) runs all of them and also greps the portable dirs for AROS drawing calls: `RectFill|BltBitMapRastPort|WritePixelArrayAlpha|ObtainBestPen|AllocBitMap|MUIM_Draw|MUI_CreateCustomClass` are **banned in** `engine/ platform/ gui/draw/ app/core/ tools/ project/ midi_io/`.

---

## 3. Hard rules (in addition to the G9 prompt §3)

1. **Commits:**
   - Commit tag `[§12.11 G9]`; trailer `Co-Authored-By: OpenCode <noreply@opencode.ai>`.
   - Stage **only your own files by path**, never `git add -A`.
   - Before every commit, run `git status` and list foreign modified files.
   - As of 2026-09-28, another session has uncommitted **Levi** work in: `engine/dsp/levi.{c,h}`, `engine/engine.{c,h}`, `engine/live.{c,h}`, `engine/mixer/mixer.{c,h}`, `engine/fx/route.h`, `engine/seq/songtrack.h`, `engine/framework/ridevice.{c,h}`, `app/core/riapp_core.{c,h}`, `project/rbng.*`, `project/rbnm.h`, `scripts/ri_audit.sh`, several `tests/unit/t7x/t8x/t9x/t100` files, `t104`, `t105`.
   - **Never stage or edit those.** S4 touches that area: see §7 S4 for the rule.
2. **Test first, with a behavioural RED:**
   - Record the first failing assert line in the commit body, then go GREEN.
   - Every new invariant needs a **mutation proof**: mutate, see FAIL, revert, see PASS.
   - The mutant must compile: this is `-Werror`, so an unused variable makes the build fail and silently re-runs the old binary. Read the build output before trusting any PASS or FAIL.
3. **Deliberate re-pins only.** When art changes on purpose, t92/t93 pins move.
   - Re-pin with the script in §4.4 **only for the sections you meant to change**. Say in the commit body which sections moved and why.
   - If a section you did not touch moves, that is a bug, not a re-pin.
4. **Clean-room (spec §1):**
   - No ReBirth, Roland or Propellerhead pixels, and no copied fonts.
   - A third-party font needs the owner's licence decision (§9).
   - The default is in-house glyphs.
5. **The owner judges looks on the Dell, by eye.**
   - Every visual step ends with the Dell running your build and **one precise question** to the owner, for example: "Is the tab bar now indistinguishable from hardware buttons, yes or no?"
   - Never claim "looks right" from a half-scale capture alone. 1-px lines vanish at half scale when their y positions share a parity.
6. **Never publish anything outward.** No trackers, no email. The stealth rule applies: do not name ReIncarnation on public trackers.
7. **Lanes:**
   - Only the owner reboots the Dell.
   - riqemu1 must come back at 1280x1024 after any reboot.
   - Never reboot or reset a lane another session is using.
   - Never quit the owner's own RIAPP instance: on the Dell it is `ram:riapp`, Process 1. Only `Break` processes you started.

---

## 4. How to build GUI here (the working method, step by step)

This is the loop that produced every accepted look. Do **every** step for every visual change.

### 4.1 Put every pixel in `gui/draw/`, never in the MUI class

- New art is a pure function `void ri_art_xxx(struct ri_dlist *dl, int x0, int y0, int x1, int y1, …)`:
  - declare it in `gui/draw/art.h` under a dated comment block;
  - implement it in `gui/draw/art_shared.c`. New files are allowed, but then add them to **all three** object lists: `MOD_draw` in `scripts/ri_build_host.sh`, the `sections` **and** `riapp` loops in `scripts/ri_build_aros.sh`, and `build/portable.mk` if the portable build needs them. Keep `scripts/` at exactly 5 files.
- Colours are `0xRRGGBB` literals or `ri_art_rgb(C_*)`. Build depth with `ri_art_shade`/`ri_art_mix`:
  - a light top-left edge, a dark bottom-right edge;
  - a 1-px drop shadow at +1,+1;
  - graded discs for anything round;
  - specular glints of 1–2 px.
- **Look at real hardware photos in your head, not at other GUIs.**
  - Aluminium is cool grey.
  - The 303 knobs are black with a white line.
  - The 808 sound switches are levers.
  - LEDs have a lens and a glow.
  - Screws are countersunk with a slot.
  - Rack rails have 1U hole triplets (gaps 16/16/12 px at our scale).
- Deterministic "noise" (brushing, grain) uses an integer hash of the coordinate **relative to the object's own box** (`art_hash` in `art_shared.c`), so partial redraws match full redraws.
- The **legend text** helper is `ri_art_text_c(dl, cx, cy, s, C_*)`. It upper-cases and centres; the backends measure. Text colour must be a `C_*` palette colour, because text replays through pens.

### 4.2 Preview on the host before touching AROS

Build the host objects once, then use a scratch renderer (keep it in your scratch dir, **not** in the repo):

```bash
bash scripts/ri_build_host.sh all            # objects in /tmp/ri/build
```

```c
/* $SCRATCH/prev.c — render any art to PNG */
#include <stdio.h>
#include "gui/draw/art.h"
#include "platform/host/raster.h"
static struct ri_dcmd B[24576]; static char SP[32768]; static uint32_t px[1400*800];
int main(void) {
    struct ri_dlist dl; struct ri_raster r;
    ri_dlist_init(&dl, B, 24576u, SP, sizeof SP);
    /* ---- your art here, e.g.: ---- */
    ri_art_bay(&dl, 0, 0, 639, 329);
    ri_art_rack_rail(&dl, 0, 0, 17, 329);
    ri_art_power(&dl, 30, 7, 110, 32, "303A", 1, 0);
    /* ------------------------------- */
    ri_raster_init(&r, px, 640, 330);
    ri_raster_clear(&r, 0xFF00FF);             /* magenta = uncovered pixels */
    ri_raster_replay(&r, &dl, 0);
    printf("cmds=%u\n", dl.n);
    return ri_raster_write_png("/ABS/SCRATCH/prev.png", &r);
}
```

```bash
gcc -std=gnu99 -O1 -I. -o $SCRATCH/prev $SCRATCH/prev.c \
  $(ls /tmp/ri/build/*.o | grep -v -E "/t[0-9]+_|main_headless") -lpng -lm
$SCRATCH/prev && magick $SCRATCH/prev.png -filter point -resize 200% $SCRATCH/prev2.png
```

- Open `prev2.png` and **be extremely critical**. Ask:
  - Does it read as a physical object at arm's length?
  - Is anything flat, including the edges, the shadow direction (always light from the top-left) and the contrast of legends?
- For a whole section, use `ri_draw_section(&dl, &ui, sec, zoom, 0, 0, &tm, 0, 0)` after `ri_sui_init(&ui, sec)`, as `t93 render_one()` does (for mixers, bind a board).
- Count commands: keep a section under ~12k and furniture under ~4k, because the AROS backing is 24576 and `s_art_back` is 16384.
- The host face is a 5x7 test font. **Text in host PNGs is not what the Dell shows** until S2 lands.

### 4.3 Guard it with tests

- **Re-pinnable art** (sections): t92 and t93 pins.
- **Tunable furniture** (the owner is still iterating): property checks in t93 `rack_checks()` style. Examples:
  - "no magenta left";
  - "mean luma < N";
  - "≥ K tone changes down a column";
  - "green pixels only when on";
  - "the pressed hash differs".

  Then mutate each property's cause and watch it fail.
- Run a test:

```bash
bash scripts/ri_build_host.sh draw >/dev/null && bash scripts/ri_build_host.sh test t93_raster_goldens
```

### 4.4 Deliberate re-pin (only the sections you changed)

```bash
S=$SCRATCH
for t in t92_draw_hash t93_raster_goldens; do
  bash scripts/ri_build_host.sh test $t 2>&1 | grep -oE "pin sec=[0-9]+ z=[0-9] got [0-9a-f]+" > $S/$t.pins
  python3 - "$t" "$S" <<'EOF'
import sys,re
t,S=sys.argv[1],sys.argv[2]; p='tests/unit/%s.c'%t; s=open(p).read(); n=0
for l in open('%s/%s.pins'%(S,t)):
    sec,z,h=re.match(r'pin sec=(\d+) z=(\d) got ([0-9a-f]+)',l).groups()
    s,k=re.subn(r'\{ %su, %su, 0x[0-9a-f]+u \}'%(sec,z),'{ %su, %su, 0x%su }'%(sec,z,h),s); n+=k
open(p,'w').write(s); print(t,'repinned',n)
EOF
done
```

**Read the `.pins` files first.** Every `sec=` listed must be a section you meant to change. Section numbers are `RI_SEC_*` in `gui/ctlreg.h`, for example 4–7 for the mixers and 8 for the master.

### 4.5 AROS side: classes and replay

- **Never** call `SetRPAttrs(RPTAG_FgColor)`: it is ignored and everything paints one colour.
- **Never** use Bitmap.mui for anything with transparency.
- Draw by building a list and calling `ri_rsection_replay(_rp(obj), &dl)`.
- Before any text, `SetFont(_rp(obj), _font(obj))`.
- **New visual element** = a new `RART_*` kind in `RArt`:
  - add a case in `MUIM_AskMinMax` (fixed sizes via `DefWidth = MaxWidth = MinWidth`) and in `MUIM_Draw`;
  - state attributes follow `MUIA_RArt_On`: in `OM_SET`, compare and `MUI_Redraw(obj, MADF_DRAWOBJECT)` only on a change;
  - tag IDs are `TAG_USER | 0x52xxxxxx`, unique; grep before choosing.
- **A background for a region** = wrap it in `bay_group(tags)` (RBay).
  - Children without `MUIA_Background` inherit the plate automatically.
  - `MUIC_Rectangle` spacers are transparent to it.
- **Hiding:** `SetAttrs(o, MUIA_ShowMe, FALSE)`. Hide whole slots (module + seams), never just the module.
- **Layout traps:**
  - Zune groups centre fixed-size children and have no alignment attribute. Top-align with `VGroup{child, Rectangle}`.
  - Keep a band at its natural height with `MUIA_Weight 1` against spacers (default weight 100).
  - `MUIA_Group_Spacing 0` plus the `MUIA_Inner*` 0 tags make things touch.
- **AROS lessons:**
  - `GetAttr` stores an **IPTR**;
  - pens are obtained per screen in `MUIM_Setup` and released in `MUIM_Cleanup`;
  - an event handler goes on `_win(obj)` in Setup;
  - a Software Failure only suspends the task: bisect crashes with one `rlog()` line per step into `RAM:RIAPP.LOG`. All RIAPP instances share this file, so make each build's log line distinct.
  - `rlog`/`RawDoFmt` args are packed 32-bit LONGs.

### 4.6 Build: the ABIv1 gate, then ABIv11 for the Dell (always from a clean worktree)

```bash
R=/home/miller/Work/projects/ReIncarnation; S=$SCRATCH
cd $R && rm -rf $S/wt && git worktree prune && git worktree add -q --detach $S/wt HEAD
for f in <YOUR FILES>; do cp $f $S/wt/$f; done          # only yours
ln -sfn /home/miller/Work/projects/Vulkan4Aros $S/Vulkan4Aros   # audit/build need the sibling
cd $S/wt && bash scripts/ri_build_aros.sh riapp            # ABIv1 gate: "AROS RIAPP BUILD OK"
```

The ABIv11 (Dell) build replaces the G9 prompt's `-O0` recipe; this is what ships:

```bash
cd $S/wt
V=/home/miller/Work/projects/Vulkan4Aros/src/abi/v11
CC=$V/toolchain-core-x86_64/x86_64-aros-gcc; SDK=$V/sdk/Developer; O=$S/dell; rm -rf $O; mkdir -p $O
CF="-std=gnu99 -O2 -Wall -Wno-pointer-sign -mcmodel=large -mno-red-zone -mno-ms-bitfields \
 -fno-strict-aliasing -ffixed-r12 -fno-builtin -fno-stack-protector -DPCF_TABLE_VERIFIED=1 \
 -Wa,-W -I$SDK/include -I$SDK/include/aros/stdc -I."
L=$(sed -n '/= riapp \]; then/,/^fi/p' scripts/ri_build_aros.sh | grep -o 'for f in .*; do' | sed 's/for f in //; s/; do//')
for f in $L; do $CC $CF -c $f -o $O/$(basename $f .c).o 2>&1 | grep -E "error|warning" || true; done
$CC -mcmodel=large -mno-red-zone -ffixed-r12 -nostartfiles -no-pie -o $O/RIAPP $O/*.o $SDK/lib/startup.o \
  -L$SDK/lib -lamiga -lmui -lintuition -lgraphics -lutility -ldos -lexec -lautoinit -lcybergraphics
$V/toolchain-core-x86_64/x86_64-aros-readelf -s $O/RIAPP | awk '$7=="UND" && $8!=""'   # must print nothing
```

- **Do not build in the shared tree.** Another session's WIP breaks it (for example the `riapp_core.c` stringop-overread `-Werror`).
- For a one-file iteration, recompile just that `.o` into `$O` and relink.

### 4.7 Deploy, launch, capture (Dell)

```bash
SP="python3 /home/miller/Work/projects/Vulkan4Aros/scripts/spike_server.py submit --spool /tmp/spike_spool_laptop"
$SP --wait 60 --exec "status" | grep -iE "riapp|RESULT"      # find YOUR previous process number
$SP --wait 220 --put $O/RIAPP:RAM:RIAPPX1 --exec "Break <yourN> C" --exec "wait 3" \
    --exec "Run >NIL: RAM:RIAPPX1" --exec "wait 12" --exec "status" --get RAM:RIAPP.LOG:$S/r.log
grep -v "hb:" $S/r.log | tail -3                               # expect your distinct line, open=1 rack=1
$SP --wait 60 --ui-capture $S/cap.png,2                        # SEPARATE submit, after the launch
magick $S/cap.png -crop WxH+X+Y -filter point -resize 400% $S/cap_z.png   # inspect details
```

- Use **absolute local paths** everywhere.
- Give each binary a **new RAM name**, so you never overwrite a running image.
- The first capture after `Run` may show an older instance: same-titled windows overlap at 0,0. Capture again after a few seconds.
- **Half scale is the minimum**; scale 1 is refused. Half-scale text is unreadable, so ask the owner about text.
- **Until S6 lands, agent clicks do not reach the app.** To capture another tab, make a throwaway build that opens on it: `sed` in `MUIA_Group_ActivePage, <n>` on the Register (after S1, on your page group). **Never commit that variant.**
- Rawkey injection works. Release a key with `CODE|0x80`, **never** `,up` (it sticks and auto-repeats).
- When you finish, leave your normal build running as `RAM:RIAPP<something>` for the owner, and **say its name** in your report.

### 4.8 Evidence

- Copy the captures into `docs/evidence/gui/hw-look/` (`dell-<step>-<what>.png`; host previews `host-<what>.png`).
- Append a dated README section: what the owner asked, what changed (functions, classes), the tests, and the captures.

### 4.9 Audit and commit

- The shared `/tmp/ri` build dir can hold **stale objects from another session's WIP**. The portable gate then fails with `undefined reference to levi_trigger` even though HEAD is clean. Run the audit with a private `/tmp/ri`:

```bash
mkdir -p $S/ri && cd $S/wt && unshare -r -m sh -c "mount --bind $S/ri /tmp/ri && bash scripts/ri_audit.sh" > $S/audit.log 2>&1; tail -3 $S/audit.log
# must end: AUDIT 0/0 PASS
```

- Then, in the real tree:
  - `cmp` each of your files against `$S/wt`;
  - `git add <your paths>`;
  - `git commit`;
  - `git worktree remove --force $S/wt`.
- **Push only when the owner asks** (they say "commit and push"). Before pushing, `git pull --rebase --autostash`.

### 4.10 Wikis (after each step, when asked to ingest)

- **ReIncarnation** `llm-wiki/`:
  - `raw/articles/YYYY-MM-DD-slug.md` (Source/Collected/Published header);
  - an index line under "Live app (§12.11)" or "GUI (§12.10)";
  - a log entry **appended at the bottom**: `## [date] ingest | title` / `- Disposition:` / `- Raw:`.
- **Vulkan4AROS** `llm-wiki/`: AROS/Zune-general lessons only, as a cross-post.
  - The log has the **newest entry at the top**.
  - The main checkout may be on another session's branch. Work in `git worktree add <scratch>/v4main main` (fast-forward `main` to `origin/main` first), commit there, push `main`, and remove the worktree.

---

## 5. Step order and dependencies

```
S0 prep ─► S1 grey ─► S2 lettering ─► S3 dirty-rect ─► S5 zoom
                                   └─► S4 master (needs Levi WIP landed, see S4)
S6 click proof (any time; unlocks self-verification of S1–S5)
S7 per-section skins (after S1; S2 helps)
```

Each step ends with:
- `AUDIT 0/0 PASS`;
- a commit of your own files only;
- the Dell running your build;
- one owner question;
- a short report (§8).

---

## 6. S0: preparation (small)

1. Run `git log --oneline -5`, `git status`, and list the foreign WIP files; they will differ from §3.1. Record them in your notes and never stage them.
2. Build the host (`bash scripts/ri_build_host.sh all`), run t92, t93, t61, t75 and t76 (all PASS), and do a baseline scratch preview (§4.2) of the Mix bay and the rail.
3. Build the ABIv11 RIAPP at HEAD (§4.6), deploy it as `RAM:RIAPPBASE`, capture the Synths tab (§4.7), and keep it as the "before" image.
4. **DoD:** baseline captures saved in `$SCRATCH`. No commit.

---

## 7. The work steps

### S1: No grey left (transport surround, window background, tab bar)

**Goal:** nothing on the RIAPP window is stock MUI grey. The whole window reads as one piece of equipment.
- The transport sits on the brushed plate.
- The tab bar is four hardware buttons: moulded, with a lit indicator on the active one.
- The Register's grey frame is gone.

**Design (E0 defaults; ledger them in the README):**
1. **Window root = RBay.** Replace the root `MUIC_Group` (`row` in `main()`) with `bay_group()` (vertical, spacing 4, inner 6/6/6/6).
   - The transport canvas goes in a centred slot with seams: reuse `rack_slot` or a horizontal variant with spacers, so it looks racked. Add rack rails at the left and right of the transport row only if it reads better; judge in the host preview.
   - The device rail stays an RBay (nested RBays are fine: each paints its own box).
2. **Replace `MUIC_Register` with a custom tab strip plus a page group:**
   - Pages: `MUI_NewObject(MUIC_Group, MUIA_Group_PageMode, TRUE, Child, synth_page, … )`. This is `s_pages`; it replaces `s_reg` everywhere, including `rail_for_tab`, which reads `MUIA_Group_ActivePage` from it.
   - Tabs: an RBay HGroup of four RArt objects of a new kind **`RART_TAB`**, labels from `ri_tab_title(g)`, `MUIA_InputMode RelVerify`, `MUIA_ShowSelState FALSE`, `MUIA_FillArea TRUE`.
     - New attribute `MUIA_RArt_Active` (BOOL): the active tab is drawn lit or sunk.
     - Notify each `MUIA_Pressed FALSE → ReturnID RIAPP_ID_TAB0+g`. In the loop, `SetAttrs(s_pages, MUIA_Group_ActivePage, g)`, set `MUIA_RArt_Active` on all four, then `rail_for_tab()`.
     - Remove the old Register notify.
   - New art `ri_art_tab(dl, x0, y0, x1, y1, label, active, pressed)` in `art_shared.c`: a moulded rectangular key, like `ri_art_bevel` but dark (face ≈ `0x3A3C40`), with a 3–4 px LED strip above the label. The strip is `C_MIX_GREEN` lit when active and dim when not. Active keys are 1 px lower and slightly darker (latched), and the label is `C_TEXT_INV` or a light palette colour.
     - **Text colour must be a palette `C_*`**, because text replays through pens.
     - Size: height `RI_ART_TAB_H` 24; width = text + 2×14.
   - **Keyboard:** the Register gave nothing we rely on. Only add tab keys if `t70_keymap` shows free keys: check `gui/keymap.c` for F1–F4 or Ctrl+1..4.
     - If free, route them in the main loop, not in canvases: the canvases own the rawkeys through the panel key owner (`MUIA_RSection_KeyOwner`), so add the keys in `gui/keymap.c` with a t70 RED first.
     - If not free: mouse only, and ledger it.
3. **Transport canvas:** check `ri_art_bg_tr` still reads right on the dark plate. The transport panel is `C_TR_PANEL`; if it looks pasted-on, frame it in seams and give it screws (like `ri_art_bg_303`). Section art changes → deliberate re-pin of `RI_SEC_TRANSPORT` only.
4. **Window:** Zune draws the window background from the root object; the root RBay covers it. Title bar and borders are system and stay.

**Tests (RED first):**
- **t93 `rack_checks()` additions** for `ri_art_tab`:
  - active shows ≥ N green LED pixels and inactive shows none;
  - the label pixels contrast with the face (luma difference ≥ 60);
  - the active hash differs from inactive;
  - the pressed hash differs from unpressed.
  - Mutate the LED colour choice and the pressed offset.
- If you move tab or page logic into a pure helper (recommended: `gui/tabpages.c` `ri_tab_rail_mask(page)` returning the device bitmask shown on the rail), host-test it in the existing tabpages test (find it with `grep -l ri_tab_devices tests/unit/*.c`). Mix/FX give all four, Synths gives 303A/303B, Drums gives 808/909. Then call it from `rail_for_tab`.

**Proof:**
- Dell captures (§4.7) of all four pages, via page-open variants until S6.
- Crop-zoom the tab bar.
- Log line: add `tabs=%ld` (custom tab strip created).

**Owner question:** "Tabs, transport and window background: does anything still look like stock software grey, and if so where?"

**DoD:** audit green, commit, owner asked, `RAM:RIAPPS1` left running.

---

### S2: Real lettering (clean-room condensed sans bitmap face)

**Goal:** panel legends look like printed hardware legends: a crisp condensed grotesque at fixed sizes, **identical on the host and AROS** (same metrics, same pixels).

**E0 decision (ledger it):**
- An in-house, clean-room **bitmap face** stored as C tables. Do **not** trace Helvetica or any font.
- Design from primitives: 1-px strokes, a flat-sided "O", a straight-legged "R", a "G" with a spur, and a narrow "M".
- Sizes by zoom: **S** (cap height 5, for z0 and compact), **M** (cap 7, z1), **L** (cap 9, z2/z3).
- Full printable ASCII 32–126, proportional advance, 1-px tracking.
- A licensed font is an owner decision (§9); do not add one without a yes.

**Design:**
1. **Glyph data:** new portable file `gui/draw/font_legend.{h,c}` (add it to all build lists, §4.1).

   ```c
   struct ri_glyph { uint8_t w; uint8_t rows[12]; };    /* 1bpp, bit7 = left, up to 8 px wide */
   struct ri_face  { uint8_t cap, asc, desc, adv_gap; const struct ri_glyph *g; /* 95 entries */ };
   const struct ri_face *ri_face_for_zoom(int z);
   int ri_face_width(const struct ri_face *f, const char *s);   /* px */
   ```

   Widths up to 8 px cover S and M. For L, use `uint16_t rows` or two bytes.

2. **Display list:** keep `RI_D_TEXT`, and **carry the face in `pad[0]`**: 0 = backend/system font (the old behaviour), 1/2/3 = S/M/L.
   - Add `ri_draw_text_face(dl, x, y, align, rgb, face_id, text)` in `canvas.c`.
   - The existing `ri_draw_text` keeps face 0, so nothing moves until you switch callers.
   - `ri_art_text_c` gains the face from the zoom. Thread `z` into it: the callers have `z`, or add a `ri_art_set_face(dl, id)` state field in `struct ri_dlist` if threading `z` is too invasive. Choose one, and say which.
3. **Host rasterizer** (`platform/host/raster.c`): for face ≠ 0, draw the glyph bits (centred by `ri_face_width`, baseline at y + cap/2). `ri_raster_text_width` uses the face when set.
4. **AROS replay** (`rsection_replay.inc`), for face ≠ 0:
   - render each glyph with **`BltTemplate`**: one template plane per face, built once at class init into chip memory with `AllocRaster`.
   - Colour comes from `rpen(rgb)` with `SetDrMd(JAM1)`. This is exact on truecolor, because the pens are `PRECISION_EXACT`.
   - Allocate the template planes with `AllocRaster`, **not** `AllocBitMap` with a NULL friend: that returns NULL on this AROS (the 2026-09-27 saga, step b).
   - A per-glyph `BltTemplate` from a packed plane: `srcX` = glyph x offset, `srcMod` = bytes per row.
   - If a template fails, fall back to per-pixel runs via `rfill_rgb` for that string (slow but correct), and log once.
5. **Switch all section legends** to the face. Legends then scale with zoom. This retires the old G9 lesson "fonts do not scale with zoom" for legends only; MUI widgets keep the system font.

**Tests (RED first):**
- **New pure test `tests/unit/t106_legend_face.c`.** Check the free number first: `ls tests/unit | sort -V | tail`; others use t103–t105. Add it to the audit's GUI test loop only via a separate, clean hunk: `ri_audit.sh` may carry foreign WIP, so stage your hunk with `git add -p` or `git apply --cached`.
  - Every printable glyph except space has ≥ 1 set pixel.
  - No glyph has bits outside its `w` or above `asc`, or descends below `desc`.
  - Advances are monotonic.
  - `ri_face_width("TB-303")` equals the sum of advances.
  - **No two uppercase glyphs are bit-identical.**
  - Mutate one glyph row, and one width.
- **t93:** add a text-parity property. A 1-line render of `"CUTOFF 303"` with face M on the host must have its left edge at the expected x and width = `ri_face_width`.
- **t92/t93 re-pin** for every section; all legends move, so this is expected. Say so in the commit body.

**Proof:**
- Host PNG sheet of all three faces (`host-legend-faces.png`).
- Dell captures of the 303 and Mix pages.
- The owner reads the legends on the Dell.

**Owner question:** "Legends: sharper and more 'printed' than before, yes or no? Any letter that looks wrong?"

**DoD:** audit green; owner verdict requested; `RAM:RIAPPS2` running.

---

### S3: Dirty-rectangle redraw (knob drags, meters, chase lights)

**Goal:** a knob drag repaints only the knob's box. Today every change rebuilds and replays the whole section (~8k commands on the 909) and blits the whole frame. Also, `meter_round()` → `ri_panel_live()` refreshes **every canvas** on each playhead step.

**Design:**
1. **Damage per control (pure, `gui/panelgeo.c`):** `int ri_geo_bbox(const struct RIGeoSection *g, uint16_t reg_id, int zoom, int *x0, int *y0, int *x1, int *y1)`.
   - It is the union of all geometry items carrying that id (knob + legend + LED, and so on), grown by the knob shadow/glow margin (+3 px).
   - Returns 0 on success.
   - Host test in t61: every item's box is inside its section; every `MX(sec, idx)` id has a bbox; mutate the margin.
2. **Canvas state (AROS):** `RSectionData` gets `int dmg_x0, dmg_y0, dmg_x1, dmg_y1, dmg_valid` (canvas-local px).
   - `changed()` in `rsection.mcc.c` unions the bbox of `d->diag.last_hit` (or of the id the key or drag changed; `ri_cev_*` knows it) and calls `MUI_Redraw(obj, MADF_DRAWUPDATE)`.
   - An outside refresh (`ri_rsection_refresh`) keeps the full redraw unless it came with a damage call. Add `ri_rsection_refresh_box(obj, x0, y0, x1, y1)` for meters and chase lamps.
3. **Partial replay (portable + AROS):**
   - In `MUIM_Draw` with `MADF_DRAWUPDATE`, a damage rect and a valid off-screen bitmap `d->bm`:
     - build the full list as now (CPU only, cheap);
     - replay **only the commands whose bbox intersects the damage** into the off-screen bitmap. Painter's order stays correct: everything that covers the box is re-drawn in order, and anything outside it is already right in the bitmap;
     - then `BltBitMapRastPort` **only the damage rect**.
   - Otherwise, full path.
   - Put the intersect test in portable code: `int ri_dcmd_bbox(const struct ri_dcmd *c, int *x0, …)` in `gui/draw/canvas.c`. TEXT needs a width: use the face metrics from S2, or a generous estimate of `strlen × 8`.
4. **Meters and chase:**
   - `ri_panel_live` returns "changed". Make the chase lamp box and the meter box known through the geometry ids of those lamp/meter items, and call `ri_rsection_refresh_box`.
   - Keep a full-refresh fallback when the box is unknown.

**Tests (RED first):**
- **Host parity property (the key test)**, new `t107_partial_redraw.c` or inside t93. For every section (z0) and **every value control id**:
  1. render the full frame F0;
  2. change the control's value (`ri_sui_set_value` to min, then max);
  3. render full F1;
  4. take F0, replay only the commands intersecting `ri_geo_bbox(id)` into it, giving P1;
  5. assert `P1 == F1` over the **whole frame**.

  This proves the bbox covers everything the control touches. Mutate: shrink the bbox margin to 0 → the knob shadow or glow must fail.
- Time on the Dell: add EClock timing around `draw_frame` (full vs partial) to the diag, log max/mean every 30 s while dragging, and compare with the owner doing a 909 knob drag for 10 s. Expect well under 1/4 of the full time.

**Proof:** the log numbers (before/after), and the owner's feel verdict.

**Owner question:** "Drag the 909 TUNE knob fast for 5 s: smooth now, yes or no?"

**DoD:** parity test green over every control; audit green; `RAM:RIAPPS3` running.

---

### S4: Master fader on the Mix tab

**Goal:** a MASTER strip at the right end of the Mix row: level fader, L/R meters and Comp switch (the existing `RI_SEC_MASTER` geometry, 464 Q tall like the strips). **It must really change the output level.**

**Facts:**
- `gui/ctlreg`: Master **Level is not automatable** (t60 asserts it: E1, the manual). The Master Comp has an automation key (`0x0B55`).
- The bridge **filters non-automatable indices**, so today no master level reaches the engine.
- `engine/mixer/mixer.h` already has a master bus. The live meters snapshot (`struct RILiveMeters`) carries `sec_peak[]`. Check whether it carries a master L/R peak; if not, it must be added.

**Rule about the Levi WIP:** `engine/mixer/*`, `engine/engine.*`, `engine/live.*`, `engine/fx/route.h` and `app/core/riapp_core.*` are **another session's uncommitted work**.
- **Do not start the engine half of S4 until those files are clean in `git status`** (the Levi work is committed or reverted by its owner).
- Until then, do only the GUI half behind a dead-end guard (see step 3 below), or skip S4 and say so.

**Design:**
1. **Live-only control path.** Read `engine/seq/ctlplane.{h,c}`, `gui/panelctl.c` and `gui/ctlreg.c` `ri_ctlreg_auto_id` to see how a non-automatable panel control could reach the engine.
   - E0: a control-plane message class **"live-only"** (for example lane key 0 / a flag bit). The render task applies it to the engine exactly like an automated one, but the recorder (`autolane`) never records it.
   - Master level is the first user.
   - **One renderer (spec §5):** an offline export has no live-only history. Ledger that master level is a monitoring control, not song data. Ask the owner if unsure; it is E1-silent.
2. **Engine:** `ri_engine` applies master gain on the post-master sum; the meter tap is post-master. Add the master L/R peaks to the meters snapshot if missing.
3. **GUI:**
   - canvas `C_MST` (`RI_SEC_MASTER`), `ri_sui_bind_board` to the shared board, `s_panel.mix[4]`;
   - add it to `val_canvas` in `sync_values`;
   - `rack_page` for Mix gets a fifth module after a **double seam gap** (a spacer of 6 px) to separate it from the device strips. It never hides;
   - feed the meters from the snapshot's master peaks.
   - Until the engine half lands: the master fader must not look live but do nothing. Either leave it out entirely, or show it with a "(monitor)" legend and **no** send. Prefer leaving it out.

**Tests (RED first):**
- **Host:** master level through the live-only path changes the rendered output amplitude by the expected dB (`t8x` live tests are the model).
- **Host:** the recorder ignores live-only (a record session with master moves writes no lane events).
- t60 unchanged.
- Parity: offline export vs live-style render with **no** master moves stays bit-exact.

**Proof:**
- The owner moves MASTER on the Dell and hears it.
- Record a WAV with W at two master positions and compare RMS in the report.

**Owner question:** "MASTER fader: does the whole mix get quieter/louder, yes or no?"

---

### S5: Window size: auto-fit zoom plus a zoom choice

**Goal:** the panel uses the screen. Today it is fixed at 918x650 on 1366x768 (and on 1280x1024 at riqemu1).

**Facts:** canvases take a zoom index at creation (`ri_rsection_create(sec, zoom)`): 0 = 1x, 1 = 1.5x, 2 = 2x, 3 = compact (transport). Sizes come from `ri_geo_px(q, zoom)`. `t76_zoom` covers the math.

**Design:**
1. **Pure fit (host):** `int ri_zoom_fit(int scr_w, int scr_h, int chrome_w, int chrome_h)` in `gui/` (or extend the zoom module).
   - It returns the largest zoom whose **widest page** (Synths: pattern + 303 row, ×2 rows; Drums; Mix with 4 strips (+ master) + rails; FX) plus the transport, rail and tabs fits.
   - Compute page sizes from the geometry, not from constants.
   - Tests: 1366x768 → the expected index (compute it and write down why); 1280x1024 → …; 800x600 → 0. Mutate the chrome allowance.
2. **At startup:** read the screen size (`_screen(win)` is not available before open; use `LockPubScreen(NULL)` → `Width/Height`, then `UnlockPubScreen`), call `ri_zoom_fit`, and create the canvases at that zoom (the transport stays compact or follows; decide by the preview).
3. **Zoom choice at runtime:** a menu strip (`MUIA_Window_Menustrip`, `MUIC_Menustrip` with "View → Zoom 1x/1.5x/2x/Fit"), because keys are scarce (check t70).
   - Changing zoom calls `DoMethod(parent, MUIM_Group_InitChange)`, sets a new `MUIA_RSection_Zoom` attribute on each canvas (add it: `OM_SET` stores the zoom, frees the off-screen bitmap), then `MUIM_Group_ExitChange`, so the window relayouts and resizes.
   - Rails and seams are unbounded or fixed and follow automatically.
4. **Persist** the choice in `ENVARC:RIAPP/zoom` (E0; ledger it; the owner may veto).
5. **Legends:** S2's face follows the zoom automatically.

**Proof:** Dell at Fit (a capture of every page); riqemu1 at 1280x1024 (keys work there; mouse does not); the owner tries the menu.

**Owner question:** "Does Fit fill the screen well, and do you want Fit as the default?"

---

### S6: Close the testing gap (lane clicks must reach the app)

**Symptom:**
- `--ui-click l,X,Y` and `--ui-move` report "injected", but RIAPP does not react: Register tabs (2026-09-28) and rail chips (2026-09-27) never responded.
- `--ui-rawkey` works.

**Where the agent code is:** `/home/miller/Work/projects/Vulkan4Aros/src/vulkan/loader/atcp_ui.c`.
- `atcp_ui_click_at()` sends **one `IND_ADDEVENT` chain**: `IECLASS_NEWPOINTERPOS` (`IESUBCLASS_PIXEL`, `iepp_Screen = IntuitionBase->ActiveScreen`), then `IECLASS_RAWMOUSE` LBUTTON down, then LBUTTON up.
- All three events have **zero `ie_TimeStamp`** and arrive at the same instant.
- The server side is `scripts/spike_server.py` (`--ui-click`, `--ui-move`).

**Safety:**
- This is the **Vulkan4AROS lane tooling**, used by other sessions.
- Rebuilding or redeploying the Dell **agent** can drop it; if it drops, **only the owner can reboot the Dell**.
- So: prove the cause first with **server-side-only** changes (Python: send separate `ui_input` messages with delays). Touch the agent binary only with the owner's explicit OK in chat.

**Diagnosis plan (each a short, logged experiment):**
1. **Probe app** (scratch, never committed to this repo): a tiny AROS program built with the §4.6 recipe. It opens a 400x300 window with `IDCMP_MOUSEBUTTONS|IDCMP_MOUSEMOVE|IDCMP_ACTIVEWINDOW` and logs `class, code, MouseX, MouseY, Seconds, Micros` to `RAM:CLICK.LOG`.
   - Deploy it, inject `--ui-click l,<centre>` and read the log.
   - Nothing arrives → the agent or the input path. Events arrive → MUI or RIAPP.
2. If nothing arrives, try in order and record each result:
   - (a) two separate submits: `--ui-move X,Y` then `--ui-click l` (no coords), 1 s apart;
   - (b) click twice (the first may only activate the window);
   - (c) a server-side split: press and release as separate `ui_input` JSON events with a 100 ms gap (Python only);
   - (d) a non-zero `ie_TimeStamp` (agent change: owner OK required);
   - (e) the `iepp_Screen` pointer: the Dell may have the Wanderer screen active while RIAPP opens on the same public screen; verify with a capture of the pointer position (move, capture, look for the pointer).
3. If events arrive in the probe but not in RIAPP:
   - check that MUI gets them: add an `RIAPP/evtrace` ENV switch that logs `MUIA_Pressed` notifications and the rsection `diag.buttons` counter;
   - check that the window is active (`MUIA_Window_Activate`).
4. **Fix** where the cause is.
   - Server-side (Python) fixes: commit in Vulkan4Aros via a worktree of `origin/main` (§4.10 method) with a wiki record.
   - Agent fixes: only with the owner's OK. Build, deploy and roll back exactly as the Vulkan4AROS agent docs say (read `llm-wiki` there: "aros-agent-handoff", "aros-verification-tooling").
5. **Acceptance script** (`docs/evidence/gui/lane-click-proof.md` plus the commands). With **no owner input**:
   - click each tab → the capture shows that page;
   - click the 303B power button → the log shows `VIS dev=1 show=0`; click again → `show=1`;
   - drag a knob (move + press, moves, release) → the log shows `CTL` lines.

**Owner question** (only if the agent must change): "May I rebuild and redeploy the Dell spike agent? If it drops, you'd need to reboot the Dell."

---

### S7: Per-section skins

**Goal:** the owner can give each module its own skin (for example the 808 in `808-RI`, the rest Classic). The design is in `docs/superpowers/specs/2026-09-26-skins-design.md` → "Per-module skin choice"; the owner requirement is recorded in that section.

**Design (as specified; restated so you don't miss parts):**
1. The **active skin becomes a per-section table**: `RI_SEC_COUNT` entries, NULL = Classic.
   - The canvas looks up its **own geometry section** (SYNTH2 follows SYNTH1 unless the user splits them).
   - `ri_skin_aros_active()` becomes `ri_skin_aros_for(section)`. `draw_section` passes the section's skin.
2. **Shared loads:** each mod directory is loaded once and refcounted across the sections that use it, and released when the last section switches away (idle-only, as today).
3. **UI:**
   - **Ctrl+M** cycles the focused section's skin among the installed mods (`SYS:Classes/ReIncarnation/Mods/*`: 808-RI, Template, Stale; `Stale` is a deliberate wrong-size fixture and must be refused cleanly);
   - a "whole panel" choice sets every section;
   - check t70 for the key first.
4. **Song chunk:** the per-section assignment needs a **new optional chunk** (section token → mod name), written only when the choice is not uniform. Readers that skip it get the first MODR mod everywhere (fail-closed).
   - **This is a file-format change: ask the owner before writing it** (irreversible choices are theirs).
   - Build the table, refcount and UI first; the chunk is a separate commit after a yes.
5. **Rack furniture** (bay, rails, power, tabs) stays unskinned in this step; note it as a follow-up.

**Tests (RED first, host):**
- the assignment table: set, get, fallback to Classic, SYNTH2 follows SYNTH1;
- refcount lifecycle: load once, share, release last, never release while drawn;
- the Stale fixture is refused with a log line and Classic stays;
- (after the owner's yes) chunk round-trip, and a reader that skips it gets MODR[0] everywhere.

Mutate the refcount decrement and the SYNTH2 follow.

**Proof:** Dell with the 808 in 808-RI and everything else Classic (a capture of the Drums page), and switching back.

**Owner questions:** "Is Ctrl+M on the focused section the right gesture?" and "May I add the optional per-section skin chunk to the song format?"

---

## 8. Reporting and handoff

After each step, give a report of at most 10 lines:
- commit id(s);
- the audit result;
- the RAM name of the build left running on the Dell;
- captures (paths);
- the one owner question;
- anything ledgered as E0.

If you stop mid-way, write one line in your final message:

`HANDOFF: at S<n>.<substep>; done: …; next: …; files touched (uncommitted): …; lanes: <Dell build name running>`

---

## 9. Owner decisions still open (do not decide; list them in your final report)

1. **Font:** keep the in-house clean-room face (E0), or license a specific face (which, and under what licence).
2. **Tab keys:** whether to add keyboard tab switching, and which keys, if t70 has free ones.
3. **Zoom:** Fit as the default; persisting it in `ENVARC:`.
4. **Master level:** confirm it is a monitoring (live-only) control and not song data.
5. **Per-section skins:** Ctrl+M as the gesture; the optional song chunk (format change).
6. **Lane agent:** permission to rebuild/redeploy the Dell spike agent if S6 needs it.
7. **Rack furniture skins:** whether bays, rails, power buttons and tabs should become skinnable later.
