# 2026-09-24 — Notify wiring + value readouts: knob values go somewhere

> Source: session work (TDD pins + AROS app wiring + device proof)
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New. First notify wiring (values finally leave the knobs) + live
readout row; makes right-click/fine device-verifiable by number.

## What
- `gui/panels.[ch]`: `ri_ctl_format_value(buf, v)` — 0..127 as
  decimal (hand-rolled, no libc), out-of-range fail-closed to
  `"---"`, NULL ignored. Pinned in t29_paneldefault (0/64/100/127
  exact, -1/200 → "---"); RED was implicit-decl, GREEN minimal.
- `app/panel909.c`: text row (4× MUIC_Text, FixWidth 80 under each
  knob); every knob `MUIM_Notify`s `MUIA_Numeric_Value`/`EveryTime`
  back to the app (IDs 100+i); loop `GetAttr`s the value and
  refreshes that cell. Window grows a row (content-sized, as ever).
- AROS -Werror compile clean. Full `ri_audit.sh` 0/0. Binary
  deployed to RAM:.
- Process note (third strike, now a rule): the AROS lane links
  CACHED `.o` — a new symbol in `panels.c`/`knob_logic.c` fails at
  link with `U <sym>` until deps rebuild. Always rebuild all lane
  objects, never just the edited TU. (Host lane: `all` before
  `test`, same class.)

## Proof (device, detached runs)
- Fresh window shows "64 64 64 64" under the knobs.
- Drag a knob: its number tracks live.
- Right-click LEVEL: readout jumps to 100 (TC-2.9.2 by number).
- No guru, no crash; old instance closed via gadget first.

## Files
- env: `gui/panels.[ch]`, `tests/unit/t29_paneldefault.c`,
  `app/panel909.c`
- Dell scratch: `/home/miller/Work/ri_build/dell2/ri_panel909`
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`
",
