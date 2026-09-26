# §12.10 G8 handed to opencode: skins/mods, zoom finish, acceptance + four carry-overs

- Source: ReIncarnation commit `a646d2a` — `docs/superpowers/plans/2026-09-26-g8-opencode-prompt.md`
- Collected: 2026-09-26
- Published: 2026-09-26

## Handoff

G1–G7 are done by the Claude GUI session, with all GUI tests t60–t73 green. The last GUI phase, G8, and the open GUI debts are handed to the opencode session, which owns the engine/sequencer, as one self-contained prompt.

## G8 scope in the prompt

- **G8.1 Skins ("Mods", appearance half):** a short design doc for owner review first. E1 p. 88–97 sets the limits: a Mod changes look and drum sounds only, never function, control placement, synth sound or FX. The Mod selection is saved in the song (RBNG `MODR`). A missing mod follows spec §17 row 3. The mod switch happens only while idle. Spec §13 constraints:
  - `picture.class` datatypes;
  - MCC subclassing only;
  - `SYS:Classes/ReIncarnation/Mods/`;
  - "Classic" as default plus the LOCKED second skin "808-RI" (original art only), a blank template and an AmigaGuide SDK.
  Per-part fallback to today's procedural drawing, so a partial skin is legal. Non-reversible choices (file format, directory layout) go to the owner.
- **G8.2 Zoom finish:** 2x masters, a filtered downscale once per zoom change (a host-tested pure resampler), 150 px screen travel at every zoom.
- **G8.3 Acceptance:** bring `docs/evidence/gui/acceptance.md` up to date. Machine-drive the ReBirth-101 tutorial rows through `sendkey` and `MIDISEND`; hearing waits for the audio lane. ReBirth has no Solo (the mixer mute is p. 56). The dummy-panel generality row feeds the extensible-device-rack goal.
- **G8.4:** close the §12.10 tracker, ingest into the wiki, commit.

## Carry-overs assigned

- **C1:** wire t60–t73 (plan G1.6, overdue) + a legends grep gate + the AROS GUI TUs into `ri_audit.sh`.
- **C2 (= G6b):** render-task sample position + engine per-section/FX meter taps + comp GR. Needs an audio lane: the Dell HDA, or QEMU AC97/HDA with an AHI driver on riqemu1 (keeping the 1280x1024 rule).
- **C3:** the camd.library `mysprintf` fix goes into the Vulkan4AROS v1 patch series (branch owner coordination) and upstream. Investigate the `DEVS:Midi/debugdriver` hang and restore the parked driver.
- **C4:** the Stop law vs E1 p. 145 (arm-only first click; the Left Locator exception).

## Left to the owner

Dist exclusivity (p. 59 vs p. 157), the G5/G7 E0 items (menu modifier, arrows stop at ends, options default off, focus-bar position, CC value laws, LED timings, the p. 199 Stop/Record typo resolution), 909 tap level, GR full scale, meter floor, and OPEN-10 (ReBirth file import, legal review).
