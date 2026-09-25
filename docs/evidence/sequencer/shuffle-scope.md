# Shuffle scope (OPEN)

**Status:** OPEN (recorded 2026-09-25, pattern-model slice).

**Rule:** this slice stores **nothing** shuffle-related in
`RIPattern`. The manual says “activate Shuffle for the desired
sections” (p. 21, 147) but is silent on whether the switch follows
pattern changes, so section state owns the flag (to be placed in
§12.9).

**Source:** E1 text (pp. 21, 147) underdetermines the storage.

**How it closes:** in ReBirth, set shuffle on section pattern A1,
switch to A2, observe whether shuffle stays on. The outcome decides
whether the flag lives in `RIPattern` or section state.
