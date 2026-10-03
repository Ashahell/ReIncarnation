# The audiodev frequency fix is rejected by experiment — verbatim, 2026-10-03

**Ingested:** 2026-10-03 into ReIncarnation `llm-wiki`
**Source:** empirical test of the fix proposed in the AC97 record, plus a partial
source trace of QEMU's audio path.
**Provenance:** verbatim QEMU diagnostics, verbatim source greps.
**Recorded in:** [correcting the AC97 record](../articles/2026-10-03-riqemu1-ac97-resamples-48k-to-44k1-so-playback-is-8-1-slow.md)

## 1. The candidate, and its rejection

The AC97 record closed with:

> **What is left is one word in a launch script, not a change to ReIncarnation:**
> `-audiodev pa,id=pa0,frequency=48000`. Untested. Whether that is sufficient
> depends on the unsettled question above, and it is the first thing to try.

Tested. **It is rejected:**

```
$ qemu-system-x86_64 -audiodev pa,id=pa0,server=/run/user/1000/pulse/native,frequency=48000 \
      -device AC97,audiodev=pa0 -vga none -display none -nodefaults
qemu-system-x86_64: -audiodev pa,id=pa0,server=/run/user/1000/pulse/native,frequency=48000: Parameter 'frequency' is unexpected
```

The VM did not start at all, which is how the rejection was first noticed:

```
  QEMU count: 0
```

## 2. No audiodev backend accepts a frequency

Every backend was tried, so this is not a `pa` peculiarity:

```
  pa,id=x,server=/run/user/1000/pulse/native,frequency=48000     REJECTED
  alsa,id=x,frequency=48000                                      REJECTED
  pw,id=x,frequency=48000                                        REJECTED
  none,id=x,frequency=48000                                      REJECTED
  sdl,id=x,frequency=48000                                       REJECTED
  coreaudio,id=x,frequency=48000                                 REJECTED
  jack,id=x,frequency=48000                                      REJECTED
```

Two gave a different first error, and both are about `driver`, not `frequency`:

```
qemu-system-x86_64: -audiodev alsa,id=x,driver=pulse,frequency=48000: Parameter 'driver' does not accept value 'pulse'
qemu-system-x86_64: -audiodev pw,id=x,frequency=48000: Parameter 'driver' does not accept value 'pw'
```

**So `pdo->frequency` — which `audio/audio.c:253` defaults to 44100 when
unspecified — cannot be overridden from the command line by any backend.** There
is no launch-line knob for the audio rate.

## 3. The guest is exonerated

The AROS `ac97` AHI driver mentions a rate exactly twice, both 48000, and
**never writes a codec rate register at all**:

```
$ grep -rn "48000|44100|MixFreq|SampleRate|SetFrequency" ac97/*.c ac97/*.h
ac97-main.c:39:  48000,    // DAT
ac97-main.c:147:  AudioCtrl->ahiac_MixFreq = 48000;

$ grep -rnE "CODEC_FMT|EXTENDED_AUDIO|VRA|Rate" ac97/*.c ac97/*.h
(no output)
```

Combined with the guest-side readback already measured:

```
RI_RATE req=48000 mode=0x00390004 got=48000 ... range=48000-48000 nfreq=1
```

**The guest asks for 48000, reports 48000, and never contradicts itself.** Nothing
in ReIncarnation is choosing 44100.

## 4. What remains unexplained, stated as such

QEMU's `hw/audio/ac97.c` defaults the codec to 48000 and takes its rate from the
guest register:

```
            mixer_store(s, AC97_PCM_Front_DAC_Rate, 0xbb80);   /* = 48000 */
            open_voice(s, PO_INDEX, 48000);
...
static void open_voice(AC97LinkState *s, int index, int freq)
{
    struct audsettings as;
    as.freq = freq;
```

And the pa backend appears to honour the requested rate rather than hardcode one:

```
$ grep -nE "ss.rate" audio/paaudio.c
516:        ss.rate = as->freq;
567:    ss.rate = as->freq;
```

**The trace could not be completed.** The QEMU source tree on this host is
intermittently unreadable — `ls -la` on `audio/paaudio.c` succeeded and an
immediately following `sed` on the same path returned *No such file or directory*,
the same behaviour seen earlier with `audio-be.c` and with a `find` that listed
files `stat` could not see. So the line of context around `ss.rate = as->freq`
— specifically whether that `as` is the device's `audsettings` or the
audiodev template's — is **not established**.

What is established is the **bound**: the effective 44100 is produced by neither
the AC97 reset default (48000), nor the AROS driver (48000), nor any command-line
option (none exists). It is therefore in QEMU's audio back end, upstream of
anything this project controls.

## 5. Lane state after the experiment

Launcher reverted, with the rejection recorded in a comment so it is not retried:

```
# AUDIO EXPERIMENT 2026-10-03, RESULT: REJECTED. ...
#   qemu-system-x86_64: -audiodev pa,id=pa0,...,frequency=48000:
#       Parameter 'frequency' is unexpected
# So the pa backend takes no rate at all and 44100 is not settable from the
# command line. Do not re-add it.
```

VM restarted and verified:

```
  display: P6 1280 1024 255
```

So the 1280x1024 GRUB pin still holds, and the audio launch line is back to the
documented one.