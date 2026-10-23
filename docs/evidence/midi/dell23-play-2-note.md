# Dell 2026-10-23 — "play the song", and why a Play press was self-cancelling

Build on the lane: `c382b38` (`RIAPP LOG build=c382b38`). Fresh instance via
`--run-script RAM:LEVIPROBE.RUN` after closing the previous instance
(`--ui-close 'RIAPP live panel - zombie-nation.rbng' [closerequest] ok`,
confirmed by re-listing windows).

## The Play presses that did nothing, and why

The earlier instance had already finished the song before I pressed anything:

    ev 23       15 TR PLAY
    ev 24    48873 TR STOP

Its `stg: playing=48864` counter never moved again while `stopped` grew, so
every later gesture was made at the end of the song. The cursor stays parked
one bar past the last sounding bar, and `song_poll_end` (`app/riapp.c`)
fires on the very next poll:

    if (cursor / RIAPP_TICKS_BAR < (uint64_t)s_core.song_bars + 1u)
        return;
    ...
    else
        song_transport_stop();      /* single song: stop at the end */

So a Play press at the end is not a broken button: it plays for one poll and
the end-of-song rule stops it again. That is the whole of the `TR PLAY` +
`TR STOP` 5 ms pair — `ev 26/27`, `28/29`, and the whole 13.5 k-event flood
from the held KPENTER key (key-down + auto-repeat, no release sent).

The transport was never the suspect. What was missing was the rewind: a Play
press does not move the cursor; `ri_tr_stop` on an already-stopped transport
does (E1 p. 145 C4 in `engine/seq/transport.c` — first stop while stopped
goes to Loop Start, or to song start when strictly before it). Stop-then-Play
is the replay gesture; Play alone at the end cannot be.

## The fresh instance is playing

App log is fresh per run, so the counters are this instance's alone:

    RIAPP stg: playing=2896   stopped=15   stopped_avg=11 us     (T0)
    RIAPP stg: playing=11759  stopped=15
    RIAPP stg: playing=38258  stopped=15
    RIAPP stg: playing=39338  stopped=15
    RIAPP stg: playing=40467  stopped=15

`playing` grows every poll and `stopped` is frozen at the 15 buffers rendered
during startup — the transport is running continuously, no self-stop, no
xruns in the `hb:` lines. `SONG load ...zombie-nation.rbng bars=151 bpm=140`
preceded `TR PLAY` at launch; the song's own length was ~48.9 s of playing in
the previous instance, so expect it to stop at the end again, and use Stop-then-Play
to replay rather than Play alone.

## What is NOT claimed

- Nothing was heard. The owner has the listening session; the counters say
  the transport is rendering, not that anything sounds right.
- `playing=` counts rendered buffers, so it also grows for a muted song. The
  ear round decides what is audible.

## Files

- `dell23-play-1.log` — the fresh instance's app log (sha `811923c3…`), taken
  ~20 s after launch.
- `dell23-play-panel.png` — the panel as the owner sees it (sha
  `0f99ee9e…`); the claim-refusal requester sits behind the panel, not over it,
  so the tab row, transport strip and keybed are all clear to click.
