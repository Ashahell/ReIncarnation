# Leviasynth fidelity P9e: transport tap tempo — a clocked press into a pure millisecond estimator

- Source: ReIncarnation session, 2026-10-02 (opencode lane, the P9 Leviasynth slice, resumed after the Dell xrun interlude)
- Collected: 2026-10-02
- Published: 2026-10-02
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P9e plan, §P9e code-time decisions, §P9e Status)
- Prior: [2026-10-01-leviasynth-fidelity-p9d-glidechord.md](2026-10-01-leviasynth-fidelity-p9d-glidechord.md) (glide hold + chord mode), [2026-10-01-dell-xruns-governor-arm-and-repaint-policy.md](2026-10-01-dell-xruns-governor-arm-and-repaint-policy.md) (the interlude that paused this slice)
- Commits: `6b354f5` (the slice) and `ad67df6` (an unrelated ASan find, committed separately), both unpushed at collection

## What was built, and where it lives

Tap tempo is the one P9 slice that is not a device slice: it is the transport panel's tempo control, so nothing in the engine moves and no song byte changes.

```
gui/secttr.h/.c     RI_STR_TAP 14u, RI_STR_NCTL 14->15, RI_STR_TAP_IV 4u,
                    RI_STR_TAP_GAP_MS 2000u; struct gains tap_iv[4], tap_last,
                    tap_n, tap_have (all cleared in ri_str_init); ri_str_tap(s, ms)
gui/sectui.h/.c     ri_sui_tap(s, idx, ms) — transport only, that index only
gui/ctlreg.c        R(TRANSPORT, 14, BUTTON, "", "Tap", 0, 1, 0, RI_MIDI_CC_NONE,
                    0, NONE, 0, 0)
gui/panelgeo.c      { TR(14), RI_GEO_RECT, 0, 490, 58, 120, 76 }  — above Play
gui/draw/art_shared.c   ri_art_tr_key: an RI_STR_TAP case drawing
                    ri_art_text_c(dl, cx, cy, "TAP", C_BLACK)
app/core/canvas_events.h/.c  ri_cev_button() gained a trailing uint32_t ms
gui/widgets/rsection.mcc.c   eclock_ms(): current EClock in ms, 0 when !s_efreq
tests/unit/t150_tap_tempo.c  38 assertions, the slice's RED-first test
```

The tempo reaches the engine for free: `app/riapp.c` already reads `RI_STR_TEMPO` and pushes it with `ri_live_set_bpm`, so no engine, no key and no song path was touched.

## The estimator, and why it is pure

`ri_str_tap(struct RISectTr *s, uint32_t ms)` takes the press time as an argument and owns only the tap window (`tap_iv[RI_STR_TAP_IV]`, `tap_n`, `tap_last`, `tap_have`). It writes **through** `ri_str_set_value(RI_STR_TEMPO)`, so the display's own 20..500 clamp is the only clamp in the path: a wild estimate lands on 20 or 500, never on a broken readout.

```
bpm = (60000 * n + sum/2) / sum        n = kept intervals, sum = their sum
```

The half-sum rounds to nearest. The average is over the kept intervals, not the last one — four intervals shrug off one clumsy tap without the display lagging a deliberate change.

The boundaries are law, not tolerances, and they follow from two numbers:

- **The first tap only records a reference.** A tempo needs two presses.
- **A pause over `RI_STR_TAP_GAP_MS` (2000 ms) starts a new measurement** rather than dragging a stale interval into the average.
- **A clock that does not advance or goes backwards restarts the same way.** Two taps in the same millisecond would otherwise measure a 0 ms interval; a backwards stamp would otherwise underflow `ms - tap_last` into a wrapped 2^32 and ask for a 0 bpm tempo. So the interval is never 0 and never wrapped.

Those two numbers also fix the reachable range, which is worth stating because it makes the clamp law one-sided in practice: four 2000 ms intervals give 30 bpm at the slow end, and a 1 ms interval asks for 60000 and clamps to 500 at the fast end.

## The clock is injected at the press, never stored

