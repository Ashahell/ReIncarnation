# Levi audibility round: the two filters and the osc-env bias (2026-11-09)

This is the ear list for the report the 2026-10-13 triage left open:

> "OSC ENV Level & Bias makes no audible difference; same for the Digital and
> Analog filter panels." — owner, 2026-10-13

That report predates P-KEY1, so on that build the keybed did not play: the only
Levi sound was the song. This round is the first one where the question can be
asked properly.

## Why this is an audibility round, not a wiring job

Measured against the registry on 2026-11-09:

- `ri_ctlreg_section_count(RI_SEC_LEVI)` → **252 rows, 180 engine-bound**. The
  72 unbound rows are panel-local actions (37 pitch keys, 16 step buttons, the
  Lane/Step/Back/display set, Op/Algo Num/Module/Master display, Page Up/Down,
  the LFO step gate, and the 8 Master Control encoders, which send the selected
  page's target).
- The 18 rows with no panel row and no page slot are exactly the v1 legacy set
  (`ri_slevi_legacy`, pinned by `t61`): Ratio, Algo Target/Morph, Op Mode,
  Filter Type `0x0e08`, Env A/D/S/R/Loop, Matrix R1–R8. They stay engine-bound
  for old songs/automation but are superseded by the v2 pages, so they have no
  control by design.
- The digital filter render reads `dtype` / `dmorph` / `dpost` / `vorder`
  (`engine/dsp/levi.c` ~2507–2566); the analog filter reads `cutoff2` /
  `reso2` (~3955, ~4264); the bias lands in the amp:
  `amp = p->init + (envl + v->bias_envl) * env_out(...)` (~3422).

So every control below is wired. What has never been tested is whether a
sustained sound *hears* it.

## Rig

Dell lane, `build=d676bba` (1 221 720 B, sha `cfb4c086…`). Nothing to stage —
the lane already runs this build.

The song is loaded and parked at its end. **Replay = KP0 (Stop), then KPENTER
(Play)** — that is the P-TR2 fix and it starts at bar 0. If the song stops
during the round, do the same pair.

## The source: the song, not a held key

A held key and a knob are both on the one pointer. A keybed key latches only
until the mouse button comes up (`SELECTUP` sends the note-off), so a note
cannot be held while the pointer turns a knob. Rather than make this a
two-handed impossibility, use the song: its Levi part is a 4-note chord on 8 of
the 16 steps at 140 bpm (bank dump `docs/evidence/midi/pkey4/levi-bank.txt`),
about 214 ms apart with a 155 ms release — effectively continuous — and it is
exactly the signal the original report
was made on. If a MIDI keyboard is plugged into the lane, hold one note instead;
the steps are identical.

## The steps

The four controls are on the top strip of the Levi panel, under three printed
headers: **OSC ENV LEVEL & BIAS**, **DIGITAL FILTER**, **ANALOG FILTER**. The
same filters also have pages reached from MODULE SELECT; step 1 tests only the
page path, steps 2 and 3 test both doors.

| # | where | control | `CTL` id | do | listen for |
|---|---|---|---|---|---|
| 1 | DIGITAL FILTER page (MODULE SELECT → DIGITAL FILTER) | MASTER CONTROL 1 = **TYPE** | `1261` | From the default **LP 12**, turn up three steps to **VOWEL**, then back. | A hard change of character, not a nuance: LP 12 is a plain low-pass, VOWEL is a formant colour. This is the strongest single test of the page-encoder path and the digital filter render. If all models sound the same, stop and say so. |
| 2a | top strip, **DIGITAL FILTER** header | **CUTOFF** (first knob of the group) | `1200` | Sweep 0 → 127, then back. | Closed/dull at the bottom, open/bright at the top. |
| 2b | DIGITAL FILTER page | MASTER CONTROL 3 = **CUTOFF** | `1200` | Sweep the same control from the page. | The same sound as 2a — same registry control, second door. If the knob works and the encoder does not (or the reverse), one door is broken; the log plus your report will say which. |
| 2c | (optional) either door | **RESO** | `1201` | At a mid cutoff, raise reso to ~100. | A whistle/ring at the cutoff, strongest near the top of the sweep. |
| 3 | top strip, **ANALOG FILTER** header, and its page | **CUTOFF** (panel knob; page encoder 3) | `122c` | Sweep 0 → 127, then back. | A sweep with a different character from the digital one — it is a second, separate filter after it. If you can, do it with the digital cutoff open (127) so the analog filter is what you hear. |
| 3b | (optional) | **RESO** | `122d` | As 2c. | Same ring, different voice. |
| 4 | top strip, **OSC ENV LEVEL & BIAS** | **ENV LEVEL** (first of the four) | `124e` | **64 is neutral.** Turn to ~0, then ~127, then back to 64. | Level and body: at 127 the envelope adds to the initial level, at 0 it subtracts. This is a live multiplier, so the playing song changes under your hand. |
| 4b | same group | **ATTACK** / **DECAY** | `124f` / `1250` | Turn each up and down; the song retriggers, so no re-strike is needed. | Attack: softer or harder front. Decay: longer or shorter notes. These are subtler than Env Level; if Env Level is clear and these are not, say so — that is a partial finding, not a failure. |

Nothing here changes the patch: the values are live and the song is the
reference. Do not re-test with a different patch.

## Reporting

Per step: **yes / no / subtle**, and what changed if subtle. Where a step has
two doors (2a/2b, 3), say which you used or whether you did both. A step that
is audible but small is a different finding from a step that is silent; please
keep that distinction.

## What the log will prove

Every move logs `CTL <id>=<value>` (the registry id, section `0x12`). After the
round I pull `RAM:RIAPP-EV.LOG` and check that each id above appears and that
the value covered the range you described, e.g.:

```
CTL 1261=17          the TYPE reached VOWEL
CTL 1200=0           the digital cutoff reached the bottom
CTL 1200=127         ... and the top
CTL 122c=127         the analog cutoff reached the top
CTL 124e=0
CTL 124e=127
```

The log proves the control moved. The ear decides whether it was heard. Both
together are the evidence; neither alone is.

## If a step is silent with the log showing the sweep

That is a real render defect, not a UI one. The next move is a measurement at a
fixed note and velocity across that control's range — the same style as the Dist
Shape table in the ledger — and then a TDD fix. No constant will be moved on a
guess.

## Not in this round

- The 18 legacy v1 rows above: engine-bound, no v2 control by design.
- The patch cluster (Patch knob, arrows, SAVE/BROWSE/FAVORITE/SYSTEM/RANDOM):
  there is no patch library (P10 is unbuilt), by owner decision.
- The left-column MASTER knobs: no engine target; `t220` keeps them unlaid-out.
- FX (Delay/Reverb/Mod): measured separately (P-FX1…P-FX4).
