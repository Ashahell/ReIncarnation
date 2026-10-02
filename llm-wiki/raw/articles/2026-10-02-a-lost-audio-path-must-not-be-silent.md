# A lost audio path must not be silent: the second instance that played nothing

- Source: ReIncarnation session, 2026-10-02 (opencode lane, commit `2ecffd0`; the finding itself was first hit by accident earlier the same day)
- Collected: 2026-10-02
- Published: 2026-10-02
- Prior: [One requester, two instances, and a silent null backend](2026-10-02-one-requester-two-instances-and-a-silent-null-backend.md) (this is the fix for the gap that article recorded)
- Evidence: [`docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md`](../../../docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md) — every log excerpt, window dump, mutant hash and RED/FAIL line quoted below, verbatim
- Deployed: `Vk4aros:ReIncarnation/RIAPP` = 1,094,648 B, `build=2ecffd0` (ev-log `RUN frames=256 vol=Vk4aros: build=2ecffd0`); rollback kept as `RIAPP-old-no-audio-fail`

## The defect

Launch a second RIAPP while the first holds `ahi.device`. The second fails
`AHI_AllocAudioA`, falls back to the null backend, and `app/riapp.c` logged exactly
one line:

```
audio: AHI unavailable - null backend active (offline render only) [err 4]
```

