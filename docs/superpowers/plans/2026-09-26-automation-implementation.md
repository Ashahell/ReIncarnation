# Automation lanes implementation plan (slice §12.9c) — from spec r2, reviewed

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans
> (inline, owner chose native). Steps use checkbox syntax.

**Goal:** implement the owner-approved r2 automation slice (spec
`docs/superpowers/specs/2026-09-26-automation-design.md`, APPROVED
2026-09-26, decisions 5.1/5.3/5.4) as pure, tested C99: lane model,
punch recording, edits, chase + emission, a render-safe publish, the ATRK
codec, and delivery to every engine block that can already take the value.

**Architecture:** `engine/seq/autolane.h` (model + edits; depends on
`transport.h` only), `engine/seq/autolane_emit.h` (emitter; adds
`sched.h`/`clock.h`), one TU `engine/seq/autolane.c`. The lane is
caller-owned (`struct RIAutoLane { n, cap, flags, *ev }`, sorted by
(tick, ctl), sized once at song load, never in the render path). Codec
transfer uses explicit triples: engine never includes project headers,
project never includes scheduler headers. Test `t77_autolane` (t74 player,
t75 skin, t76 zoom are taken).

**Tech stack:** C99 host gcc (`-std=c99 -Wall -Wextra -Werror -pedantic
-ftrapv`); `x86_64-aros-gcc` compile-only through the audit loop;
`ri_assert.h`; `rbng_test_*` helpers.

**Already landed (do not re-implement):**
- `RI_EV_AUTOMATION` sort key + engine dispatch for 303A/303B only
  (`engine/engine.c` case `RI_EV_AUTOMATION`; t37 pins 303B `0x0312`).
- RBNG `AUTO` chunk (legacy ≤256 form); `ri_bar_quantize_next` (t58);
  the m68 refuse-first record gate.
- **Task 1 — `2834f7d` "automation lane model + punch recording":**
  - `ri_auto_allowed` — the allow-list, currently the 16 TB-303 IDs; see R1 below;
  - `ri_auto_value` — found/not-found, no sentinel;
  - `ri_auto_touch` — RECORD gate, ppq%8 law, forward quantize, sorted insert/replace, sticky FULL, punched/touched sets;
  - `ri_auto_sweep` — a STUB returning 2.

## Review 2026-09-26 (applied to the tasks below)

