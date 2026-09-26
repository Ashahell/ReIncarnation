# §12.11 G9 live app handoff to opencode ("first sound from the panel")

- Source: ReIncarnation session, 2026-09-26
- Collected: 2026-09-26
- Published: 2026-09-26
- Prompt: `docs/superpowers/plans/2026-09-26-g9-opencode-prompt.md` (commit `b8c00c2`)

## Finding that motivated G9
- The engine, sequencer, formats and GUI are all complete in isolation, but **nothing plays live**:
  - `audio_ahi_play.c` renders a whole song, then streams it with blocking `CMD_WRITE`;
  - `au_render_frames` is the legacy mono 303 path, not the engine;
  - `RISECT` has no engine;
  - `app/main.c` is still a bare Intuition window.
- The owner noticed this on the Dell ("the 303 doesn't produce sound").

## G9 scope as handed off
- **G9.0** — design note. Recommended E0s:
  - the session runs at the negotiated device rate (the Dell's best mode is 44100/16 per M1.1) while offline export stays 48 kHz;
  - the control plane is an SPSC ring of lane keys, with newest-per-key coalescing on overflow, counted.
- **G9.1** — control plane (host, t80). The address space is exactly the automation lane keys (`ri_ctlreg_auto_id`, gated by `ri_auto_allowed`); one path for knobs, MIDI CC and automation.
- **G9.2** — live session core (host, t81). Per buffer: snapshot and publish apply, control drain, player block, automation emit, §8 merge, engine render. Live must equal offline bit-exact at the same rate across buffer sizes, with automation and control moves.
- **G9.3** — AROS render Task, plus a low-level AHI `PlayerFunc` that only calls `Signal()`, a double buffer, xrun counting (spec §4.2, §17 #5), and P-21 priority measured.
- **G9.4** — the `RIAPP` application. It closes G6b: the meters and position come from engine/transport snapshots. Owner listening proof plus a 5-minute soak on the Dell.
- **G9.5** — automation recording from the panel: record, sweep, publish, save and reload through ATRK.

## Carry-overs (G10) and open owner decisions
- **G10 carry-overs:**
  - per-module skin choice (the song chunk needs owner review);
  - the proof-app zoom for grouped views and the 2x fit (E0 recommendation: cap at the largest factor that fits the screen);
  - the G8 deferred minors;
  - the 909 knobs without an engine param (28 unrecordable);
  - camd patch landing plus the debugdriver hang.
- **Still open for the owner:**
  - dist exclusivity;
  - G5/G7 E0 items;
  - 909 tap level;
  - GR full scale;
  - meter floor;
  - OPEN-10;
  - 2x zoom policy;
  - the per-section skin chunk.
