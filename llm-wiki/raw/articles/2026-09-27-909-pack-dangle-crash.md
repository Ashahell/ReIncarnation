# 909 pack descriptor dangle → render-task guru (Dell 2026-09-27)

- Source: ReIncarnation session, 2026-09-27 (Dell E6320 lane forensics + host repro)
- Collected: 2026-09-27
- Published: 2026-09-27
- Commits: `cb0e702` (fix) … `948c761` (device proof)
- Evidence: `docs/evidence/portability/909-pack-dangle.md`

## RED (device, build `646cb4f`)

Owner pressed a 303A piano key (G# pitch entry); AROS raised `Software
Failure!`: task `riapp render`, `Illegal address access`, top frame
`ri_layer_mix + 0x226`. Ev-log froze at the keypress round; heartbeat
buffers froze with it. Single instance, 0 xruns to that point; 808/909
step programming had worked minutes earlier. Owner-read values: task,
error, and top stack frame (requester pixels unreadable remotely).

## Root cause

`platform/aros/pack_909.c` bound each voice with a **stack**
`struct RISampleLayer lay[3]`; `rb909_set_layers` stored the caller's
pointer (`v->layers = layers`). On return the frame recycled; the render
task kept reading descriptors through the dangling pointer. Minutes of
correct playback were luck (stale stack intact) until reuse put garbage
in `L[k].data` → fault in `ri_resample_linear` called from
`ri_layer_mix` (the guru PC is the post-call return address; `-mcmodel=large`
emits the call as `movabs`+indirect, verified against the shipped binary).
The piano keypress was coincidence — any stack churn could have fired it.
The header (`engine/dsp/rb909.h`) always documented copy semantics
("copies the descriptors, NOT the sample data") — code and doc disagreed.

Host could never catch it: without the pack, 909 voices stay silent and
never touch layer data. A full-walk host repro (drum programming, knob
sweeps, mid-render pitch walk, ASan+UBSan) survived — portable code
proven innocent before the bind path was read.

## Fix

`RB909Voice` carries `store[RI_909_MAX_LAYERS]`; `set_layers` copies the
descriptors (`memcpy`, `project/rbnm.c` precedent on both targets) and
points `layers` at the store — the documented contract made true. Sample
data stays caller-owned (AllocVec'd for the process). No `pack_909.c`
change needed; `tools/render.c` callers unaffected (synchronous use).
Only `rb909.c` itself ever touched `v->layers` (init/store/read), so the
struct growth (64 B/voice) is contained.

## Tests

t99 (`tests/unit/t99_pack_bind_lifetime.c`) pins the contract:
out-of-scope bind still renders bit-identical audio; `layers != caller
ptr` (host RED FAIL before the fix, PASS after). Pointer-store mutant
FAILs (killed). Audit 0/0.

## Proof

Owner session on `cb0e702`: 1132-event run (~24 min, 272k buffers), 909
beat reprogrammed + knob sweeps + pitch-walk repeat (key=12 with
accent/slide toggles), clean TR STOP, no guru, 0 xruns (render_max 53%
of period — the sounding 909 costs load, within budget). Recorded in the
evidence file and `948c761`.
