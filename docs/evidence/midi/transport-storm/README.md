# The transport strobed for 21 hours on one injected key-down

**Found** 2026-10-26 06:39 (host) while checking the Dell lane after the P-KEY4
re-stage. **Cause** a single `--ui-rawkey 0x40` (space, key-down only) injected on
2026-10-25 ~09:23, never released. **Ended** by `--ui-rawkey 0x40,up` at
2026-10-26 06:38:59, i.e. 21 h 15 min later.

> **Correction 2026-10-26 07:20 — read `CORRECTION.md`.** This file first counted
> `TR` *lines* as toggles (19.45/s). One press made **two** lines: a press started
> playback and `song_poll_end` stopped it 43 ms later, because the transport
> could not be rewound off the song's end (P-TR2). The press rate is **9.73/s**
> (746 900 presses), and the strobe needed both defects. The P-TR1 fix is
> unaffected.

## What the evidence says

`RAM:RIAPP-EV.LOG` (39 582 889 B, 1 493 824 lines) contains **nothing but the
transport** after ev 25: 1 493 800 `TR PLAY` / `TR STOP` lines and **zero** other
events. See `stats.txt`, `onset.txt`, `tail.txt`.

| | |
|---|---|
| app start, auto-play | `ev 23  15 TR PLAY` |
| song reached its end | `ev 24  48883 TR STOP` (48 868 buffers of the 48 866-buffer song) |
| injected space key-down | `ev 25  57591 TR PLAY` (46 s after the song ended) |
| the end guard stops it | `ev 26  57599 TR STOP` (8 buffers = 43 ms later — **not** a repeat) |
| last press | `ev 1493824  14454370 TR STOP` |
| span | 14 396 779 buffers = 76 782.8 s = **21.33 h** |
| presses | **746 900** (one per PLAY/STOP pair), **9.73/s**, 53 B each -> 39.5 MB of `RAM:` |

