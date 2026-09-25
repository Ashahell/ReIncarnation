# Random / Alter distributions (E1 scope, E0 weights)

**Status:** adopted 2026-09-25, pattern-model slice.

**Rule:** ReBirth's random distributions are unknowable and are **not**
a parity target; only the *scope* of each op is E1 (p. 53–54).
Alter = seeded Fisher–Yates permutation (E1: “randomly shuffling the
data in an existing Pattern”), so the multiset of rows/columns is
preserved and an empty pattern stays empty.

**Source:** E1 (pp. 53–54) for op scopes; E0 for the weights below
(documented here so the generator is reviewable and frozen by test).

**E0 weights** (all over 16 rows, ignoring length; one LCG stream):
- Pitches: key = pick(13) per row; flags untouched.
- Accents etc.: per row, REST if pick(16) < 4 (75 % notes); ACCENT if
  pick(16) < 4; SLIDE if pick(16) < 3; octave r = pick(16): r < 2 →
  Down, r < 4 → Up, else none. Key untouched.
- Pattern = pitches pass, then accents pass (same stream, that order).
- Drum lane: per row r = pick(16); 909: r < 8 off, < 12 low, < 15
  high, else flam; 808: r < 11 off else on. AC flags untouched.
- Alter flags-column variant preserves REST/ACCENT/SLIDE/UP/DOWN
  combinations as one tuple per row.

**How it closes:** scope rows close only by ReBirth menu inspection;
weights are frozen by the t54 seed-1 literal and never claimed as
parity.
