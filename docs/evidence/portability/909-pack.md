# 909 sample pack in RIAPP (owner request, 2026-09-27)

- Pack is clean-room CC0 (`reference/packs/classic-01`, synthesized from
  scratch with per-layer recipes + MANIFEST provenance) — no external
  sourcing needed. Covers 6 of 11 voices (BD/SD/CH/OH/CR/RD, 1–3 layers
  each); toms/rim/clap have no in-repo samples and stay silent, logged.
- `platform/aros/pack_909.{h,c}` (new, AROS-only): resolves
  `<PACKS>/classic-01/pack.rbnm` via `RI_PATH_PACKS` (new PAL enum;
  AROS `SYS:Classes/ReIncarnation/Packs/`, host `packs/`), inventories
  with `rbnm_pack_layers`, decodes layers into AllocVec buffers (bounded
  88200 frames/layer, ≤3/voice, rate-mismatch refuses the voice), binds
  idle via `ri_engine_909_bind`. Buffers live for the process; every
  failure path frees. Returns voices bound; pack-level errors reported.
- `app/riapp.c`: binds after session init, BEFORE `au_live_run`
  (idle-only swap rule); logs `N voices bound` or the pack error
  (replacing the old unconditional "unbound" line).
- stdio on AROS: `rbnm.c` links against `-lstdcio/-lposixc` (v1) and
  `-lcrt/-lcrtprog` (v11, verified with a probe link). No codec changes;
  all format goldens untouched.
- Tests: t86 pins `RI_PATH_PACKS` resolution. No host test for the bind
  itself (AROS-only AllocVec/paths); proven by v1+v11 `-Werror` builds,
  0-UND links, and the Dell by-ear run (owner).

## 11 voices (owner 2026-09-27): toms/rim/clap synthesized

- Missing 5 voices (LT/MT/HT/RS/CP) synthesized clean-room with
  `tools/mk909.py` (stdlib-only, seeded LCG, 44100/16, -3 dBFS peak,
  d[0]=0): pitch-drop toms, burst rimshot, bandpass-noise clap. 13 layers
  (toms LOW/MID/HI, RS/CP A/B) + recipe `.txt` + MANIFEST rows, same
  conventions. All pass the audibility floor.
- `tools/mkpack.c` (new): reproducible pack builder (chunk-walking RIFF
  reader, manifest pre-validation, dup refusal). Rebuilds classic-01
  bit-exact for old layers modulo 832 one-LSB diffs in 793800 (original
  rounding unrecoverable — documented in-file; carry-over done by exact
  16-bit extraction instead).
- Format: `RBNM_MAX_LAYERS` 16→32 (old files parse identically) and
  `RBNM_MAX_FILE` 2→4 MiB (pack is 2.4 MB). t30 updated 14→27 (pins the
  new fact); S909 goldens re-verified identical (old voices bit-exact).
- New pack replaces `pack.rbnm` in-tree (old in git history). Host bind
  check: 11/11 voices. Dell timbre verdict: owner by ear (pending).

## Tom body v2 (owner 2026-09-27: whole kit thin/string-like)

- Host measurement first: demo 909 bus RMS 0.2087 vs 808 bus 0.1123
  (repo pack) — levels fine, so timbre/envelope, not gain. Fix:
  membrane modes 1.00/1.51/2.09 (mix 1/.4/.22, tau x1/.5/.3), 25 ms
  noise transient (was 10 ms), taus 0.45/0.40/0.35, mild tanh 1.3;
  same seeds. Showcase hits accented (HIGH + AC backbeats) to match
  the accented 808 floor.
- Rebuild with bit-exact carry-over (the 2bdd88a procedure): old 18
  layers extracted as 16-bit WAVs from the current pack (s16/32768 is
  exact in binary32, round-trips losslessly), new tom WAVs from
  `mk909.py`, `mkpack` over carry spec + manifest (old rows
  byte-identical incl. 2026-09-20 dates). Proof: per-layer memcmp —
  OLD-EXACT=18, TOMS-CHANGED=9. (Rebuilding old layers straight from
  the 24-bit WAVs gives documented 1-LSB rint diffs — not taken.)
- 909 bus RMS 0.2087 -> 0.2205 host. Dell timbre verdict: owner by ear
  (open).