The counter is a render-buffer counter that advances whether or not the transport
runs (14.45 M buffers at 187.5/s = 21.4 h = the app's uptime), which is why the
span can be read off the log without a clock.

## Why it happened

`ri_key_decode` maps space to `RI_KT_STOPPLAY` and **only key-down reaches it**
(key-up returns `RI_KA_NONE`, `gui/keymap.c:64`). `RI_KT_STOPPLAY` is the one
transport command that *toggles*: it reads the PLAY LED and presses the other
button (`gui/panelui.c:133`). So every key auto-repeat from `input.device` acted
as a press. AROS repeated the held key at **9.73 presses/s** (~101 ms), so a held
space is a press stream — from a stuck key, a lost release, or an injection that
only sends down.

Each press made a **pair** because the transport was parked at the song's end and
could not be moved off it: the press started playback, and `song_poll_end`
stopped it a median 8 render buffers (43 ms) later. See `CORRECTION.md`; that
second half is P-TR2.

`--ui-rawkey CODE` is key-down only by design; the release is `CODE,up`. The
harness must pair them, and a paired press is two submits, not one.

## What it cost

- The staged P-KEY4 instance (`build=afa8551`) was unusable for the owner's
  volume ear round for 21 h: the transport never stayed in either state.
- 39.5 MB of `RAM:` in one log, plus 30 MB of `RIAPP.LOG`, on a 768 MB machine.
- Every evidence pull of that log is a 39 MB transfer (no ranged read).

## What it is not

Not a P-KEY4 regression: the DSP and the keybed gain law were untouched (the
song plays normally between the toggles; `xruns=0`, `overloads=0` were recorded
at staging). This is a transport-key input defect, independent of the keybed.

## The defect, stated for the fix

A transport *toggle* key must act on the press edge, not on every auto-repeat.
`RI_KT_TEMPO_UP/DOWN` want their repeats (a held tempo key ramps); `RI_KT_STOPPLAY`
must not. So the guard belongs on the toggle command alone, not on `RI_KA_TRANSPORT`
as a class.

## Files

- `onset.txt` — the log from `ev 21` (song load) to `ev 45` (the first 20 toggles).
- `tail.txt` — the last 12 lines of the 1.49 M.
- `stats.txt` — the numbers above, with the sha256 of the full log.
- `list-evlog.txt` — `List RAM:RIAPP-EV.LOG` showing the size frozen at the key-up.
- `panel-before-relaunch.png` — the guest at 06:42, scale-2 capture.

## The P-TR2 stage (2026-10-28): the fix reached the engine, the latch did not re-arm

Staged `build=d35b6fe` (1 221 720 B, sha `7b277e2f264c526f5b6bd2eceb45c556c207b4dee7431717b85db70e5df1c041`),
relaunched, one RIAPP window, `SONG load … bars=151 bpm=140`, `TR PLAY` — `ev-ptr2.log`,
998 B / 23 lines, no storm. Then, each press paired (down, then `,up`):

| step | key | result |
|---|---|---|
| 1 | `0x0F` (KP0 = STOP) | `ev 24  3145 TR STOP` — the state path works, and one press is one line |
| 2 | `0x0F` again | **no line at all** — `ev-ptr2c.log` is byte-identical to `ev-ptr2b.log` (sha `cdc8ef64…`) |
| 3 | `0x40` (space = toggle) | `ev 25  36727 TR PLAY` |
| 4 | `0x40` again | **nothing** — `ev-ptr2d.log` |

So the propagation fix worked and the **press-edge latch never cleared**: every press-count
key worked once per launch. Press 2 should have been the rewind (clicks 0 → 1, `TR STOP`),
and press 4 should have stopped the transport.

**Cause.** `ri_cev_key()` (`app/core/canvas_events.c`, the body of
`gui/widgets/rsection.mcc.c`'s RAWKEY branch) decoded the key first and returned early on
`RI_KA_NONE` — which is exactly what a key-up decodes to (`gui/keymap.c`). `ri_panel_key()`,
which owns the release-clear, was therefore **never called for a release** on the real app.
The host tests called `ri_panel_key()` directly, one level below the door, so they were
green. P-TR1's latch had this hole from the day it shipped; P-TR2's STOP latch inherited it.

**Fix.** The door calls `ri_panel_key()` for every key event and uses the decode only to
decide whether to eat it. `t242` now pins it at the canvas level: RED **4**, exactly the
release pins (the press after the release must act, for KP0 and for space), and mutant
**m7 (decode-first)** is killed by `t242` only.

**Lesson.** Test the door the app actually uses, not the function below it. A latch whose
clear path lives in a function the app never calls is a one-shot key, and the host test
that calls that function directly cannot tell the difference.

## The re-stage (2026-10-28): both halves proven on the lane

Staged the door fix, `build=d676bba` (1 221 720 B, sha
`cfb4c086b941cbb6ed032c33d30b179ab54d7f211d42a4185a1d2aa8e414cd9c`), relaunched by
`RAM:LEVIPROBE.RUN` (`rc=0`, 15 098 ms). Every press below is paired (down, then `,up`),
and the log was pulled after each pair. `ev-ptr2e.log` is the launch; `ev-ptr2k.log` is the
final state.

| step | key | ev | buffer | result |
|---|---|---|---|---|
| launch | — | 23 | 15 | `TR PLAY` (auto-play) |
| 1 | `0x40` space | 24 | 3341 | `TR STOP` |
| 2 | space again | 25 | 4281 | `TR PLAY` — **the release re-arms** (dead in `d35b6fe`) |
| 3 | `0x0F` KP0 = STOP | 26 | 6072 | `TR STOP` — the state path |
| 4 | KP0 again | 27 | 7107 | `TR STOP` — **the clicks path reached the engine** (P-TR2; no line before the fix) |
| 5 | `0x43` KPENTER = PLAY | 28 | 9081 | `TR PLAY`, and no stop after it |
| song end | — | 29 | 57945 | `TR STOP` — 48 864 buffers after 9081, the song's length |
| 6 | KP0 at the song's end | 30 | 69913 | `TR STOP` — a stop press while stopped |
| 7 | KPENTER | 31 | 70853 | `TR PLAY` and **no `TR STOP`** — the song replays without a relaunch |

40 s after step 7 the log still ended at ev 31, so the injected keys were released and the
song was still playing. Steps 6+7 are the exact user story that was broken: "the song plays
once per launch".

The instance was left playing from bar 0 for the P-KEY4 ear round, on `build=d676bba`.