| # | Finding | Fix |
|---|---------|-----|
| R1 | Allow-list = the 16 303 IDs only. The engine delivers `RI_EV_AUTOMATION` to 303A/303B only, so this is honest today. But E1 p. 72 names *"level changes, effect controls"* as core automation. Mixer rides, FX and the 808/909 knobs are the flagship use, and they already have engine setters. | New Task 5: route `RI_EV_AUTOMATION` for the blocks that have setters (FX `0x0A0x` → `ri_engine_fx_set`; 808 `0x04xx` / 909 `0x09xx` → the kit `set_param` paths; pan/send/insert need IDs), then widen the allow-list. Each widening step comes with a delivery test. The §5.2 unbound list shrinks as it goes. |
| R2 | Sweep contract undefined. A forward-quantized touch lands at a tick ≥ cursor, and the next sweep covering that tick would erase the pass's own write. | Pass-write marker: `RIAutoEv.pad` bit `RI_AUTO_EV_PASS` set by `touch`. `sweep` erases, for punched controls in `[from, to)`, only events **without** the marker. `ri_auto_pass_end` clears all markers and both sets. Caller order per update: `sweep(last, cursor)` then `touch` at the new value. |
| R3 | Render-thread safety missing. The render task emits from the lane while the GUI thread `memmove`s it, a torn read (spec §4 LOCKED: SPSC + snapshot enforcement). | New Task 3b: double-buffered lane (front/back) with a staged pointer swap at block boundaries, using the `RISeq` pending/active handshake idea. Render reads only the published front. The GUI mutates the back, publishes, then re-syncs. The publish rate is bounded; the host test proves render never sees a half-applied edit. |
| R4 | Emission cap had no carry: a dropped event in window k was lost (song-track R1 lesson). | `struct RIAutoCarry { uint32_t next; }` = lane index of the first event not yet emitted. `emit_range` resumes from it, so an event dropped by the cap is emitted in the next window, late but never lost. |
| R5 | Cursor jumps while recording (FF/Rew/Bar arrows, loop wrap) were unspecified. | E0, from p. 81–84: forward moves while RECORD sweep `[old, new)` for punched controls. That is the p. 81 step-record law generalised: the Bar arrow advance holds the value for the passed bars. Backward moves and loop wrap punch every control out (p. 84). After any discontinuity: `punch_out_all` first, **then** `chase`. |
| R6 | ATRK storage undefined. 32768 × 8 B = 256 KB cannot live inline in `RISong` (stack-allocated on some paths; AROS stacks are small). | `RISong` gains `struct RBAutoEv *atrk; uint32_t natrk, atrk_cap;`, a caller-provided buffer. Read with `atrk == NULL` + an ATRK chunk rejects with its own err text. `rbng_read/write_song` signatures unchanged (`check_sig`). |
| R7 | Tasks 2 and 3 were too large to review, and each bundled 5+ behaviours. | Split into 2a/2b/2c, 3a/3b/3c and a separate 4, each its own RED→GREEN commit. |
| R8 | The spec says "emit in §8 order", but lane order is (tick, ctl), not the §8 key. | `emit_range` emits in lane order; the block merger (player) applies the §8 sort across producers, as it already does for pattern events. |
| R10 | The WIP `struct RIAutoClip` holds `ev[RI_AUTO_CLIP_EVENTS]` inline = 32768 × 8 B = 256 KB, the same stack hazard as R6 (a local clip in any GUI handler overflows an AROS task stack). | Caller-owned: `struct RIAutoClip { uint32_t base_tick, n, cap; struct RIAutoEv *ev; }`. Copy/cut refuse rc 2 (clip untouched) when `n > cap`. |
| R9 | The cross-check (Task 1 step 4) needs `ctlreg.o` built. | Every t77 run: `bash scripts/ri_build_host.sh gui && … sched && … formats && … test t77_autolane` (chained with `&&`: a failed build never feeds a stale link). |

## Global rules
- TDD: each task's Step 1 adds stubs → behavioural RED; record the first
  failing `RI_ASSERT` line in the commit body; then GREEN.
- Full `bash scripts/ri_audit.sh` = `AUDIT 0/0 PASS` before every
  commit. If a sibling's WIP breaks the tree, verify in a scratch
  worktree at HEAD + your files (needs a sibling `Vulkan4Aros` symlink).
- Commit only your files; messages end `[§12.9c]` +
  `Co-Authored-By: OpenCode <noreply@opencode.ai>`.
- No alloc/IO/RNG/libm/mutable statics in `engine/`; the audit greps are
  literal (no `free(` even in comments).

---

