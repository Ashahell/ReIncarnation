# The Knife (Genesis) as deep house — a faithful-progression re-cut, and what it cost to build one

- Source: ReIncarnation session, 2026-10-02 (owner request: "The Knife by Genesis, modern Deep House, progression faithful, arrangement changed, no vocals, consult the LLM-wiki, research online")
- Collected: 2026-10-02
- Published: 2026-10-02
- Prior: [2026-09-30-songs-playlists-zombie-nation.md](2026-09-30-songs-playlists-zombie-nation.md) (the songs pipeline and the rights decision), [2026-10-01-dell-deploy-abiv11-usb-stick-layout.md](2026-10-01-dell-deploy-abiv11-usb-stick-layout.md) (the stick layout this deploy reuses)
- Repo: `songs/local/the-knife/` — private, git-ignored, same rights position as Zombie Nation

## Request and the standing rights answer

"We need you to work on a new demo song. Take 'The Knife' by Genesis and create a
modern Deep House version of it. Progression wise the song needs to be faithful to
the original, but change the arrangement as needed. No vocals."

The 2026-09-30 owner decision already covers this case and was not reopened: the
repository is public and this is someone else's music, so a faithful arrangement stays
in the git-ignored `songs/local/` and ships only to the Dell. The song/playlist
support and the tools stay committable; the arrangement does not. Everything below was
built under that rule and nothing was committed.

## The research: what "faithful progression" actually means here

Two public sources plus the MIDI itself settled it, in that order, and the MIDI
overruled both where they disagreed.

- **Songparts** (`songparts.com/songs/genesis/the-knife`): G# minor, 135 BPM, 4/4,
  8:48. This is the Ab-minor studio version of *Abacab* (1982) — the key is the first
  load-bearing fact, because the 303s fold everything into C1..C4 around E0.
- **Two ChordU transcriptions** disagreed with each other: the live version is listed
  as C minor at 146 BPM with chords Abm/Ab/Ebm/Db/Gb, the "Official Audio" one as
  Ab/Abm/Eb/Db/C/Bb/Bbm. Neither is usable on its own.
- **The MIDI won.** The free Simon Goodwin transcription (midi-karaoke.info,
  `20e6a739.mid`, 67,803 bytes, format 1, 11 tracks, ppq 120, 8:36) carries a
  "Meaty Organ", "Floaty Organ", "Chunky Bass", two "Fuzz Guitar" channels, a "Flute"
  and General MIDI drums. Track 1 names the song, track 9 credits Goodwin. That is a
  real transcription of the studio arrangement, not a karaoke file: it has the organ's
  harmony, the fuzz guitars and a flute carrying a genuine melodic line.
- **Cross-check:** the Chroma/HPS pitch profile of the bass stem of the finished
  ReIncarnation render reads Ab → Bb → C → Ab → Ab → Bb → C for the harmonic blocks
  in exactly the bars the map assigns them. The progression is not asserted from the
  chord sheets; it is measured back out of the audio.

So the source of record is the MIDI, and the chord sheets were only useful for
confirming the key and the tempo.

## What the song's harmony actually is

Folding all 150 bars of the transcription into blocks (organ track 2, bass track 4)
gives a much simpler picture than the sheets suggest, and it is a pedal-based one:

| Source bars | Block | Bass | Organ figure |
|---|---|---|---|
| 1-8 | the hook riff | Ab pedal | Eb-Ab-B / E-Ab-Db / Eb-Gb-Bb / Eb-Ab-B, four stabs a bar |
| 9-12 | Bbm + bVI | Bb pedal | F-Bb-Db / Gb-Bb-Eb / F-Ab-C / Bb-Eb-Gb |
| 13-16 | Cm/Fm | C pedal | Eb-C-G / C-F-Ab / C-Eb-G / D-Bb-G |
| 17-24 | the riff again, Abm7 | Ab pedal | same shape with C and F instead of B and Db |
| 29-32 | the bright block | C pedal | E-C-G / C-F-A / C-E-G / D-B-G (Cmaj7 / Am7) |
| 41-44 | the bridge walk | C→Bb→G→Gb→F→Eb | chords over a descending walk |
| 45-48 | the descent | Ab, Gb, E, Bbm | one held chord a bar |
| 97-100 | the chorus | Bb pedal + Ab-F turnaround | the Bbm figure with an Ab-F bass turn |
| 130-137 | the "we have won" hook | Ab then Gb | the vocal line, flute |

