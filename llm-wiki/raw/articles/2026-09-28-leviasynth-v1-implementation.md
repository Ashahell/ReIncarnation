# Leviasynth v1 implementation (slices 3b–3c-iv, owner 2026-09-28)

- Source: ReIncarnation session, 2026-09-28 (opencode lane, owner verdicts in chat)
- Collected: 2026-09-28
- Published: 2026-09-28
- Requirement + E1: [2026-09-28-planned-device-asm-leviasynth.md](2026-09-28-planned-device-asm-leviasynth.md)
- Crash record (found during proof): [2026-09-28-levi-filter-nan-crash.md](2026-09-28-levi-filter-nan-crash.md)
- Proof runs: [2026-09-28-dell-levi-proof-runs.md](2026-09-28-dell-levi-proof-runs.md)

## Decisions taken (owner, chat 2026-09-28)

- v1 voice: POLYPHONIC (kills the mono-vs-poly open in the requirement).
- Demo: full dedicated part (Bm–G–D–A quarter triads, slot 0).
- Order: Dell proof before further slices.
- Rail LED fix (sibling session): approved.
- Timbre: deferred to Dell ears.
- Still open: RBNG chunk IDs (owner review), focus-5/keyboard + MIDI map.

## Slices (all TDD RED→GREEN + mutant + `scripts/ri_audit.sh` 0/0 + pushed)

- 3b `59a3e01`: 5-wide engine (INSTANCES/ROUTE_NSECTIONS/MIX_NBUS 4→5,
  SLEVI 0x10, song compat v1.4).
- 3c-i `c50d490`: control table `0x0E` block + automation allow-list +
  engine section-wide apply (t106).
- 3c-ii `838bdf6`: `sectlevi` chord editor (t107: lane select + 16 step
  toggles, middle-C on enable, edit-step piano keys, FM/PM toggle);
  voice canvas geometry + `art_levi`; Levi panel slot 4
  (mixer/transport shift to 5/6); PAT_LEVI (19) behaviour dispatch
  (`sectui`/`sectpat` `ispat` had silently skipped 19; voice sec 18
  misclassified as pattern); t92/t93 pins for secs 18/19
  (title pipeline uppercases: `PATTERN LEVI`).
- 3c-iii `fc3c787`: visdev/tabpages 5-wide (t97/t98; Synths =
  303A/303B/Levi via explicit membership — ranges broke at dev 4);
  t100 Levi gate (0x1F law, triad contributes, gated render
  bit-identical to the 4-device mix, triad-silenced mutant FAILs);
  RIAPP 5-chip rail + C_P4/C_LEVI canvases + LSTEP (16×6) + knob syncs
  + 0x1F startup mask. Bit law device==bit holds (dev 4 = 0x10).
- Demo `657a990`: Bm–G–D–A quarters (59/62/66, 55/59/62, 62/66/69,
  57/61/64), lanes 3–5 rest (t95 + mutant). t96 unaffected (mask
  excludes the Levi bit).
- NaN fix `bc9237c`: see crash record.
- 3c-iv `12dd8b2`: MIX_LEVI strip end-to-end. Section 20, strip 5,
  master stays strip 4 (planted `sectmix.c` comment); route owner 4
  via explicit maps (board, auto keys, engine apply). Registry rows
  (CC 109/110/111 — 23/24/25 taken by 303A), allow-list 0x0B60–65
  (sorted insert; 104→110, mapped 102→108, CC count 99→102), MIDI
  `ui_of` + panel `mix[6]`, shared mixer geometry + `LEVI` tag, draw
  pins for sec 20. RIAPP C_MXL canvas, 5-strip Mix page, 5th meter
  from bus 4 (`sec_peak` already 5-wide). Key-space stable: master
  comp stays 0x0B55, old songs unaffected. Proofs: t66 strip/steal,
  t79 level/pan/insert land on route 4, t60/t83 key pins, 3 mutants
  kill (strip→4, owner-identity, registry-map).

## Non-obvious findings

- `energy()` counts inf/NaN as nonzero: t103/t104 passed with a
  poisoned voice. Finiteness needs its own assertions.
- `t77` layer guard bans the word "mixer" in `autolane.h` (comment
  tripped it).
- `t73`/`t60` pin the CC count (99→102); `t77` pins allow-list size
  and mapped count.
- `ri_build_host.sh` takes ONE target; header changes need full `all`
  rebuilds or stale objects mislead.
- `RI_ROUTE_MASTER` is 5, not 4; route owners are 0..4 sections + 5.
- Text pipeline uppercases titles (`ri_art_text_c`).
