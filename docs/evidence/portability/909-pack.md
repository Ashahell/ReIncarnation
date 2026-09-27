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
