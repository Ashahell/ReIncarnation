# One requester, two instances, and a silent null backend — what the second "cannot load song" actually was

- Source: ReIncarnation session, 2026-10-02 (opencode lane; the owner reported the "Cannot load song" requester was up again, hours after the `..` fix was deployed and proven)
- Collected: 2026-10-02
- Published: 2026-10-02
- Prior: [2026-10-02-aros-does-not-resolve-dotdot-playlist-entry-failure.md](2026-10-02-aros-does-not-resolve-dotdot-playlist-entry-failure.md) (the `..` root cause and its fix), [2026-10-02-head-misses-the-audio-deadline-on-a-real-song.md](2026-10-02-head-misses-the-audio-deadline-on-a-real-song.md) (whose "revert to `RIAPP.prev2` for a clean listening session" advice caused this incident — corrected below), [2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md](2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md) (the lane traps), [2026-09-26-g8-handoff-opencode.md](2026-09-26-g8-handoff-opencode.md) (the first "never two instances" rule)
- Stick state: `RIAPP` = HEAD `406500f` (1,093,672 B, the `..` fix); `RIAPP-old-no-dotdot-fix` = the 00:15 build (843,448 B); `RIAPP.prev` = 776,608 B

## It was not a regression

The fix was deployed and proven 40 minutes earlier: HEAD resolved `..` on hardware, with
no same-directory copy present so folding was the only possible mechanism. The requester
that came back had a different, entirely expected cause:

```
RIAPP playlist Vk4aros:ReIncarnation/songs/local/dotdot.rbpl: 3 songs
RIAPP song Vk4aros:ReIncarnation/songs/local/the-knife/the-knife.rbng: 104 bars at 124 BPM
RIAPP song Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng: 151 bars at 140 BPM
RIAPP song Vk4aros:ReIncarnation/songs/local/../demo/riapp-demo.rbng: open failed
```

`Process 8 Loaded as command: Vk4aros:ReIncarnation/RIAPP.prev2` — the **pre-fix** binary,
running `dotdot.rbpl`, the fixture whose entry 3 is `../demo/riapp-demo.rbng` *on purpose*.
The old binary cannot resolve `..`; that is the bug it predates. Nothing new broke.

The cause was mine: I had left `RIAPP.prev2` beside `RIAPP` as "the clean-audio binary" and
left the deliberately-broken `dotdot.rbpl` on the stick next to the real playlists. A name
that reads "the app, one build back" plus a fixture that fails by design on that build is a
trap with the requester already loaded.

## The finding that actually matters: contention fails *silently*

I launched a second instance while the owner's was still up. `status` showed both:

```
Process 8 Loaded as command: Vk4aros:ReIncarnation/RIAPP.prev2
Process 9 Loaded as command: Vk4aros:ReIncarnation/RIAPP-old-no-dotdot-fix
```

and my instance's second log line was:

```
audio: AHI unavailable - null backend active (offline render only) [err 4]
```

**No requester.** `app/riapp.c:1839` only logs that line — the startup path carries on with
a null backend and renders offline-only. So a contended launch *looks like it worked*:
window opens, panels draw, playlist loads, transport plays, and there is no sound and no
complaint.

That is strictly worse than a requester, and it is the opposite of the project's own rule
("a silent failure reads as *nothing happens*", which is why `song_fail` was written at
all). The obvious fix is to requester, or refuse to start, when `s_live` is 0 after a
requested audio backend — but that is `app/riapp.c`, so it is a decision for whoever owns
that file, not something to slip in from here.

Two corrections to what I said while diagnosing, because both were wrong in the direction
of blaming the second instance:

- The requester belonged to **process 8**, not to mine. There was exactly one requester.
- The AHI failure produced **no requester at all** — it produced silence.

## A second instance destroys the first one's evidence

`RIAPP.LOG` is opened `MODE_NEWFILE` at startup (`app/riapp.c`, the log-path comment:
"*MODE_NEWFILE at startup*"). Two instances therefore write the same file, and the second
launch truncates the first's. Under contention the log cannot answer the question either,
because the run you want to read about is the one that was erased.

This is the same shape as the ev-log hazard already recorded for the scripted A,B,B,A
runs, and it has the same fix: **close every instance, then pull**, and treat a successful
`--get` while anything is still running as unverified.

## Two operational traps in the close path

**A pre-flight `--ui-close "#0"` is wrong when a requester is up.** With one up, the
window order is:

```
RIAPP            488,323 391x123 [active,no-close]     <- index 0: the requester
RIAPP live panel 0,0 1349x680 [close@5,0]             <- index 1: the app
```

Index 0 is the requester, and it is `[no-close]` because it is modal. A harness that
closes `#0` twice as a pre-flight — the documented shape — closes nothing that matters and
then launches a second instance. The pre-flight has to close the *panel* index and verify
with `--ui-windows`, not with the close result.

**`--ui-close` returning `ok` does not mean the window closed.** Closing the panel returned
`[closerequest] ok (727 ms)` and, on re-listing, the requester and the panel were both
still there: the process is wedged on its own modal box. Two repeats did not clear it.
`--ui-close` refusing is ambiguous between "gone" and "not started"; `--ui-close`
*succeeding* is equally ambiguous. Only `--ui-windows` plus `status` settles either.

## The playlist was invalid as written — my own defect

