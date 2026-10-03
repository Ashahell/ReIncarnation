# riqemu1 lane session 2026-10-03 — raw measurements (rate, display, exec, input)

**Ingested:** 2026-10-03 into ReIncarnation `llm-wiki`
**Source:** host-side command output and guest agent replies on riqemu1
(`start_riqemu1.sh`, ABI v1 lane), collected by the ReIncarnation opencode lane.
**Provenance:** verbatim terminal output. Every figure below is a literal from a
command; derived values show their components.
**Recorded in:** [the AC97 44100 resample](../articles/2026-10-03-riqemu1-ac97-resamples-48k-to-44k1-so-playback-is-8-1-slow.md);
[the lane cannot be driven by injection](../articles/2026-10-03-riqemu1-cannot-be-driven-by-injection.md)

## 1. The guest/host rate mismatch

Guest, from `RIAPP.LOG` (`RAM:RIAPP.LOG`):

```
audio: AHI low-level mode=0x00390004 mix=48000 Hz buffer=256 frames period=5333 us
```

Host, `pactl list sink-inputs`:

```
Sink Input #2337
	Format: pcm, format.sample_format = "\"s16le\""  format.rate = "44100"  format.channels = "2"  format.channel_map = "\"front-left,front-right\""
	Corked: no
		application.name = "riqemu1"
```

QEMU launch stanza, `start_riqemu1.sh`:

```
exec qemu-system-x86_64 -name riqemu1 -m 2048 -smp 2 -enable-kvm -cpu host \
  -drive file=/home/miller/Work/vms/riqemu1_dh0.img,format=raw,if=ide,index=0,media=disk,cache=directsync \
  -drive file=/home/miller/Work/vms/riqemu1_test.iso,format=raw,if=ide,index=2,media=cdrom \
  -boot c -vga vmware -display gtk -monitor tcp:127.0.0.1:4477,server,nowait \
  -serial file:/tmp/riqemu1_serial.log -netdev user,id=net0,ipv6=off -device rtl8139,netdev=net0 \
  -audiodev pa,id=pa0,server=/run/user/1000/pulse/native -device AC97,audiodev=pa0 \
  -usb -device usb-tablet -daemonize
```

Ratio: `44100 / 48000 = 0.91875`, so playback is 8.1 % slow.

## 2. The guest is not slow — its own counters

From the same run, `RIAPP closed:` and the heartbeat `RIAPP hb:`:

```
RIAPP hb: buffers=3808 xruns=0 render_max=3113 us wake_max=87 us wake_n=3806 prio=21 arm_us=0 load=3/1000 overloads=0 snd=0/0/0/0 pend=0/0/0/0
RIAPP closed: buffers=14501 xruns=0 render_max=54 us render_total=286 ms period=5333 us wake_max=132 us wake_total=316 ms wake_n=14499 stg_total_avg=0 us stg_dsp_avg=0 us stg_evt_avg=0 us stg_playing=0 stg_stopped=14501
```

and, while actually playing:

```
RIAPP hb: buffers=35876 xruns=0 render_max=3113 us wake_max=228 us wake_n=35874 prio=21 arm_us=0 load=3/1000 overloads=0 snd=0/0/0/0 pend=0/0/0/0
RIAPP stg: playing=848 stopped=2960 stopped_avg=6 us
RIAPP dstg block   avg=462 us  max=976 us
RIAPP dstg levi    avg=172 us  max=647 us
RIAPP dstg lev-voice avg=124 us  max=573 us
```

`render_max` 3113 us against a 5333 us period is 58 % of the period; `xruns=0`;
`load=3/1000`.

Pacing: 3808 buffers x 5333 us = 20.3 s of wall clock, carrying
3808 x 256 / 48000 = 20.3 s of audio. Ratio 1.00.

Capture while playing, `parec --device=@DEFAULT_MONITOR@`:

```
  samples 2719744  peak 0  rms 0.0  non-zero 0.00%
```

## 3. Black screen: the display surface had zero width

`screendump` over the monitor port 4477, before and after restarting the VM the
documented way (spooler already up on 9295, then plain `./start_riqemu1.sh`):

```
before   P6 0 1600        <- width 0: QEMU had no display surface
after    P6 1280 1024     <- correct
```

Host window mean luminance, same window, captured with `grim`:

```
  host window mean luminance: 2.1    (a black screen)
  host window mean luminance: 151.4
```

The launch that produced the 0-width surface wrapped the script in `setsid` with
stdin redirected from `/dev/null`. `start_riqemu1.sh` already ends in `-daemonize`,
so the extra detachment is what differed.

## 4. Injected clicks do not reach the guest

QEMU's own device list:

```
  Mouse #2: QEMU PS/2 Mouse
* Mouse #3: QEMU HID Tablet (absolute)
```

After `mouse_set 2`:

```
  Mouse #3: QEMU HID Tablet (absolute)
* Mouse #2: QEMU PS/2 Mouse
```

`--ui-click l,340,82` on the Play button and `l,100,165` on the DRUMS tab both
reported success at the protocol level while nothing on screen changed:

```
[ui  ] injected 3 event(s)
```

The log shows the consequence: with RIAPP running, `stg_playing=0` and no
`RIAPP play` line at all.

## 5. RIAPP running kills the guest exec channel

```
[exec] 'version' -> rc=1 (3 ms)
       (no output)
```

repeated five times, while `--ui-windows` on the same connection returned:

```
[ui  ] windows 1280x1024 screen, 3 window(s) (3 ms)
       RIAPP live panel                         0,0 974x680 [active,close@5,0]
```

After `--ui-close "#0"`:

```
[exec] 'wait' -> rc=0 (1031 ms)
[exec] 'version' -> rc=0 (21 ms)
       Kickstart 51.51, Workbench 40.0
```

`RAM:` listing while RIAPP was closed, showing `RIAPP.LOG` present:

```
       RIAPP.LOG                  67933 ---rwed Today       05:01:04
       RIAPP-EV.LOG                 183 ---rwed Today       05:01:04
```

and `T:` at the same moment, holding the log from the run before `RIAPP_LOG` was
pinned to `RAM:`:

```
Directory "T:" on Saturday 10/03/26:
       Tmp1178072802231             642 ---rwed Today       04:40:04
       Tmp5178072802251            1202 ---rwed Today       04:40:04
       RIAPP.LOG                  23159 ---rwed Today       04:47:23
       3 files - 51 blocks used
```

## 6. A modal requester grabs the pointer

Guest at boot, after repeated hard resets:

```
Smart Filesystem request
Device DH0: (data.device, unit 0)
Has an unfinished transaction which will be loaded now.
```

QEMU monitor `sendkey ret` did not dismiss it; `sendkey tab` then `ret` did, and
the guest then showed `Please turn off your system using the power Switch.`,
i.e. the pending ACPI powerdown had been registered after all.

## 7. The mutation tally for the song-name work

```
MUTANTS t153_sticky_log: total 7 killed 7 survived 0
MUTANTS t159_tr_song_name: total 17 killed 6 survived 11
```