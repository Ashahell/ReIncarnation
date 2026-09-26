# Automation lanes (§12.9c): spec r2 approved + implementation plan review R1–R10

- Source: ReIncarnation session, 2026-09-26
- Collected: 2026-09-26
- Published: 2026-09-26
- Commits: `6ee2892` (spec r2 + reviewed plan); Task 1 landed earlier as `2834f7d`

## Spec r2 (docs/superpowers/specs/2026-09-26-automation-design.md) — APPROVED
- The owner's decision (2026-09-26): "we agree with your decisions. Actual usage will reveal if we need to adjust."
- Decided items:
  - 5.1: chase on every discontinuity, including loop wrap (Song mode only).
  - 5.3: capacities are 32768 events per lane and 64 controls per pass (punched/touched sets).
  - 5.4: a new RBNG chunk ATRK (v1.2), exclusive with the legacy AUTO chunk.
  - Controls without shared IDs cannot be recorded until IDs exist.
- Grid law: the 32nd note is ppq/8 (requires ppq%8==0), with forward quantize (E1 p. 84). This conflicts with the master spec's "ppq/24"; an amendment is due.

## Task 1 as landed (`2834f7d`)
- `ri_auto_allowed`: the allow-list is only the 16 TB-303 IDs (ledger R-ALLOW: the only IDs with end-to-end delivery).
  - The engine routes `RI_EV_AUTOMATION` only for the 303A/303B blocks.
- `ri_auto_value`: returns found / not-found, with no sentinel value.
- `ri_auto_touch`: refuse-first checks, then forward quantize, then a sorted insert/replace; the FULL flag is sticky.
- `ri_auto_sweep`: a stub.

## Plan review findings (applied to docs/superpowers/plans/2026-09-26-automation-implementation.md)
- **R1 — 303-only allow-list.** E1 p. 72 names level changes and effect controls as core automation. New Task 5 widens delivery, then the list, one block at a time, each with a delivery test:
  - 5a FX via `ri_engine_fx_set`;
  - 5b 808/909 kit parameters;
  - 5c mixer strips, which need a new ID block and possibly an engine level setter (owner call).
- **R2 — sweep erased the pass's own writes.** A forward-quantized touch lands at or after the cursor, so a later sweep would delete it. Fix:
  - a pass-write marker bit in `RIAutoEv.pad`;
  - sweep erases only unmarked events;
  - a new `ri_auto_pass_end` clears the markers and both sets.
- **R3 — render/GUI data race.** The GUI thread memmoves the lane while the render task reads it. Fix: a double-buffered lane with a publish at block boundaries; render reads only the published copy.
- **R4 — cap-dropped events were lost.** Fix: `struct RIAutoCarry { next; }` resumes emission from the first unemitted index.
- **R5 — cursor jumps while recording** (an E0 decision generalising p. 81–84):
  - a forward move sweeps `[old, new)`;
  - a backward move or loop wrap runs `punch_out_all`, then chase.
- **R6 — ATRK storage.** 32768 × 8 B = 256 KB must not sit inline in `RISong`, which is stack-allocated on some paths. Fix: a caller-provided buffer (`atrk`, `natrk`, `atrk_cap`); `rbng_read/write_song` signatures are unchanged.
- **R7 — task split.** Tasks 2 and 3 became 2a (sweep + pass laws), 2b (range edits), 2c (cut/copy/paste mirror), 3a (chase + emit + carry), 3b (publish), 3c (ATRK codec), 4 (audit/mutants/records) and 5 (widening).
- **R8 — emission order.** `emit_range` emits in lane order (tick, ctl); the player's block merge applies the §8 sort.
- **R9 — build before test.** Every t77 run builds `gui` first: the cross-check links `ctlreg.o`.
- **R10 — inline clip.** The WIP `struct RIAutoClip` held `ev[32768]` inline (256 KB, the same stack hazard as R6). Fix: a caller-owned pointer + `cap`, and a size guard test.

The plan keeps opencode's WIP Task 2 stub names (`sweep(..., vals)`, `clear_loop`, `stamp`, `copy_touched`, `cut/copy/paste/paste_replace`); `vals` re-anchors held values at the sweep end, as marked events.
