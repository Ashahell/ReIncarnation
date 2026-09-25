# Improvement-doc execution tracker

Source: `docs/2026-09-24-improvement-opportunities.md` (§12 order, Terry rule: one thing at a time).
Rules: TDD RED-first per change; `ri_audit.sh` 0/0 before each commit; feat:/docs: pairs on `main`; wiki record per slice; scratch in `/home/miller/Work/ri_build/`.

## Adoption gate (normative first)
- [x] Adopt §11 decisions D-a..D-j into spec `docs/superpowers/specs/2026-09-20-reincarnation-spec.md` (doc is non-normative until adopted)

## §12.1 Kernel totality + 808 deactivation + clip removal (§2.1, §2.3, §2.4)
- [x] `ri_exp` total over finite floats (0 for x ≤ −87, reduction |k|≤128, +Inf above 88.72283) + property test full-range (t3)
- [x] `ri_sin` domain safety (int64 reduction to |x|<5.7e19, bounded 0 beyond) + `ri_pow2` total (±127/128)
- [x] In-domain paths bit-identical (t2 green + 303 first-light re-render cmp IDENTICAL)
- [x] Long-silence regression test (t33; tau-scaled 3 s ≡ 30 s in t/tau coverage, stated in-test)
- [x] 808 voice deactivation at −100 dBFS (t35; quiet-at-death + exact-0-after)
- [x] 808 section-sum linear (t34 storm-vs-solo-sums; 909 clip deferred to §12.6)
- [x] Deliberate 808 golden re-baseline (bd/lt/mt/ht/oh/storm, each root-caused) + ledger notes
- HELD: per-sample multiplicative envelopes SKIPPED — deactivation bounds t < ~50 s always
  (max tau 4.0 → death at 46 s; float t exact there); absolute-time total exp is
  overflow-proof; cost goes to the §12.11 bench pass with numbers, not guesses.
- HELD: `ri_scale2` keeps the 32-iteration loop for |k|≤32 (wide bound totality-only) —
  inflating it cost 2.4× storm CPU (t1_808/t21/t23 budgets RED); caught by suite, fixed.

## §12.2 Control registry + 303 ID fix + 303B dispatch + Tune (§2.2)
- [x] Fix 0x0305 volume-vs-wave (panel row renamed waveform@0x0305 def 0; volume moves to 0x0306 def 100; guide corrected, audit strings kept)
- [x] Route 0x031x via shared dispatch (set_param normalizes block; twin-voice behavior test t36)
- [x] Add Tune (0x0307/0x0317, ±24 st, live ratio bend, pitch-exact) + Waveform to panel (both sections)
- [x] Panel/engine contract test-enforced (all 16 IDs reachable from panels; rows whitelisted to handled map; defaults pinned)
- [x] 303 goldens byte-identical (tune 0 = ×1.0 exact); render-path 303B voice scoped to §12.3 (comment in render.c, no mistarget)
- HELD: full single-source codegen (generate panels/guide/greps from one table) —
  engine IDs canonical in rb303.h, panel uses BASE+offset, contract pinned by t36;
  codegen when a third consumer appears (YAGNI).

## §12.3 Integrated stereo engine skeleton (§5.1, §5.3)
- [x] One `ri_engine_render` (stereo) + `ri_engine_render_mono` shared by CLI/export/AHI sinks
- [x] Event routing unified (3 per-path copies retired; device/block routing, unknown ignored)
- [x] render_song + render_rbngsong + au_render_frames rewired; first-light goldens byte-identical
- [x] Audit I1 tripwire retired → wired-core gate (engine call required in audio.c)
- [x] t6 file-vs-live IDENTICAL through the shared core (stronger than before)
- NOTE: audit Phase 0a greps `realloc` — the word "preallocated" in a comment
  failed the gate; reworded ("storage lives here"). Comment diction is load-bearing.

## §12.4 303 fidelity pass (§4.1)
- [x] Gate-length rule (D-h E0: half-step fall, ties hold) + t38 + ledger + re-baselines (m48)
- [x] MEG/VEG split + accent-sweep state + accent→min-MEG-decay (E1 lineage; t39; revoiced goldens) (m49)
- [x] Log-domain slide (τ untouched — OPEN-01; ri_log2 kernel; t40; glide re-baselines) (m50)
- HELD for measurement per review: band-limited osc, 4-pole ladder option, reso taper,
  cutoff/envmod anchors (no ears/captures available; §8 plan stands)
- [ ] Per-parameter smoothers at control rate

## §12.5 808 rebuild (§4.2)
- [x] 11 slots + 5 switches (last-wins) + MA + per-sound Level + accent-level law + LEVEL/ACCENT knob wiring (m51)
- [x] Metal fixed oscillators + choke rules + per-sound Tune/Decay/Snappy/Tone wiring (m52)
- [x] BD 808 regime (62 Hz / 4 ms sigh, paper-read) + SD structural verification + recursion held (m53)

## §12.6 909 completion (§4.3)
- [x] LT/MT/HT/RS/CP as sample-layer voices + per-voice Decay/Level (m54)
- [x] Flam bit/width decouple (compat kept) + shared CH/OH level + OH-wins rule + 909 clip removal (m55)

## §12.7 Scheduler + RBNG v2 (§5.2, §7.1) — DEFERRED after §12.8 (FX first:
  self-contained, no format surgery; rack iteration builds on the §12.3
  section mask meanwhile)
- [ ] 4 sections, pattern banks/lengths/shuffle flags; streaming emission; song track; RBNG chunk plan

## §12.8 FX routing + parity + PCF envelope (§3.2, §4.4)
- [x] Delay beats-honoring + caller-owned lines + tap slew + live-rate comp + pool retired (m56)
- [x] Delay Steps/triplet/fb-infinite/sustain-structure (m57)
- [x] PCF envelope + integer clock + Decay knob + HP drop, patterns open (m58)
- [x] Delay pan + stereo return + routing matrix/exclusivity (m62)
- [x] Appendix-D patterns (55 E1 rows + wrap + resolution wired) (m59)
- [x] Dist 2x oversample + comp ratio/GR meter (m61)

## §12.9 Song mode + transport + pattern edits (§3.1)
- [ ] Transport state machine; song track + automation; edit ops as pure functions

## §12.10 GUI parity + skins + MIDI (§9, §3.1)
- [ ] Panel inventory; focus bar; meters; skins via MCC classes; knob modifiers; transport/playhead from audio clock (§2.9 last row); MIDI maps from manual

## §12.11 Perf pass vs W1.1 budget (§6)
- [ ] `tools/bench` per-section evidence; per-section budget; continuous gating before beta exit

## Done
- (none yet)
