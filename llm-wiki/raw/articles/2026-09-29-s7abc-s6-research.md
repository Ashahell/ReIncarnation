# S7a–c per-section skins + S6 click research (owner granted both)

- Source: ReIncarnation commits `ea11f38`, `e60f911`, `eb81381`; Vulkan4AROS detached commit `72a3fe4a` (unpushed); AmigaOS Input Device wiki; AROS muimaster/window + Zune test sources, 2026-09-29
- Collected: 2026-09-29
- Published: 2026-09-29
- Related: `2026-09-29-zoom-guard-proof-spooler-fix.md`
- Specs: `docs/superpowers/specs/2026-09-26-skins-design.md` (per-module section), `docs/superpowers/plans/2026-09-28-gui-round3-opencode-prompt.md` (S6/S7)

## S7a: assignment core (`ea11f38`)

- Pure `gui/skinsect.c`: section -> mod (empty = Classic); SYNTH2 follows SYNTH1 unless split; uniform-Classic detection (SKAS written only when non-uniform); refcount acquire/release.
- TDD RED->GREEN (`t124` FAIL 12 first); mutant killed (follow-cut). No behavior change.

## S7b: registry, canvas, Ctrl+M (`e60f911`)

- AROS loader is now an 8-slot shared registry (refcounted through the host-pinned core; failed loads stay slotted-but-empty = Classic fallback); `for(section)` answers the canvas every draw at emit + replay (furniture replays unskinned).
- Ctrl+M cycles the focused device's voice section; Shift+Ctrl+M (new `RI_KM_SELECT_MOD_ALL` chord, t70-pinned, Shift provably not leaking) sets the whole panel; `skin_current` mirrors the focus for existing detectors.
- riapp scans Mods (PAL, Classic first), syncs on assignment change only (memcmp shadow + repaint), follows zoom; sectproof migrated to sync (whole-panel `mod=` preserved).
- TDD RED->GREEN (t70 chord, t71 cycling); mutants killed (shift-cut, section-cut). Audit green except pre-existing t75; ABIv1 + portable clean. Dell proof waits for the lane agent.

## S7c: SKAS song chunk (`eb81381`, minor 3)

- `u16 n` + `u8 section/u8 name` entries, strictly ascending, manifest-class names, no duplicates; written only when non-uniform (legacy shapes byte-identical); readers validate all-or-nothing. Pure apply helper: SKAS verbatim + dirty on unknown mods, first-MODR fallback, Classic default. No RIAPP load/save UI exists yet — helper waits for it.
- TDD RED->GREEN (`t128` link errors first); mutants killed (minor-cut, reject-cut, incl. a byte-patched duplicate file). Corpus neighbors green. Free minor was 3 (0/1/2/4 taken).

## S6 click research (no lane: agent down since host restart)

- Chain today: NEWPOINTERPOS + LBUTTON down + up, all `ie_TimeStamp` zero, qualifier 0, one DoIO.
- Official docs findings: the timestamp exists "to determine the sequence in which the events occurred" (zero on all three = simultaneous, zero-duration click); "the left and right mouse buttons are tracked in the message qualifiers for use in such things as dragging" (real presses carry `IEQUALIFIER_LEFTBUTTON`, ours carry 0); `IND_ADDEVENT` itself "updat[es] event qualifiers" (input.device may heal quals downstream — timestamps it cannot reconstruct).
- Ranked suspects: (1) zero timestamps + same-instant down/up; (2) missing button qualifier; (3) move+click needing separate submits; (4) ActiveScreen pointer.
- Server-side preparation (detached `72a3fe4a`, unpushed): `--ui-press/--ui-release` (`l|r|m[,X,Y][,qQ]`, exact-bytes unit-tested 31/31) so press/release splits and qualifier-carrying presses need no agent change. Timestamp stamping still needs the agent (permission granted); probe + experiments run when it redials.
- Menu finding (settles S5 verification): stock MUI/Zune menus are an RMB pull-down, never a visible bar — captures can never show the View menu; the notify pattern matches Zune `test.c` exactly.
