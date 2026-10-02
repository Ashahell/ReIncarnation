# AROS does not resolve `..` in a path — and a playlist that reaches a sibling directory fails silently at the end of every cycle

- Source: ReIncarnation session, 2026-10-02 (opencode lane; diagnosing the "Cannot load song" requester on the Dell after the The Knife deploy)
- Collected: 2026-10-02
- Published: 2026-10-02
- Prior: [2026-10-02-the-knife-deep-house-song-cut.md](2026-10-02-the-knife-deep-house-song-cut.md) (the song whose deploy exposed it), [2026-09-30-songs-playlists-zombie-nation.md](2026-09-30-songs-playlists-zombie-nation.md) (the playlist that has carried the bad entry since it was written), [2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md](2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md) (the lane traps hit on the way)
- Code: `project/playlist.c` (`ri_playlist_dirname` + `ri_playlist_parse`), `project/rbng.c` (`read_file` → `fopen`), `app/riapp.c` (`song_fail`, which logs before it requesters)

## The symptom

A requester on the Dell saying `Cannot load song`. Nothing else: no audio, no
crash, no log line anyone was reading. The song itself was provably fine — see
the exoneration list at the end.

## The cause

`ri_playlist_dirname` resolves an RBPL entry by **plain concatenation**:

```c
/* project/playlist.c — dir = "Vk4aros:ReIncarnation/songs/local" */
dir + "../demo/riapp-demo.rbng"  ->  "Vk4aros:ReIncarnation/songs/local/../demo/riapp-demo.rbng"
```

and `read_file` in `project/rbng.c` opens that with `fopen(path, "rb")`.
**AROS does not canonicalise a `..` component.** Proven on the guest with plain CLI
commands, no GUI involved:

```
type Vk4aros:ReIncarnation/songs/demo/riapp-demo.rbng          -> rc=0   FORM
type Vk4aros:ReIncarnation/songs/local/../demo/riapp-demo.rbng -> rc=10  object not found
list Vk4aros:ReIncarnation/songs/local/../demo                 -> rc=20  object not found
```

So the same file is reachable without the `..` and unreachable with it, and the
only thing that changes is a path component the host filesystem eats silently.

The app's own log line, once it could be read, said it exactly:

```
RIAPP playlist Vk4aros:ReIncarnation/songs/local/demos.rbpl: 3 songs
RIAPP song Vk4aros:ReIncarnation/songs/local/the-knife/the-knife.rbng: 104 bars at 124 BPM
RIAPP song Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng: 151 bars at 140 BPM
RIAPP song Vk4aros:ReIncarnation/songs/local/../demo/riapp-demo.rbng: open failed
```

**Why this hid for so long:** it is not a startup failure. Entries 0 and 1 load
fine and play, so "the playlist loads" is true and the demo looks healthy. The
failure only appears when the playlist **auto-advances past the last song** — at
`ev 17 … t=38139` for entry 2, roughly 7m45s into a run. Nobody waits that long
in a smoke test, and the requester then sits there looking like it belongs to
whatever was launched most recently.