### Task 2a: Sweep + pass end + step/loop/seek laws (t77 part 2)
**Interfaces (add to `autolane.h`):**
```c
#define RI_AUTO_EV_PASS 0x01u /* RIAutoEv.pad: written in the current pass */
/* signatures as stubbed in the WIP tree; pass_end is new */
int  ri_auto_sweep(struct RIAutoLane *l, const struct RIAutoPass *p, uint32_t from, uint32_t to,
                   const uint8_t *vals); /* erase unmarked events of punched ctls in [from,to); vals re-anchor at `to` (marked) */
void ri_auto_punch_out_all(struct RIAutoPass *p);   /* loop wrap / backward move; touched set kept */
void ri_auto_pass_end(struct RIAutoLane *l, struct RIAutoPass *p); /* Stop / Record off: clear markers + both sets */
```
`ri_auto_touch` sets `RI_AUTO_EV_PASS` on the event it writes or replaces.
`vals` is indexed like `p->punched` (one current value per punched control); NULL = erase only. The re-anchor is required so a held value survives the erase: without it, the lane would fall back to the older value after `to`.
- [ ] Step 1 (RED), test cases:
  - Untouched controls keep every event through sweeps.
  - Touched-then-held: after `sweep(t0, t0+4 bars)` exactly one event remains for that control, its touch.
  - Erase happens with no further moves.
  - The pass's own writes inside a later sweep survive (marker).
  - `from >= to` is a no-op.
  - Step record: advance one bar (sweep that bar) → one event + an empty bar; punch-out before advancing → the single event, and old events after it return.
  - Loop: `punch_out_all` at wrap keeps the touched set; the previous lap plays back.
  - Touching after the wrap punches in again and erases this lap's span.
  - Backward move → punch out all.
  - `pass_end` clears markers (re-scan: no `pad` bit left) and both sets.
  - Record-without-playback: a stopped RECORD touch writes one event and sweeps nothing.
- [ ] Step 2 (GREEN), then audit 0/0, then commit `feat: automation sweep + pass laws [§12.9c]`.

### Task 2b: Range edits — init song/loop, copy touched, clear (t77 part 3)
```c
/* as stubbed in the WIP tree */
int ri_auto_clear_loop(struct RIAutoLane *l, uint32_t start_tick, uint32_t len_ticks); /* drop [start,start+len), all ctls */
int ri_auto_stamp(struct RIAutoLane *l, uint32_t tick, uint16_t ctl, uint8_t val);    /* exact tick, no quantize, denied refused */
int ri_auto_copy_touched(struct RIAutoLane *l, const struct RIAutoPass *p, uint32_t start, uint32_t end,
                         const uint8_t *vals);  /* per touched ctl: clear its [start,end), one event at start; vals ~ p->touched */
/* Init Song = clear_loop(0, song_end) + stamp per allowed ctl; Init Loop = clear_loop(loop) + stamp.
 * The caller pre-checks capacity (n - cleared + stamps <= cap) so the pair is all-or-nothing. */
```
All are all-or-nothing on capacity: pre-count, refuse rc 2, lane untouched. Refused IDs (not allowed) → rc 2, lane untouched.
- [ ] Step 1 (RED):
  - Init song: `[0, end_of_song)`.
  - Init loop: `[loop_start, loop_end)`; the rest of the loop is cleared (p. 83); outside is kept.
  - Copy touched: only touched controls; others untouched even inside the range.
  - Edge ticks: start included, end excluded.
  - Capacity refusal leaves the lane byte-identical.
  - A denied ID refuses.
- [ ] Step 2 (GREEN), then audit, then commit `feat: automation range edits [§12.9c]`.

### Task 2c: Cut / Copy / Paste / Paste Replace mirror (t77 part 4)
The bar-based signatures (as stubbed: `ri_auto_cut/copy/paste/paste_replace(…, uint64_t bar, …, uint32_t ppq)`) mirror `songtrack.h`. The clip is caller-owned per R10: `struct RIAutoClip { uint32_t base_tick, n, cap; struct RIAutoEv *ev; }`, ticks relative to `base_tick`.
- [ ] Step 1 (RED):
  - Cut removes the bars and shifts later ticks left by `bars × 4 × ppq`.
  - Paste inserts at a bar and shifts later ticks right.
  - Events that would land at or beyond bar 999 are dropped (the song-track overflow law).
  - Paste Replace clears the target bars, then inserts.
  - A combined test runs a song-track edit and a lane edit with the same arguments and asserts slots and events still line up bar-for-bar.
  - Capacity all-or-nothing, for both the lane and the clip (`n > cap`).
  - A static assert or test that `sizeof(struct RIAutoClip)` < 64 bytes (R10 guard).
- [ ] Step 2 (GREEN), then audit, then commit `feat: automation measure edits mirror the song track [§12.9c]`.

