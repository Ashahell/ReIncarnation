# RBNG song codec — evidence ledger (Task 13, gate G13)

Spec §13 (IFF forward-compat rules + SHA-256 identity) + §8 (AUTO at
ppq/24, shared control IDs) + §17 failure 2 (requester names chunk ID
+ byte offset, rollback, nothing half-loaded).

## Layout (executor-defined, `project/rbng.h` is the contract)

`FORM u32_tot 'RBNG' { chunk }`, chunk = `ID32 u32_size data + even
pad`, all integers big-endian. Chunks: `VERS` (u16 major=1, u16
minor=0, u32 flags=0), `SONG` (tempo 30..300, ppq 24..960, nsteps
1..64), `PATT` (nsteps × {u8 note 0..127, u8 flags}; length must equal
nsteps×2; rest+accent illegal), `AUTO` (u16 n ≤ 256; per u32 tick in
song-ppq ticks, u16 ctl, u8 val 0..127, u8 pad0), `MODR` (u16 n ≤ 16;
per u8 namelen + name, u8 shalen=64 + 64 lowercase-hex sha, u16 vers),
`CPRG` (u8 len + UTF-8 text ≤ 127). Optional `SKIN` artwork ref (see
art-fallback). No allocation: single static 64 KiB image; every chunk
size range-checked before use, never trusted for allocation.

## Forward-compat rules (asserted in `tests/unit/t1_formats` §3–5)

- Major ≠ 1 → reject (`VERS @12: VERS unsupported major`).
- Minor newer → load known chunks, preserve unknown bytes verbatim.
- Unknown `feature_flags` bits → reject (all flag bits are
  mandatory-by-definition: the file needs behavior we lack).
- Unknown optional chunk (ID[0] `A`..`Z`, incl. `CPRG`-class and
  `TST1`) → skip length-delimited + preserve verbatim; anything else
  (lowercase id) → reject naming the id.
- Serialize-parse-serialize is byte-identical INCLUDING unknown bytes
  (10/10 corpus + injected-`TST1` round trip, `cmp` clean).
- Corrupt length (inflated size) → abort with `<ID> @<off>:` reason;
  the 500-mutation fuzz proves no crash/timeout on any input.
- Resave normalizes the VERS stamp to the writer's 1.0 (writer-emits-
  current) while preserving unknown chunks verbatim — a minor-newer
  file round-trips its data bytes but not its version bytes.

## SHA-256 identity

`project/sha256.c` (FIPS 180-4, no alloc): `abc` vector
`ba7816bf…15ad` exact; file identity via `rbnm_sha256_file`
(double-run identical). MODR stores name + 64-hex sha + vers (not
CRC32-only) per spec §13. Defect found by the test: K[61] was first
truncated to 7 digits, then "fixed" from memory to `…ce5`; exact
integer cube-root (`icbrt(p<<96)`, validated against the other 61
entries) proves the FIPS value is `…ceb`. Lesson: never hand-type
crypto constants from memory — verify computationally.

## AUTO ppq/24

AUTO ticks are song-ppq ticks; the GUI 30 Hz tweak recorder quantum
(ppq/24 at 96 ppq) is exactly representable. `tools/render
--rbngsong` converts tick→sample and merges AUTOMATION events into
the same sorted event list the text path renders (one renderer).
Save/load bound ±1 unit, asserted exact (u8 is lossless).

## MODR + CPRG

Missing mod → warn prompt `MODR: mod '<name>' vers <v> not found —
expected sha <sha>`; render continues without the mod (asserted
prefix in t1_formats §6; live in the s10 render log). CPRG on-load
hook (`rbng_cprg_line`, surfaced by `inspect --rbng` and the render
log); missing CPRG is fallback, not error.

## Text-scaffold swap (flagged since Task 4)

`tools/render --song` (text) is UNTOUCHED: all Task-4/7 goldens
re-render byte-identical in audit Phases 1/7 below. `--rbngsong`
is the additive real-codec path. No re-pin: goldens byte-identical
through the swap by construction, proven by the unchanged audit.
