# ExAll fix, lane acceptance numbers, key forensics (S6/S7 close-out)

- Source: Dell lane sessions e6320/e6320x, probe/DIRWALK logs, captures, evlog, commits `e262a34`, `b3f7091`, v4 `77b1b25a`, 2026-09-29
- Collected: 2026-09-29
- Published: 2026-09-29
- Related: `2026-09-29-s6-closed-s7-proven.md`, `2026-09-29-zoom-guard-proof-spooler-fix.md`
- Evidence: `docs/evidence/gui/hw-look/` (`dell-s7-808ri-*.png`, README S6/S7 section)

## ExAll returns FALSE with entries pending (PAL fix `e262a34`)

- Dell DIRWALK trace on the 3-entry Mods dir: `EXALL 0 3` then `IOERR 0` — FALSE means "nothing more follows", not "nothing returned". Both walkers broke on the return code; loop-shape and buffer-alignment theories were killed on device first.
- Fix: process entries BEFORE testing the return (sectproof's walker already had the shape). Proof rerun: `ENTRY 808-RI/Template/Stale, RC 0 N 3`; RIAPP logs `installed=4`.
- Adjacent: ULONG-aligned ExAll buffer kept (harmless); trailing slash on Lock fine either way.

## Acceptance numbers (all evlog-pinned, no owner input)

- Tabs switch (`TAB page=1`, Mix→Synths single click); first click on an inactive window only activates — proof jobs double-click. Evlog-grounded tab x-map: 70/130/194/250.
- Rail power toggles 303B (`VIS dev=1 show=0 mask=1d` / `show=1 mask=1f`).
- Knob drag emits `CTL 0104=0 → 25`.
- Rendering: whole-panel 808-RI check-build (uncommitted) — dark strips + amber headers where bound; Classic Levi/MASTER and Classic 909 beside them (per-part fallback visible).

## Key-forensics lessons (shared keyboard with the owner present)

- Held keys repeat (~3 Hz): a press without a prompt release cycles Ctrl+M hundreds of times (1789 sync lines observed). Lane key jobs must press+release back-to-back.
- Owner input interleaves invisibly: a whole-panel Classic reset in the log matched no lane keypress (Shift-shaped event). Attribute only what the evlog pins to your own job's window.
- Probe correction: `0x08` is `MOUSEBUTTONS`, `0x40000` is `ACTIVEWINDOW` — earlier "code 0 button event" reads were activation events; the old agent's clicks never arrived at all.
- Release proof shape: press `0x68` + release `0xE8` at targeted coords, ~10 µs apart, is a healthy injected click.