`ms == 0` means "no clock" and is **ignored, not guessed**. The AROS canvas passes `eclock_ms()` on select-down only, and `eclock_ms()` returns 0 when `timer.device`'s EClock is unavailable — a host state, not a tap. No panel field stores a clock, so a keyboard or menu press can never tap: `ri_str_press`, which has no clock, returns 0 for the key, and `ri_sui_tap` is the only route in.

The alternative (a clock field on the panel, set by the widget) would have made every non-pointer press a tempo edit.

MIDI is deliberately **not** wired: the panel's MIDI path is not in `app/riapp.c` yet, so a cc would be a second, untested clock. The registry row therefore has bind `NONE` and no cc, which also leaves `0x0ECC` as the first refused key (the P9d boundary).

TAP is momentary with no lamp — the tempo display is the thing that changes. `ri_str_value`/`ri_str_led` read 0 and `ri_str_step`/`ri_str_set_value`/`ri_str_reset` refuse it.

## The test: arithmetic read off the implementation

`t150_tap_tempo.c` (38 assertions) is written from the arithmetic rather than from a wish, so every expected number is checkable on paper:

| taps (ms) | window kept | expected | why |
|---|---|---|---|
| 100 | — | no change | first tap is a reference only |
| 1100 | `[1000]` | 60 | 60000*1/1000 = 60.5 → 60 |
| 1300 | `[1000,200]` | 100 | 120000/1200 = 100.5 → 100 |
| 1500 | `[1000,200,200]` | 129 | 180700/1400 = 129.07 → 129 (this is the half-sum's law) |
| 1700 | `[1000,200,200,200]` | 150 | 240800/1600 = 150.5 → 150 |
| 1900 | `[200,200,200,200]` | 300 | the fifth interval drops the oldest: 240400/800 = 300.5 → 300 |

Both sides of the 2 s boundary are pinned: a gap of exactly 2000 ms is **kept** (`[1000,2000]` → 40 bpm), a gap of 2002 ms **restarts** (tempo unchanged, and the next tap measures only from the new reference → 120 bpm). `ms == 0`, a repeated stamp and a backwards stamp are each pinned. Both clamp ends and the latching (a second 1 ms tap returns 0) are pinned. Three taps in song mode leave the transport state, the click count and the cursor untouched. A manually set 137 bpm is **not** an input: the next tap re-derives from the taps (82 bpm). `ri_sui_tap` moves the tempo for the key and for nothing else.

Two test traps, each of which cost a probe:

- The "the display's old value is not an input" law has to be **computed**: after taps at 100/1100/1300 the window holds `[1000,200]`, and a fourth tap 1000 ms later fills it to `[1000,200,1000]` — sum 2200, n 3 — which is 82 bpm, not the 60 the first draft asserted.
- `RICtlDef` has no `index` field: the index is the low byte of `reg_id`, so the "the transport has exactly NCTL rows and nothing past it" law walks `ri_ctlreg_at()`.

## Proof: 30 mutants in three sets, every kill behavioural

```
mut_fixE1.txt  22 against t150   estimator, routing, registry rows
mut_fixE2.txt   4 against t91    the press, the clock, the hit box
mut_fixE3.txt   4 against t92    the label's art
```

E1 covers the no-clock guard, the reference tap, the non-advancing clock, the gap boundary in both directions (`>` → `>=`), a long pause that keeps the stale window, the window slide (dropping the wrong end), the interval counter, both averages (collapse to the last, use only the oldest), the rounding half-sum, dropping the interval count, `60000` → `600000`, the destination index (Shuffle instead of Tempo), always claiming a repaint, a fixed 1 ms interval, an init that pretends a tap is on record, the routing guard, and four registry rows (kind, legend, a cc claimed, the wrong index).

E2 covers tap-AND-press instead of OR, a press that throws its clock away (`ms - ms`), a key with no hit box, and a key laid out as a legend — the hit test (`ri_geo_hit_opt`) handles `KNOB`/`RECT`/`OPTION`/`STEPPER` and skips everything else, so a legend-shaped box is genuinely unhittable.

E3 covers the label case never being taken, its colour matching the button face, its centring, and its removal.

Three things the mutation run taught, which are the reusable part:

- **Two first-pass kills were build breaks, not behaviour.** Dropping the tap call left `ms` unused, which is fatal under `-Werror`. Both were replaced by mutants of the same shape that keep the parameter in use (tap AND press; `ms - ms`).
- **One geometry mutant survived, and the reason is a property of the hit test, not of the test.** "File Tap on top of Play" is invisible from the outside because `ri_geo_hit_opt` returns the **first** matching item, and Tap was then earlier in the array than Play. Replaced by the legend-shaped box. The generalisation: *the item order is the overlap law*, which is why the TAP row is now filed after Record, next to the buttons it belongs with.
- **One guard carries no mutant on purpose.** `ri_sui_tap`'s `!istr(s)` clause cannot be mutated meaningfully: with it removed, the write lands in the section union at a layout-dependent offset, so no host test can be relied on to see it. A mutant there would be theatre, so it is declared out of scope in the mutant file's header instead.

## Goldens

`t92_draw_hash` pins the display-list hash of every section × zoom, so the new item moves the transport's four pins — twice, in fact: adding the row changed them, and **moving the row changed them again**, because the hash covers the command order, not just the pixels.

```
transport z=0..3: 0x7ca92536/0x8ba1ae5a/0xc13b6e7b/0x2ce1a837  (row added)
                   0xefcd5bc0/0xf1b192a8/0x68332c8b/0x34b55613  (row filed after Record)
```

`t93_raster_goldens` pins **pixels**, and its transport hashes did not move at all (`0xbfae0587/0x01a16d77/0x326d005a/0x8f296ed2`) — the second re-pin was unnecessary, which is itself the proof that the two tests pin different things. `scripts/ri_audit.sh` → `AUDIT 0/0 PASS`.

## Two defects ASan found while this slice was being proved

Neither is tap tempo; both were fixed in the same session and the production one is committed separately.

- **`gui/skin.c` `ri_skin_modref_set` read past the caller's string.** It range-checks the name (`nl > RI_RBNG_MAX_MOD_NAME` refuses it) and then copied `RI_RBNG_MAX_MOD_NAME + 1` bytes out of `name` regardless of its length, so storing a 6-character mod directory name read one byte past its NUL. ASan reported it in t75 as a `global-buffer-overflow` on `"808-RI"`. **No behavioural law for it is possible**: the old and new code leave the same bytes in the song, so ASan is the only detector and a mutation set here would be theatre. Commit `ad67df6`.
- **`tests/unit/t93_raster_goldens.c`'s `read_file` handed `ri_skin_parse` an unterminated buffer** (`malloc(sz)` with no `(*out)[sz] = 0`), so the parser's printable-ASCII charset scan ran one byte past the file — a real over-read that had stayed invisible because t93 had never been in an ASan set. The AROS loader does this correctly (`gui/skin_aros.c`: `text[ntext] = '\0'`), so the test was the odd one out. It also leaked every decoded master and zoom copy (34 MB in 58 allocations); `gui/skin.h` is explicit that those buffers are caller-owned, so the test now frees the masters it never renders and the zoom copies after the render — leaving t93 ASan-clean **with unchanged pixel goldens**, which is the confirmation that the render path only ever reads the zoom copy.
- **t75 could not be ASan-built at all** (`sprintf` into a fixed buffer under `-Werror`), so its 151 laws had no memory-safety coverage whatsoever. `snprintf` with room for the widest case, and the skin over-read above became visible.

## Honest gaps

- **Not deployed.** The panel change needs owner eyes (the button's pixels and the label), and the Dell is running the glitch-fix proof build, so a second binary would only confuse that run. No on-target proof exists for P9e.
- The MIDI tap is deferred by decision, not by oversight — see the MIDI note above.
- The keyboard/menu path deliberately cannot tap, so a tap tempo is pointer-only until some non-pointer route carries a clock.
- Standing gaps unchanged from P9a–P9d: panel pixel proofs, encoder-press navigation.