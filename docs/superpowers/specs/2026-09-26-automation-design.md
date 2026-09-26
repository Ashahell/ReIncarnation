# Automation lanes (slice §12.9c) — design

**Date:** 2026-09-26. **Status:** r2 **APPROVED by the owner 2026-09-26** (decisions 5.1, 5.3, 5.4 accepted as proposed: "actual usage will reveal if we need to adjust"). The implementation plan must be regenerated from this r2 (`docs/superpowers/plans/2026-09-26-automation-implementation.md` was written against r1).
**Scope:** control-event storage per 32nd grid, the automatable-control allow-list, the E1 recording model (punch-in per control, touched set, loop punch-out, step recording, record without playback), playback (chase on every position jump, punched-in suppression), the knob halves of Initialize Song/Loop, Copy Touched Controls to Loop/Song, the automation half of Cut/Copy/Paste measures, capacity + the RBNG codec extension, lane→event emission.
**Explicitly OUT:** panel control-ID allocation for controls that have none yet (§2.2 remainder — this spec lists the dependency, §5.2), per-parameter smoothing (§12.4), a GUI automation editor, tempo automation (Tempo is not automatable, E1), engine dispatch changes (the engine already consumes `RI_EV_AUTOMATION` for 303A/303B, pinned by `t37_engine_single`).
> **Status update (2026-09-26, Task 5a `c6dd53f`):** engine dispatch now also consumes the FX block (`0x0Axx` → `ri_engine_fx_set`, pinned by `t52_engine_fx` block g); the "303A/303B only" clause above is outdated. Drum kit params (5b) and mixer strips (5c) remain undispatched — see §5.2 note.
**Fidelity order:** ReBirth RB-338 2.0.1 Owner's Manual (E1; this repo's `/tmp/rb201-manual.pdf`); page numbers are the printed ones. Every quote below was re-located by grep in the manual text for r2.

## 0. E1 evidence (verbatim)

- **What is automated (p. 72):** *"Practically all front panel control can be recorded."* / *"The only controls that are not affected by automation are Tempo, the Mute buttons in the Mixer and the Master Level controls!"* **Shuffle (p. 73):** *"You can always mute and unmute sections, adjust the master faders and adjust the Shuffle. These controls are not part of the Song automation."* **Shared-instrument switches (p. 34):** *"The settings of the shared instrument switches are part of the Song automation, like most other controls on the panel."*
- **Song mode only (p. 72):** *"In Song mode, settings and movement of panel controls are always under automation control."*
- **Grid (p. 84):** *"Pattern changes are only recorded on downbeats (the beginning of a measure) while control changes are recorded on each 32nd note."*
- **Signal flow (p. 74):** *"While the sequencer records, it plays back previous recordings."* **Conflicts:** *"If you have already recorded some automation for a control, and move the same control manually, at the same time as it is being controlled by the automation, there will be “conflicting” input to the audio engine. This may lead to a control “flickering” on screen"* — no lock-out: the sequencer output and live input are mixed.
- **Punch-in per control (p. 82):** *"Nothing gets recorded until you actually “touch” a control."* / *"As soon as you “touch” a control in this way, that control is considered “punched in” and will from now on be showing and playing it’s current setting, rather than the settings recorded in the Song. Old events for this control will now be deleted, as long as recording continues. However, “punch-in” happens individually for each control, controls that you haven’t touched during this recording pass are not affected in any way."*
- **Record without playback (p. 82):** *"Activate Record but not playback"* / *"Set the control(s) to the desired initial position. That/Those controls are now considered “punched in”"*.
- **Stopping (p. 82):** *"When you reach the Song position where you want to return to the previously recorded changes, hit Stop or deactivate Recording"*.
- **Step recording (p. 81):** *"If move a control and then punch out before advancing to the next measure, that control setting will only be valid for a short moment at the beginning of the measure."* / *"If you advance to the next bar, the setting you leave the control at will be valid for the entire previous measure. In other words, that control will remain at a static position throughout that measure."*
- **Loop recording (p. 84):** *"when you reach the loop end, all controls will be “punched out”, so that on the next lap, they will instead play back movements recorded on the previous lap."* / *"if you move the control just when the Song loops, it will be considered as “punched-in” again. That is, you will again be recording that control, which means you erase actions performed on previous laps."*
- **Copy touched controls (p. 86):** *"select “Copy touched controls to Loop/ Song” from the Edit menu."* / *"events for the controls you have touched during this recording pass, will be inserted at the beginning of the Loop/Song."* **(p. 177):** *"This menu item allows you to insert “static” settings for any control, for a number of bars."* Shortcuts Ctrl+L / Ctrl+T (p. 222; decoded by `gui/keymap.c` as `RI_KM_TOUCHED_LOOP/SONG`).
- **Initialize Loop (p. 83):** *"Please note that this command clears all the Pattern changes and knob recordings currently inside the Loop!"* / *"Events that correspond to the settings currently made in Pattern mode, will be inserted at the beginning of the Loop. The rest of the loop is cleared completely."* **Initialize Song (p. 177):** *"all measures in the Song are filled with the settings currently made in Pattern mode. That is, the three Patterns selected in Pattern mode will be used, as well as all knob and other control settings."*
- **Measures move with their automation (p. 85):** Cut/Copy Loop + Paste / Paste Replace at Song Position operate on *measures*; *"Whatever was at bar 13 before the operation, is now at bar 17"* (Paste inserts). The slot half shipped in the song track (m66); the control half is this spec.
- **Playback from any position (p. 73):** *"You can use the Transport controls … to move to any position within the Song and start playback from there."* / *"When you open a Song, it will play back just as recorded, together with all Pattern changes, panel settings and automation."*

