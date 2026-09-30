# Method findings from the songs & playlists session (2026-09-30)

- Source: ReIncarnation session, 2026-09-30 (songs & playlists work, commit `547bbf8`, pushed)
- Collected: 2026-09-30
- Published: 2026-09-30
- Feature record: [2026-09-30-songs-playlists-zombie-nation.md](2026-09-30-songs-playlists-zombie-nation.md)

## Mutation testing traps

- **Shell quoting:** a mutant whose search or replace text contains a single quote (`' '`, `'/'`, `'.'`) breaks a single-quoted argument list; those mutants silently test something else and "survive". Pass such mutants in double quotes, and re-run any survivor by hand before trusting it.
- **Stale objects:** a mutant that does not compile leaves the previous object in place and reports a false PASS. The same happens when a mutant sits in a file of another build module (`engine/seq/autolane.c` is in `sched`, `engine/live.c` in `engine`), unless that module is rebuilt.
- **Redundant paths hide mutants:** a player re-arm in both `ri_core_load_song` and `ri_live_play` made the core one untestable; it was removed and the play-side one kept.

## Transport and session behaviour worth knowing

- First Stop pauses (the cursor stays); a second Stop rewinds (E1 p. 145). Only the transport state edge reaches the session; the UI's rewind click does not.
- The live session's tempo map puts tick 0 at sample 0; its sample cursor has to be re-anchored whenever the tick cursor jumps (restart, loop wrap), or events are dropped.
- At 120 BPM with 256-frame buffers every bar line falls on a block edge, which is the case that exposed the one-bar pattern-change lag. Tests that asserted the lag had to be updated deliberately.

## Mixing and analysis

- The mixer faders are square-law: gain = (v/127)^2. The master compressor adds make-up gain (the demo went from peak 0.49 to 1.99 with it on), so balance with faders and patch levels first.
- Stacked Levi notes are loud: a 4-note lead with 2 oscillators peaked around 6x the other devices at default patch level.
- Headless checks that worked: per-4-bar RMS/peak to follow an arrangement, the first non-zero sample for start latency, autocorrelation per step for pitch. Harmonic-product-spectrum pitch guessed octaves wrong; autocorrelation was reliable except on rests. A per-bar distinct-note song is a quick test that pattern changes land on the right bar.

## Side notes

- The Dell screen reported 1366x768 in the app's zoom log during this run.
- The AROS pull-request reviews for #1313 (getrusage) and #1314 (strto*_l) are tracked in `docs/2026-09-24-improvement-todo.md` under "Upstream (AROS pull requests)".
