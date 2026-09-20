# ARexx — evidence ledger (Task 13, gate G13)

Spec §1 (LOCKED): primary port `ADDRESS REINCARNATION`; the parser
accepts `REBIRTHAROS` as a deprecated alias.

## Exact strings (`project/arexx.c`, t1_formats §13)

`OPENSONG <path>` | `PLAY` | `STOP` | `EXPORTWAV <song> <wav>` |
`SETRPPARAM <ctl> <val>` — uppercase, single-space separated, no
leading/trailing/double spaces. `ctl` 0..65535 (shared 16-bit
control-ID intent range, §13), `val` 0..127. An optional leading
`REINCARNATION ` / `REBIRTHAROS ` port token is stripped; anything
else verbatim fails. Lowercase (`play`), unknown verbs (`DANCE`),
argless `OPENSONG`, and `val > 127` all reject (nonzero, no partial
parse). The AROS RexxMsg reply glue is Task-14 app wiring, not here.

Defect found by the gate: the test itself typed `SETTRPPARAM`
(11 chars) while the brief/code say `SETRPPARAM` (10) — the
exact-string parser correctly rejected it, and the mismatch (not a
code bug) was fixed test-side. Kept as evidence the gate bites.
