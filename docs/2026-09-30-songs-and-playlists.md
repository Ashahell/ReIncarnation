# Songs, playlists and the Zombie Nation demo (owner 2026-09-30)

## Request

"We want 'Zombie Nation' by 'Kernkraft 400' to be one of our demo songs. It
needs to be as musically faithful to the original as possible with our
current synths (including the Levi), without vocals. Use all synths to their
full capabilities, download free samples if needed. There are probably free
midi versions of the song already, use the best of those as a base ... There
will be other demo songs, so we need to be able to load songs and
playlists."

## Owner decisions

- **Rights (2026-09-30):** the repository is public and the song is someone
  else's music (the hook comes from David Whittaker's "Lazy Jones"). Chosen:
  the faithful version stays **local only** — `songs/local/` is git-ignored
  and the song goes to the Dell, never to GitHub. The song/playlist support,
  tools and an own demo song are committed.
- **Levi patterns in songs (2026-09-30):** RBNG could not store Levi
  patterns. Chosen: BANK chunk kind 2 (Levi), 16 steps x {6 lane notes, on
  mask} = 112 bytes per pattern, written as VERS 1.5. Old readers refuse
  such files as they do any unknown kind; every existing file keeps its
  bytes (the audit's corpus goldens are unchanged).

## What was built (committed)

| Piece | Where | What |
|---|---|---|
| Song script | `project/songscript.{h,c}` | Line-based text source for a song: patterns for all five devices, song track, automation by key or by registry name (`SET 0 303a.Cutoff 40`). Errors name the line. |
| Compiler | `tools/rbsc.c` | Script -> RBNG through the real codec, then reads it back. Deterministic. |
| MIDI arranger | `tools/mid2rbs.py` | MIDI + arrangement map (JSON) -> song script: quantises to the 16-step grid, folds each device's bars into at most 31 patterns (slot 0 silent), writes the track, appends a hand-written sound program. |
| Headless player | `tools/songplay.c` | RBNG -> WAV through the app core (the live path), 909 pack bound like the app; prints peak, RMS, clipping, xruns, section peaks; `--playlist` checks every entry. |
| Playlists | `project/playlist.{h,c}` | `RBPL 1` text format, relative/absolute (host and AROS volume) paths, titles, next/previous with wrap. |
| App core | `app/core/riapp_core.{h,c}` | `ri_core_load_song`: banks, track, tempo, automation lane (tick-0 sound program chased at play); song length (through the last sounding bar) and devices. The automation lane is now wired into the live session. |
| App (AROS) | `app/riapp.c` | Songs menu (Load Song/Playlist via ASL, Next, Previous, Pattern Mode), `SONG=` / `PLAYLIST=` start arguments, Song mode (the song track owns the selections; the pattern panels only show them), panels and knobs follow the loaded song, playlist auto-advance after the last bar. |
| Demo song | `songs/demo/riapp-demo.{rbs,rbng}`, `songs/demo/demos.rbpl` | The built-in demo as a 16-bar song (own material). |
| Tests | `tests/unit/t140_songs.c` | Script statements and errors, Levi bank round trip and VERS 1.5, Levi + automation read-back, playlists, core load + play on the bar grid, restart, pause/resume, loop wrap. 26 mutants killed. |
| Audit | `scripts/ri_audit.sh` | t140; every `songs/demo/*.rbs` recompiles to its committed `.rbng` byte for byte, plays headless with 0 clipped samples and 0 xruns; the demo playlist loads. |

## Engine bugs found and fixed on the way

1. **Pattern changes landed one bar late** whenever a render block ended
   exactly on a bar line (at 120 BPM with 256-frame buffers, every bar).
   The player adopted the next selection at the block end, before that
   downbeat's selection was sampled (it belongs to the next block).
   Fix (`engine/seq/player.c`): an occurrence that ends exactly at the
   block edge waits for the next block. t74/t96 asserted the old state and
   the old one-bar lag; both were updated deliberately (t74's own comment
   already said content from the downbeat on is the new slot).
2. **Play started on stale player state**: the player was only initialised
   when the banks were set, so a loaded song (or a restart) started on the
   previous selection. Fix (`engine/live.c`): play from STOPPED re-arms the
   player at the cursor bar, and a resume inside a bar picks every pattern
   up at the cursor phase.
3. **Restarts and loop wraps dropped audio**: event samples come from the
   tempo map (tick 0 = sample 0) while the session's sample cursor kept
   running, so after a stop/restart or a loop wrap the first events fell
   behind the cursor and were discarded. Fix: the sample cursor is
   re-anchored to the tick cursor at play-from-stop and at the loop wrap.
4. **RBNG VERS minor**: a Levi-track song with automation wrote minor 2
   next to a 5-wide STRK and could not be read back. The minor is now the
   highest level any carried feature needs.

## How the Zombie Nation arrangement was made (local)

1. **Base:** four free MIDI versions were downloaded (BitMidi, MidisFree);
   two were duplicates, one was another song. The best is the full 153-bar
   sequence by Arne Mulder (2000, 18 tracks, notated at 70 BPM = 140 BPM
   half-time, B-flat minor, which matches Beatport's key). A 15 s loop
   version served as a cross-check of the hook.
2. **Analysis:** every track was folded into distinct bar patterns: lead
   A-B-C-D (the hook B-flat, D-flat, E-flat, F; then G-flat, F, D-flat;
   E-flat, D-flat, F, B-flat), octave bass figures, a breakdown pad, the
   drum kit (kick, clap, tambourine 16ths, china/crash, guiro) and FX hits.
3. **Arrangement map** (`songs/local/zombie-nation/zombie-nation.json`):
   - 303A: the B-flat octave bass (two MIDI bass tracks);
   - 303B: the off-beat counter bass, then the hook melody an octave down
     through the Dist unit (the gritty lead double);
   - Levi: the full lead (melody + octave pulses, up to 4 notes a step) as
     a custom algorithm of a saw and a detuned square into a driven 24 dB
     ladder, and the breakdown pad with automated slow envelopes;
   - 909: kick, clap, hats (the tambourine line), crashes and impacts;
   - 808: the percussion colour (guiro -> cowbell, FX hits -> cymbal/rim).
   Result: 151 bars, 3/4/25/3/6 patterns (303A/303B/909/808/Levi).
4. **Sound and mix** (`zombie-nation-sound.rbs`): tick-0 settings for every
   device plus breakdown/return moves. Levels were set by measurement
   (square-law faders, the Levi's patch level, no master compressor, which
   added make-up gain) to peak 0.85 with no clipping over 261 s.
5. **Verification:** pitch tracking of an isolated 303B stem confirmed the
   hook notes on the right bars; per-4-bar RMS follows the arrangement
   (intro, lead, full, breakdown, build, lead, full, outro).
6. **Samples:** none needed: the drum parts map onto the 808 and the
   clean-room 909 pack (all 11 voices), and the engine has no sample-lead
   device.
7. **Dell:** `RAM:RIAPP PLAYLIST=RAM:songs/demos.rbpl` loads the playlist
   (Zombie Nation, then the RIAPP demo) and plays in Song mode.

Rebuild it after changing the map or the sound program:

```
python3 tools/mid2rbs.py songs/local/zombie-nation/zombie-nation.json songs/local/zombie-nation/zombie-nation.rbs
rbsc songs/local/zombie-nation/zombie-nation.rbs songs/local/zombie-nation/zombie-nation.rbng
songplay songs/local/zombie-nation/zombie-nation.rbng /tmp/zn.wav
```

## Limits (E0)

- Levi patterns retrigger every "on" step and cannot tie notes across
  steps; the arrangement plays lead notes as 16ths shaped by the release.
- The 303 range is C1..C4 around its base note; notes outside fold by
  octaves (the melody double is played an octave down to stay in range).
- One song tempo (no tempo changes); songs end after their last sounding
  bar plus one bar of tail.
- Knob display follows tick-0 values only; later automation moves the
  sound but not the knobs.
