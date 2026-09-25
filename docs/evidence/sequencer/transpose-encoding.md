# Transpose encoding (E0)

**Status:** E0 rule adopted 2026-09-25, pattern-model slice.

**Rule:** absolute semitone `s = key + 12·oct`, `s' = s + n`, fold by
±12 octaves into [−12, 24], then encode canonically: `semi < 0` →
Down with key = semi + 12; `0 ≤ semi ≤ 12` → no octave flag,
key = semi; `semi > 12` → Up with key = semi − 12. Octave 0 is
preferred for 0 ≤ s' ≤ 12 (so high C encodes as key 12, no octave).
Encoding never produces Up+Down.

**Source:** E0 (the manual, p. 54, only states the range fold:
pitches “moved one octave in either direction so that they appear
within the valid range”).

**How it closes:** transpose a known pattern in ReBirth 2.0.1 and read
the step editor to confirm the key/Up/Down encoding.
