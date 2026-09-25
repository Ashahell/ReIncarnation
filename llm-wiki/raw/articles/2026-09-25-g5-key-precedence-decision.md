# G5 key precedence decided: the focused section's programming keys win overlaps

- Source: ReIncarnation commits `4849786`, `8b62e81` — `docs/evidence/gui/keyboard.md` §Decisions, `gui/keymap.h`
- Collected: 2026-09-25
- Published: 2026-09-25

## Decision

The owner delegated the choice ("do what you would recommend or what would happen with real hardware"). When both Options "Program Synth from Keyboard" and "Select Patterns from Keyboard" are on and a key has two jobs, the programming key of the focused section wins. Examples: `C` = synth pitch C vs 909 pattern 3; `S` = 808 SD tap vs 808 pattern 2. Pattern keys keep working wherever they do not overlap; for example the digit row still selects Synth 1 patterns while Synth 2 is being programmed.

## Rationale

- Hardware: one physical key has one job at a time, set by the mode. The TB-303's note keys double as its pattern-select keys, and its write/play mode decides which job they do. The TR-808/909 step keys likewise program in write mode and select patterns in play mode. "Program Synth from Keyboard" is the software's write mode.
- ReBirth: it presents the two options as independent Options-menu checkmarks (manual p. 179, 224), so non-overlapping keys keep both jobs.
- Rejected: making the options mutually exclusive, like a mode switch. That is closer to the hardware, but it changes ReBirth's own menu behaviour, which ranks higher in the fidelity order.
- The manual does not settle the overlap, and no online source was found that does. Pinned by the t70 precedence cases.

Still E0 (owner may overrule): Ctrl + Right-Amiga menu modifier, focus arrows stop at the ends, both options default off, focus bar position.
