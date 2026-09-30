# Songs and playlists

ReIncarnation plays songs (RBNG files) one at a time or from a playlist
(RBPL files). A song holds the pattern banks of all five devices (303A,
303B, 808, 909, Levi), the song track (which pattern each device plays in
each bar), the tempo and an automation lane (the sound program at tick 0
plus any moves during the song).

## Folders

- `songs/demo/` — demo songs that ship with ReIncarnation (own material,
  CC0). Each `.rbng` is compiled from the `.rbs` script next to it; the
  audit recompiles every script and requires identical bytes.
- `songs/local/` — private songs that must never be published (covers of
  other people's music, work in progress). Git-ignored (`songs/.gitignore`).

## Playing songs in the app

- Start with a song or a playlist: `RIAPP SONG=<file.rbng>` or
  `RIAPP PLAYLIST=<file.rbpl>` (e.g. `Run >NIL: RAM:RIAPP PLAYLIST=RAM:songs/demos.rbpl`).
- Or use the **Songs** menu: Load Song..., Load Playlist..., Next Song,
  Previous Song, Pattern Mode.
- A loaded song switches the transport to Song mode and sets the tempo; the
  pattern panels show the bar-0 patterns and the knobs show the song's
  sound. After the last sounding bar (plus one bar of tail) a playlist
  moves on to the next song and wraps at the end; a single song stops.
- Pattern Mode (menu) hands the pattern selectors back to the panel.

## Playlist format (`.rbpl`)

```
RBPL 1
TITLE Demo songs
riapp-demo.rbng | RIAPP demo
../local/my-song.rbng
```

One song per line, optionally `| title`. Relative paths are relative to
the playlist's folder; paths with a volume (`Work:x.rbng`) or a leading
`/` are used as they are. `#` starts a comment. At most 64 songs.

## Making a song

1. Write a song script (`.rbs`, grammar in `project/songscript.h`):
   `P303`, `PDRUM`, `PACC` and `PLEVI` lines define patterns, `TRACK`
   lines arrange them by bar, `SET` / `AUTO` lines set sounds and mix
   (`SET 0 303a.Cutoff 40`, `SET 0 mix-levi.Level 90`). Slot 0 of every
   device is silent, so unarranged bars are quiet.
2. Or generate the script from a MIDI file: `python3 tools/mid2rbs.py
   MAP.json OUT.rbs` (needs `mido`). The map says which MIDI tracks play
   on which device, over which bars, transposition, note ranges and drum
   lanes; it appends a hand-written sound program (see the docstring).
3. Compile: `rbsc IN.rbs OUT.rbng` (built like the other tools:
   `gcc $CFLAGS -o rbsc tools/rbsc.c /tmp/ri/build/*.o -lm -lpng -pthread`).
4. Listen/check headless: `songplay OUT.rbng OUT.wav` renders through the
   same app core as the live app and prints peak, RMS, clipped samples,
   xruns and per-section peaks; `songplay --playlist X.rbpl` checks that
   every entry loads.

Keep the master peak below 1.0 (no clipped samples): the faders are
square-law ((v/127)^2), the Levi is loud with stacked notes (lower its
`levi.Patch_Level`), and the master compressor adds make-up gain.

The record of how this was built (and the Zombie Nation arrangement) is
`docs/2026-09-30-songs-and-playlists.md`.
