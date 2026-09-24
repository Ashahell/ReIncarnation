# 2026-09-24 — 2.10 step-toggle proof: 16 buttons + pattern readout

> Source: session work (proof vehicle + device proof)
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New. First 2.10 on-device slice: click-toggle proven by number.
Chase/LED timing (≤33 ms, 174 BPM) stays host-pinned; live chase
needs a beat clock (no source in tree yet — next slice).

## What
- `app/stepproof.c` (new, AROS-only): 16 RStp buttons in a row +
  pattern readout (16-bit state as decimal via the m31-pinned
  `ri_ctl_format_count` — full 0..65535 range reused, no new
  helper). Any button notify → re-read all sixteen → refresh.
- No new pure logic (bit-OR aggregation inline; toggle rule
  pinned in t1) → no new host test, standing rule.
- Audit Phase 12 gates stepproof.c now.
- AROS -Werror compile clean. Full `ri_audit.sh` 0/0. Binary
  deployed to RAM:.

## Proof (device, detached runs)
- Fresh RI-STEPS window: readout 0, no guru.
- Click step 0 → button depresses, readout 1.
- Click step 3 → readout 9 (bits 0+3).
- Click step 0 again → back to 8 (toggle off reads back).
- Old instance discipline (gadget close) as ever.

## Files
- env: `app/stepproof.c` (new), `scripts/ri_audit.sh` (gate it)
- Dell scratch: `/home/miller/Work/ri_build/dell2/ri_stepproof`
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`
",