### Task 3a: Chase + windowed emission with carry (t77 part 5)
```c
struct RIAutoCarry { uint32_t next; };                  /* first lane index not yet emitted */
uint32_t ri_auto_chase(const struct RIAutoLane *l, const struct RIAutoPass *p, uint32_t tick,
                       const struct RITempoMap *map, uint32_t ppq, struct RIEvent *out, uint32_t cap, uint32_t *seq);
uint32_t ri_auto_emit_range(const struct RIAutoLane *l, const struct RIAutoPass *p, struct RIAutoCarry *c,
                            uint32_t first, uint32_t count, const struct RITempoMap *map, uint32_t ppq,
                            struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq);
```
Emitted event: `type = RI_EV_AUTOMATION`, `device` = section from the ID block, `value` = ctl, `flags` = val (the t37 contract). Punched controls are suppressed in both functions. Chase emits the latest value ≤ tick per control that has any event, at the sample of `tick`.
- [ ] Step 1 (RED):
  - Chase from an arbitrary tick; never-automated controls get nothing.
  - Loop wrap: `punch_out_all`, then chase at the loop start (5.1; the order is pinned).
  - Emission window edges.
  - Cap 1 across a window with 3 events → the carry resumes, and every event appears exactly once over the next windows.
  - Suppression of punched controls.
  - Field contract.
- [ ] Step 2 (GREEN), then audit, then commit `feat: automation chase + emission [§12.9c]`.

### Task 3b: Render-safe publish (R3)
`struct RIAutoPub { struct RIAutoLane lane[2]; volatile uint32_t front; }` (or reuse the `RISeq` staged/active pointer pair):
- the GUI thread mutates `lane[back]`, then publishes (swap `front` at the next block boundary, via the existing staged-pointer handshake);
- then it re-syncs the new back by copying `n` events (≤ 256 KB) — bounded, off the render thread;
- the render thread reads only `lane[front]` and never writes.

- [ ] Step 1 (RED), host test with a deterministic interleaving harness:
  - a render "block" between any two GUI operations sees either the old or the new lane, never a mix (check `n` and a checksum);
  - publish-during-emit resumes correctly through `RIAutoCarry`: re-index by tick after a swap, because indices differ between lanes.
- [ ] Step 2 (GREEN). Document the publish rate law (at most one publish per render block) in the header. Audit, then commit `feat: automation lane publish (render-safe) [§12.9c]`.

### Task 3c: ATRK codec + RISong buffer + spec amendments (R6)
- `project/rbng.h`:
  - `RISong` gains `struct RBAutoEv *atrk; uint32_t natrk, atrk_cap;`, caller-provided;
  - `rbng_song_init` sets them to NULL/0;
  - the layout comment documents `ATRK` (v1.2): `u32 n`, then n × `u32 tick, u16 ctl, u8 val, u8 pad0`, sorted by (tick, ctl), no duplicates.
- Writer:
  - ATRK only when `natrk > 0`;
  - minor 2 when ATRK is present (1 for banks/track only, 0 legacy);
  - `AUTO` and `ATRK` are never both written.
- Reader:
  - rejects, each with its own `err` text: ATRK below minor 2, ATRK with `atrk == NULL` or `n > atrk_cap`, unsorted, duplicate, tick ≥ 999 bars, ID not allowed, AUTO+ATRK together;
  - validates the whole body before storing (the song-track R7 law).
- Engine↔codec: `ri_auto_load_triples` / `ri_auto_store_triples` in `autolane.h` take explicit `tick/ctl/val` arrays (engine never sees `RBAutoEv`).
- Spec §6 amendments:
  - the master spec gets a Status: Outdated block on "30 Hz tweak recorder at ppq/24" / "AUTO ppq/24" (E1 p. 84: 32nd = ppq/8);
  - the `rbng.h` comment is updated.