## 1. Ownership and layering

- New model `engine/seq/autolane.{h,c}` owns the automation event store; the codec (`project/rbng.c`) maps it to/from chunks; the song track owns slots; transport owns the cursor. Same one-direction layering as the song track: `autolane.h` → `transport.h` only; the emitter (needs `RIEvent`/`RITempoMap`) lives in `autolane_emit.h` (the `songtrack_emit.h` split, review R5 of the song-track plan), so project code never pulls scheduler headers.
- The model is gate-blind (no transport reads): the record path passes the transport state and cursor in. No alloc/IO/RNG/libm; no mutable static state (audit grep); caller-owned storage.

## 2. Laws (normative)

### 2.1 Grid
- One 32nd = `ppq/8` ticks (quarter = `ppq`). The lane REQUIRES `ppq % 8 == 0` (refuse rc 2 otherwise — `ri_ppq_or_default` admits 4, which has no integer 32nd). Default 96 → 12 ticks.
- Live writes quantize FORWARD to the next 32nd line (a write exactly on a line keeps it); a downbeat is always a grid line, so slot track and lane share every bar line.

### 2.2 What can be automated — an ALLOW-list
- Automatable = every panel control the registry marks `automatable` (`gui/ctlreg.c`), which already excludes exactly Tempo, the four Mixer mutes (On/Off), Master Level and Shuffle plus every non-control kind (displays, meters, LEDs, transport buttons) — pinned by `t60_ctlreg` against p. 72–73 — **minus the Bank and Pattern selectors of the four Pattern sections**, which are recorded as slot changes by the song track (downbeat grid, p. 84), never as lane events. The Pattern-section on/off switch stays a lane control (p. 56: to put silent parts in the Song "use the Pattern on/off switches instead"); the per-section Shuffle on/off stays non-automatable until E1 confirms it (registry note, `shuffle-scope` ledger).
- The engine side keeps its own static sorted table of automatable **control IDs** (the shared control-ID space of the AUTO chunk, spec §13 `0x030x/04xx/08xx/09xx/0Axx/0Bxx`); unknown or excluded IDs are refused (fail-closed). A test cross-checks the engine table against the registry's automatable set through the registry's control-ID mapping (engine never includes `gui/`). A deny-list is rejected: it is fail-open for every ID nobody thought of.
> **Status update (2026-09-26, Task 5a `c6dd53f`):** the table is now 16 303 IDs + 14 FX IDs (all `0x0Axx` with engine delivery; BEATS `0x0A00` has none and MIX `0x0A02` is topology-fixed — neither is listed). Still excluded: per-voice drum params (5b BLOCKED — one ID shared across all voices, unaddressable by the `(tick, ctl)` lane key, and no production knob path consumes the `808V/808ALL/909V/909HAT` binds yet) and ID-less mixer strips (5c — owner dependency, §5.2).

