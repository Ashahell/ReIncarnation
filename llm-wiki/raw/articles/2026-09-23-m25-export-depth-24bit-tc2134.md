# 2026-09-23 — M2.5 export depth: TC-2.13.4 24-bit WAV (TDD feature, --depth + dc-24 golden)

> Source: session evidence (test output ×4, audit 0/0 ×2, golden re-render compare), compiled by agent
> Collected: 2026-09-23
> Published: 2026-09-23

## Disposition
New. Gap analysis of TC-2.13.4 against the tree found a genuine
hole: both WAV writers (offline `tools/render.c`, engine
`audio_io/audio.c` export) were 16-bit/48 kHz-only — 24-bit,
44.1 kHz, and AIFF had no implementation anywhere. This slice
implements 24-bit depth for both writers with a contract pin
(`t28_wavdepth`, audit Phase 1) + 24-bit golden. 44.1 kHz (needs a
resampler — none exists in the tree) and AIFF stay OPEN, named in
the ledger, not waved.

## What
- `audio_io/audio.{c,h}`: `auf_wav_header(hdr44, total, depth, sr)`
  + `auf_f32_to_s24()` (non-static, the contract owners),
  `au_render_song_to_wav_depth()` + `AuRenderToFileDepth()`
  (`AuRenderToFile` delegates depth 16; the dead
  `auf_write_wav_header` folded away).
- `tools/render.c`: global `--depth 16|24` (default 16, bad value
  exits 2); writer takes float PCM + depth (16-bit path converts
  through the same `f32_to_s16` — pre-existing goldens
  byte-identical by construction); all 8 int16 fixture buffers
  become float; calls the shared audio.c header/packer (no third
  implementation); help text updated.
- Golden `tests/golden/303/dc-24.wav` + `.sha256` (288000 data
  bytes); audit Phase 1 pins existence + sidecar + sha256 +
  determinism re-render + the t28 line.
- Ledger `docs/evidence/formats/export.md` (new): PARTIAL status,
  twin-rounding doctrine, OPEN 44.1/AIFF/external-validation.

## Proof
- TDD RED first, watched clean: link-phase, exactly the three
  missing symbols (`auf_wav_header`, `auf_f32_to_s24`,
  `AuRenderToFileDepth`) — the test file itself compiled without
  a single diagnostic.
- GREEN: all t28 checks pass — 16-bit golden structural, header
  matrix 2×3 exact, s24 edges, dc-24 structural, wrapper
  null-fail-closed. 16-bit `math-dc.wav` re-renders
  byte-identical through the rewired float path (blast radius
  zero, measured not argued).
- One REAL finding pre-GREEN, resolved against the test:
  `auf_f32_to_s24(-1.0)` yields -8388607, not -8388608 —
  float→int truncates toward zero, twin-consistent with the
  16-bit -32767 edge. Symmetric full scale would diverge the
  twins; the pin now asserts identical rounding (documented in
  the test + ledger). Production untouched by the finding.
- Full `ri_audit.sh` 0/0 TWICE (pre-change baseline + final tree,
  0 FAIL lines).
- Editing honesty: two self-caused fumbles on the way (stray
  declaration left in panels-style haste in render.c's converter
  ordering — implicit-decl + unused-function errors, both fixed
  pre-GREEN; final build warning-free). The green tree is the
  verified one, not the drafts.

## Files
- env: `audio_io/audio.c` (+40), `audio_io/audio.h` (+decls),
  `tools/render.c` (writer + flag + 8 float buffers),
  `scripts/ri_audit.sh` (Phase 1 dc-24 + t28 lines)
- tests: `tests/unit/t28_wavdepth.c` (new)
- goldens: `tests/golden/303/dc-24.wav` + `.sha256` (new)
- ledgers: `docs/evidence/formats/export.md` (new)
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

No external websites consulted: the canonical 44-byte PCM header
is reproduced from the two proven in-tree writers (golden parses
agree), and the remaining contracts are WBS + ledger-local;
nothing was open that needed the web. spirv-val vacuous (no
SPIR-V in this repo — noted honestly per standing).