The entry has been in the playlist since the songs feature landed
(`547bbf8`, 2026-09-30). The 2026-09-30 record says "Dell: `RAM:RIAPP
PLAYLIST=RAM:songs/demos.rbpl` loads the playlist … and plays in Song mode" —
which was accurate and still hid the bug, because loading entry 0 is not
reaching entry 2.

## The fix applied

The playlist no longer depends on `..`. `riapp-demo.rbng` is copied next to the
playlist so all three entries are plain names in one directory:

```
the-knife/the-knife.rbng        | The Knife (Genesis) - deep house
zombie-nation/zombie-nation.rbng | Zombie Nation (Kernkraft 400)
riapp-demo.rbng                 | RIAPP demo
```

with the reason written into the file's comment block, because the next person to
"tidy up" a playlist will reach for `..` again.

Proof on the Dell, one full playlist cycle and a wrap (`Vk4aros:RIAPP-EV.LOG`):

```
ev 9     SONG load .../the-knife/the-knife.rbng      bars=104 bpm=124
ev 17    SONG load .../zombie-nation/zombie-nation.rbng bars=151 bpm=140   (t=38139)
ev 25    SONG load .../riapp-demo.rbng               bars=16  bpm=140   (t=87010)
ev 33    SONG load .../the-knife/the-knife.rbng      bars=104 bpm=124   (t=92485, wrapped)
```

no `open failed`, no requester, and `xruns=0` across 162,746 and 110,640 buffers
on the two runs (`render_max` 3699 / 3692 us). The Knife plays on the Dell for
~8.5 minutes of wall time between two other songs.

## The fix that is still owed, and the fork in it

`project/playlist.c` is still wrong in the general case: **any** playlist that
reaches a sibling directory, or names a file above itself, is broken on AROS. The
song-level fix removes the symptom for this one playlist and leaves the trap armed
for the next one. Two defensible answers, and this is an owner call:

- **Reject at parse time.** `ri_playlist_parse` refuses any entry containing a `..`
  component, with a message naming the entry. Fails loudly and immediately, in the
  same place the author can act, instead of minutes later as a load failure.
- **Canonicalise.** Resolve `dir + entry` and fold `.` / `..` textually before use.
  Friendlier, and it makes `../demo/…` work as any other filesystem would, but it
  is new path logic in a shared file and needs its own tests for `..` at the start,
  `..` past the root, and an entry that escapes the volume.

Either way the *host* and the *guest* must agree: the host PAL resolves `..`, so a
host-side test of the same playlist passes while the Dell fails. A test that only
runs on the host cannot see this class of bug — which is the more general lesson.

## Why the diagnosis was hard, and what fixed it

`app/riapp.c` logs the reason **before** it puts up the requester:

```c
static void song_fail(const char *what, const char *path, const char *why) {
    rlog("RIAPP %s %s: %s\n", what, path, why);        /* line 814 */
    EasyRequestArgs(NULL, &es, NULL, (RAWARG)args);   /* then the modal box */
}
```

That ordering is what made this recoverable at all: the evidence existed on disk
while the app sat blocked on the box. Two things then had to be true.

1. **The log had to be somewhere a reboot cannot reach.** The binary deployed on
   the stick was built at 00:15, which *predates* `0c2ba7f` (09:09 the same day,
   "put RIAPP.LOG where a reboot cannot reach it"). Before that commit
   `RI_PATH_TEMP` was hard-coded to `"RAM:"`, so the reason was in
   `RAM:RIAPP.LOG` and a cold reboot erased it — confirmed: after the reboot
   `delete RAM:RIAPP.LOG` returned `rc=5, "No file to delete"`.
2. **The failure had to be timed, not assumed.** It was not at startup. Polling
   the window count once a minute is what showed 3 windows holding steady and then
   **4 at 17:12:05** — matching 7m45s after a 17:04 launch. One probe at startup
   would have looked fine, and so would "it played for a while".

And the bisect that isolated it never needed the requester's text: the knife-only
playlist loaded (104 bars, 124 BPM), which killed "the song is broken", and the
three-entry playlist loaded entry 0 and then failed 7m45s in, which killed
"the playlist file is malformed". The remaining suspect was the entry path, and
two `type` commands settled it.

## The song was not at fault (so it does not get re-investigated)

Verified locally with the app's own code before the guest ever answered:

- `tools/rbsc` round-trips `the-knife.rbng` through the same `rbng_read_song` that
  `song_load_path` calls; byte-identical on a re-run.
- `tools/songplay` renders 203.3 s at peak 0.850, rms 0.123, **0 clipped, 0 xruns**,
  and never prints `automation refused`, so all 223 ATRK events were accepted.
- `RI_RBNG_MAX_FILE` 65536 against 15,538 bytes; `RI_CORE_AUTO_CAP` 8192 against
  223 events; `RI_PLAYLIST_PATH` 256 against a 58-character resolved path.
- `song_load_path` and `RI_CORE_AUTO_CAP` have not changed since `547bbf8`.
- The stick held `songs/demo/riapp-demo.rbng` at 5,764 bytes the whole time, which
  is what killed the "missing file" hypothesis and pointed at the path.

## See Also

- [Songs, playlists and the Zombie Nation demo (owner 2026-09-30)](2026-09-30-songs-playlists-zombie-nation.md)
- ["The Knife" (Genesis) as deep house: a faithful-progression re-cut](2026-10-02-the-knife-deep-house-song-cut.md)
- [The Dell lane has two silent traps: the audit's link gate builds ABIv1, and `--get` serves stale bytes for a file the guest still has open](2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md)