The chorus line is the giveaway that this is Ab minor with a Neapolitan-ish flat-II
colour, not the C minor of the live arrangement: bar 133 of the transcription descends
Ab → Eb5 → Db5 → B → Ab → Gb → F over the Ab pedal. That single bar is the hook, and it
is the reason the re-cut keeps an Ab pedal under almost everything.

## Device roles (one job each, all five devices)

- **303A — sub bass.** Genesis' own bass figures (roots on 8ths, with the Ab-Gb-Eb-Db-Bb
  turnaround figure in source bar 18 and the Bb-Ab-F turn in the chorus block),
  transposed down an octave so it sits at Ab1/Bb1/C2 (52-65 Hz).
- **303B — the riff's top line**, an octave under the organ through the Dist unit:
  Genesis' fuzz-guitar grit standing in for the doubling the original gets from the two
  fuzz channels.
- **Levi — the percussive organ.** The 3-4 note stabs of the hook figure as chord stabs,
  which is what Genesis' "Meaty Organ" actually is; from bar 72 the same patch opens
  into a sustained lead and carries the "we have won" line at written pitch.
- **909 — four-on-the-floor.** Kick, clap on 2 and 4, offbeat 16th hats, open hat, and
  Genesis' own cowbell/rim/toms in the fill positions.
- **808 — the tribal colour.** Cowbell 16ths and rim, the percussion layer the original
  gets from the live kit's tambourine and guiro.

## The arrangement (104 bars, 124 BPM, PPQ 96, ~3:23)

Tempo is the one number the re-cut changes on purpose: 135 is a rock tempo and 124 is
the deep-house tempo the genre sits at. The riff is quarter-note stabs, so it transfers
without re-writing.

```
 0- 7  intro       filtered chords (the bridge descent), sub + hats, kick from bar 6
 8-23  groove A    hook riff figure, Ab pedal          (first half clean, rim from 16)
24-31  groove B    Bbm + bVI, Bb pedal
32-39  groove C    Cm/Fm, C pedal, cowbell 16ths
40-47  groove A2   the riff's second variation (Abm7), CC 16ths
48-55  breakdown   descent chords over the C-Bb-G-Gb-F-Eb walk, no kick
56-63  build       riff back behind a closing filter that opens over three bars, tom roll
64-71  drop        the chorus block (Bb/Ebm) with the Ab-F bass turn, everything up
72-87  hook        the "we have won" line on the Levi, Ab then Gb under it
88-95  full        the bright C block (Cmaj7/Am7)
96-103 outro       descent chords, kit thinning, level down
```

Level shape by 4-bar RMS (measured, `tools/songplay` render): intro 0.026, groove
0.081-0.094, breakdown 0.038, build 0.136-0.144, drop 0.220, hook 0.145-0.206, full
0.143, outro 0.058 falling to 0.043. Peak 0.850, 0 clipped samples, 0 xruns over
203.3 s. The drop is the loudest sustained section by design.

## The tool gap: `tools/mid2rbs.py` cannot re-arrange

`mid2rbs.py` maps a *contiguous* MIDI bar range onto a *contiguous* song bar range:
`song bar = midi bar - first_bar`. That is enough to transcribe a song but not to
re-cut one, because a re-cut needs the source's bar 97 to land on song bar 64.

The wrapper `songs/local/the-knife/arrange.py` closes it without touching the committed
tool: it runs `mid2rbs.py` once per section (so the proven quantise/tie/pick logic is
reused unchanged), collects the per-device patterns, de-duplicates them into one global
slot table, and writes the song track from the destination bars. Drum sections are
written directly by the wrapper, because the house kit is hand-authored and the
transcription's rock kit is not mappable.

### The bug that wrapper had, and what caught it

