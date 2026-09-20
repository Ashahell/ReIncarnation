# GREEN-run defects caught by t1_formats before first pass (Task 13)

The pre-implementation RED (`red-t1_formats.txt`: missing
`project/rbng.h`) is archived separately. On the first GREEN attempt
the test failed 495 assertions across 5 independent defects — all
fixed, none waived:

1. **SHA-256 K[61] truncated** (`project/sha256.c`): 7 hex digits
   (`0xa4506ce`). Fixed from memory to `…ce5` — still wrong. Exact
   integer cube-root (`icbrt(p<<96)`, cross-validated against the
   other 61 entries) proves FIPS K[61] = `0xa4506ceb`. Rule: never
   hand-type crypto constants from memory; verify computationally.
2. **Undo range over-strict** (`project/undo.c`): commit rejected
   values > 127, but the stack is a generic byte store (the 0..127
   bound belongs to callers). Check removed; script pushes 0..199.
3. **MMC pause over-mapped** (`midi_io/midi.c`): draft mapped 09
   pause → stop. The engine has no pause transport state; pause now
   returns −1 (fail closed).
4. **Test-side command typo**: test typed `SETTRPPARAM` (11 chars);
   brief/code say `SETRPPARAM` (10). The exact-string parser
   correctly rejected it — fixed test-side, kept as proof the gate
   bites.
5. **Test buffers too small**: A/B compared at 8 KiB but `pack.rbnm`
   is 1,589,904 B. Bumped to 2 MiB statics (BSS, test-only).

Final: `PASS formats`, rc 0.
