# Two Dell-lane traps that cost a session: the bare-path launch wedges the agent, and `--ui-capture` can hang forever

- Source: ReIncarnation session, 2026-10-02 (opencode lane, deploying the The Knife deep-house song)
- Collected: 2026-10-02
- Published: 2026-10-02
- Prior: [2026-10-02-dell-lane-scripted-ab-procedure-and-click-map.md](2026-10-02-dell-lane-scripted-ab-procedure-and-click-map.md) (the verified click map and protocol; it records `Run RIPA` without a volume starting nothing, but not what a bare *path* launch does to the lane), [2026-10-02-the-knife-deep-house-song-cut.md](2026-10-02-the-knife-deep-house-song-cut.md) (the song whose deploy exposed both)
- Spool: `/tmp/spike_spool_laptop`, host server `scripts/spike_server.py serve --port 9292` (pid 1202), identity `e6320`

## What happened

The Knife was deployed to the stick and launched to let the owner hear it:

```
--exec 'Vk4aros:ReIncarnation/RIAPP PLAYLIST=Vk4aros:ReIncarnation/songs/local/demos.rbpl'
```

That job timed out host-side after 90 s. Every later `--exec` and every `--ui-capture`
then queued without running, and the guest reported a **"Cannot load song"** requester
that could not be photographed or diagnosed, because the only channel to the guest was
the one that had wedged.

Two separate causes, in order.

## 1. A bare-path launch blocks the lane, not just the job

The click-map record says `Run RIPA` (no volume prefix) *starts nothing*. The opposite
error is worse: **`Vk4aros:ReIncarnation/RIAPP` as an `--exec` command does start
something, and the agent never comes back.**

`--exec` runs the command line through the agent's own shell, blocking on the child's
output pipes. RIAPP is a GUI program that does not exit, so the pipes never reach EOF and
the agent stays inside that one action. It is not the job that is stuck — it is the whole
identity: `status` showed `state = busy:<job-id>` and every subsequent action, including
`--ui-windows`, queued behind it.

The verified launch form is the one the lane already knows:

```
--exec 'Run Vk4aros:ReIncarnation/RIAPP PLAYLIST=...'
```

`Run` returns as soon as the command is accepted, so the agent's pipes close and the next
action runs. Combined with the record's own warnings — `rc=0` from `Run` proves only that
the command was *accepted*, the window needs ~20 guest `wait`s, and never two instances —
that is the only shape to use for anything long-lived.

**Rule: never launch a GUI program on this lane except through `Run <volume>:<command>`.**

## 2. `--ui-capture` can hang indefinitely, and it blocks everything behind it

After the wedged launch eventually released (≈16 minutes), the agent picked up a queued
`--ui-capture` and **stayed inside it for over 40 minutes**. The session state advanced
to `busy:<the capture job id>` and stayed there; the TCP connection was still
`ESTABLISHED` and `last_seen` kept refreshing, so the agent was alive and heartbeating —
it was simply never going to finish that one action.

This is worse than the first trap, because a screenshot is exactly what you reach for when
something needs investigating. Note the shape of the evidence: `state = busy` plus a fresh
heartbeat reads as "working", and only the job-id in the state string says it is stuck on
the same action.

There is no server-side cancel. The queue is strictly serial.

## What actually recovered the session, and what it cost

`spike_server.py serve` handles **`SIGUSR1` as an operator sweep**: `kick_all()` shuts
every live session socket down, releasing the names and heartbeats. One signal:

```sh
kill -USR1 <pid of the serve process>
```

Effect: the session went `disconnected`, the two stuck jobs were marked done, and the
three queued `--exec` jobs stopped being counted as pending. So the wedge is clearable
from the host in one action, without touching the guest.

**The cost: the guest agent did not reconnect.** It held no connection for 5 minutes of
polling and had still not come back; the lane has to be restarted *on the Dell*. Earlier in
the same day the lane had reconnected on its own (session 1 → session 4 earlier, session
1 again after a reboot), so a reconnect does happen sometimes — which is exactly why the
no-reconnect case is a trap worth writing down rather than an assumption to rely on.

