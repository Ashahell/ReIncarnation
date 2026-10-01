# Leviasynth fidelity P6c (stereo/scales/vintage) + P6d (8 voices): voice complete (owner 2026-09-30/10-01)

- Source: ReIncarnation sessions, 2026-09-30/10-01 (opencode lane; owner 8-voice + stereo calls 2026-09-30)
- Collected: 2026-10-01
- Published: 2026-10-01
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P6c plan + status, §P6d plan + status, E0 ledgers)
- Prior: [2026-09-30-leviasynth-fidelity-p6a-p6b.md](2026-09-30-leviasynth-fidelity-p6a-p6b.md)
- Commits: `da4c32b` (P6c), `662776e` (P6d). Not pushed at collection.

## P6c: stereo + scales/microtuning/vintage (`da4c32b`, t136, 19 mutants)

- Stereo render beside the frozen mono path: per-op pan = clamp(voice + (osc + matrix) × width + spread×ordinal) through BALANCE (linear, default, center l=r=1)/POWER (equal-power ri_sin)/WIDE (150% law); dual-mono filters (state parameterized, suite-verified identical); section-4 stereo branch (per-channel inserts, meter/send on mid, strip pan as balance). Center renders dual-mono bit-identical — t104 passes unchanged. P5b `DO_PAN` live. `voice_pass` takes optional R accumulation (NULL = legacy path exactly).
- 16 own scale masks (computed by script, not hand-rolled — mental math was wrong on 7 of 16) + nearest-degree quantize (ties lower) in trigger + legato retune; 8 own microtuning tables (`2^(c/1200)`); vintage bit-depth/SR-degrade post-gain with exact bypass (1-bit quanta law on a driven-hot voice: a ±0.6 voice sits honestly below the LSB).
- Keys `0x0E68..74` (13); rows 145..157; VOICE 4 pages (3: 8 OSCPAN, 4: VINTAGE/SCALE/MICRO/KEYLOCK/SPREAD + 3 dead reserved); `pan`/`width`/`mode`/`bend` matrix folds already existed.
- E0: SPREAD static per voice index; width 0 collapses; keylock off chromatic; micro table 0 inert; mono sum frozen as the bit-identity reference; `vint` field removed as write-only dead weight.
- Proof: audit 0/0 clean worktree, Dell 0 UND + 0 r12, ASan/UBSan clean; riaudio window opens, no crash (VOICE page pixels still deferred — tab nav needs focus).

## P6d: eight voices (`662776e`, t137, 7 mutants)

- One line (`RI_LEVI_NVOICES` 6→8); allocator/limits/ordinals were already macro-driven, no DSP edits. Pattern lanes stay 6 (songs bit-identical; lanes 6–7 live-only). New t137 (rotate fill + steal, unison 8, lane-8-refused, default limit 8); deliberate constant moves in t103/t108/t109/t129/t134. E0: stored ULIMIT values map higher under 8 voices (deliberate, minor).
- Proof: audit 0/0, Dell 0 UND, ASan clean; riaudio Levi tab live capture, PLAY/STOP ink, host wav peak 0.855, 0 xruns over 5199 buffers.
- The audit caught t129's bad-voice constant (missed by hand) — the clean-worktree audit as backstop, as designed.

## Method findings (portable)

- Pipeline-masked build failures: `build ... | tail` hides nonzero exits behind `tail`'s 0 — tests then run on STALE objects and pass vacuously. Check exit codes (`BUILD EXIT: $?`), never output tails. Same class as the stale-object trap, new face.
- Header mutants need full rebuilds: `mut.sh` rebuilds one module, so a header mutant links mixed-ABI objects (struct sizes differ) — result meaningless. Redo header mutants with `all` + test + restore + `all`.
- Magnitude mutants need magnitude laws: a "moves" law survives any nonzero change (micro `/1200`→`/120` survived; fixed with an exact 114-cent ratio law).
- Vacuous mutant application: `mut.sh` continues past a failed python assert (no `set -e`) and reports false SURVIVED. Verify counts on every surprise survival.
- Test-called vs test-covered: the keylock-clamp mutant survived because the test relied on the init default and never called the mutated setter — make the call explicit.
- Coupled behaviors need coupled mutants: pingpong reads+writes (either alone still crosses), tape wow+dark (either alone still differs), wet L+R (law read R only). Reason per case; don't assume single-line coverage.
- Computed > hand-rolled constants (scale masks); generated-and-checked UI maps (vintage bits/dec verified by the quanta law, not by reading).
