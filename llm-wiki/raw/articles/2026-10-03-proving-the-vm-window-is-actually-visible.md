# Proving the VM window is actually visible, because the documented launcher does not make it so (2026-10-03)

- Source: ReIncarnation session, 2026-10-03 (opencode lane, host session, riqemu1)
- Collected: 2026-10-03
- Published: 2026-10-03
- Raw: [making the window visible, verbatim](../evidence/2026-10-03-window-visibility-and-session-scope.md)
- Related: [the riqemu1 lane cannot be driven by injection](2026-10-03-riqemu1-cannot-be-driven-by-injection.md), [AHI on riqemu1: the loader ignores `sh_addralign`](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md), [riqemu1 lane measurements](../evidence/2026-10-03-riqemu1-lane-measurements.md)

The instruction was two words long — *restart the vm, make sure it's visible* —
and the second half took six attempts, three of which failed in ways that looked
like success. The lane was up the whole time. **The window was not.**

## The trap: `screendump` cannot answer this question

The obvious check says the VM is fine:

```
P6 1024 768 255
```

Width non-zero, valid PPM header, and the compositor disagrees completely:

```
=== 2. is the window mapped and visible to the compositor? ===
  no QEMU window in the compositor client list
```

**A `P6` header describes the guest's VGA framebuffer. It says nothing about
whether a window exists on the host.** This is the recorded `P6 0 1600` lesson
pointing the other way: that header proved there was *no* display surface when
the symptom was a black screen. Here a perfectly valid header coexists with no
window at all. Either way the instrument is about QEMU's video device, and a
question about the host desktop needs a different instrument.

## Nothing from the agent's shell can map a window

Before blaming QEMU, the shell was tested with something known-good. `foot` is a
terminal the owner is obviously able to run:

```
=== can a GUI app map a window from THIS shell? (foot) ===
  clients: ['slack', 'nordvpn-gui', 'ai.opencode.desktop']
  foot mapped: False
```

It never appeared. Nor did QEMU, across every launch method. So the constraint
is **session scope, not GTK** — and the host had everything a Wayland client
needs:

```
  DISPLAY=:0  WAYLAND_DISPLAY=wayland-1  XDG_SESSION_TYPE=wayland
  1486 Xwayland :0 -rootless -core -listenfd 51 -listenfd 52 -displayfd 91 -wm 88
  srwxr-xr-x 1 miller miller 0 Oct  3 10:19 /run/user/1000/wayland-1
  DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
```

**Worth stating plainly, because it is invisible until it wastes an hour: a
process can be in the right session bus, have the right `WAYLAND_DISPLAY`, and
still be unable to map a window.**

## Dead end: the compositor cannot launch it either

Using Hyprland itself as the launcher is the obvious next move, and it is
unavailable here:

```
$ hyprctl dispatch exec "foot -e 'sleep 30'"
error: [string "return hl.dispatch(exec foot -e 'sleep 30')"]:1: ')' expected near 'foot'

$ hyprctl eval 'hl.dispatch("exec", "/tmp/opencode/probe_gui.sh")'
error: ... hl.dispatch: expected a dispatcher (e.g. hl.dsp.window.close())

$ hyprctl eval 'hl.dsp.exec("/tmp/opencode/probe_gui.sh")'
error: ... attempt to call a nil value (field 'exec')

$ hyprctl eval '... enumerate pairs(hl.dsp) ...'
ok
```

`ok` with an **empty** list: `hl.dsp` has no members on this build. Also
`hyprctl lua` does not exist — the Lua entry point is `hyprctl eval`, and
`dispatch` is a Lua shorthand whose quoting rules differ from what its own
`--help` implies.

## The real culprit: `-daemonize` and `systemd-run` are incompatible

Running the documented script under a user service looked like the fix for the
session scope. It reported success:

```
$ systemd-run --user --unit=riqemu1-vm2 --wait --pipe /tmp/opencode/launch_riqemu1.sh
Running as unit: riqemu1-vm2.service
          Finished with result: success
Main processes terminated with: code=exited, status=0/SUCCESS
               Service runtime: 101ms
```

**Success, in 101 ms, and no QEMU afterwards.** The wrapper was a one-liner around
the unmodified `start_riqemu1.sh`, so this is the script's own behaviour.

`start_riqemu1.sh` ends in `-daemonize`. QEMU forks, the parent exits, systemd
declares the unit finished, and `KillMode=control-group` kills the orphaned
daemon. `--collect` then removed even the evidence:

```
=== unit status ===
Unit riqemu1-vm.service could not be found.
```

**"Finished with result: success" from a unit whose whole purpose was to keep a
daemon alive is the exact shape of a silent failure**, and it is worth distrusting
any systemd-run result until the process itself is confirmed present.

