# Making the riqemu1 window actually visible — verbatim, 2026-10-03

**Ingested:** 2026-10-03 into ReIncarnation `llm-wiki`
**Source:** host-side session after the second cold-reboot of the day, bringing
riqemu1 up with a window the owner can see, and verifying that claim rather than
assuming it.
**Provenance:** verbatim command output. Every tool invocation that failed is
included with its error, because the dead ends are the useful part.
**Recorded in:** [proving the VM window is actually visible](../articles/2026-10-03-proving-the-vm-window-is-actually-visible.md)

## 1. The host had rebooted again, 31 minutes earlier

```
up 31 minutes
         system boot  2026-10-03 10:19
current boot id: edc2506c-381d-4547-a269-60a805981e35
```

Boot id `edc2506c…` matches the second `-- Boot` marker in the Dell spooler's
journal, which is how the second reboot was identified at all.

## 2. The documented launch produced a running VM with no window

Launched plainly, no `setsid`:

```
  confirmed: no QEMU running
=== QEMU alive? ===
16978 qemu-system-x86_64 -name riqemu1      (that was the later, working launch)
```

The first plain launch gave a healthy framebuffer:

```
=== 1. is there a display surface? (width 0 = the old black screen) ===
P6 1024 768 255
=== 2. is the window mapped and visible to the compositor? ===
  no QEMU window in the compositor client list
```

and the launch log held nothing but PulseAudio noise:

```
pulseaudio: set_sink_input_volume() failed Reason: Invalid argument
pulseaudio: set_sink_input_mute() failed Reason: Invalid argument
pulseaudio: set_source_output_volume() failed Reason: Invalid argument
```

The host session, for the record:

```
  DISPLAY=:0  WAYLAND_DISPLAY=wayland-1  XDG_SESSION_TYPE=wayland
  1486 Xwayland :0 -rootless -core -listenfd 51 -listenfd 52 -displayfd 91 -wm 88
  srwxr-xr-x 1 miller miller 0 Oct  3 10:19 /run/user/1000/wayland-1
  DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
```

Everything a GUI client needs was present.

## 3. Nothing launched from the agent's shell can map a window

`foot` is a terminal the owner is demonstrably able to run. Launched from this
shell it never appeared:

```
=== can a GUI app map a window from THIS shell? (foot) ===
  clients: ['slack', 'nordvpn-gui', 'ai.opencode.desktop']
  foot mapped: False
```

The full client list across every attempt stayed at three or four entries —
`slack`, `nordvpn-gui`, `ai.opencode.desktop` — and never gained a QEMU.

## 4. `hyprctl` dispatch is not usable on this build

```
$ hyprctl dispatch exec "foot -e 'sleep 30'"
error: [string "return hl.dispatch(exec foot -e 'sleep 30')"]:1: ')' expected near 'foot'

$ hyprctl dispatch exec /tmp/opencode/probe_gui.sh
error: [string "return hl.dispatch(exec /tmp/opencode/probe_g..."]:1: attempt to perform arithmetic on a nil value (global 'exec')

$ hyprctl lua 'hl.dispatch("exec", "/tmp/opencode/probe_gui.sh")'
unknown request

$ hyprctl eval 'hl.dispatch("exec", "/tmp/opencode/probe_gui.sh")'
error: ... hl.dispatch: expected a dispatcher (e.g. hl.dsp.window.close())

$ hyprctl eval 'hl.dsp.exec("/tmp/opencode/probe_gui.sh")'
error: ... attempt to call a nil value (field 'exec')

$ hyprctl eval 'return table.concat((function() local t={} for k,v in pairs(hl.dsp) do t[#t+1]=k end table.sort(t) return t end)(), ", ")'
ok
```

`ok` with an empty list: **`hl.dsp` is empty on this build**, so the compositor
cannot be used as a launcher. `hyprctl lua` does not exist either; the Lua entry
point is `hyprctl eval`.

## 5. `systemd-run` with the documented script kills the VM — measured

```
$ systemd-run --user --unit=riqemu1-vm2 --wait --pipe /tmp/opencode/launch_riqemu1.sh
Running as unit: riqemu1-vm2.service
          Finished with result: success
Main processes terminated with: code=exited, status=0/SUCCESS
               Service runtime: 101ms
                 CPU time consumed: 73ms
                       Memory peak: 31.1M (swap: 0B)
```

**Success, in 101 ms, and no QEMU afterwards.** `/tmp/opencode/launch_riqemu1.sh`
was a one-line wrapper around the unmodified
`/home/miller/Work/vms/start_riqemu1.sh`, so this is that script's behaviour and
not an artefact of the wrapper. The script's last flag is `-daemonize`, so QEMU
forks and the parent exits; systemd then declares the unit finished and
`KillMode=control-group` kills the orphaned daemon. `--collect` made the unit
vanish entirely:

