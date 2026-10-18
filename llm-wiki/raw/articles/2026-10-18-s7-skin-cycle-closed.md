# S7 closed: one Ctrl+M = one skin, and why two skins look identical

- Source: Dell E6320 lane e6320, bounded-exec agent `SYS:ATCPBIN` 144,752 B (`1bb12484…`), 2026-10-18
- Collected: 2026-10-18
- Published: 2026-10-18
- Related: `2026-09-29-s6-closed-s7-proven.md`, `2026-10-18-atcp-bounded-exec-dead-lane.md`
- Evidence: `docs/evidence/gui/hw-look/dell-s7-cycle-*.png`, `docs/evidence/gui/lane-click-proof.md`

## The gesture, measured

Four presses, four log lines, full cycle:

```
SKIN sync=1 current=808-RI
SKIN sync=1 current=Template
SKIN sync=1 current=Stale
SKIN sync=0 current=Classic
```

Requirements that make this deterministic, both learned the hard way:

- **press and release back-to-back in one job.** A press without its release is a *held* key; the keyboard repeats at ~3 Hz, so one logical press produced 1789 `sync` lines in the earlier run. That oscillation was never a defect in the cycling code — it was key repeat plus owner input interleaving on a shared keyboard.
- **Shift must not be down.** `Shift+Ctrl+M` sets the whole panel (whole-panel assignment) instead of the focused section's, so a stray Shift silently changes the meaning of the test.

Focus first with a click on a neutral spot inside the section (a click on a control changes a value). One Ctrl+M then moves exactly one section.

## Identical renders are explained, not defects

Pixel diffs of the four captures: 808-RI↔Template differ (4,944 px); Template↔Classic **identical**; 808-RI↔Stale **identical**. Both pairs have a filesystem reason, verified on the guest:

- `Mods/Template` contains **only** `Skin.manifest` (no images). Every part therefore falls back to Classic — rendering identically is the *correct* behaviour and is the cleanest possible proof that per-part fallback works.
- `Mods/Stale/bg02.png` is **byte-identical** to `Mods/808-RI/bg02.png` (md5 `6146b2fcfe0ef164211cbb160b35e134`). Two directories holding the same artwork must look the same; the app is not at fault.
- `Classic` is the **built-in fallback, not a Mods directory**. `Mods/` holds exactly `Stale`, `Template`, `808-RI` and the app reports `installed=4` — three directories plus Classic. Reading "installed=4" as four Mods dirs sends you looking for a directory that does not exist.

## Method worth repeating

When a visual result looks wrong, diff the captures numerically first (`compare -metric AE`), then ask the filesystem why. Two of the three "identical" cases here were correct behaviour that a screenshot alone would have had you debug for an hour.

## Lane facts consolidated

- Coordinate map is per build; re-ground from a capture: tabs y=167 (Synths 70, Drums 130, Mix 250), rail y=128 (x=33 toggles the 808 on the Drums page), 808 CUT OFF knob (420,256), neutral focus spot (700,200).
- A rail toggle's log line proves the handler ran, not the redraw — capture as well (`VIS dev=2 show=0 mask=1b`).
- `--ui-move` takes `x,y` with no button prefix while `--ui-press/--ui-release` take `button,x,y`; mixing them aborts the job *after* the press was delivered, so the guest state and the host's view of the job disagree.