# Delay send dry everywhere (owner finding, 2026-09-27)

Owner swept the 808 delay send 127→0→127 (300+ `CTL 0604` lines, all
delivered — the control path is proven by the comp switch beside them)
and heard nothing. Cause: `RIEngine.dline` was NULL in every live path —
no caller ever provided the caller-owned delay storage, so the shared
send renderer (`if (e->dline)`) stayed dry. Offline FX goldens pass
because they drive the FX unit directly, not the engine send path.

Fix: `RIAppCore` carries `dline[131072]` (~2.7 s @48 kHz; longer musical
delays clamp inside the FX unit); `ri_core_init` attaches via
`ri_engine_set_delay` (knob fields preserved). Covers AHI, null, and
headless paths at once; AROS needs no changes.
Tests: t95 pins attachment + pre-echo identity + audible return.
Mutant (detach): 2 FAILs (killed). Device proof: owner delay sweep re-run.
Tempo clock: `ri_live_render` pushes session BPM into the engine (t95 pins
`eng.tempo == bpm`; without it the 140 default stands — observed live as
the stale-object RED). Device retest decides timing by ear.
