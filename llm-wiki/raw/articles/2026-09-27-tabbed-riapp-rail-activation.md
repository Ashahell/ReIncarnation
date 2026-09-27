# Tabbed RIAPP + device rail + activation (owner 2026-09-27)

- Source: ReIncarnation session, 2026-09-27 (opencode implementation + Dell E6320 lane proofs)
- Collected: 2026-09-27
- Published: 2026-09-27
- Plan: `docs/superpowers/plans/2026-09-26-portability-plan.md` §12.11 G9; decisions in `docs/2026-09-24-improvement-todo.md` (RIAPP full panels)
- Commits: `0a5e434` (decisions) … `60f3fd2` (t97) … `646cb4f` (tabs+canvases) … `1046958` (Devices tab) … `59700bd` (rail) … `4cbfdaa` (rail proof) … `f2bcbb2` (activation); every code commit RED→GREEN + mutant + `AUDIT 0/0 PASS`
- Evidence: `docs/evidence/portability/delay-line.md` (delay clock), `docs/evidence/portability/t2-lane-proofs.md` (soak rerun), `docs/evidence/sequencer/pattern-change-fix.md` (selection inaudibility)

## Owner decisions (all 2026-09-27, recorded in improvement-todo)

- Full panel set via **tabbed groups** (Synths/Drums/Mix/FX) with stock `Register.mui` — no custom tab widget.
- **Device visibility up front** in the tab model (aligns with the 2026-09-24 extensible-rack requirement; Korg/Leviasynth later must not need a GUI rewrite).
- **303B silent until programmed** (no demo content change).
- 909 demo content rides with panels (owner taste); new-voice timbre verdict by ear.

## What landed

- **t97 `RIVisSet`** (`60f3fd2`, `gui/visdev.[hc]`, `tests/unit/t97_visdev.c`): visible-device set over the 4 classic devices; init-all-shown, hide/show round-trip, fail-closed bad-index, tab titles single-sourced from `ri_panel_get`. RED FAIL 12 (stub, UB-free after title-init fix) → PASS; init-hidden mutant FAIL 8 (killed); AROS-confinement grep clean.
- **t98 tab pages** (`646cb4f`, `gui/tabpages.[hc]`, `tests/unit/t98_tabpages.c`): 4 fixed groups; per-group visible device rows with explicit (voice, pattern) sections (no arithmetic on section IDs); Mix/FX are frameless. RED FAIL 12 (`strcmp(NULL)` short-circuit fix) → PASS; voice-swap mutant 2 FAILs (killed). A Devices 5th tab was added then **reverted** (see rail below): model stays 4 tabs, nothing consumes a 5th.
- **Tabbed RIAPP** (`646cb4f`, `app/riapp.c`): Register with Synths/Drums/Mix/FX, transport framed above; 14 canvases (TR + 4 PAT + 303A/303B/808/909 voices + mixer board + 4 FX); device rows built from the t98 model; engine flags +S303B|S909; `sync_303v` per 303 voice, new `sync_drumv` (11 lanes + AC row, lane-bits/flags shadow split), all value canvases in `sync_values`, startup burst extended (bridge filters non-automatable by `key==0`); DSTEP/DSTEPAC ev-log lines. v11 build 69 TUs, 0 UND.
- **Rail replaces Devices tab** (`1046958` tab → `59700bd` rail): the 5th tab worked (owner-approved) but failed the elegance challenge — it hid the control a tab-switch away and managed the wrong noun (VISIBLE; the rack requirement says ACTIVE). Same visibility-only bit (`RIVisSet` + ShowMe rows + ReturnIDs 1001..1004 + VIS log) moved to a slim always-visible rail above the Register. Owner verdict: rail works as intended; proof ink `VIS dev=1 show=0/1` twice with `TR PLAY/STOP` around the toggles (`4cbfdaa`).
- **Activation** (`f2bcbb2`, t100): the rail bit now means ACTIVE. `RILiveSession.sections` is `ri_atomic_u32` (precedent: `autolane_emit`, `ctlplane`), read once per `ri_live_render` — flips apply at the next block by construction, no locks on the render thread (the industry-standard UI→audio flag pattern, confirmed by 2026-09-27 web research). Disabled = voice never triggers (t100 proves output identical to an all-REST bank session); banks/voices/patterns keep state; mid-stream re-enable sounds again. New API `ri_live_set_sections` / `ri_live_sections` (NULL fail-closed). t100 RED (undeclared API) → PASS; no-op mutant 4 FAILs (killed). Mask law `1u<<d` pinned (`ALL == 0x0F`). Device proof open (Dell agent redial pending at commit time).

## Device proofs closed this session (Dell E6320, 0 xruns throughout)

- Pattern-selection flips audible (empty-slot dropouts as designed; heartbeat `pend` tracks selections) — `71a04d9`.
- Delay send on-grid after the tempo-clock fix (`ri_live_render` pushes session BPM; t95 pins `eng.tempo == bpm`) — `8916144`, `71a04d9`.
- 6.1-minute soak rerun: 68,657 buffers, render_max 34% of period, graceful close (`d661d04`; log `docs/evidence/portability/dell-soak-rerun.log`). Earlier freeze unreproduced in 4+ interactive runs; leading theory = two RIAPP instances contending one AHI device (pre close-verify discipline).
- 808/909 step programming live (DSTEP lines), 909 knob sweeps, pitch-mode walk.

## Open after this article

- Activation device proof (toggle 303B off/on while playing; `mask=` ink).
- 909 demo content + new-voice timbre verdicts (owner ear).
- 303A downbeat pitch-edit audibility question (traced clean through the live-read player; decisive test = reselect slot 0, listen to beat 1).
- Visibility-selection UI is done (rail); extensible-rack engine work (classes/instances) not started.