### 2.3 Recording model (E1 p. 81–84)
- **Pass.** A recording pass runs from Record-on to Record-off/Stop. Caller-owned `RIAutoPass` holds the *punched* set and the *touched* set (both ≤ `RI_AUTO_MAX_TOUCH` controls, E0 64; overflow refuses the new touch, counted).
- **Touch = punch-in.** The first write to a control in a pass adds it to both sets. Only allowed IDs, only in Song mode with transport state RECORD.
- **Erase while punched.** While a control is punched in, every tick the cursor sweeps is erased for that control (old events deleted "as long as recording continues"), whether or not the control moves; new writes by the touch land on the 32nd grid inside the swept span. Order per sweep `[from, to)`: erase punched controls' events in the span, then insert this pass's writes. A control held still after touching therefore leaves one event (its touch) and silence of old events behind it — the E1 "static position".
- **Punched controls play live.** Emission suppresses lane events for punched controls; the panel value is what sounds (E1 "showing and playing its current setting").
- **Loop end** punches every control OUT (the touched set is kept for Copy Touched); touching again after the wrap punches it in again and erases on this lap (E1 p. 84).
- **Record without playback.** Setting a control while stopped in RECORD punches it in and writes one event at the (quantized) cursor.
- **Step recording (p. 81).** Advancing the cursor by a bar while a control is punched in sweeps that bar (erase + the one event at its start) → the value holds for the whole previous measure; punching out before advancing leaves just the one event at the measure start (valid "a short moment", then the old recording resumes). Both fall out of the sweep law — no separate code path.
- **Conflicts (p. 74).** Manual input outside a recording pass is never blocked: the engine receives both, last writer wins; the model stores nothing for it.

### 2.4 Playback
- Song mode only; in Pattern mode no lane events are emitted.
- **Chase on every position discontinuity** — play start, seek/locate (Rewind, FF, Bar arrows, Stop-to-start), loop wrap: emit, at the new position, the latest value ≤ position of every control that has any event (the §0 "play back just as recorded" promise from any start point). E0: the manual does not say whether a loop wrap chases; chasing makes every lap sound identical to a play-start at the loop start, which is the only reading under which "repeat a section, infinitely" (p. 73) is deterministic. Without chase, a sweep recorded late in a loop would leak its end value into the next lap's start.
- Within a window: emit events in `[first, first+count)` as `RI_EV_AUTOMATION` (`device` = section from the ID block, `value` = control ID, `flags` = value 0..127 — the `t37` contract), tick→sample via `ri_map_tick`, lane order (tick, then ID) preserved, cap drops the tail deterministically and the caller's carry makes the next window resume (song-track R1 lesson: never mark a dropped event as sent).

### 2.5 Edits
- **Initialize Song** (knob half): clear the whole lane, then one event per allowed control at tick 0 with its current Pattern-mode value.
- **Initialize Loop** (knob half): clear `[loop_start, loop_end)` for all controls, then one event per allowed control at `loop_start`.
- **Copy Touched Controls to Loop/Song:** for each control in the pass's touched set: clear its events in `[range_start, range_end)` (Loop = loop bars, Song = whole song), then one event at `range_start` with the control's current value. Controls not touched are untouched.
- **Cut/Copy/Paste measures** mirror the song-track edits on the lane: Cut removes the events inside the bars and shifts later ticks left by the cut length; Paste inserts the clip's events shifted to the paste bar and shifts later ticks right, dropping any event that would land at or beyond bar 999; Paste Replace clears the target bars then inserts. The clip carries events relative to its first bar. Slot track and lane are edited by one caller operation so they can never diverge.
- All bulk edits are all-or-nothing on capacity (refuse rc 2, lane untouched) — the song-track refusal law (R6), never silent truncation.

