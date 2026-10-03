# riqemu1 rate probe after the host cold-reboot, and QEMU's AC97 is not fixed at 44100 — verbatim

**Ingested:** 2026-10-03 into ReIncarnation `llm-wiki`
**Source:** `audio_io/probe_rate.c` (v1 lane build) run on riqemu1 after a host
cold-reboot, plus source reads of QEMU's `ac97.c`/`audio.c` and AROS's
`ac97-main.c` from trees already on this host.
**Provenance:** verbatim agent output and verbatim source lines with file and
line numbers. Derived values show their components.
**Recorded in:** [correcting the AC97 article](../articles/2026-10-03-riqemu1-ac97-resamples-48k-to-44k1-so-playback-is-8-1-slow.md)
and [the Dell rate record](../articles/2026-10-03-dell-ahi-hands-back-44100-for-every-rate.md)

## 1. Context: what the reboot cost

```
up 4 minutes
         system boot  2026-10-03 09:45
```

`/tmp` was wiped, so: the riqemu1 VM, its spooler on 9295, its spool directory,
its serial log and every `/tmp/ri/aros/*` artifact were gone. The Dell lane was
unaffected — **its spooler is a systemd user unit** (`spike-laptop.service`) and
came back at boot; riqemu1's spooler had no unit, which is why it alone did not:

```
Oct 03 09:45:06 frostmourne systemd[1137]: Started Spike bridge for Dell E6320 lane (port 9292).
Oct 03 09:45:06 frostmourne python3[1208]: [srv] persistent multi-agent server on :9292  spool=/tmp/spike_spool_laptop  max-sessions=16  bulk-port=9092
```

The Dell guest had **not** re-dialled, because it does not reboot with the host.

## 2. riqemu1 came back clean

```
[exec] 'status' -> rc=0 (10 ms)
       Process 2 Loaded as command: ConClip
       Process 3 Loaded as command: Decorator
       Process 4 Loaded as command: � IPrefs �
       Process 5 Loaded as command: RAM:ATCPBIN
       Process 6 Loaded as command: WANDERER:Wanderer
       Process 1 Loaded as command: RAM:net/c/AROSTCP
       Process 7 Loaded as command: status
[ui  ] windows 1280x1024 screen, 2 window(s) (3 ms)
                                                0,18 1280x1006 [no-close]
       ReIncarnation agent (ATCPBIN)            96,20 640x180 [active,no-close]
```

Two windows only: **no `Smart Filesystem request`**, despite the power loss
having happened while RIAPP was running. And `exec` returns `rc=0`, which is the
positive signal that RIAPP is **not** running — that defect is RIAPP-dependent.

Display surface, from the QEMU monitor:

```
P6 1280 1024 255
```

Width 1280, not the `P6 0 1600` that diagnosed the earlier black screen.

Guest state that survived on disk:

```
[exec] 'dir SYS:Classes/ReIncarnation/Songs' -> rc=0 (11 ms)
         zombie-nation.rbng
[exec] 'dir T:' -> rc=0 (7 ms)
         Tmp11780747818231                Tmp51780747818251
```

## 3. The measurement — riqemu1's AHI has exactly one rate

```
[exec] 'RAM:probe_rate' -> rc=0 (28 ms)
       RI_RATE probe=probe_rate candidates=6
       RI_RATE version=6
       RI_RATE req=48000 mode=0x00390004 got=48000 bits=16 stereo=1 hifi=0 maxch=128 range=48000-48000 nfreq=1
       RI_RATE list[0]=48000
       RI_RATE driver=[]
       RI_RATE req=44100 mode=INVALID (BestAudioID)
       RI_RATE req=44100 retry_with_known_mode=0x00390004
       RI_RATE req=44100 mode=0x00390004 got=48000 bits=16 stereo=1 hifi=0 maxch=128 range=48000-48000 nfreq=1
       RI_RATE req=44100 CONVERTED_TO=48000
       RI_RATE req=32000 mode=INVALID (BestAudioID)
       RI_RATE req=32000 retry_with_known_mode=0x00390004
       RI_RATE req=32000 mode=0x00390004 got=48000 bits=16 stereo=1 hifi=0 maxch=128 range=48000-48000 nfreq=1
       RI_RATE req=32000 CONVERTED_TO=48000
       RI_RATE req=22050 mode=INVALID (BestAudioID)
       RI_RATE req=22050 retry_with_known_mode=0x00390004
       RI_RATE req=22050 mode=0x00390004 got=48000 bits=16 stereo=1 hifi=0 maxch=128 range=48000-48000 nfreq=1
       RI_RATE req=22050 CONVERTED_TO=48000
       RI_RATE req=11025 mode=INVALID (BestAudioID)
       RI_RATE req=11025 retry_with_known_mode=0x00390004
       RI_RATE req=11025 mode=0x00390004 got=48000 bits=16 stereo=1 hifi=0 maxch=128 range=48000-48000 nfreq=1
       RI_RATE req=11025 CONVERTED_TO=48000
       RI_RATE req=8000 mode=INVALID (BestAudioID)
       RI_RATE req=8000 retry_with_known_mode=0x00390004
       RI_RATE req=8000 mode=0x00390004 got=48000 bits=16 stereo=1 hifi=0 maxch=128 range=48000-48000 nfreq=1
       RI_RATE req=8000 CONVERTED_TO=48000
       RI_RATE done
```

