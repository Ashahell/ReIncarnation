# RBNM-full — evidence ledger (Task 13, gate G13)

The Task-9 S909-subset validator (`rbnm_validate_file`,
`rbnm_validate_manifest_text`, writer, loader) is UNTOUCHED: the
clean pack still validates and every Phase-9 gate re-runs green
below. Task 13 appends (`project/rbnm.c` full section):

- **CPRG hook** (`rbnm_read_cprg`): `CPRG` rides the existing
  unknown-optional skip, so old packs stay valid. Present → text
  extracted; absent → `present=0`, caller mounts the copyright
  fallback (never a load failure). `reference/packs/classic-01`
  asserts the fallback (`present == 0` in t1_formats §15, TC-2.12.x
  partial path).
- **Verbatim reserialize** (`rbnm_reserialize`): validates first
  (corrupt input produces no output), then re-emits every top-level
  chunk (id + size + data + even pad) in file order. Proves
  unknown/CPRG preservation: `pack.rbnm` (1,589,904 B) reserializes
  `cmp`-identical.
- **SHA-256 identity** (`rbnm_sha256_file` over `project/sha256.c`):
  double-run identical on `first-light.rbng`; `abc` vector exact.

Art-fallback (TC-2.12.x): RBNM packs carry no art chunks in this
task; the song-side `SKIN` rule (`rbng_art_fallback`: absent → 1,
load continues on the built-in placeholder) is asserted in t1_formats
§3, and the AROS datatype shells (`project/datatypes/*.datatype.c`)
report the fallback state to the app instead of failing the load.
