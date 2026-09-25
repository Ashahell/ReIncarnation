# §12.10 G5: computer keyboard + focus bar — Appendix E map, one front panel, proven with real keys

- Source: ReIncarnation commit `64a8a30`; ledger `docs/evidence/gui/keyboard.md`, capture `docs/evidence/gui/img/2026-09-25-rikeys-sendkey.png`, trace `docs/evidence/gui/2026-09-25-rikeys-trace.txt`
- Collected: 2026-09-25
- Published: 2026-09-25

## What landed

- `gui/keymap.{h,c}` (t70): the Appendix E map (p. 221–226, with p. 18–22, 30–33, 43–44) keyed by positional Amiga raw codes ("The keys are on absolute positions on the keyboard", p. 224):
  - pattern rows Synth 1 `1`–`8`, Synth 2 `Q`–`I`, 808 `A`–`K`, 909 `Z`–`,`;
  - keypad transport (PC column: `4` FF, `5` Rewind, `1`/`2` loop start/end, `7`/`8` bars, `+`/`-` tempo, `0` Stop, Enter Play, space Stop/Play, `*` Record);
  - up/down focus;
  - synth programming — pitch `C F V G B N J M K , L . /`, Return Step, Backspace Back, `-` Note/Pause, `P` Accent, `[` Slide, `;`/`'` octave, Tab tap, Shift+Tab delete;
  - drum tap (`-` AC, `A`…`'` instruments 1–11, Shift delete);
  - menu chords (Ctrl as ReBirth/Windows, plus Right-Amiga).
- `gui/panelui.{h,c}` (t71): one front panel owns the focus (p. 22: click in a section, select a pattern in it, or up/down arrows) and applies pattern / transport / synth keys to the section states. `ri_str_goto_loop` added for keypad 1/2.
- RSection canvas: focus bar (`ri_geo_focus_bar`, orange on the focused section, p. 22), click-to-focus, exactly one raw-key owner per window.
- `RISECT keys`: Transport, the four Pattern sections and Synth 1 on one panel, with a per-change trace to `RAM:RISECT.LOG`.

## E0 decisions (owner may overrule)

- Precedence: menu chord > keypad transport > focus arrows > programming keys of the focused section > pattern keys. The manual is silent when both Options are on and keys overlap (e.g. `C` = synth C and 909 pattern 3).
- Focus arrows stop at the ends; both Options default off (Ctrl+G / Ctrl+F toggle); the focus bar is placed in the pattern section's right margin (p. 22 figure not measured).
- Taps and the other menu commands are decoded only: tap recording needs the live playhead (G6); menus are G8. The 909 Ctrl/Alt/Ctrl+Alt step-click modifiers (p. 30–31) belong to the mouse path.

## Proof and findings

- First automated INPUT proof on the lane. QEMU monitor `sendkey` reaches the active MUI window (remote mouse never did). The sequence `ctrl-g w 3 d x up kp_enter kp_add×3 ctrl-f up up ret×3 p` ends at `FOCUS 0 OPT PS PAT A3 A2 A3 A2 STEP 4 BPM 123 ST 1 … N 24 CLK 0`, with identical traces in two consecutive runs.
- Bugs found by the proof:
  - my focus-bar insertion captured the draw chain's final `else`, so non-pattern sections also got the 303 background (fixed);
  - the key-up of Ctrl+F decoded as a drum TAP_END (fixed, pinned in t70).
- Two earlier runs showed transport state changes that the key counter did not account for. These were probably canvas clicks, but the click counter was not traced yet, so this stays unresolved. A CLK field was added to the trace.
- Verification discipline: the live tree carried the song-track session's WIP (host build red in `engine/seq/songtrack.c`), so the full audit was run on HEAD + this slice in a scratch worktree. The worktree needs a sibling `Vulkan4Aros` symlink, because `ri_build_aros.sh` finds the toolchain via `../Vulkan4Aros`. Result: `AUDIT 0/0 PASS`. Only this slice's files were committed.