### 2.6 Capacity and codec
- **The current RBNG `AUTO` chunk is far too small:** `u16 n (0..256)` and `RI_RBNG_MAX_AUTO 256` for the whole song — one 8-bar filter sweep at the 32nd grid is 256 events. Keep `AUTO` as the legacy/≤256 form; add a v1.2 chunk `ATRK` (`u32 n`, then n × `u32 tick, u16 ctl, u8 val, u8 pad`, sorted by (tick, ctl), duplicates rejected, ticks < 999 bars, IDs on the allow-list) under the STRK precedent: minor raised only when present, omitted when empty so legacy files stay byte-identical, `AUTO` and `ATRK` never both present.
- Lane capacity `RI_AUTO_MAX_EVENTS` (E0 proposal 32768 = 256 KB, caller-owned, sized once at song load — never in the render path). Full → record refuses the new event and raises a sticky `lane_full` flag the GUI shows (spec §17 style: never silent).

## 3. Model sketch (not ABI; the plan fixes signatures)

```c
struct RIAutoEv   { uint32_t tick; uint16_t ctl; uint8_t val; uint8_t pad; };
struct RIAutoLane { uint32_t n, cap, flags; struct RIAutoEv *ev; };   /* sorted (tick, ctl) */
struct RIAutoPass { uint16_t npunched, ntouched; uint16_t punched[RI_AUTO_MAX_TOUCH], touched[RI_AUTO_MAX_TOUCH]; };

int  ri_auto_allowed(uint16_t ctl);                                  /* the allow-list */
int  ri_auto_value(const struct RIAutoLane *, uint32_t tick, uint16_t ctl, uint8_t *out); /* 1 found, 0 none */
int  ri_auto_touch(struct RIAutoLane *, struct RIAutoPass *, uint8_t tr_state, uint32_t cursor, uint32_t ppq,
                   uint16_t ctl, uint8_t val);                        /* punch-in + write */
int  ri_auto_sweep(struct RIAutoLane *, const struct RIAutoPass *, uint32_t from, uint32_t to);
void ri_auto_punch_out_all(struct RIAutoPass *);                     /* loop end */
int  ri_auto_copy_touched(struct RIAutoLane *, const struct RIAutoPass *, uint32_t start, uint32_t end,
                          const uint8_t *cur_val_by_touched);
int  ri_auto_init_range(struct RIAutoLane *, uint32_t start, uint32_t end, const uint16_t *ctl, const uint8_t *val, uint32_t n);
int  ri_auto_cut / ri_auto_copy / ri_auto_paste / ri_auto_paste_replace (bar-based, song-track signatures);
/* autolane_emit.h */
uint32_t ri_auto_chase(const struct RIAutoLane *, uint32_t tick, ...out...);
uint32_t ri_auto_emit_range(const struct RIAutoLane *, const struct RIAutoPass *, uint32_t first, uint32_t count, ...out...);
```
`ri_auto_value` returns found/not-found separately: the r1 "127 = no automation" sentinel collided with the legal value 127.

## 4. Seams to other slices
- GUI: the record path (G5/G6a panel) calls `touch` on control changes during a pass, `sweep` from the live feed, `punch_out_all` on loop wrap, `copy_touched` on Ctrl+L/Ctrl+T; the panel shows lane values in Song-mode playback (display projection) and the `lane_full` flag.
- Transport: pass begin/end on Record/Stop edges; loop-wrap and seek edges drive chase.
- MIDI: Standard-Mapping CCs are control changes like the mouse (p. 74 "front panel and incoming MIDI control messages are first mixed together").

## 5. Open items (ledger rows before they lock)

| # | Item | Why open | How it closes |
|---|------|----------|---------------|
| 5.1 | Chase on loop wrap | E1 silent (E0 above) | **DECIDED 2026-09-26 (owner): chase on every wrap.** Pinned by t77; revisit if usage shows otherwise |
| 5.2 | Control IDs for automatable registry controls that have none | AUTO stores the shared ID; only bound controls have one today | **Accepted (owner 2026-09-26) as a dependency:** §2.2 remainder allocates IDs; until then those controls cannot be recorded, and a test lists them (never silently dropped). **Update 2026-09-26:** 5a FX done (14 IDs delivered, `c6dd53f`); 5b drum per-voice BLOCKED (shared IDs across voices + no knob path — needs a lane-model voice key, beyond this slice); 5c mixer still waits on the `0x0Bxx` block + GUI-registry owner (plus a possible engine level setter — owner call) |
| 5.3 | `RI_AUTO_MAX_EVENTS`, `RI_AUTO_MAX_TOUCH` | sizing | **DECIDED 2026-09-26 (owner): 32768 events / 64 touched controls.** Named constants, one place; soak with a dense 999-bar song; adjust from real use |
| 5.4 | ATRK chunk | format extension | **DECIDED 2026-09-26 (owner): approved as RBNG v1.2.** Codec tests incl. legacy byte-identity |

