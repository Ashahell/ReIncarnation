# Slide direction (E1 + E0)

**Status:** adopted 2026-09-25, pattern-model slice.

**Rule:** the pattern model stores the ReBirth meaning — slide on a
step means “tie this step to the next” (E1, Owner's Manual p. 154;
the TB-303 slide switch likewise lengthens the *current* gate and
glides into the *next* pitch). The walker's step flag `RI_STEP_SLIDE`
means “slide INTO this step” (gate held from the previous note), so
the converter shifts slide by one step: walker SLIDE on step i ⇔
`row[i−1]` is a note with SLIDE set. A slide flag on a Pause row does
nothing (nothing is sounding to tie). Last-row slide wraps to row 0
when cyclic.

**Source:** E1 (p. 154) for the stored meaning.

**How it closes:** E0 portion — slide into a Pause holds the gate
through that step (existing §8 NOTE_CONTINUE row); pending ReBirth
check against an exported slide-into-rest pattern.
