# Datatypes — evidence ledger (Task 13, gate G13)

Spec §13: songs/samples load through the datatypes system
(`picture.class`/`sound.class`; 8SVX/SMUS-style conventions reused
where they fit). The CODECS are host-tested (`project/rbng.c`,
`project/rbnm.c`); the shells below are AROS-only wiring, following
the probe_ahi.c / MCC-shell precedent exactly:

- `project/datatypes/rbng.datatype.c` — `FORM….RBNG` magic probe +
  load entry (`rbng_read_song`) reporting the art-fallback state
  (`rbng_art_fallback`) so the app mounts the placeholder skin
  (TC-2.12.x partial-art path) instead of failing.
- `project/datatypes/rbnm.datatype.c` — `FORM….RBNM` magic probe +
  load entry (`rbnm_validate_file` + `rbnm_read_cprg`) reporting the
  CPRG hook state (absent → copyright fallback, never a failure).

Both carry `#ifndef __AROS__` + `#error`, are excluded from
`scripts/ri_build_host.sh` (audit greps), and are compile-checked
for AROS in audit Phase 13. Full datatype registration (class
structs, dispatcher tables) is Task-14 installer/app wiring; these
TUs own the probe/load discipline so the gate is checkable now.
