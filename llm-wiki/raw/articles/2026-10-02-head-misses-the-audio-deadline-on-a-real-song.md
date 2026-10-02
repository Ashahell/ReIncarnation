# HEAD does not meet the audio deadline on a real song: 19,703 xruns on the Dell, with a controlled binary pair

- Source: ReIncarnation session, 2026-10-02 (opencode lane; deploying the owner's two decisions — canonicalise `..`, log fallback `T:` — to the Dell)
- Collected: 2026-10-02
- Published: 2026-10-02
- Prior: [2026-10-02-aros-does-not-resolve-dotdot-playlist-entry-failure.md](2026-10-02-aros-does-not-resolve-dotdot-playlist-entry-failure.md) (the fix this deployment carried), [2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md](2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md) (the render-stage work these numbers belong to), [2026-10-01-dell-xruns-governor-arm-and-repaint-policy.md](2026-10-01-dell-xruns-governor-arm-and-repaint-policy.md) (the earlier xrun campaign)
- Binaries: `Vk4aros:ReIncarnation/RIAPP` = repo HEAD `406500f` (1,093,672 B, built with `~/bin/build_v11.sh`, 81 TUs, 0 undefined, v11 `r12` convention); `RIAPP.prev2` = the 00:15 build that was on the stick (843,448 B), kept as the control

## The measurement

Both binaries were run on the **same playlist, on the same machine, back to back**, each
for ~9 minutes of playback (The Knife 104 bars, then Zombie Nation 151 bars, then the
demo, then a wrap):

| binary | `..` entry | xruns | `render_max` |
|---|---|---|---|
| `RIAPP.prev2` (00:15) | **`open failed`** | **0** | 3,679 us |
| HEAD `406500f` | **resolved and loaded** | **19,703** | 18,299 us |

```
RIAPP closed: buffers=94489 xruns=19703 render_max=18299 us render_total=540069 ms
  period=5333 us wake_max=5823 us stg_total_avg=5703 us stg_dsp_avg=5626 us
  stg_evt_avg=29 us stg_playing=94439 stg_stopped=50
```

The buffer period is 5,333 us. **`stg_dsp_avg=5626 us` — the DSP stage alone exceeds the
whole period**, before the event stage (29 us) and before anything else in the chain.
19,703 of 94,489 buffers is one dropout in five.

## What this is not

- **Not the song.** The same playlist on the old binary gives 0 xruns.
- **Not the playlist or the fix.** `project/playlist.c` is reached only while parsing an
  RBPL file, once, before any audio; `RI_PAL_STICKY_FALLBACK` is a string consulted once,
  and only when no candidate volume mounts (`Vk4aros:` does, so it is never used here).
  Neither can plausibly change a 256-frame render loop.
- **Not a shared-hardware ghost.** Both arms ran minutes apart on the same guest, and the
  difference is 0 against 19,703 — far outside the multi-hundred-millisecond stall the
  Dell is documented to produce on its own.
- **Not measured by ear.** Every figure is from the app's own log.

## The one alternative explanation, stated rather than buried

The old number is **not apples-to-apples**: `RIAPP.prev2` has no stage instrumentation,
so its `render_max=3679 us` is an uninstrumented figure, and HEAD's 18,299 us includes the
cost of measuring five stages per buffer in an `-O0` debug build (the owner's
debug-only rule means the measurement cannot be optimised away). So the honest split is
unresolved from here:

- the DSP genuinely got more expensive, or
- the instrumentation is most of the 18 ms, or
- some of both.

What is **not** unresolved: HEAD, as it stands, does not fit the audio deadline on a real
five-device song on this machine. That is the fact the render-stage and governor work
needs, and it is the first on-device number for HEAD — every earlier xrun figure in the
wiki came from the 00:15 binary.

## How to reproduce in two runs

```sh
# control
Run Vk4aros:ReIncarnation/RIAPP.prev2 PLAYLIST=Vk4aros:ReIncarnation/songs/local/dotdot.rbpl
# candidate
Run Vk4aros:ReIncarnation/RIAPP         PLAYLIST=Vk4aros:ReIncarnation/songs/local/dotdot.rbpl
# then close, and pull RAM:RIAPP.LOG (prev2 logs there) or
# Vk4aros:RIAPP.LOG (HEAD logs to the stick) -- close BEFORE pulling
grep "closed:" <log>
```

The playlist is `songs/local/dotdot.rbpl` on the stick: three songs where entry 3 is
`../demo/riapp-demo.rbng` on purpose, with no `riapp-demo.rbng` beside the playlist, so it
only loads if the `..` fold works. That makes the same playlist a two-purpose fixture —
it proves the path fix and gives the render work a repeatable five-device song.

## What was left deployed, and why

HEAD (`406500f`) is deployed as `RIAPP`, because it is what the repository builds and it
carries both owner decisions. The old binary is preserved as `RIAPP.prev2`, so reverting
is one command if clean audio matters more than the fix for a listening session:

```sh
copy Vk4aros:ReIncarnation/RIAPP.prev2 Vk4aros:ReIncarnation/RIAPP
```

> **Status: Outdated — 2026-10-02, later the same day.**
> **Do not follow that one-command revert as written.** Leaving the old binary beside
> `RIAPP` under the name `RIAPP.prev2`, while a playlist fixture containing `..` sat on the
> stick, produced the next incident: the old binary was launched, could not resolve `..`,
> and put up a "Cannot load song" requester that read as the fix not working. The binary is
> now named `RIAPP-old-no-dotdot-fix`, the `..` fixture has been removed from the device,
> and the recommendation is inverted: **`RIAPP` (HEAD) is the only binary to launch.** It
> xruns on this song, and that is a real defect owned by the render-stage lane — but the
> old binary's clean audio came with a silent path bug, which is worse.
> See [One requester, two instances, and a silent null backend](2026-10-02-one-requester-two-instances-and-a-silent-null-backend.md).

## See Also

- [AROS does not resolve `..` in a path — and a playlist that reaches a sibling directory fails silently at the end of every cycle](2026-10-02-aros-does-not-resolve-dotdot-playlist-entry-failure.md)
- [Render-stage breakdown: voices are 94 percent, not the FX chain](2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md)
- [Scripted A,B,B,A settles it — arm wins, repaint policy regresses](2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md)