Practical consequence: **use `Run`, keep launches short, and prefer one `--exec` probe over
a `--ui-capture` you are not prepared to lose the lane to.** If the lane does go quiet,
`SIGUSR1` is the host-side reset, and it costs a guest-side agent restart.

## The song itself was not at fault (recorded so it is not re-litigated)

Every local check on `songs/local/the-knife/the-knife.rbng` passed against the app's own
code before the lane went down, and the transfer itself was sha-verified twice:

- `tools/rbsc` compiles it and reads it back through `rbng_read_song` — the exact call
  `song_load_path` makes — byte-identical on a re-run.
- `tools/songplay` drives it through `ri_core_load_song` into the live session and renders
  203.3 s: peak 0.850, rms 0.123, **0 clipped, 0 xruns**. It never printed
  `automation refused`, so all 223 ATRK events were accepted by
  `ri_core_load_triples` (a 223-event load that would silently drop the whole sound
  program if it were over the lane's capacity).
- `RI_RBNG_MAX_FILE` is 65536 against a 15,538-byte file, so `read_file`'s only size
  rejection cannot apply; `RI_CORE_AUTO_CAP` is 8192 against 223 events;
  `RI_PLAYLIST_PATH` is 256 against a 58-character resolved entry path.
- `ri_playlist_dirname` + `ri_playlist_parse` on the Dell-style path
  `Vk4aros:ReIncarnation/songs/local/demos.rbpl` resolve all three entries to the same
  absolute paths the previous day's working playlist produced.
- `song_load_path` and `RI_CORE_AUTO_CAP` have not changed since the songs commit
  (`547bbf8`), so the older RIAPP build on the stick cannot be running different song
  code.
- On the guest, `list` confirmed `the-knife.rbng` at 15,538 bytes and the playlist at
  267 bytes, after two `sha_ok=True` writes.

The requester's own text lives in the guest log
(`rlog("RIAPP %s %s: %s\n", what, path, why)` in `app/riapp.c`), which is the one piece of
evidence still owed — and reading it needs the lane back.

## See Also

- [Driving the Dell lane by script: the verified click map, the startup clock, and the two ways a run silently produces nothing](2026-10-02-dell-lane-scripted-ab-procedure-and-click-map.md)
- [The Knife (Genesis) as deep house: a faithful-progression re-cut](2026-10-02-the-knife-deep-house-song-cut.md)
- [The Dell lane has two silent traps: the audit's link gate builds ABIv1, and `--get` serves stale bytes for a file the guest still has open](2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md)

## The lane is shared, and the reset is not mine to spend

Found while waiting for the guest to come back: the spool carries jobs from **both**
opencode sessions on this machine. When the reset happened, five jobs were queued and
**four of them were the other lane's** — a `put RIAPP.v11 -> RAM:RIAPP_SEC`, a 4-action
`ui_close/exec` run, a 27-exec run, and a 190-action run. Mine was one `list`.

Two consequences that belong in the procedure:

1. **`kill -USR1` is not a private reset.** It closes the one session the whole lane
   shares. It was the right call here (the session was wedged inside *my* action) and it
   still cost a re-dial that did not come for 20 minutes, with another session's 190
   actions sitting behind it. Reach for it only after reading
   `state = busy:<job-id>` and confirming that job is yours.
2. **A queued `ui_click` is the same trap as `ui_capture`.** The other lane's newest job
   begins with a `ui_click`, against a guest that still had a modal requester on screen.
   If a UI action hangs on a guest whose frontmost window is a requester, that run is what
   wedges next, and the loss is theirs, not mine. Diagnose from the **log**, never from a
   UI action, on this lane.

The queue is serial and has no cancel, so the lane's blast radius is every session using
that spool.
