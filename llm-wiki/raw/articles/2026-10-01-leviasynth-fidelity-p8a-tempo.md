# Leviasynth fidelity P8a: device tempo + BPM sync, P7d closed (owner 2026-10-01)

- Source: ReIncarnation session, 2026-10-01 (opencode lane)
- Collected: 2026-10-01
- Published: 2026-10-01
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P8a plan + status, E0 ledger)
- Prior: [2026-10-01-leviasynth-fidelity-p7c-modfx.md](2026-10-01-leviasynth-fidelity-p7c-modfx.md)
- Commit: P8a (this session; unpushed at collection).

## Scope (P8 split; rest to follow)

- P8a (this slice): device tempo + LFO/ENV/delay BPM sync (closes P7d). P8b: arp parameter set + phrases. P8c: sequencer tracks + step record. P8d: ribbon + step-LFO editor.

## P8a status: done (t141, 17 mutants)

- Device tempo cache (`levi_set_tempo`, clamped 20..500, default 140 = engine default), pushed per block by the engine Levi section render (one line, no struct change). MIDI-clock derivation stays in the interop lane.
- Unconditional per-render refresh of flagged units from retained UI (no dirty flag — knob edits on flagged units apply instantly; ~600 branches/block worst case). Pure helpers: `ri_levi_beats_time` (0..4 beats → seconds), `ri_levi_lfo_sync_hz` (4 beats..1/32 per cycle), `ri_levi_delay_snap` (idempotent straight-16th snap, 1 ms..2 s).
- 13 ENV BPM keys `0x0E93..9F` (per-op/per-menv, section-wide, no registry rows — dynamic `SLOT_OPBPM` (-48, param 32) + `SLOT_MEBPM` (-150, free gap) across 7 slot consumers); LFO BPM slot bound to the existing 0x10 key; SYNC/FREE texts.
- E0: flags off = bit-identical; ui=0 = instant; delay snap stepped; speed range irrelevant when synced.
- Proof: audit 0/0; riaudio SPACE→PLAY, 11882 buffers 0 xruns. Deviations: no host wav; page pixels deferred; 909 pack missing.
- Adjacent: t77 page-slot ownership rule for rowless keys (0x0E93..9F, same pattern as 0x0F/0x10 blocks).

## Method findings (portable)

- Test-buffer sizing is a correctness issue, not hygiene: 48000-sample renders into 4800-float buffers overflow silently into adjacent statics and fail unrelated asserts (`aggressive-loop-optimizations` caught it at build). Size render buffers from the sample count at declaration; name long buffers distinctly (xa/ya).
- `feq` must be inf-safe: `inf <= inf` is true, so a divide-by-zero mutant passes a naive relative comparison. Exact-equality fast path + finite-magnitude gate.
- Unrouted modulators are inaudible: an LFO rate law with no matrix destination passes vacuously both ways. Every "X moves output" law needs an audibility path (here LFO0→cutoff); audit new laws for one.
- Mutants that narrow a range survive when tests touch only index 0: always exercise range ends (OPBPM7/MEBPM4) for id-range decoders.
- Engine-seam lines (tempo push) have no unit law by construction — verify the survival explicitly (remove → green) and ledger as lane-covered rather than assuming.
