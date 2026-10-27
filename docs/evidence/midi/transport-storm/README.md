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
