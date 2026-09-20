# ReIncarnation llm-wiki — index

Project knowledge for ReIncarnation (AROS audio upgrade + Rebirth 2.0 re-creation).
Seeded 2026-09-20 by ingesting audio material from Vulkan4AROS `llm-wiki`.

## Spec (normative) — v5 current

- [../docs/superpowers/specs/2026-09-20-reincarnation-spec.md](../docs/superpowers/specs/2026-09-20-reincarnation-spec.md) — **Engineering Specification v5 (review #3 applied 2026-09-20: §0.1 first-light rule, engine-block/device-buffer split, §4.2 AHI hook-context contract + render Task, RIEvent payload mapping + total sort order, f32 bus/f64 master reconciled, OPEN-09/P-21 AHI latency floor).** Front normative-summary + machine-readable OPEN table (9 rows); Appendix A pending-measurement ledger (P-01–P-21); Appendix B 303 filter candidate equations; Appendix C 808/909 skeletons; Appendix D demoted sketches + ABI freeze gate; locked clock rules (mid-buffer split, 16th-boundary switches, shuffle order, overflow); 303 gate/slide state machine; §17 degraded-mode table; §18 strict build gates + W1.1 report gate; GUI owned-UX policy; PCF black-box plan; `docs/evidence/` ledger dirs.

## Reference

- [../reference/prior-art.md](../reference/prior-art.md) — **2026-09-20 — Prior Art Register (32 entries, 4 parallel research streams).** 303 lineage (Open303 constants, DF ranges, Stinchcombe/a1k0n ladders, Sonic Potions timing, Whittle accent, Huovilainen/D'Angelo, shipped ZDF refs); 808/909 (service notes, Werner kick/cymbal/snare, clap bursts, 909 hybrid + ROM rules, Hydrogen layers, clean-room pack rules); AROS platform (AHI dual API + ≥20 ms shim floor, CAMD/realtime, Poseidon MIDI scope, Zune MCC + Numeric widgets, IFF/iffparse/datatypes, RB-338 behavior); engine methods (Tracktion single-graph, Zrythm PPQ, rational clock, CLAP offsets, Bencina RT rules, cochlea tiers, det-math kernels, snapshot testing). Each entry carries evidence class + concrete spec impact; applied deltas are referenced inline in the spec as register entries 1–32.

## Ingested articles

- [raw/articles/2026-07-16-hosted-aros-wsl2-audio-ahi-alsa-pulse-bridge.md](raw/articles/2026-07-16-hosted-aros-wsl2-audio-ahi-alsa-pulse-bridge.md) — ✅ **2026-07-16 — hosted AROS SDL games (MBX, xRick, SokobanGP2X) black-screen + SIGILL-cascade under WSL2 — missing ALSA→Pulse bridge, NOT r12.** `SDL_SetError: Unable to open AHI device! Error code -1` → trap-handler re-entry (RSP −0x400/iter) → stack overflow. Fix: `libasound2-plugins` + `~/.asoundrc` (`type pulse`). Env-only, no repo change.
- [raw/aros-dev-portal/documentation/roadmap-audio-excerpt.md](raw/aros-dev-portal/documentation/roadmap-audio-excerpt.md) — AROS roadmap audio status: AHI ported, i386 drivers, AHI Prefs + MP3 player.
- [raw/aros-dev-portal/documentation/specifications/drafts/hidd-sound-excerpt.md](raw/aros-dev-portal/documentation/specifications/drafts/hidd-sound-excerpt.md) — HIDD sound-system draft: internal/Zorro/MIDI/PC layouts + `HIDDA_Capabilities` (`MIDI/SFX/Music/Speech`).

## Plans

- [raw/articles/2026-09-20-aros-audio-modernization-rebirth-2.0-plan.md](raw/articles/2026-09-20-aros-audio-modernization-rebirth-2.0-plan.md) — **2026-09-20 — Comprehensive Plan: AROS Audio Modernization + "ReBirth 2.0 for AROS" (user-provided, verbatim).** W1 audio foundation (ahi.device compat + new `audio.library` float32/SMP/SIMD layer, CAMD/USB-MIDI, WAV/AIFF) → W2 RB-338 v2.0.1 parity (2×303, 808, 909, 32×16 patterns, song mode, FX, skins/mods) → W3 Power Mode. Legal constraint: functionally identical, original artwork/samples only.
- [raw/articles/2026-09-20-reincarnation-deep-technical-dives.md](raw/articles/2026-09-20-reincarnation-deep-technical-dives.md) — **2026-09-20 — Deep Technical Dives (user-provided, verbatim).** §1 TB-303 DSP model (VCO + accent/slide VCA + tanh 18 dB ladder, `RB303Voice`/`rb303_render` contract) → §2 `audio.library` API sketch (tag-based graph, lifecycle, render/export, `AuPlaySample`, SMP/SIMD internals) → §3 `FORM RBNM` skin/mod format (SKIN/SNDS/SONG/CPRG, control IDs, mod browser).
- [raw/articles/2026-09-20-reincarnation-deep-technical-dives-part2.md](raw/articles/2026-09-20-reincarnation-deep-technical-dives-part2.md) — **2026-09-20 — Deep Technical Dives, Part 2 (user-provided, verbatim).** §1 PCF (12 dB SVF + 54-pattern deterministic table, `pcf_render`) → §2 TR-909 multi-layer sampler voice (`RB909Voice`, layer crossfade, accent/flam, `rb909_render`) → §3 `FORM RBNG` song format (TMAP/SECT/PATT/SONG/AUTO/PCFP/MIXR/MODR/SHFL, shuffle quirk, round-trip CI guarantee).
- [raw/articles/2026-09-20-reincarnation-deep-technical-dives-part3.md](raw/articles/2026-09-20-reincarnation-deep-technical-dives-part3.md) — **2026-09-20 — Deep Technical Dives, Part 3 (user-provided, verbatim).** §1 TR-808 voice-by-voice recipes (`RB808Voice`, BD/SD/toms/congas/RS/CP/hats/cymbal/cowbell) → §2 transport & sequencer scheduler (master clock, event pipeline, 303 legato semantics, `SeqSnapshot`, recording, determinism) → spec-complete table + build order.
- [raw/articles/2026-09-20-reincarnation-wbs-m2-2-to-m2-6.md](raw/articles/2026-09-20-reincarnation-wbs-m2-2-to-m2-6.md) — **2026-09-20 — Work Breakdown Structure M2.2→M2.6 (user-provided, verbatim).** Device-framework pre-decision (`RIDevice`/`RISeq`) → 16 modules 2.1–2.16 with interfaces + TC acceptance gates → milestone gate table → open items (interaction spec, artwork/samples, W3 appendix).
- [../docs/superpowers/plans/2026-09-20-reincarnation-implementation.md](../docs/superpowers/plans/2026-09-20-reincarnation-implementation.md) — **2026-09-20 — Implementation Plan (13 tasks, phased).** Lab bootstrap → kernels → clock/events → first light → W1 audio → scheduler → 808 → 909 → PCF/FX → mixer/registry → GUI → formats/MIDI → REL. Host-GCC TDD, AROS cross at boundaries, Vulkan4AROS toolchain/audit reuse, W3 deferred.

## Source cross-references (Vulkan4AROS, not copied)

- `llm-wiki/log.md:3273-3295` — same 2026-07-16 audio incident summary.
- `llm-wiki/index.md:366` — same article catalog entry.
