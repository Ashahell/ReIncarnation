# Leviasynth fidelity P8c: device sequencer + step record (owner 2026-10-01)

- Source: ReIncarnation session, 2026-10-01 (opencode lane)
- Collected: 2026-10-01
- Published: 2026-10-01
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P8c plan + status, E0 ledger)
- Prior: [2026-10-01-leviasynth-fidelity-p8b-arp.md](2026-10-01-leviasynth-fidelity-p8b-arp.md)
- Commit: P8c (this session; unpushed at collection).

## Scope (P8 split; rest to follow)

- P8c (this slice): runtime tracks + playback + record + `DM_SEQ`. P8d: ribbon + step-LFO editor. No song-format change (persistence rides P10 owner review); measured gate + per-step UI editing deferred.

## P8c status: done (t143, 26 mutants)

- Runtime store (~3 KB): 2×128 steps (4 notes + vel/gate/trig/prob/drift/entropy) + 8×128 macro lanes. Transport (playing/tick/ppq) pushed synchronously by the live task next to tempo (additive engine fields); steps song-locked.
- Playback: parallel/series, division 1/2/4/8, swing, gate, prob threshold, drift (linear approx), trig sub-queue (64), entropy, transpose, macro-on-strike-only. Strike/release via the arp path; own gates; seek/restart/clear drain futures; schedule-once traversal (any granularity, hitch cap 16).
- Record: realtime crossed-steps (empty held = rests) + stopped cursor preview; step-default knobs feed per-step params; macro captured.
- 15 keys `0x0EAD..BB`, 15 rows, SEQ 2 pages, `DM_SEQ` (8).
- Proof: audit 0/0; riaudio SPACE→PLAY, 11322 buffers 0 xruns. Deviations: no host wav; SEQ pixels deferred; 909 pack missing.

## Method findings (portable)

- Test-harness time must respect the device contract: 128-tick advances per 256-sample block (100× over contract) starved the traversal, poisoned the queue model, and inverted failure analysis for a full round. Production uses ~1 tick/block. When behavior looks insane, check the harness clock first.
- Design for any granularity anyway: tick-span-derived reach + schedule-once + hitch cap made the device correct from 1 to 128+ ticks/block — and the coarse tests then cover a legit edge (big host buffers).
- Tick-keyed futures, never sample-keyed: absolute sample scheduling breaks under any tick/sample warp; due-ticks pop correctly at every granularity.
- Macro-follow must be gated on striking: rest steps stomping live knobs poisoned later records (found via step-content divergence: notes everywhere, macros only early). Rests observe, never write.
- Release tails (150 ms) saturate active-voice metrics: gate laws need instant release + accumulated active-blocks, and strict `>` beats ratio thresholds under saturation.
- Unrouted-modulator inaudibility (P8b lesson re-bit): LFO value range never pushed gate below legato — split route-fill (flag) from fold (pinned offsets) laws.
- Vacuous timing laws keep appearing: assert the discriminating checkpoint (`tr[b]`, deltas, counters), never the aftermath. Every timing law survived a disable-mutant check before counting.