The first version emitted `mid2rbs`'s *own* slot numbers instead of the wrapper's
global ones. Because the slot number was baked into the text of each pattern line, the
patterns were still distinct as strings, so the dedupe and the numbering both "worked"
while the emitted file contained `P303 0 1` twice and left slots 3-6 of the 303A bank
undefined. The song played — bars 32-39 and 64-95 simply had **no bass at all** — and
nothing about the compile or the render said so.

What caught it was the only check that could: pitch-tracking the render instead of
trusting it. With the 808/909 tracks silenced (`muted.py` rewrites the TRACK lines to
zero those two instances), the 40-130 Hz harmonic-sum profile showed *no energy at all*
in bars 32-92 while bars 0-30 had Ab and Bb. Two fixes followed: the slot
re-pointing, and normalising the slot out of the dedupe key so two runs that produced
the same music collapse to one global slot. After the fix the same measurement reads
Ab / Bb / C / Ab / walk / Ab / Bb / Bb / Ab-Gb / Ab-Gb / C / Ab on exactly the bars the
map assigns, which is the proof the progression is Genesis'.

The lesson generalises: **a wrapper that renumbers another tool's output must rewrite
the numbers in the payload, not just in its own table.** And the reason it stayed
invisible is that a song is supposed to be forgiving — silence in eight bars of a
three-minute arrangement reads as "mixing", not as "half the bass is missing".

## Verification, in the order it was worth doing

1. **Headless render** (`tools/songplay`) — peak/RMS/clipped/xruns per section.
2. **Per-bar level and brightness profile** — catches an arrangement whose dynamics are
   inverted (this caught an outro that was louder than the drop, twice).
3. **Drums-muted stem + harmonic-sum pitch class per bar** — the only check that proves
   the *notes*, and the one that found the slot bug.
4. **Pattern-data diff against the MIDI source** — the hook slots decode to the
   transcription's flute bars 130-137 note for note, modulo 16th-grid rounding
   (bar 130's step-2 note lands on step 1, step 13 on step 12, and so on).
5. **Dell deploy** — `the-knife.rbng` (15,538 B, `sha_ok=True`) and the playlist to
   `Vk4aros:ReIncarnation/songs/local/`, verified by `list` on the machine, then
   `Vk4aros:ReIncarnation/RIAPP PLAYLIST=Vk4aros:ReIncarnation/songs/local/demos.rbpl`.

## Two findings worth carrying

- **`tools/inspect --rbng` cannot read any song with automation.** The reader needs a
  caller-provided ATRK buffer (`rbng_read` checks `s->atrk`/`s->atrk_cap`), and
  `inspect` never sets one, so it reports `ATRK without buffer` for the approved
  Zombie Nation song too. It is a tool gap, not a file defect: `rbsc` and `songplay`
  both allocate the buffer and read the same files back. Any song *with* automation is
  currently un-inspectable, which is the whole point of ATRK.
- **The 303 note range is MIDI 24..60, folded by octaves** (`RI_303_BASE_NOTE 36`,
  `RI_303_SEMI_MIN -12`, `RI_303_SEMI_MAX 24`, `ri_p303_fold` in `project/songscript.c`).
  Writing an out-of-range note does not fail — it silently moves by whole octaves. The
  bass therefore needs an explicit transpose to land where it is meant to, which is why
  the arrangement map carries a per-section `transpose` rather than a global one.

## Limits (E0)

- Levi patterns retrigger every "on" step and cannot tie notes across steps, so the
  held chords in the breakdown and the descent are shaped by the release, not by ties.
- One song tempo; the transcription's mid-piece tempo changes (150, 75 and a rubato
  ramp at the end) are not representable and were not represented.
- Knob display follows tick-0 values only; later automation moves the sound, not the
  knobs.
- No reverb in the FX section yet (the reverb send is still un-integrated), so the
  breakdown's air comes from the delay and the levi's own filter, not from a verb.
- `arrange.py` lives under `songs/local/` and is therefore private with the song. If a
  second re-cut ever needs it, it should move to `tools/` with `mid2rbs.py`.