`songs/local/demos.rbpl` named `riapp-demo.rbng` in its own directory. That file had only
ever existed on the Dell, between the upload that made `demos.rbpl` work and the deletion
I did while setting up the `..` fixture. It was never in the host directory
(`ls songs/local/` before the fix: `demos.rbpl the-knife zombie-nation`), so the playlist
was broken as committed to disk and only happened to work on one machine at one moment.

Fixed by putting the song in the directory the playlist names — which buys a property worth
more than the fix itself: **`demos.rbpl` now contains no `..`, so it loads on every
binary**, including the old one. The listening playlist should be the boring one; the
clever path belongs in a fixture that never ships to the device.

## Rules that came out of it

- **Never leave a fixture that fails by design next to something you listen to.** The `..`
  playlist lives on the host now; it has already served its purpose on the device.
- **Name a binary for what it lacks**, not for how old it is. `RIAPP.prev2` reads as
  "the app, one build back"; `RIAPP-old-no-dotdot-fix` cannot be launched by accident.
- **Contention must be loud.** A second instance that cannot open the sound card is
  currently silent, which is the worst possible failure mode for a music app.
- **Close, then pull.** Under contention the log is being rewritten underneath you.
- **Verify a close by listing.** Both the success and the refusal of `--ui-close` are
  ambiguous on this lane.

## The wedged process, and the proof that was still owed

`Process 8` (`RIAPP.prev2`) held a modal "Cannot load song" requester that repeated
`--ui-close` on its panel did not dismiss, and the process stayed in `status`. Clearing it
needed a guest-side `kill` (the lane's CLI has no `kill` in reach) or a reboot.

**It resolved itself** — on the next check the process was gone, no RIAPP window remained
and AHI was free, consistent with a guest reboot. No `kill` was needed, which is worth
knowing: a process wedged on its own modal box may not stay wedged, so "the lane has no
`kill`" is not automatically a dead end.

That left the verification this article's whole argument rested on still unrun: **does the
fixed binary actually work as a listening session?** Run with a verified pre-flight —
confirmed no RIAPP window before launch, then confirmed *one* instance after it rather
than assuming — HEAD plus `demos.rbpl`, one instance, full cycle:

```
RIAPP playlist Vk4aros:ReIncarnation/songs/local/demos.rbpl: 3 songs
RIAPP song Vk4aros:ReIncarnation/songs/local/the-knife/the-knife.rbng: 104 bars at 124 BPM
RIAPP song Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng: 151 bars at 140 BPM
RIAPP song Vk4aros:ReIncarnation/songs/local/riapp-demo.rbng: 16 bars at 140 BPM
RIAPP song Vk4aros:ReIncarnation/songs/local/the-knife/the-knife.rbng: 104 bars at 124 BPM
audio: AHI low-level mode=0x003e0001 mix=48000 Hz buffer=256 frames period=5333 us
```

Three songs, the fourth line is the wrap, AHI is the **real** backend (not the null one),
and there is no `open failed`, no requester and no error line anywhere in the log. The
self-contained `demos.rbpl` works on hardware: `riapp-demo.rbng` resolves out of the
directory the playlist actually names. That closes the playlist defect recorded above.

One detail worth keeping: `delete Vk4aros:RIAPP.LOG` returned `rc=5` ("No file to delete")
and the *job* verdict came back FAIL while every action that mattered, including the
`Run`, was `rc=0`. Read per-action `rc`; a non-zero from a cleanup step you did not need
is not a failure of the run.

## The xruns survive every explanation except the song

This run removes the confounders one at a time. `demos.rbpl` contains **no `..`**, so the
path bug cannot be involved. It ran on a **single** instance with AHI live, so contention
cannot be involved. And it is not the `..` fixture's particular playlist:

```
RIAPP closed: buffers=97444 xruns=20998 render_max=21573 us render_total=563088 ms
  period=5333 us wake_max=5823 us wake_n=97442
  stg_total_avg=5766 us stg_dsp_avg=5689 us stg_evt_avg=29 us
  stg_playing=97396 stg_stopped=48
```

The single clearest number in the whole investigation: **`stg_total_avg=5766 us` against a
`period=5333 us` deadline.** The *average* buffer is 8 % late. No scheduling, no governor
arm, no repaint policy and no contention can explain an average that exceeds the budget —
and `stg_dsp_avg=5689 us` against `stg_evt_avg=29 us` says the cost is DSP, not the
instrumentation wrapped around it. Two stages carry it (`stg[5]` avg 5658 us, `stg[7]`
avg 5735 us), and inside the DSP the largest single device is Levi at avg 747 us /
max 15710 us.

This confirms the earlier measurement (19,703 xruns / 94,489 buffers) on a second, clean
run: 20,998 / 97,444. Same order of magnitude, same story. **It is the song's DSP cost on
this hardware**, and it belongs to the render-stage lane — not to the path fix, not to the
log fix, and not to anything this lane did.

So HEAD is the binary to launch, and it is still xrunning on a real song. Those two facts
are now independent of each other, which is what makes the recommendation stable.

## See Also

- [AROS does not resolve `..` in a path — and a playlist that reaches a sibling directory fails silently at the end of every cycle](2026-10-02-aros-does-not-resolve-dotdot-playlist-entry-failure.md)
- [HEAD misses the audio deadline on a real song: 19,703 xruns on the Dell](2026-10-02-head-misses-the-audio-deadline-on-a-real-song.md)
- [The Dell lane's two wedges — bare-path launch, hung `--ui-capture`](2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md)
- [G8 handed to opencode: skinned, zoomed, audited](2026-09-26-g8-handoff-opencode.md)