## 6. Spec amendments required
- Master spec §8/§13 "GUI-thread 30 Hz tweak recorder at ppq/24" and "AUTO ppq/24" → **Outdated** by E1 p. 84 (32nd = ppq/8). Record with a Status block, do not silently rewrite.
- RBNG layout comment (`project/rbng.h`) gains `ATRK`; `AUTO` documented as the legacy form.

## 7. Testing (new `t77_autolane`; t74 player, t75 skin, t76 zoom are taken)
- Grid: forward quantize (mid-32nd → next line, on-line stays), downbeat shared with the slot track, `ppq % 8 != 0` refused.
- Allow-list: each of Tempo / 4 mutes / Master Level / Shuffle refused; a neighbour allowed; an unknown ID refused; engine table == registry automatable set (cross-check).
- Punch law: untouched controls keep their events through a pass; a touched-then-held control erases every old event over the swept span and leaves exactly its touch; erase happens even when the control never moves again; punched controls are suppressed in emission; Stop ends the pass.
- Loop: punch-out-all at wrap; the previous lap plays back next lap; touching after the wrap erases this lap's span.
- Step record: advance-by-bar gives one event + cleared bar; punch-out-before-advance leaves the single event.
- Chase: play from an arbitrary tick emits the latest value per automated control, none for never-automated controls; loop wrap chases (5.1).
- Edits: init song/loop, copy touched (only touched controls, range clear + start event), cut/copy/paste/paste-replace mirror the slot track bar-for-bar, overflow drop at 999, capacity refusal all-or-nothing.
- `ri_auto_value` found/not-found incl. a stored 127; emission window, order, cap with carry resume, `t37` field contract.
- Codec: ATRK round-trip, sort/duplicate/range/allow-list rejects each with its own `err` text, legacy AUTO files byte-identical, AUTO+ATRK together rejected.
- Mutants (each must FAIL): punch erase skipped when the control does not move; deny-list swap (unknown ID accepted); forward quantize → nearest; chase removed; paste shift omitted on the lane; value sentinel reintroduced.

## 8. Non-goals restated
No interpolation between events (ReBirth records 32nd steps; smoothing is §12.4), no automation of Tempo/mutes/Master/Shuffle, no GUI lane editor.

## 9. Review log (r1 → r2, 2026-09-26)
- Added the E1 recording model that r1 lacked: punch-in per control with erase-while-punched (p. 82), touched set + Copy Touched Controls to Loop/Song (p. 83, 86, 177), loop punch-out (p. 84), record-without-playback and step recording (p. 81–82), conflicts (p. 74). r1's "replace at (tick, ctl), append otherwise" could not delete old events of a held control and broke lane sort order on later passes.
- Added chase on position jumps (r1 emitted only windowed events: playing from bar 40 would sound with bar-0 settings).
- Added the automation half of Cut/Copy/Paste measures (p. 85): r1 would have left knob moves behind when measures moved.
- Capacity: r1 inherited the 256-event AUTO cap (one 8-bar sweep); r2 adds ATRK + an explicit refuse-and-flag capacity law.
- Allow-list instead of deny-list; `ppq % 8` law; `ri_auto_value` sentinel collision (127 is a legal value) removed; emitter header split; draft-thinking remnants ("— NO:") removed.
- Test number moved to t77 (t75 skin, t76 zoom now exist). Spec §8/§13 ppq/24 conflict surfaced as an amendment, not silently overridden.
- Kept from r1: gate-blind model, 32nd forward quantization, init halves, exclusions set, t37 event contract, caller-owned storage.
- 2026-09-26: owner approved r2 and the E0 decisions (chase on loop wrap, 32768 / 64 capacities, ATRK v1.2, the control-ID dependency), to be adjusted if actual usage shows the need.