```
=== unit status ===
Unit riqemu1-vm.service could not be found.
```

## 6. The fix: identical arguments, minus `-daemonize`, tracked

```
$ chmod +x /tmp/opencode/start_riqemu1_visible.sh
$ systemd-run --user --unit=riqemu1-vm3 --collect /tmp/opencode/start_riqemu1_visible.sh
Running as unit: riqemu1-vm3.service; invocation ID: 0f8c023de01b4b588cc0a99ab53f5154
=== QEMU alive? ===
16978 qemu-system-x86_64 -name riqemu1
=== window mapped? ===
  total clients: 4
  qemu window: True
   at [1287, 38] size [1261, 1390] mapped True
```

## 7. It renders — measured on the compositor, not inferred

`grim` on this host takes only a positional output file:

```
$ grim --help
grim: invalid option -- '-'

$ grim -o full.png
unknown output 'full.png'

$ grim full.png
  captured 1149179 bytes
```

Screenshots the `P6` header cannot settle, because it describes the guest's
VGA framebuffer and not the host window at all:

```
  screenshot 2560x1440
  QEMU window region (1287,38 1261x1390): mean=73.3 max=255.0
    -> a black/blank surface would read mean~2
```

Monitor geometry, so "fits on screen" is a fact rather than an assumption:

```
=== monitors ===
  DP-1: 2560x1440 at None
```

Window spans x 1287–2548, y 38–1428: entirely inside 2560×1440.

## 8. The guest is alive and idle

```
[ping] -> ok=True (4 ms)
[exec] 'status' -> rc=0 (19 ms)
       Process 2 Loaded as command: ConClip
       Process 3 Loaded as command: Decorator
       Process 4 Loaded as command: � IPrefs �
       Process 5 Loaded as command: RAM:ATCPBIN
       Process 6 Loaded as command: WANDERER:Wanderer
       Process 1 Loaded as command: RAM:net/c/AROSTCP
       Process 7 Loaded as command: status
[submit] RESULT: PASS (agent anon, session 2)
```

`exec rc=0` is the positive signal that RIAPP is not running, so there is no AHI
contention.

Guest framebuffer, twice, both after boot:

```
P6 1280 1024 255
P6 1024 768 255
```

## 9. `pgrep -f` matched its own command line

This reported a VM that did not exist:

```
=== riqemu1 process: PID and age ===
    PID                  STARTED     ELAPSED CMD
   9540 Sat Oct  3 10:50:09 2026       00:00 /usr/bin/bash -c echo "=== riqemu1 process: PID and age ===" && ps -o pid,lstart,etime,cmd -p "$(pgrep -f 'name riqemu1' | head -1)" 2>/dev/null | cut -c1-140
```

The only match was the shell running the `pgrep`, whose own command line
contained the pattern. The same trap produced a false "running" a moment
earlier:

```
  riqemu1 VM:        running
```

which was wrong; the VM had been killed by the 10:19 reboot.

## 10. The reboot-survival gap, and why only this lane had it

Only three spooler services exist, and all three are units:

```
spike-laptop.service   active running Spike bridge for Dell E6320 lane (port 9292)
spike-s6.service       active running Spike bridge for S6/S7 proof lane (port 9294, pair e6320x)
spike-v4.service       active running Spike bridge for the Vulkan4AROS AROS QEMU/SLIRP lane (port 9091, anonymous slot)
```

riqemu1's spooler on 9295 is started by hand and is therefore the only lane that
did not come back:

```
  port 9295 spooler: DOWN (no systemd unit)
  port 9292 Dell:    up (unit)
```

## 11. The Dell connection, established and then killed by the reboot

```
09:49:06  non-agent connection from ('127.0.0.1', 60552) dropped (no hello)
10:17:00  session 1 (e6320): agent connected from ('192.168.1.60', 1024)
10:17:00  running job 20261003-094938-4850-1 (1 actions)
10:17:00  job 20261003-094938-4850-1 OK
-- Boot edc2506c381d4547a26960a805981e35 --
```

The job that ran at 10:17 was a ping submitted at 09:49 which had already timed
out from the submitter's side. The host address did not change across either
reboot:

```
  enp12s0    192.168.1.81/24
  nordlynx   10.5.0.2/16
```

and the Dell remained reachable throughout:

```
192.168.1.60: 3 packets transmitted, 3 received, 0% packet loss, ttl=255
```