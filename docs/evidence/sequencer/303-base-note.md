# 303 base note (E0)

**Status:** E0 design decision (adopted 2026-09-25, pattern-model slice).

**Rule:** `RI_303_BASE_NOTE = 36` (C2). The 303 row key 0–12 (low C …
high C) maps to MIDI `36 + key + 12·oct`. Reason: keeps every
first-light / v1.0 note in range (first-light uses MIDI 45 = key 9,
no octave), and the 303's three-octave span −12…+24 then covers MIDI
24–60 (C1–C4), the commonly cited TB-303 playable range (verify).

**Source:** E0 hypothesis (not stated in the manual).

**How it closes:** export a key-0 step from ReBirth 2.0.1 and measure
f0 (review §8 E3 measurement).