## The fix: identical arguments, minus `-daemonize`, tracked

```
$ systemd-run --user --unit=riqemu1-vm3 --collect /tmp/opencode/start_riqemu1_visible.sh
Running as unit: riqemu1-vm3.service; invocation ID: 0f8c023de01b4b588cc0a99ab53f5154
=== QEMU alive? ===
16978 qemu-system-x86_64 -name riqemu1
=== window mapped? ===
  total clients: 4
  qemu window: True
   at [1287, 38] size [1261, 1390] mapped True
```

QEMU stays the main process, so systemd keeps it, and a user service carries the
session scope a Wayland client needs. `/home/miller/Work/vms/start_riqemu1.sh`
was **not modified**; the alternative launcher lives in `/tmp/opencode/` and
carries a comment saying not to "simplify" it back without checking the window,
because the screendump looks identical either way.

## Verifying it renders, not just that it maps

A mapped window can still be black, so the pixels were checked. `grim` on this
host takes only a positional output file:

```
$ grim --help
grim: invalid option -- '-'
$ grim -o full.png
unknown output 'full.png'
$ grim full.png
  captured 1149179 bytes
```

Sampling the window's region out of the compositor capture:

```
  screenshot 2560x1440
  QEMU window region (1287,38 1261x1390): mean=73.3 max=255.0
    -> a black/blank surface would read mean~2
```

That `mean~2` is not a guess: the earlier black-screen episode measured **2.1**
for the same computation, and the fixed case **151.4**. So 73.3 is a rendering
window, and the comparison has a precedent to stand on.

"Fits on screen" was checked rather than assumed:

```
=== monitors ===
  DP-1: 2560x1440
```

Window spans x 1287–2548, y 38–1428 — inside 2560×1440.

The guest was then confirmed alive, and `exec rc=0` doubles as proof that **RIAPP
is not running**, so there is no AHI contention:

```
[ping] -> ok=True (4 ms)
[exec] 'status' -> rc=0 (19 ms)
       Process 5 Loaded as command: RAM:ATCPBIN
       Process 1 Loaded as command: RAM:net/c/AROSTCP
[submit] RESULT: PASS (agent anon, session 2)
```

## One loose end: the framebuffer changed size

```
P6 1280 1024 255      (before the reboots)
P6 1024 768 255       (after)
```

The guest is healthy and the agent is up, so this looks like the display driver
settling on a different mode rather than a fault — but it is **not confirmed**,
and it is recorded as unresolved rather than smoothed over. `-vga vmware` is what
makes wide modes reachable at all on this lane, so a mode change is worth a look.

## A general trap, in one line

```
=== riqemu1 process: PID and age ===
   9540 Sat Oct  3 10:50:09 2026  00:00 /usr/bin/bash -c ... "$(pgrep -f 'name riqemu1' | head -1)" ...
```

`pgrep -f` matched **its own command line**, which contained the pattern, and
reported a VM that had been dead since the reboot:

```
  riqemu1 VM:        running
```

A pattern search over process command lines will find the search. `pgrep -x` is
also unusable here — the binary name exceeds 15 characters, so it warns and
matches nothing.

## Why only riqemu1 dies on a reboot

Three spooler services exist and all three are units:

```
spike-laptop.service   active running Spike bridge for Dell E6320 lane (port 9292)
spike-s6.service       active running Spike bridge for S6/S7 proof lane (port 9294, pair e6320x)
spike-v4.service       active running Spike bridge for the Vulkan4AROS AROS QEMU/SLIRP lane (port 9091, anonymous slot)
```

riqemu1's spooler on 9295 is started by hand, so it is the only lane that does
not come back by itself:

```
  port 9295 spooler: DOWN (no systemd unit)
  port 9292 Dell:    up (unit)
```

Six lines would fix it, and it is the last thing standing between this lane and
surviving a reboot unattended.

## Method

- **A framebuffer header is not a window.** For "can the user see it", ask the
  compositor and then ask the pixels. Two instruments, two questions.
- **When a launch "succeeds", confirm the process exists.** `Finished with
  result: success` in 101 ms was the whole failure.
- **Test the harness with something known-good.** `foot` failing is what proved
  the problem was the shell and not QEMU's GTK.
- **`pgrep -f` finds itself.** Match on something the pattern cannot contain.

## See Also

- [the riqemu1 lane cannot be driven by injection](2026-10-03-riqemu1-cannot-be-driven-by-injection.md) — the other half of this lane's harness limits, and where the `-daemonize` black-screen trap is recorded
- [riqemu1 lane measurements](../evidence/2026-10-03-riqemu1-lane-measurements.md) — the original `grim` luminance pair, 2.1 black against 151.4 working