# Fuzz — evidence ledger (Task 13, gate G13)

`scripts/ri_fuzz.sh [N]` (default 500): deterministic seeded
mutations (byte flips, truncations, FORM/chunk length inflation,
EOF garbage) over the 10-song RBNG corpus + the RBNM clean pack,
each run through `tools/inspect --rbng/--rbnm` under `timeout 5`.

Pass rule (spec §17 failure 2): exit 0 (still valid), 1 (invalid
with chunk ID + byte-offset reason), or 2 (usage/IO). Signal death
(rc > 128), timeout (rc 124), or any other rc fails the run.

Measured 2026-09-20 (host GCC, committed run): **500/500 no-crash,
no-timeout** — valid=30, invalid=470, ioerr=0. The 30 still-valid
are small mutations in pad/CPRG-text regions (length-delimited skip
working as designed); every invalid names its chunk + offset.

RED note: `docs/evidence/formats/red-t1_formats.txt` archives the
pre-implementation failure (missing `project/rbng.h`); the five
defects the GREEN run caught first are logged in
`docs/evidence/formats/green-defects.md` (two-commit RED→GREEN
discipline per progress.md Task-9 ruling).