— then carried on. Window, panels, playlist and transport all came up, with no sound
and **no requester**. Worse than a requester, which at least reports that something
happened; and the exact failure mode this project already has a rule against ("a
silent failure reads as *nothing happens*" — the reason `song_fail` exists). The rule
had been applied to songs and not to audio.

## RED on hardware, before any code

Reproduced deliberately, with a control first:

```
control   instance A alone
          audio: AHI low-level mode=0x003e0001 mix=48000 Hz buffer=256 frames period=5333 us

RED       A settled, then B launched
          Process 8 Loaded as command: Vk4aros:ReIncarnation/RIAPP
          Process 9 Loaded as command: Vk4aros:ReIncarnation/RIAPP
          4 window(s): RIAPP live panel ... [active] / RIAPP live panel ...
          -> no requester. B was silent.
```

Two panels, zero requesters. That is the whole defect in two lines of window state.

## The distinction the fix turns on

`audio_ahi_live.h` documents `err` as a *failure step*, and the source shows exactly
two interesting cases:

| err | set at | meaning |
|-----|--------|---------|
| **2** | `audio_ahi_live.c:192`, immediately after `OpenDevice("ahi.device")` fails | **no AHI device at all** |
| **4** | `audio_ahi_live.c:236`, after `AHI_AllocAudioA` returns NULL | device opened, card already taken |

Under contention the device *opens* — it is the allocation that fails. That is the
whole reason a single comparison can separate "this machine has no sound card" from
"another program is using yours", and why the observed `err 4` was never a hardware
problem.

So the policy (`ri_core_audio_failure_is_loud`, `app/core/riapp_core.h`):

- **err 2 stays quiet.** There the null backend is the *documented* offline-render
  fallback — `RI_AUDIO_NULL_MSG` is byte-exact and pinned by `tests/unit/t6_w1backend.c`
  — and a machine with no sound card must not be nagged about it.
- **Everything else is reported**: port, alloc, load, task, open-timeout, and
  **err 0**, which means `au_live_open` succeeded and `au_live_run` then failed.
- **An unrecognised code is reported too.** Fail loud, never fail silent — that
  inversion is how this shipped in the first place.

## GREEN on hardware

Same two-instance scenario, rebuilt binary:

```
Process 8 ... RIAPP / Process 9 ... RIAPP
RIAPP   519,289 328x191 [active,no-close]     <- the requester
RIAPP live panel 0,0 1349x680 [close@5,0]
RIAPP live panel 0,0 1349x680 [close@5,0]
```

`328x191` rather than the `391x123` of a song requester — the message is multi-line.
The log gains the reason:

```
RIAPP audio: sound card unusable, continuing without sound [err 4]
```

Single instance afterwards: live AHI, one panel, **no requester**. So the carve-out
works in both directions and the fix is not simply "always nag".

## TDD, honestly

The behavioural RED was the shipped behaviour itself: `ri_core_audio_failure_is_loud`
first returned 0 — "never loud", which is what deployed. That produced **10 failures
on exactly the lost-path cases, with the absent-device carve-out already green**:

```
FAIL t158:51: err 1 (msgport): loud=0, want 1
FAIL t158:51: err 4 (AHI_AllocAudioA failed ...): loud=0, want 1
... errs 3, 5, 6, 7, plus err 0 and the two unknown codes
```

Mutants, each on a hash-verified fresh object:

| mutant | result |
|--------|--------|
| `==` → `!=` | killed |
| carve-out → `0` | killed |
| carve-out → `3` | killed |
| always loud (`? 1 : 1`) | killed |
| always quiet (`? 0 : 0`) | killed |
| `err == K` → `K == err` | **object byte-identical — provably equivalent, not a survival** |

Two earlier attempts (`return 1;` / `return 0;`) hit the documented
`-Wunused-parameter` + `-Werror` trap: the build *fails*, no object is written, and a
naive "did it pass? no → killed" reading calls that a kill when nothing ran. They were
recorded **inconclusive** and redone with the parameter still referenced. A kill
claimed from a build that did not compile is the worst kind of false evidence,
because it looks like rigor.

The test pins the whole table rather than "everything except 2", plus the loud/quiet
counts (6 and 1), so a mutant that flips one comparison dies there — and a *future*
failure step that nobody classified fails the test rather than passing through silently.

## Two corrections this work produced

**The guest CLI does have `kill`.** The earlier record says clearing a wedged process
"needs a guest-side `kill` (the lane's CLI has no `kill` in reach)". That conflated two
things: the *lane* (`spike_server.py`) has no kill action — its full action set is
`--exec/--get/--put/--run-script/--ui-*` — but the **AROS CLI on the guest does**:

```
[exec] 'kill Process8' -> rc=0 (5 ms)
       kill: object not found
```

`rc=0` with "object not found" on a bad argument, which is the AROS CLI's usual
shape. So "no kill in reach" was wrong, and it was wrong in a way that would have sent
someone to reboot when a `kill` was sitting there.

**The new requester is modal, and its process lingers holding the card.** Closing the
panel reports `ok` and dismisses the box, but the process stays in `status` with **no
window at all** for a few minutes. That is not cosmetic: those leftovers still held
`ahi.device`, so a *subsequent single* launch also got `err 4` and its own requester —
a stale holder poisons later launches until it exits. Identified by the combination of
"in `status`" plus "absent from `--ui-windows`", which is the only signature that
distinguishes a live-but-windowless holder from a stale line.

Practical consequence for the lane: after any two-instance experiment, **wait for
`status` to clear before launching again**, or the next launch inherits the failure and
it looks like a new bug.

## Method note

One `--get` was taken while the app was still running, for the startup AHI line. The
startup lines are present and were the point, but the shutdown line is not — the stale
-bytes rule still holds, and it was broken deliberately and narrowly rather than by
accident. Everything load-bearing was re-pulled after close: the shutdown summary, the
`build=2ecffd0` ev-log line, and the final single-instance control.

## See Also

- [One requester, two instances, and a silent null backend](2026-10-02-one-requester-two-instances-and-a-silent-null-backend.md) — the gap this closes
- [AROS does not resolve `..` in a path — and a playlist that reaches a sibling directory fails silently at the end of every cycle](2026-10-02-aros-does-not-resolve-dotdot-playlist-entry-failure.md)
- [HEAD misses the audio deadline on a real song: 19,703 xruns on the Dell](2026-10-02-head-misses-the-audio-deadline-on-a-real-song.md)
- [The Dell lane's two wedges — bare-path launch, hung `--ui-capture`](2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md)
- [The scripted A,B,B,A settles it — arm wins, repaint policy regresses](2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md)