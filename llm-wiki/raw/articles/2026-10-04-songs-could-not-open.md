# Songs could not open: a static path, a flat join, and ASL's dot padding (2026-10-04)

- Source: ReIncarnation session, 2026-10-04 (opencode lane, Dell E6320 + riqemu1, ABIv11, `-O0`)
- Collected: 2026-10-04
- Published: 2026-10-04
- Raw: [verbatim](../evidence/2026-10-04-songs-could-not-open.md)
- Related: [the LFO path never runs](2026-10-04-the-lfo-path-never-runs.md), [the guest crashed in utility.library](2026-10-04-the-guest-crashed-in-utility-library.md)

"RIAPP fails to open songs." It was three separate defects stacked, each hiding
the next, and all three had to be fixed before a song would load unattended on
either lane.

## The proof, in two lines

```
RIAPP song Vk4aros:ReIncarnation/songs/local/zombie-nation.rbng: open failed
RIAPP song Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng: 151 bars at 140 BPM
```

Same run, same command, no argument: the flat join misses, the descent finds it,
and **the demo song loads itself**. Before the fix the same run said
`not found; built-in demo only`.

## 1. A static path can only ever serve one lane

```c
case RI_PATH_SONGS: s = "SYS:Classes/ReIncarnation/Songs/"; break;
```

The Dell's library is on its own stick at `Vk4aros:ReIncarnation/songs/`; riqemu1
has `SYS:Classes/ReIncarnation/Songs/`. Both are real, so a path that works on
one lane is silently wrong on the other, and the failure reads as "songs are
broken" rather than "this path is wrong here".

Now probed, in the order the layout requires:

1. `<stick>/ReIncarnation/songs/local/`
2. `<stick>/ReIncarnation/songs/`
3. `SYS:Classes/ReIncarnation/Songs/`

with the whole sticky candidate table probed per step and `SYS:` last, so a
stick-mounted library wins where both exist. A miss raises no requester — the
Dell boots unattended. New: `ri_pal_probe_sub`, declared in the shared
`ri_pal_fs.h` and stubbed in the host backend, with the declared return code
pinned in `t153_sticky_log`.

**`RI_PATH_SONGS` is now the only non-static path in the enum.** That asymmetry
was the tell.

## 2. The library root is not the song's directory

```
[exec] 'dir Vk4aros:ReIncarnation/songs' -> rc=0 (63 ms)
            local (dir)
            demo (dir)
[exec] 'dir Vk4aros:ReIncarnation/songs/local' -> rc=0 (58 ms)
            zombie-nation (dir)
            the-knife (dir)
```

The song sits at `<root>/<song-dir>/<song>.rbng`, so `root + leaf` can never
name it. The demo path now tries the flat join first (riqemu1's layout), then
walks the root's subdirectories and takes the first that opens.

This is a **layout constraint, not a quirk**: the wiki records that playlist
entries are relative (`zombie-nation/...`, `../demo/...`), so `songs/local` and
`songs/demo` *must* stay together. A loader that flattens that layout breaks
the playlists too.

## 3. ASL pads the file name with dots

```
RIAPP Load song: picked Vk4aros:.../zombie-nation.rbng.......................................................................................
```

**151 dots.** Not a buffer overrun, not a stale pointer: `fr_File` is the file
gadget's *text*, and ASL pads a partial name out to the pattern with dots. The
path handed to `AddPart` therefore could never open — the file browser was
broken on every platform, and only the dots' shape said so. Trailing dots are
now trimmed from a bounded copy before the join.

## Two errors of mine, corrected on the record

**"An explicit `SONG=` did not win" was wrong. It did.** `RIAPP.LOG` **appends
across runs**, and I read the first `RIAPP song` line in the file, which belonged
to an earlier session. My own run is further down:

```
  9:RIAPP song Vk4aros:.../the-knife/the-knife.rbng: 104 bars at 124 BPM   <- an earlier run
 23:RIAPP song RAM:zombie-nation.rbng: 151 bars at 140 BPM                 <- mine, and it worked
```

The code's precedence comment was right all along. **A conclusion drawn from "the
first line says X" is worthless on an appending log; anchor on the last run.**

**My spacebar injection used the wrong scancode.** `RI_RAW_SPACE` is `0x40`
(**64**); I sent **57**. No spacebar ever reached RIAPP on that lane, so every
"playback started after the key injection" claim from it is unsupported — RIAPP
auto-plays, which is why songs played anyway. `--ui-rawkey` takes the same raw
codes as `RI_RAW_*`.

## Two lane facts

**A crashed RIAPP survives `--ui-close`, and Q (`0x10`, the QUIT shortcut) does
not stop it either.** Three closes returned `ok` and the process stayed, which
is how a second RIAPP launched alongside the first — and AHI contention plus an
interleaved log followed from that. **Check `status` for a leftover process
before launching, not after.**

**Offline-render-only is silent.** The successful run logged `RIAPP play` and
repeated draw lines, and emitted **no** `stg:`, `dstg`, `vcount` or `hb:` line at
all. Every one of those sits behind `if (s_live && s_lv.drv.session)`, so their
total absence means offline mode — not an idle engine, and not broken counters.
**A heartbeat that logs draw lines but no stage lines is offline mode.**

That last one is what defeated the measurement this session: the `vus` pairing
wanted blocks with sounding voices, and the run that finally loaded a song came
up offline, so every logged block was `voice_active=0` at `vus ≈ 13 µs`.

## Open

- **The `vus` regression is unrun.** It needs a clean guest: one RIAPP, AHI
  actually held, and Zombie Nation's Levi part sounding. The instrumentation is
  built and gated (`RI_ESTAGE_E_STORE`, `vc_voice_us`, contract pinned in
  `t136`), and it adds no clock reads over the stage pair it already pays.
- **`levi_voice_render_sum_stereo` is still unsplit** at 81–91 % of LEVI. This
  is now the last big target and the wall is unchanged: a 4 µs clock against
  ~8 µs of work per sample.
- **The other lanes' song roots are unprobed.** A Windows or plain-AROS host
  still resolves `RI_PATH_SONGS` statically; only the AROS backend learned to
  probe.
- **Why the Dell came up offline** is not established. A stale AHI holder is the
  recorded mechanism, but the holder here was a RIAPP I had already closed, and
  it survived the close.

## Method

- **One symptom, three stacked causes.** Fixing the first (the static path)
  *revealed* the second (the flat join) rather than ending the problem — and
  the log looked no different, so a second defect would have been easy to
  declare impossible.
- **`dir` the tree before believing a path.** Every one of these was settled by
  listing both lanes, not by reading the code that built the path.
- **Verify a control's constant against its header.** One grep of `RI_RAW_SPACE`
  would have caught the wrong scancode before a measurement depended on it.

## See Also

- [the LFO path never runs](2026-10-04-the-lfo-path-never-runs.md) — the counters that make the remaining split possible, and why `voice_active` is the variable that matters
- [the guest crashed in utility.library](2026-10-04-the-guest-crashed-in-utility-library.md) — the AHI contention that makes offline substitution a standing risk