- [ ] Step 1 (RED):
  - Round-trip of a 3000-event lane (more than the old 256 cap).
  - Legacy AUTO-only files are byte-identical on rewrite.
  - Each reject is asserted by its `err` substring, with the bad byte in the LAST record.
  - Minor raised if and only if ATRK is present.
- [ ] Step 2 (GREEN), then audit, then commit `feat: ATRK automation chunk (RBNG v1.2) [§12.9c]`.

### Task 4: Audit wiring + mutants + records
- [ ] `scripts/ri_audit.sh`, in the seq block beside t59/t74:
  - the `t77_autolane` line;
  - the static-state grep over `engine/seq/autolane.c`;
  - `engine/seq/autolane.c` added to the AROS compile-only loop;
  - a layer-guard grep: `autolane.h` names no `sched.h`/`clock.h`/`project/`/`gui/`.

  Don't edit it under a running audit.
- [ ] Mutants: each must FAIL; record its line, then revert.
  - Sweep ignores the pass marker (erases own writes).
  - Erase skipped when the control does not move.
  - Allow-list accepts an unknown ID.
  - Forward quantize → nearest.
  - Chase removed at wrap.
  - Paste shift omitted on the lane.
  - Carry ignored (dropped event lost).
  - Publish swaps mid-block.
  - The ATRK validate-then-store merged back into one loop.
  - The value sentinel reintroduced.
- [ ] Records + tracker: raw `llm-wiki/raw/articles/2026-09-26-automation-lanes.md`, log, index; tick §12.9c; commit + push.

### Task 5: Widen delivery + allow-list (R1)
Follow the owner's decision ("controls without IDs cannot be recorded until IDs exist") step by step. Each block below is its own commit:
- [ ] 5a FX (`0x0A0x`, already dispatched by `ri_engine_fx_set`): the engine routes `RI_EV_AUTOMATION` with value in the FX block to `ri_engine_fx_set(id, val)`. Add the FX IDs to the allow-list. Test: an emitted FX automation event changes the FX parameter the same way the knob path does (render-identical to `ri_engine_fx_set` on a golden fixture).
- [ ] 5b 808 / 909 kit parameters with engine IDs (`0x04xx`, `0x09xx`): route to the existing `rb808_set_param` / `rb909_set_param` paths used by the registry bindings (`RI_BIND_808V/808ALL/909V/909HAT` + voice). Widen the list. Test likewise.
- [ ] 5c Mixer strips (Level/Pan/Delay send ×4, Dist/PCF/Comp inserts, Master Comp): these have **no shared IDs yet**; the registry binds them by section (`RI_BIND_PAN/SEND/INSERT`).
  - Allocate a block (spec §13 lists `0x0Bxx`) in coordination with the GUI registry owner. `gui/ctlreg.c` `engine_id` column only; `t60_ctlreg` must stay green.
  - Route the block to `ri_engine_set_pan/send/assign_insert`.
  - Widen the list.
  - Level faders need an engine level setter if none exists; if so, record it and stop at 5c-level, owner call.

  E1 p. 72 "level changes" makes this the most user-visible gap. Do not skip it silently: if blocked, the §5.2 unbound list and the tracker must say so.
- [ ] The cross-check test (registry automatable ⇄ engine allow-list) is updated at each step. The expected-unbound list only ever shrinks.

## Review focus
- The punch law: marker protects own writes, erase-without-move, suppression, pass end clears.
- Chase on every discontinuity including loop wrap; `punch_out_all` before chase.
- No torn reads: render reads only the published lane.
- Carry: no dropped automation event is ever lost.
- Measure edits keep slots and events aligned bar-for-bar; overflow at 999; capacity all-or-nothing; sticky `lane_full` shown, never silent.
- ATRK/AUTO exclusivity, legacy byte-identity, minor iff present, a caller buffer (no 256 KB structs on stacks).
- The allow-list only widens with a delivery test; the unbound list only shrinks.
