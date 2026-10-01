# Leviasynth fidelity P7c: pre/post 9-type mod engines (owner 2026-10-01)

- Source: ReIncarnation session, 2026-10-01 (opencode lane)
- Collected: 2026-10-01
- Published: 2026-10-01
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P7c plan + status, E0 ledger)
- Prior: [2026-10-01-leviasynth-fidelity-p7b-reverb.md](2026-10-01-leviasynth-fidelity-p7b-reverb.md)
- Commit: P7c (this session; unpushed at collection).

## Scope (P7 split; rest to follow)

- P7c (this slice): pre/post 9-type engines + P5b FX matrix destinations. P7d: BPM-sync wiring (needs the P8 clock).

## P7c status: done (t140, 27 mutants)

- One shared 9-type mod engine (`RILeviMod`), two slots in `RILeviFx` (pre before delay, post after reverb, inside `sum_stereo` — shared `engine/fx` + mixer untouched, t51 green). Own algorithms: chorus (20 ms modulated delay, R anti-phase), flanger (3 ms + fixed 0.55 loop), rotary (dual-rate tremolo + cross-pan impression), phaser (4 swept allpass stages), lo-fi (bitcrush 16→4 + decim hold), tremolo (sine AM), EQ (one-pole bass/treble shelves ±12 dB), compressor (peak follower 10/100 ms + set-time makeup), distort (asymmetric soft clip + tone LP). Static lines (50 ms mod line shared), cleared in `levi_fx_clear`, comp gains init to unity.
- 4 own factory tuples per type (`[9][4]` of p1/p2/drywet); PRESET writes the knobs; pure accessor `ri_levi_mod_preset` (t140 pins knob-write + factory content).
- `DM_PREFX` (31) / `DM_POSTFX` (32): P1, P2, DRYWET (prompt list exactly), lead-voice driven, ±0.5 span on params.
- Keys `0x0E87..92` (14); FXPRE (row 64) / FXPOST (row 67) rebound as panel-ON/engine-bypass (P7a/b pattern); rows 173..182; PREFX/POSTFX pages live except PARAM 3–5 (dead reserved — prompt wants Param 1/2); per-type P1/P2 value texts (Hz/dB/bits/X:1/%); FXPRE/FXPOST MODE-style press toggles.
- E0: bypassed by default (songs bit-identical); flanger fb fixed 0.55; rates in Hz (no BPM flags); presets are positions 1–4; FTZ on + per-loop flushes.
- Proof: audit 0/0 clean worktree; riaudio: window opens, SPACE→TR PLAY inks, AHI 0x003e0001, buffers=10943 xruns=0. Deviations: no host wav; mod page pixels deferred (tab-focus gap); lane 909 pack missing.
- Adjacent fix: t107 comment (FXPRE/FXPOST now bound toggles; FXDLY/FXREV press-dead since P7a — pre-existing, noted not fixed).

## Method findings (portable)

- Sweep laws beat spot laws: "P1 moves on chorus" left 8 types' internals unpinned; "every knob moves every type" (18 renders, cheap) kills all internal-constant mutants mechanically. Write sweep laws first for parametric engines.
- Whole-case-dry mutants need the `+ 0.0f * var` idiom to stay compilable under `-Werror` (set-but-unused locals fail the build and report false PASS via stale objects — the documented trap, hit 3 times this slice).
- Mutants that preserve the behavior class survive honestly: flanger-no-fb is still a modulated short delay (audible, sweeps move). Ledger E0 constants as benign survivals; don't contort laws to pin every constant (preset table content IS pinned — factory data, different category).
- Delay-time/window trap again (P7a finding re-bit): full-wet 153 ms delay in a 100 ms window renders priming zeros on both sides. Chain-order tests need short delays (11 ms) or long windows.
- `evlog build=` is the worktree HEAD hash, not a content hash — on-target binary identity rests on byte size + fresh single-process logs, stated every round.
