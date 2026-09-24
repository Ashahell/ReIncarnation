# 2026-09-24 — Third-party open ReBirth alternatives: consultant note, verification, and what we touch

> Source: consultant brief (via user) + agent web verification 2026-09-24
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
Reference. A consultant listed open ReBirth-adjacent projects; two
verified, two did not match their descriptions. This article locks
what we checked, what stands, and the license boundaries for our
tree. No code, no behavior change.

## Verification (agent, 2026-09-24)
- **JC-303 — CONFIRMED.** `github.com/midilab/jc303`: JUCE port of
  Robin Schmidt's Open303 DSP, VST2/VST3/LV2/CLAP/AU, Win/Mac/Linux.
  License is MIXED in reports: JC-303 shell GPL, Open303 DSP core
  MIT (Robin Schmidt's original). Treat as two separate artifacts.
- **RP-8 — CONFIRMED.** `luchak.itch.io/rp8` (PICO-8, CDM coverage
  2023-03): two "vaguely 303-inspired" monosynths + 808-ish drums,
  16-step sequencer with slide/accent, song mode, 8-bit 5512 Hz.
  Workflow reference only — its own pages say "vaguely inspired".
- **RE:BORN 338 — UNVERIFIED.** Two searches surfaced no repo, demo,
  or Node.js project under that name. Do not rely on it until
  someone produces a URL. (Possible name confusion; not our problem
  to resolve.)
- **jsynth (as described: 303-style synth playing .rbs) — NOT FOUND.**
  The only `jsynth` on GitHub (`alex-zhilkin/jsynth`) is a generic
  web synth (sine/square/saw + filter/delay), NOT a ReBirth .rbs
  player. The .rbs ecosystem itself is real (peff mod archive,
  Nordbeat ReMaker MIDI extraction), but the consultant's entry
  does not match a locatable project. Do not cite it.
- **ReBirth history — CONFIRMED** (Wikipedia): dual-303 + 808 in v1,
  +909 in v2.0; desktop discontinued 2005; iOS version REMOVED 2017
  after a Roland IP claim. This is the precedent behind our
  clean-room rule, not a new fact.

## Assessment for our WBS
- **303 DSP:** Open303's *published circuit analysis* (decade of
  scope traces + KVR debate) is legitimate study material for our
  own filter/squelch work; the MIT-licensed DSP core may serve as
  an EAR/BEHAVIOR reference (A/B our render against it). GPL code
  (JC-303 shell) MUST NOT enter our tree. Nothing here ports
  directly anyway (JUCE/VST vs our AROS-targeted C engine).
- **UX/workflow:** RP-8's song mode ("almost every control
  automatable") is one more vote for our M2.6 automation-record
  scope; pattern/song parity is already spec'd. No design change.
- **Formats:** .rbs import is NOT in scope. If ever proposed, it
  starts as a reader-only spike against our MIDI persistence
  (M2.6), with the format's IP status checked first.
- **Legal (unchanged, reinforced):** original artwork, no
  Roland/Propellerhead assets, distinct external name — exactly
  the 2026-09-20 plan article's stance. The 2017 iOS takedown is
  why. The consultant's "fully open source, run locally" framing
  changes nothing about OUR obligations.

## Files
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`
- No repo files touched. No URLs added to code or docs beyond
  this article (per truthfulness rule: unverified names get no
  links).
",
