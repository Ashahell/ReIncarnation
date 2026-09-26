# Lane proof round: G7 re-proof, T2 pixel-identity, Dell 0 xruns (§12.12/§12.11)

- Source: ReIncarnation session, 2026-09-26 (riqemu1 + Dell E6320 lanes)
- Collected: 2026-09-26
- Published: 2026-09-26
- Evidence: `docs/evidence/portability/t2-lane-proofs.md`,
  `docs/evidence/gui/img/2026-09-26-t2-identity-{old,new}-*.png`,
  `docs/evidence/gui/2026-09-26-riremote-trace-t5-reproof.txt`,
  `docs/evidence/gui/img/2026-09-26-dell-riapp-t4.png`

## G7 remote re-proof (T5): MATCH

Current RISECT + MIDISEND (PAL send/receive) through real camd.library on
riqemu1, same `remote1/remote2.mid.txt` (41 + 2 messages). Final trace state
equals the committed G7 trace field-for-field. T5 device re-proof closed.

## T2 pixel-identity (riqemu1): PROVEN

OLD RISECT (`626d217`, pre-move painters) vs NEW (HEAD replayer), all six
demo modes, same window rects, scale-2 captures cropped to the window:
303/mix/fx 0 diffs; 808/909/tr showed 20/36/32 diffs that an OLD-vs-OLD
control reproduced pixel-for-pixel (full overlap), and NEW-vs-OLD2 is
byte-identical. The first OLD run was the outlier (lane had just come off
an sb128 Software Failure episode). The port contributes zero diffs.
Open: zoom sweep + 808-RI skin (Dell carries Mods; replayer is
zoom-agnostic by construction).

## T4 Dell run: 0 xruns on hardware

v11 RIAPP (64 TUs, 0 UND) on the E6320: AHI low-level 48 kHz/256 frames,
`RIAPP play`, 2885 buffers, 0 xruns, worst buffer 33% of period. Panel
paints (focus orange, green LEDs in capture). 909 unbound as designed.
Open: 5-minute soak, owner by-ear listening, knob/click feel.

## Lane lessons

- A `Software Failure` was found open (`RIAPP render`, sb128
  `DriverInit+0x135` — known fault, another session's `PROBE_AHI`
  processes were on the lane). Dismissed via HMP `sendkey esc`, lane
  verified clean. Rule: check `Status` for foreign processes AND the
  window list for dead requesters before trusting a lane capture.
- First run after lane turbulence can be an outlier: always run an
  OLD-vs-OLD (or NEW-vs-NEW) control before attributing diffs to a change.
- `RAM:` files persist across jobs (trace/log appends); close windows
  before `--get` (open-file get was REFUSED once, succeeded after close).
