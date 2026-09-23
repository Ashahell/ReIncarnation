# 2026-09-23 — WBS 2.5 FX acceptance: TC-2.5.1–2.5.5 pins (M2.4 opens, no production change)

> Source: session evidence (test output ×6, audit 0/0 ×2, golden re-render compare), compiled by agent
> Collected: 2026-09-23
> Published: 2026-09-23

## Disposition
New. M2.4 opens with 2.5 FX (unblocks 2.6 mixer). Gap analysis of
TC-2.5.1–2.5.5 against landed Task 10 code (`fx.c`, `pcf.c`, `t1_fx`,
Phase 10) found no unwired surface (unlike 2.3/2.4's panel gaps —
FX owns its `RI_FXID_*` block through the `RiFX*` wrapper; there is
no FX panel yet, and FX GUI is 2.11). The slice is five per-TC
contract pins (`t25_pcfcutoff/delay/dist/swap/pcfopen`, wired in
`ri_audit.sh` Phase 10 after `t1_fx`) + ledger lock. One pin caught
a real bug pre-GREEN — in the test, not the engine (below).

## What
- `t25_pcfcutoff` (TC-2.5.2): 10 ledger rows ±2 cents (mirrors
  t1_fx §1) + white-box edges beyond t1 — v=64 unity exact,
  v=0/amt+4 = 1/16 (measured 0.0625), clamp rails.
- `t25_delay` (TC-2.5.3): 3-BPM sync <0.1% + echo exactly on
  delay_smp (mirrors t1_fx §3 values: 0% / 0.0028% / 0.0017%) +
  TC-literal 5-minute no-drift beyond t1 — beat-locked impulse
  train, fb=0, full wet, 14,400,000 samples bit-exact end to end
  (chunked 48k windows with input-tail overlap; whole test 0.119 s).
- `t25_dist` (TC-2.5.4): unity ±0.2 dB + DC-normalization exactness
  (full-scale DC 1.0 → 1.0 within 1e-6 on a 3×3 drive×shape grid —
  the header's stated mechanism, unpinned until now) + monotonic
  loudness growth over the drive sweep (TC-literal, not in t1) +
  engaged-differs + grid finite.
- `t25_swap` (TC-2.5.5): mid-stream dist↔pcf edge energy ≤ 64.0
  (measured 0.8101, ledger's 0.81).
- `t25_pcfopen` (TC-2.5.1 status pin): LOCKS THE REFUSAL — 54×16 all
  neutral 64, count constants, loader double-load identical. This
  pin passing is NOT TC-2.5.1 coverage (needs the 54 black-box
  captures); a future capture landing must update the pin
  deliberately.
- `docs/evidence/pcf/engine.md` → LOCKED at TC-2.5.2/2.5.3/2.5.4/
  2.5.5 with t25 citations; TC-2.5.1 stays OPEN-04 explicitly.
- `scripts/ri_audit.sh` Phase 10: five `test t25_*` lines after the
  `t1_fx` line, TC-tagged FAIL echoes (the refusal pin's tag reads
  TC-2.5.1-OPEN so the gate never masquerades as pattern coverage).

## Proof
- TDD: pins written first; four went green immediately (property
  pins, t22 precedent). The fifth caught a REAL bug pre-GREEN —
  mine, not the engine's: `memcmp` over two `PCFTable` loads
  differed because the loader writes rows[0..n)+n only and my
  structs were uninitialized stack (tail = my garbage, not engine
  state). Suspect-the-assertion-first held: memset both, pin
  green. Production code untouched by the whole slice.
- GREEN all five, 0 FAILs. Full `ri_audit.sh` 0/0 TWICE
  (pre-change baseline + final tree, 0 FAIL lines); PCF goldens
  re-render identical (no production change, nothing to regen).
- Audit-grep strings (`P-15`, `OPEN-04`) verified intact post-edit.

## Files
- tests: `tests/unit/t25_pcfcutoff.c` `t25_delay.c` `t25_dist.c`
  `t25_swap.c` `t25_pcfopen.c` (new; no `engine/` file touched)
- env: `scripts/ri_audit.sh` (Phase 10 t25 lines)
- ledgers: `docs/evidence/pcf/engine.md` → LOCKED (scoped)
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

No external websites consulted: contracts are WBS TC-2.5.1–2.5.5 +
spec §12/§13 + ledger P-15 — all local and authoritative; nothing
was open that needed the web. spirv-val vacuous (no SPIR-V in this
repo — noted honestly per standing).
