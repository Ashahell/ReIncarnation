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
- [ ] One control table as source; generate panels.c / set_param dispatch / guide rows / audit greps
- [ ] Fix 0x0305 volume-vs-wave; route 0x031x (303B); render.c forward full 0x03xx
- [ ] Add Tune + Waveform to panel

## §12.3 Integrated stereo engine skeleton (§5.1, §5.3)
- [ ] One `ri_engine_render` for CLI/export/AHI; fixtures on engine API; first-light goldens byte-identical or deliberately re-baselined

## §12.4 303 fidelity pass (§4.1)
- [ ] MEG/VEG split + accent-sweep state + accent→min-MEG-decay
- [ ] Gate-length rule + NOTE_OFF at gate fraction
- [ ] Log-domain slide; band-limited osc; 4-pole ladder option; reso taper; cutoff/envmod measurement anchors
- [ ] Per-parameter smoothers at control rate

## §12.5 808 rebuild (§4.2)
- [ ] 11 slots + 5 switches + MA; per-sound controls; metal oscillators; BD/SD models; accent level; choke rules; recursive envelopes

## §12.6 909 completion (§4.3)
- [ ] LT/MT/HT/RS/CP; analog-vs-ROM split; accent/flam decouple; per-voice decay/level; shared CH/OH level + rule; layer morph stays internal

## §12.7 Scheduler + RBNG v2 (§5.2, §7.1)
- [ ] 4 sections, pattern banks/lengths/shuffle flags; streaming emission; song track; RBNG chunk plan

## §12.8 FX routing + parity + PCF envelope (§3.2, §4.4)
- [ ] Routing matrix w/ exclusivity; delay steps/triplet/fb=1.0/pan/sustain; dist oversample + curve; comp ratio + GR meter; PCF envelope + Appendix-D patterns + transport lock; drop HP (→Power Mode)

## §12.9 Song mode + transport + pattern edits (§3.1)
- [ ] Transport state machine; song track + automation; edit ops as pure functions

## §12.10 GUI parity + skins + MIDI (§9, §3.1)
- [ ] Panel inventory; focus bar; meters; skins via MCC classes; knob modifiers; transport/playhead from audio clock (§2.9 last row); MIDI maps from manual

## §12.11 Perf pass vs W1.1 budget (§6)
- [ ] `tools/bench` per-section evidence; per-section budget; continuous gating before beta exit

## Done
- (none yet)
