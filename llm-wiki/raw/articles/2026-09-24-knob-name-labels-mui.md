# 2026-09-24 — Knob name labels in the MUI app (hardware silkscreen parity)

> Source: session work (MUI text row + device proof)
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New. Closes the last composition gap vs the Intuition proof
vehicle: names above, knobs middle, values below — the hardware
row order (silkscreen TUNE/LEVEL/DECAY/FLAMRES over each knob on
TR-909 and ReBirth alike).

## What
- `app/panel909.c`: name row (4× MUIC_Text, FixWidth 80 over each
  knob, same literals as knobproof). Layout is now three stacked
  horizontal groups (names/knobs/values), window content-sized.
- No new pure logic (static strings + MUI objects) → no new host
  test, same standing rule as all AROS shells/vehicles.
- AROS -Werror compile clean. Full `ri_audit.sh` 0/0. Binary
  deployed to RAM:.

## Proof (device, detached runs)
- Fresh window: TUNE/LEVEL/DECAY/FLAMRES legible over the knobs,
  64s below, no guru; old instance closed via gadget first.
- Spelling + column alignment eyeballed against knobproof.

## Files
- env: `app/panel909.c` (name row only)
- Dell scratch: `/home/miller/Work/ri_build/dell2/ri_panel909`
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`
",
