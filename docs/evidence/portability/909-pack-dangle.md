# 909 pack descriptor dangle → render-task guru (2026-09-27)

## RED (device, build 646cb4f)
Owner pressed the 4th black piano key (G#, step pitch) on 303A; AROS
raised `Software Failure!`: task `riapp render`, `Illegal address
access`, top frame `ri_layer_mix + 0x226`. Ev-log froze at the keypress
round (`STEP v=0 slot=0 step=4 key=8`); heartbeat buffers froze with it.
Single instance, 0 xruns to that point; 808/909 step programming (DSTEP
lines) had worked minutes earlier.

## Root cause
`platform/aros/pack_909.c` bound each voice with a **stack**
`struct RISampleLayer lay[3]`; `rb909_set_layers` stored the caller's
pointer (`v->layers = layers`). On return the frame recycled; the render
task kept reading descriptors through the dangling pointer. Minutes of
correct playback were luck (stale stack intact) until reuse put garbage
in `L[k].data` → fault in `ri_resample_linear` (called from
`ri_layer_mix`; the guru PC is the post-call return address). The piano
keypress was coincidence (any stack churn could have fired it).
The header (`rb909.h`) always documented copy semantics ("copies the
descriptors, NOT the sample data") — code and doc disagreed; code won.

Host could never catch it: without the pack, 909 voices stay silent and
never touch layer data (full-walk host repro + ASan/UBSan survived).

## Fix
`RB909Voice` carries `store[RI_909_MAX_LAYERS]`; `set_layers` copies the
descriptors and points `layers` at the store (doc now true). Sample data
stays caller-owned (AllocVec'd for the process). No pack_909.c change
needed. `render.c` callers are unaffected (synchronous use).

## Tests
t99 pins the contract: out-of-scope bind still renders bit-identical
audio; `layers != caller ptr` (host RED FAIL before the fix, PASS
after). Mutant (pointer store): FAIL (killed). Audit 0/0.
Device proof CLOSED 2026-09-27 (owner, Dell, build cb0e702): 1132-event
session (~24 min, 272k buffers), 909 beat reprogrammed + knob sweeps +
pitch-walk repeat (key=12, accent/slide toggles), clean TR STOP, no
guru. 0 xruns throughout (render_max 2811 us, 53% of period — the
sounding 909 costs load, within budget).
