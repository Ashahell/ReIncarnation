# Keyboard + focus (§12.10 G5) — ledger

**Status:** implemented and proven with real key events on riqemu1 2026-09-25.
**Source (E1):** ReBirth RB-338 2.0.1 Owner's Manual: Appendix E p. 221–226, and p. 18–22, 30–33, 43–44.
**Code:** `gui/keymap.{h,c}` (decode, t70), `gui/panelui.{h,c}` (focus + dispatch, t71), `gui/secttr.c` (`ri_str_goto_loop`), `gui/widgets/rsection.mcc.c` (focus bar, click-to-focus, raw keys), `gui/panelgeo.c` (`ri_geo_focus_bar`).

## Key map (E1)

| Group | Keys | Manual |
|-------|------|--------|
| Pattern select (Options "Select Patterns from Keyboard") | Synth 1 `1`–`8`, Synth 2 `Q`–`I`, 808 `A`–`K`, 909 `Z`–`,`; pattern 1–8 of the current bank; focus follows | p. 20, 22, 224 |
| Transport (keypad, PC column) | `0` Stop, Enter Play, space Stop/Play, `*` Record, `4` Fast Forward, `5` Rewind, `1` Loop Start, `2` Loop End, `8` Next Bar, `7` Previous Bar, `+`/`-` Tempo | p. 224 |
| Focus | Up / Down arrows | p. 22 |
| Synth programming (Options "Program Synth from Keyboard", focused synth) | pitch C C# D D# E F F# G G# A A# B C = `C F V G B N J M K , L . /`; Return Step, Backspace Back, `-` Note/Pause, `P` Accent, `[` Slide, `;` Octave Down, `'` Octave Up; Tab tap, Shift+Tab delete | p. 43–44, 225 |
| Drum tap (same option, focused rhythm section) | `-` AC, `A S D F G H J K L ; '` = instruments 1–11 in Instrument Selection order; Shift = delete | p. 32, 226 |
| Menu shortcuts | Ctrl + N O W S I Q X C V L T J K R Y U M F G | p. 222–223 |

Keys are PHYSICAL positions (p. 224), so the map is keyed by Amiga raw key codes (positional by design).

## Decisions

- **Precedence:**
  1. menu chord;
  2. keypad transport;
  3. focus arrows;
  4. programming keys of the focused section (when "Program Synth" is on);
  5. pattern keys (when "Select Patterns" is on).

  The manual doesn't say which wins when both options are on and the keys overlap. For example, `C` is the synth's C pitch key and also 909 pattern 3; `S` is the 808 SD tap key and also 808 pattern 2.

  **DECIDED 2026-09-25** (owner delegated: "do what you would recommend or what would happen with real hardware"). Programming keys of the focused section win the overlap, and pattern keys keep working everywhere else.
  - **Hardware:** on the originals one physical key has one job at a time, set by the mode. On the TB-303 the note keys double as the pattern-select keys, and the pattern-write/pattern-play mode decides. On the TR-808 and TR-909 the step keys program in write mode and select patterns in play mode. Having "Program Synth from Keyboard" on is the software's write mode, so a key that programs the focused section must program it and not also switch a pattern.
  - **ReBirth:** it shows the two options as independent Options-menu checkmarks (p. 179, 224). So keys that do only one job keep it: the digit row still selects Synth 1 patterns while you program Synth 2.
  - **Alternative rejected:** making the two options mutually exclusive, like a mode switch. That is closer to the hardware, but it would change ReBirth's own menu behaviour, which ranks above the hardware in this project's fidelity order.
  - **Pinned in t70:** the `C` and `S` precedence cases.

Still E0 (the owner may overrule these):

- **Menu modifier:** `Ctrl` (ReBirth for Windows) and Right-Amiga (the AROS menu-shortcut key) both work.
- **Focus arrows stop at the first/last section** (no wrap-around); the manual only says "up and down arrow keys".
- **Focus bar position:** the right margin of each Pattern section, at Q (272, 232), 10 × 420 — the p. 22 figure was not measured. It is not a control: clicks there hit nothing.
- **Both options default OFF**, like unticked Options-menu items; they are toggled with Ctrl+G / Ctrl+F.
- **Taps and the other menu commands are decoded, not applied yet.** Recording a tap at the playhead needs the live audio clock (G6); the menu system is G8.
- 909 Ctrl / Alt / Ctrl+Alt click modifiers (p. 30–31, 226) belong to mouse clicks on steps: not in this slice.

## Proof on riqemu1 (real key events)

`RISECT keys` shows the Transport, the four Pattern sections and Synth 1 on one front panel. Keys were typed through the QEMU monitor's `sendkey`, which delivers real PS/2 keystrokes: remote key injection works where remote mouse injection never did. The sequence was `ctrl-g w 3 d x up kp_enter kp_add×3 ctrl-f up up ret×3 p`. Final readout: `FOCUS 0 OPT PS PAT A3 A2 A3 A2 STEP 4 BPM 123 ST 1 LAST 4/22 N 24 … CLK 0`. The orange focus bar sits on Synth 1, EDIT STEP 04, and the Accent LED is lit.

Evidence: capture `img/2026-09-25-rikeys-sendkey.png`, and the per-change trace (raw code / qualifier per step) in `2026-09-25-rikeys-trace.txt`. Two consecutive runs gave identical traces with CLK 0 (no mouse button reached the transport canvas).

Two findings from the proof runs:
1. **Spurious TAP_END (fixed and pinned):** the key-up of Ctrl+F decoded as a drum TAP_END. A pin in t70 now covers it.
2. **Stray transport changes (unresolved):** two earlier runs showed transport changes that no key accounted for — the change counter did not move. The click counter was not traced yet at that point. The CLK field was added to catch this if it recurs.

## ReBirth-101 key runs (2026-09-26)

22-key run (`ctrl-f`, 13 pitches, `ctrl-g`, `kp_enter`, `3`, `kp_0`):
pitches decoded but EDIT STEP frozen — pitch keys advance only in pitch
mode (mouse-only switch), a Step-mode rerun was needed. Trace
`2026-09-26-rb101-keys-patswitch.txt` still proved row 5 (PAT A1→A3 at
ST 1, N20→N22).
33-key run (16× pitch + `ret` in Step mode, `[` slide on step 5, `p`
accent on step 9): trace `2026-09-26-rb101-303-steps.txt` (EDIT STEP
1→16) + capture `img/2026-09-26-rb101-303-steps.png` (EDIT STEP 16).

Finding: **`sendkey comma` mis-delivers.** In the first 22-key run the
`,` keystroke arrived as raw `0x37` (M) instead of `0x38` (comma position);
`.` (0x39) and `/` (0x3A) in the same run arrived correctly, as did every
letter. The 33-key rerun avoids `,` (12 distinct pitches + repeats).
Uncharacterized whether the fault is QEMU's name table or the guest
keymap — one deliberate probe would settle it, not done.
