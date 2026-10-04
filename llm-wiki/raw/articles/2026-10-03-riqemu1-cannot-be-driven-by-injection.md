# riqemu1 cannot be driven by injection, and two host mistakes looked like guest faults (2026-10-03)

- Source: ReIncarnation session, 2026-10-03 (opencode lane, riqemu1)
- Collected: 2026-10-03
- Published: 2026-10-03
- Raw: [riqemu1 lane measurements](../evidence/2026-10-03-riqemu1-lane-measurements.md)
- Related: [AHI on riqemu1](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md), [AC97 resamples to 44.1 kHz](2026-10-03-riqemu1-ac97-resamples-48k-to-44k1-so-playback-is-8-1-slow.md)

Three things about this lane cost hours, and **all three were mine**. None of them
was a guest fault. The record is here because the diagnostic shape — *the target
is broken* when the harness is broken — is the expensive mistake, not the click
that failed.

## 1. Black screen: I started the VM wrongly

```
before   P6 0 1600        <- QEMU's display surface had WIDTH ZERO
after    P6 1280 1024     <- correct
```

`P6 0 1600` is a PPM header with no width. QEMU had allocated **no display
surface at all**, which is why the window was black (host mean luminance **2.1**,
against **151.4** once correct).

The difference was my launch. I had wrapped the script:

```bash
setsid ./start_riqemu1.sh > /tmp/riqemu1_start.log 2>&1 < /dev/null &
```

`start_riqemu1.sh` **already ends in `-daemonize`**. The extra `setsid` with
stdin redirected from `/dev/null` is what produced a 0-width surface. Launched the
documented way — spooler already listening on 9295, then a plain
`./start_riqemu1.sh` — it comes back at 1280x1024 and renders.

**`screendump` is the instrument that settles this.** "The window is black" is a
symptom with several causes; `P6 0 1600` says the surface was never allocated,
which points at launch rather than at the guest, the driver or the video mode.
Reach for it first next time instead of reading guest files.

## 2. A modal requester grabs the pointer, and hard resets cause the requester

The guest was halted at:

```
Smart Filesystem request
Device DH0: (data.device, unit 0)
Has an unfinished transaction which will be loaded now.
```

**The mouse was not broken.** AROS requesters are modal and grab the pointer by
design; `info mice` confirmed QEMU's side was correct throughout
(`* Mouse #3: QEMU HID Tablet (absolute)`).

Two errors here, both mine:

- **I created the requester.** Reloading the patched binaries meant repeated
  `system_reset` — a hard reset with no filesystem flush — which is what leaves
  DH0 mid-transaction. `system_powerdown` is the clean shutdown. It did not work
  at the time because the guest was halted in early boot before ACPI was up, but
  it is the right tool and the hard reset is the wrong one.
- **I could not click OK.** `sendkey ret` did not dismiss it; `sendkey tab` then
  `ret` did. This host has no mouse-injection tool (`ydotool`, `xdotool` absent, no
  `hyprctl` cursor dispatcher, QEMU's relative `mouse_move` missed), so for a while
  the only way out looked to be a human.

## 3. Injected clicks do not reach the guest

This is the standing blocker and it is **not** a requester artefact — the
requester was long gone.

```
--ui-click l,340,82     (Play)   -> [ui] injected 3 event(s)   -> nothing
--ui-click l,100,165    (DRUMS)  -> [ui] injected 3 event(s)   -> tab unchanged
```

The protocol reported success every time. The log agreed:

```
RIAPP stg: playing=0      (never started)
RIAPP closed: ... stg_playing=0 stg_stopped=14501
```

and no `RIAPP play` line at all. I had measured Play's position three times from
captures before accepting that the click was not the problem — and switching the
active pointer to PS/2 (`mouse_set 2`, verified `* Mouse #2` afterwards) changed
nothing.

`--ui-capture` and `--ui-windows` work perfectly on the same connection, so it is
specifically the input path. **Audible audio had to be confirmed by a hand click.**

That is a real gap in the lane, not a workaround to apologise for: any harness
that needs to press Play on riqemu1 is stuck, which is why the Levi sub-split
measurement had to wait for the Dell.

## 4. RIAPP running kills the guest `exec` channel

Separate from clicks and just as obstructive, because it silently invalidates
any harness that reads a log while playing:

```
[exec] 'version' -> rc=1 (3 ms)
       (no output)
```

five times, while on the same connection:

```
[ui  ] windows 1280x1024 screen, 3 window(s) (3 ms)
       RIAPP live panel                         0,0 974x680 [active,close@5,0]
```

`ui-*` works, `exec` does not. After closing RIAPP:

```
[exec] 'version' -> rc=0 (21 ms)
       Kickstart 51.51, Workbench 40.0
```

Immediate and total. **This is why I read "the log is unreadable behind the
requester" for hours** — there was no requester involved at all. `exec` was dead
while RIAPP ran, and closing the panel brought it straight back. Every log read on
this lane has to be sequenced: launch, measure, **close, then read.**

Not root-caused. It is reproducible and it is not the input path.

> **Correction 2026-10-04: the `exec`/ui split is riqemu1-SPECIFIC, not a
> general lane law.** On the Dell, with `RAM:RIPP-VCOUNT` live as Process 8 and its
> panel up, `exec` works normally:
>
> ```
> [exec] 'status' -> rc=0 (48 ms)
> ```
>
> So "RIAPP running kills the guest exec channel" belongs to riqemu1 alone. It had
> been recorded here in general terms, and the lane sequencing rule derived from it
> ("launch, measure, close, then read") is still the right practice on riqemu1 —
> it is simply not a property every lane has. Record:
> [the LFO path never runs](2026-10-04-the-lfo-path-never-runs.md).

## Method

- **`screendump` before theorising.** It converted "black screen" into "width 0",
  which is a statement about QEMU rather than about the guest.
- **A protocol-level success is not a delivered action.** `injected 3 event(s)` is
  the server's acknowledgement, not evidence the guest saw a click. Only the log
  (`stg_playing=0`) settled it.
- **Check what a tool reports, then check what happened.** `rc=1` with no output
  from `exec` while `ui-windows` succeeded on the same socket was the evidence
  that split the two paths.
- **Read the launch script before wrapping it.** `-daemonize` was in the script
  the whole time; `setsid` was mine and was the black screen.
- **A running VM and a visible window are different facts.** The framebuffer
  check (`P6 1024 768`) said the VM was healthy while the compositor listed no
  QEMU window at all. Same instrument as above, opposite question: `screendump`
  describes the guest's VGA device, and nothing about the host desktop. Proved
  by `foot` also failing to map — the constraint is session scope, not GTK — and
  resolved by running the identical arguments minus `-daemonize` under a tracked
  user service. **`-daemonize` and `systemd-run` are incompatible**: the unit
  reported `Finished with result: success` in **101 ms** and systemd's
  `KillMode=control-group` killed the orphaned daemon. Full record:
  [proving the VM window is actually visible](2026-10-03-proving-the-vm-window-is-actually-visible.md).

  > **Superseded detail.** This record's `-vga vmware` note stands, but the
  > related claim that `grim` needed anything beyond a capture is corrected in the
  > new record: on this host `grim` takes **only a positional output file** — no
  > `-o`, no region argument — and `grim --help` fails. Window luminance is
  > obtained by capturing the whole output and sampling the window's rectangle.

## See Also

- [AHI on riqemu1: the loader ignores `sh_addralign`](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md)
- [AC97 resamples 48 kHz to 44.1 kHz](2026-10-03-riqemu1-ac97-resamples-48k-to-44k1-so-playback-is-8-1-slow.md)