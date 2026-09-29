# S6 closed + S7 proven on the Dell (clicks reach the app, skins render)

- Source: Dell lane sessions e6320/e6320x, probe logs, captures, evlog,
  commits `72a3fe4a`…`4f03fd7d` (Vulkan4AROS, on main) + `b3f7091` (here), 2026-09-29
- Collected: 2026-09-29
- Published: 2026-09-29
- Related: `2026-09-29-s7abc-s6-research.md`
- Evidence: `docs/evidence/gui/hw-look/` (`dell-s7-808ri-*.png`, README S6/S7 section)

## S6: root causes (all guest-side, all proven by probe diff)

- `IND_ADDEVENT` ignores every class but RAWMOUSE/RAWKEY (AROS source): the pointerpos rode along and died — clicks landed at x=y=0.
- Missing `IEQUALIFIER_LEFTBUTTON` on press: Intuition delivered code 0 and dropped the release.
- Digit-only `json_get_ulong` (TWO copies: `arostcp.c` + `arostcp_shared.h`): JSON `true` parsed as 0, so every release was built as a second press.
- Fix: moves via `IND_WRITEEVENT`, qualifier mirrored per button, both parsers bool-aware; server gained `--ui-press/--ui-release` + `qQ` on `--ui-rawkey`.
- Probe GREEN: press `0x68` + release `0xE8` at targeted coords.

## S6 acceptance (no owner input)

- Tab clicks switch pages (`TAB page=1`, Mix→Synths single click; first click activates).
- Rail power toggles 303B (`VIS dev=1 show=0 mask=1d` / `show=1 mask=1f`).
- Knob drag emits `CTL 0104=0 → 25`.
- Lane-ops lessons: two agents on one pair ping-pong every ~30s (mutual redial); drain with `--bye` (Break won't kill agents); open log files lock (close first); e6320 needs manual redial after reboot; private pair+port isolates proof traffic (`e6320x`/9294, `spike-s6.service`, enabled).

## S7 rendering proof

- Seeded whole-panel 808-RI check-build (uncommitted): dark strips + amber headers where bound; Classic Levi/MASTER (no part / stale `bg08`) and Classic 909 (stale `bg03`, t75) beside them — per-part fallback visibly working.
- Ctrl+M mechanics via evlog (`SKIN sync=`); full key UX waits for owner hands (shared-keyboard forensics lesson).
- Still open: t75 asset (sibling), knob-frame skin roles in the draw path (backgrounds only today), song load/save UI for SKAS.
