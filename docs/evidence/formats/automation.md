# Automation + undo — evidence ledger (Task 13, gate G13)

Spec §8/§13: automation lanes travel with the song (AUTO chunk),
share the GUI/MIDI/ARexx control IDs, and render through the same
event list (inline route, sample-accurate); knob commit-on-release =
one undo unit.

## AUTO lanes

Recorded at song ppq (recorder quantum ppq/24 exactly
representable); rendered by tick→sample conversion merged into the
sorted walker event list as `RI_EV_AUTOMATION` (value = ctl, flags =
val), applied inline at event boundaries in `tools/render
--rbngsong`. The 0x0300 block drives the 303A voice. Save/load is
lossless u8 (bound ±1 unit, asserted exact in t1_formats §1; the
10-song corpus renders automation audibly — s01 peak 24975 with a
cutoff sweep lane).

## Undo (`project/undo.c`)

Fixed 200-deep `(ctl, val)` stack, no allocation. `ri_undo_commit`
fails closed at 200 (no silent rotation — losing user data
silently is worse than refusing). Scripted in t1_formats §8: 200
commits, 201st refused, 200 undos replay values 199..0 exactly,
underflow refused, 200 redos replay 0..199. A defect draft rejected
values > 127 in commit; the stack is a generic byte store (the
0..127 bound belongs to callers), so the check was removed and the
script pushes 0..199 raw.
