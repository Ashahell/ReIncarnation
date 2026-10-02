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

## After a host cold reboot: the lane returns, but not the way you would guess

Added 2026-10-02 (later the same day), after the host itself was cold-booted mid-session.
Every part of this cost time, and three of the four checks give the *wrong* answer.

**What comes back, and what does not.** The spool directory reappears but empty
(`done/ jobs/ results/`, `pending=0`), `pairs.json` is intact, and all three `serve`
processes return — this box hosts three lanes (`9091/9092` on `nvkspike`, `9292` here,
`9294` on `spike_spool_s6`) and they all restart. But the guest shows:

```
e6320   /tmp/spike_spool_laptop   disconnected pending=0   last=0
```

`last=0` means it has never been seen by *this* spool instance, which is the tell.

**Two checks that mislead:**

- `ping 192.168.1.60` succeeds, 2/2, ~0.15 ms — and tells you nothing about the lane.
- `nc`/connect to the Dell's own `9292` returns **refused**, and also tells you nothing:
  the server lives on the *host*; the guest agent dials out to it. "Refused" on the Dell
  is the expected steady state, not a fault.

The only meaningful signal is the lane's own `status` line. Check that, not the network.

**It reconnected on its own**, within minutes, with no `SIGUSR1` and no guest-side action
— which is consistent with the note above that a reconnect sometimes happens. So the
honest version of the earlier claim is: a `SIGUSR1` reset *may* cost a guest-side agent
restart, and after a **host** reboot it often does not. Poll `status` before concluding
the lane is dead, and do not spend a shared reset on a lane that is merely young.

**`/tmp/ri` does not survive.** `~/bin/build_v11.sh` says so in its own header, and it is
true: the v11 Dell build output is gone, so a deploy needs the 81-TU rebuild before
anything can be put on the stick.

**`x86_64-aros-gcc` is not on a bare shell's `PATH` after a reboot.** Probing it by hand
fails with `command not found` and looks like a missing toolchain. It is not: the audit
sources `../Vulkan4Aros/scripts/aros_build_env.sh` itself before every AROS phase. So
probe AROS-side compiles *through the audit*, not by invoking the compiler directly.

**A `status` line with no window is not noise.** After a two-instance experiment, wedged
processes stayed in `status` while being absent from `--ui-windows` entirely — and they
still held `ahi.device`, so the next single launch failed too. Present in `status` but
absent from `--ui-windows` is the signature of a live-but-windowless holder; see
[A lost audio path must not be silent](2026-10-02-a-lost-audio-path-must-not-be-silent.md).
After any two-instance experiment, **wait for `status` to clear before launching again.**

**And the guest CLI does have `kill`.** Correcting the earlier record: the *lane* has no
kill action, but the AROS CLI on the guest does (`kill Process8` → `rc=0`, "kill: object
not found" on a bad argument). Reach for it before concluding a reboot is required.

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