**`range=48000-48000 nfreq=1`, `list[0]=48000`.** The riqemu1 driver offers one
rate, and every request returns it. Note also **`hifi=0`** here against `hifi=1`
on the Dell, while `AHI_BestAudioID` had been asked for `AHIDB_HiFi = TRUE`.

## 4. QEMU's AC97 is guest-programmable, and defaults to 48000

From `hw/audio/ac97.c` (QEMU 11.1.1, tree at
`/home/miller/.cache/yaw/qemu-git/src/qemu`):

```
   29?? :case AC97_Extended_Audio_Ctrl_Stat:
        if (!(val & EACS_VRA)) {
            mixer_store(s, AC97_PCM_Front_DAC_Rate, 0xbb80);
            mixer_store(s, AC97_PCM_LR_ADC_Rate,    0xbb80);
            open_voice(s, PI_INDEX, 48000);
            open_voice(s, PO_INDEX, 48000);
        }
```

and the guest-writable path:

```
    case AC97_PCM_Front_DAC_Rate:
        if (mixer_load(s, AC97_Extended_Audio_Ctrl_Stat) & EACS_VRA) {
            mixer_store(s, addr, val);
            dolog("Set front DAC rate to %d", val);
            open_voice(s, PO_INDEX, val);
        } else {
            dolog("Attempt to set front DAC rate to %d, but VRA is not set",
                  val);
        }
        break;
```

`0xbb80` = 48000. And the rate is used raw:

```
static void open_voice(AC97LinkState *s, int index, int freq)
{
    struct audsettings as;

    as.freq = freq;
```

**So the model is not fixed at 44100, and there is a rate knob** — it is the
guest's `AC97_PCM_Front_DAC_Rate` register, writable when `EACS_VRA` is set.
This disproves the "QEMU's AC97 is fixed at 44.1 kHz" claim recorded earlier
today.

## 5. AROS's ac97 AHI driver hardcodes 48000

`workbench/devs/AHI/Drivers/ac97/ac97-main.c`:

```
   39:  48000,    // DAT
  147:  AudioCtrl->ahiac_MixFreq = 48000;
```

So the guest never asks for 44100 either: AHI reports 48000, the driver
programs 48000, and there is no 44100 anywhere in the guest path.

## 6. Where 44100 does come from — narrowed, not settled

The `-audiodev` in `start_riqemu1.sh` specifies no frequency:

```
-audiodev pa,id=pa0,server=/run/user/1000/pulse/native -device AC97,audiodev=pa0
```

and QEMU defaults that to 44100, in `audio_validate_per_direction_opts()`
(`audio/audio.c:253`):

```
    if (!pdo->has_frequency) {
        pdo->has_frequency = true;
        pdo->frequency = 44100;
    }
```

The host sinks are all 48000, so the host is not the source either:

```
57	alsa_output.pci-0000_11_00.1.hdmi-stereo-extra2	PipeWire	s32le 2ch 48000Hz	SUSPENDED
58	alsa_output.pci-0000_11_00.6.analog-stereo	PipeWire	s32le 2ch 48000Hz	SUSPENDED
60	alsa_output.usb-HP__Inc_HyperX_Cloud_III_S_Wireless_C1V52808ZF-00.analog-stereo	PipeWire	s24le 2ch 48000Hz	SUSPENDED
```

**What is NOT settled:** whether the device's `as.freq = 48000` overrides that
`-audiodev` template default. The function that merges the two
(`sw_pcm_init`) is **not present** in this tree — `grep -rn "sw_pcm_init"
audio/` returns nothing, and `audio-be.c` is listed by `ls` but absent to
`stat`. So the merge order could not be read from source here.

Two candidates remain:

1. the `-audiodev` template default of 44100 wins, or
2. the guest never wrote `AC97_PCM_Front_DAC_Rate`, so `PO_INDEX` was never
   re-opened at 48000 and the template default stayed in force.

**The decisive experiment, not run:** add `frequency=48000` to the `-audiodev`
in `start_riqemu1.sh`, play, and read the host stream rate. If it becomes 48000,
candidate 1 is confirmed and the fix is one word in a launch script rather than
anything in ReIncarnation.

## 7. What this does to the recorded recommendation

The AC97 article recommends "run AHI at 44100 so AC97 never resamples".

**On riqemu1 that is not achievable.** `nfreq=1`, `list[0]=48000`: there is no
44100 mode to select, and a 44100 request is silently converted to 48000. The
fix as recorded cannot work on that lane.

On the Dell it remains a no-op, since that driver already returns 44100 for
every request.