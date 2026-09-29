# GUI round 3: S4 master, tab keys, visual/wiring pass, S5 auto-fit zoom

- Source: ReIncarnation repo commits `62665dd`, `2a46c8b`, `945065f`, `61c1464`, `b92876a`, `dd49cf0` (OpenCode, following plan `31694d5`), 2026-09-29
- Collected: 2026-09-29
- Published: 2026-09-29
- Related: `2026-09-29-gui-round3-s1-s3-and-interop-requirement.md`, `2026-09-28-rack-pass-2-and-gui-round3-plan.md`
- Specs/plans: `docs/superpowers/plans/2026-09-28-gui-round3-opencode-prompt.md`
- Evidence: `docs/evidence/gui/hw-look/` (README decision record, `dell-v2-mix.png`, `dell-v2-drums.png`, `dell-v2-drums-edge.png`, `dell-v2-synths.png`), `gui/zoomfit.c`, `tests/unit/t121_zoomfit.c`

## S4: master fader, live-only monitoring path (`62665dd`)

- RED: `FAIL t115_master_live.c:104 master live-only (stubs refuse)`.
- Live-only key `0x0B50` admitted (compile-checked vs lane table); unity bit-neutral; 64 scales by square law; peaks publish as twins; master touch sounds with no lane writes; cutoff still records.
- Mutants killed: automation drop -> ratio 1.0; lane touch -> no send; open gate -> comp/junk admitted. t60/t81/t82/t83/t92/t93 green.
- Engine: master gain (unity-skip, bit-identical neutral) + L/R taps; snapshot `master_peak[2]`; bridge MASTER FADER -> live key.
- Dell: ABIv11 `RAM:RIAPPS4` (`open=1 rack=1 tabs=5`, AROS link clean, 0 UND): MASTER strip renders (fader at 100, meters dark at rest).
- E0 ledger (superseded same day): master level is monitoring, not song data; default 100 (registry def). Owner decision 2026-09-29 reverses this: master becomes song data (S4b pending).

## Tab keys (`2a46c8b`)

- Owner 2026-09-29 (decision #2): Ctrl+1..4 switch Synths/Drums/Levi/Mix; FX stays mouse-only.
- Decode in `ri_key_decode` (Ctrl and RAmiga share the branch); request travels in `RIPanelUI.tab_req` (init -1, EAT-only, no canvas state); main loop polls and tab_switches. 4 maps to Mix (performance page).
- Owner-confirmed on the Dell. Open diagnosis: a click/pop on the keypress; prime suspect is an xrun in the tab-show relayout; evlog buffer counts + `xruns=` will confirm.

## Owner visual pass + wiring audit (`945065f`, `61c1464`, `b92876a`)

- Three small fixes (owner 2026-09-29 Dell verdicts), proven on the Dell as `RAM:RIAPPS6`:
  1. Mixer strip names (TB-303 A etc.) + MASTER read one face above the section face (new `ri_art_text_c_face`; z2 stays L). RED: t93 header face sec=4.
  2. 909 row padded right 1460 -> 1472 Q so the Drums rows align with the 808 (content left-anchored at manual positions). RED: t61 909 row matches 808.
  3. 303 Note/Pause toggle 132 -> 110 Q (LEDs stand clear). RED: t61 note/pause width.
- Wiring audit: new t116 proves 189 controls wired end-to-end (21 wide classified). Found live: the transport tempo knob was display-only (session bpm fixed at init) -> new `ri_live_set_bpm` + app follow in sync_transport.
- Evidence commit records the 2026-09-29 owner decisions (song-data master, Ctrl+1-4, Fit/menu/persist) and the click/pop diagnosis state.

## S5: auto-fit zoom plus zoom choice (`dd49cf0`)

- Pure `gui/zoomfit.c`: largest 2/1/0 whose geometry-derived content (widest page per tab via `ri_tab_devices`, transport compact, rail/tab/root furniture, root gaps) plus chrome fits the screen; fail-closed 0.
- Hand-computed pins: Mix binds width everywhere (954 content at z0); Dell 1366x768 -> 0, riqemu1 1280x1024 -> 0, 800x600 -> fallback 0, 1920x1080 -> 1 (height-bound), 2560x1440 -> 2.
- App: Fit measures the frontmost public screen at startup (canvases at fit, transport compact); View menu 1x/1.5x/2x/Fit with checkmarks (fail-soft); InitChange + `MUIA_RSection_Zoom` per canvas + ExitChange; choice persists in `ENVARC:ReIncarnation/zoom` (PAL PREFS path, ledgered deviation from the literal `ENVARC:RIAPP/zoom`); S2 legends follow zoom automatically.
- Tests: new `t121_zoomfit` (content sizes, five screens, chrome mutation, parse/format). Mutants killed (ascending loop picks smallest; dropped root inner caught a real model bug pre-fix). t92/t93 pins unmoved (S5 paints no pixels). t60/t61/t70/t76 green.
- ABIv1 RISECT+RIAPP link clean (-Werror, 0 UND); ABIv11 0 UND; portable build holds (zoomfit + visdev/tabpages registered).
- Dell (partial, lane flaky): ABIv11 `RAM:RIAPPZ5` booted live-audio clean. Log, verbatim:
  - `RIAPP zoom: mode=-1 zoom=0 screen=1366x768`
  - `RIAPP panel: tabbed Synths/Drums/Levi/Mix/FX + transport, rack bay (open=1 rack=1 tabs=5)`
- Pending lane recovery: all-page captures, z2 Mix, menu/persist try.

## Lane + audit findings (2026-09-29)

- Dell lane went flaky mid-session: a `ui-capture` job failed with `ConnectionResetError: [Errno 104] Connection reset by peer` (agent `e6320`, session 3, empty log); follow-up `status`/`ui-windows` jobs queued without results while the agent stayed busy.
- Full audit green except pre-existing t75 bg03 (proven identical at pristine HEAD; sibling skin asset, not this work).
- E0 ledger: chrome 32x72 fail-safe generous; furniture width estimates never bind; explicit zooms may overflow small screens.

## Open (owner / next session)

- S4b: master as song data (reverses the S4 monitoring E0).
- S5 captures + owner menu/persist try when the lane recovers.
- Click/pop-on-Ctrl cause (evlog buffer counts + xruns).
- Owner ear/eye verdicts: tempo audible, MASTER loudness, drag feel, legends, remaining grey.
