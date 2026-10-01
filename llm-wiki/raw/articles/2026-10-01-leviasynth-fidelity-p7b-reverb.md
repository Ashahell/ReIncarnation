# Leviasynth fidelity P7b: device reverb types + freeze (owner 2026-10-01)

- Source: ReIncarnation session, 2026-10-01 (opencode lane)
- Collected: 2026-10-01
- Published: 2026-10-01
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P7b plan + status, E0 ledger)
- Prior: [2026-10-01-leviasynth-fidelity-p7a-delay.md](2026-10-01-leviasynth-fidelity-p7a-delay.md)
- Commit: P7b (this session; unpushed at collection).

## Scope (P7 split; rest to follow)

- P7b (this slice): reverb types + freeze. P7c: pre/post 9-type engines. P7d: BPM-sync wiring (needs the P8 clock).

## P7b status: done (t139, 21 mutants)

- Reuses the v2 core (`ri_reverb_*`, untouched — t123 pins it): two instances (L/R) with static backing in `RILeviFx` (6 lines × 2048 × 2ch); own type tunings (ROOM/HALL/PLATE/CHAMBER — authored tap sets, never the core's); own predelay lines (250 ms max, static).
- Params: type, predelay 0..250 ms (0 = dry straight in), time→comb fb 0..0.95, tone (wet LP), hi-damp (2nd wet LP), lo-damp (wet HP — own interpretation, no loop hooks in the shared core), dry/wet, freeze (held wet-mix drone over live dry; core + predelay untouched so tails resume on release), bypass (exact dry).
- Chain: delay → reverb → out. `DM_REVERB` (30): TIME, TONE, HIDAMP, LODAMP, DRYWET (prompt list exactly), lead voice drives the device.
- Keys `0x0E7E..86` (9: RTYPE/RPREDLY/RTIME/RTONE/RHIDAMP/RLODAMP/RDRYWET/RFREEZE + RBYPASS on the FXREV row, same single-truth pattern as FXDLY). Rows 165..172; REVERB pages 1/2 (all 8 live) + 2/2 (FREEZE + 7 dead reserved).
- Build lists: `engine/fx/reverb.c` added to AROS sections + riapp lists and `build/portable.mk` (host already had it) — the audit caught the missing link (undefined `ri_reverb_*` in RISECT), not me.
- E0: bypassed by default (songs bit-identical); damps are wet-EQ; freeze holds bounded content; predelay line keeps shifting under freeze; FTZ globally on.
- Proof: audit 0/0 clean worktree; riaudio (Dell still crash-loops everything): window opens, SPACE→TR PLAY inks, AHI mode 0x003e0001, buffers=6184 xruns=0. Deviations: no host wav this round (lane QEMU writes no wav file); REVERB page pixels deferred (standing tab-nav-focus gap); 909 pack missing on lane (renders silence, unrelated).

## Method findings (portable)

- Pointers in DSP state break whole-struct memcmp tests: the reverb core's line pointers are absolute per instance, so t79/t116 twin-engine comparisons differ in pointer bytes. Fixed with hole-aware comparison (exempt pointer words, pin handle scalars) — the second such test to need it is the signal the design smells, but offsets would touch the pinned core, so the exemption is the honest shape. New rule: DSP state with embedded pointers needs a documented comparison helper, not raw memcmp.
- UI-door clamps hide DSP-layer mutants: `rtime` clamped at the set-param door means the DSP `0.95` clamp mutant survives through UI paths. Ledger such survivals as defense-in-depth (matrix offsets can still push past the door), don't chase white-box laws for every second layer.
- 1-ulp pass is not a law: the lo-damp mutant survived because the test compared `>` on brightness ratios where the tone-door float rounding alone gives 1 ulp. True effect was 1.77× — laws need margins (`> 1.2×`), not bare inequalities, wherever float plumbing sits between the knob and the meter.
- Retrigger history leaks into comparative laws: envelope/LFO state carries across renders in reused sets, so A/B comparisons need fresh sets both sides (the hi-damp law failed until both sides were fresh-inited).
- Lane hygiene with overlapping processes: two RIAPP instances share `RAM:*.LOG` names and both appear in status. Break-all + delete logs + redeploy + single-process status before collecting, or the proof is ambiguous. `build=` in evlog is the worktree HEAD hash, not a content hash — byte size + fresh single-process logs are the identity check.
- Transport keys need press AND release (`CODE` then `CODE|0x80`); SPACE (0x40) toggles play. The old 0x63/0x03 sequence never drove transport — P7a's PLAY ink came from elsewhere.
