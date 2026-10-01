# Leviasynth fidelity P8b: device arp params + phrases (owner 2026-10-01)

- Source: ReIncarnation session, 2026-10-01 (opencode lane)
- Collected: 2026-10-01
- Published: 2026-10-01
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P8b plan + status, E0 ledger)
- Prior: [2026-10-01-leviasynth-fidelity-p8a-tempo.md](2026-10-01-leviasynth-fidelity-p8a-tempo.md)
- Commit: P8b (this session; unpushed at collection).

## Scope (P8 split; rest to follow)

- P8b (this slice): device live-arp params + phrases + `DM_ARP`. P8c: sequencer tracks + step record. P8d: ribbon + step-LFO editor. Player song path untouched (bit-identical); tap rhythm deferred (no device wall clock).

## P8b status: done (t142, 22 mutants)

- Per-block `levi_arp_block` (engine hook next to tempo push): absolute sample clock, chord = allocator held (latch buffer after release), change detect with clocklocked grid restart vs free-run position ride-through, division from STEPSQ × tempo, octave transpose, gate (<100 explicit OFFs w/ stale guard, ≥100 legato), swing-deferred strikes, chance skips, ratchet sub queue (16), length rewind, stepoff rotation, entropy leaps, last-chord cleanup on empty unlatch.
- Strike path: `note_on_core(hold)` wrap (t134 green), `levi_note_strike` (no held insert), `levi_arp_release_note` (no held removal). Stepper + PHRASE mode (rule factory, zero bytes) + rewind; modes 0..7 identical (t113-115 green).
- 13 keys `0x0EA0..AC`, rows 183..195, ARP 2 pages, per-param texts; `DM_ARP` (33, prompt's 11) with lead-voice fold.
- E0: off default; block-granular strikes; rest = 100; user bank unison; offsets ±64 UI.
- Proof: audit 0/0; riaudio SPACE→PLAY, 16474 buffers 0 xruns. Deviations: no host wav; ARP pixels deferred; 909 pack missing. Adjacent: `levi_arp.c` into AROS sections list (audit caught it).

## Method findings (portable)

- Observe the struck voices, not the held ones: user-held notes sit on v0..v2 and mask arp output. MONO mode (voice 0 = last struck) makes order/phrase/stepoff/length laws exact via per-block trajectories.
- Vacuous timing laws: asserting `tr[b-1]==60` passes with or without swing — the discriminating check is `tr[b]` itself (straight struck, swung pending). Every timing law needs the positive control stated (mutant: disable the feature, law must fail — proven for swing).
- Range-width mutants survive single-index tests: OPBPM range needed an OPBPM7 law; allow-list needed the full 0x0EA0..AC sweep. Same lesson as P8a, second verse.
- Test-buffer overflow returns (P8a finding re-bit in a new shape): N-sized `la/lb` reused for NL renders. Distinct long buffers, always.
- LFO value range ≠ ±1: the DM fold law failed because the LFO never pushed gate below legato. Split route laws (flag fills, white-box) from fold laws (pinned offsets).
- `feq`-style helpers and sign-compare (`int k` vs `BS`) keep biting in test code — `-Werror` is doing its job; fix, don't pragma.
