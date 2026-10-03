# Dell AHI rate probe 2026-10-03 — verbatim

**Ingested:** 2026-10-03 into ReIncarnation `llm-wiki`
**Source:** `audio_io/probe_rate.c`, built for ABIv11, run on the Dell E6320
over the spike agent (`agent e6320, session 1`), and its v1 build on the
`ri_build_aros.sh` lane.
**Provenance:** verbatim agent output and build/gate output. Derived values show
their components.
**Recorded in:** [the Dell hands back 44100 for every rate, and
`AHI_BestAudioID` cannot select 44100](../articles/2026-10-03-dell-ahi-hands-back-44100-for-every-rate.md)

## 1. Build (ABIv11, the Dell's lane)

```
$ /home/miller/Work/vms/ri-p9/build_probe_v11.sh
PROBE v11 BUILD OK (/tmp/opencode/probe_rate.v11, 21576 bytes, r12moves=24, und=0)
```

`r12moves=24` is the discriminator: `r12moves == 0` means the v1 build-pc SDK
leaked into PATH and the binary is for the wrong machine, which faults on the
Dell's first LVO call.

Transfer, `sha_ok=True`:

```
[put ] /tmp/opencode/probe_rate.v11 -> RAM:probe_rate2  21576 B in 4 chunks, 326 ms  sha_ok=True written=21576 OK
```

## 2. The measurement

```
[exec] 'RAM:probe_rate2' -> rc=0 (76 ms)
       RI_RATE probe=probe_rate candidates=6
       RI_RATE version=6
       RI_RATE req=48000 mode=0x003E0001 got=44100 bits=16 stereo=1 hifi=1 maxch=128 range=44100-192000 nfreq=5
       RI_RATE list[0]=44100
       RI_RATE list[1]=48000
       RI_RATE list[2]=88200
       RI_RATE list[3]=96000
       RI_RATE list[4]=192000
       RI_RATE driver=[]
       RI_RATE req=48000 CONVERTED_TO=44100
       RI_RATE req=44100 mode=INVALID (BestAudioID)
       RI_RATE req=44100 retry_with_known_mode=0x003E0001
       RI_RATE req=44100 mode=0x003E0001 got=44100 bits=16 stereo=1 hifi=1 maxch=128 range=44100-192000 nfreq=5
       RI_RATE req=32000 mode=INVALID (BestAudioID)
       RI_RATE req=32000 retry_with_known_mode=0x003E0001
       RI_RATE req=32000 mode=0x003E0001 got=44100 bits=16 stereo=1 hifi=1 maxch=128 range=44100-192000 nfreq=5
       RI_RATE req=32000 CONVERTED_TO=44100
       RI_RATE req=22050 mode=INVALID (BestAudioID)
       RI_RATE req=22050 retry_with_known_mode=0x003E0001
       RI_RATE req=22050 mode=0x003E0001 got=44100 bits=16 stereo=1 hifi=1 maxch=128 range=44100-192000 nfreq=5
       RI_RATE req=22050 CONVERTED_TO=44100
       RI_RATE req=11025 mode=INVALID (BestAudioID)
       RI_RATE req=11025 retry_with_known_mode=0x003E0001
       RI_RATE req=11025 mode=0x003E0001 got=44100 bits=16 stereo=1 hifi=1 maxch=128 range=44100-192000 nfreq=5
       RI_RATE req=11025 CONVERTED_TO=44100
       RI_RATE req=8000 mode=INVALID (BestAudioID)
       RI_RATE req=8000 retry_with_known_mode=0x003E0001
       RI_RATE req=8000 mode=0x003E0001 got=44100 bits=16 stereo=1 hifi=1 maxch=128 range=44100-192000 nfreq=5
       RI_RATE req=8000 CONVERTED_TO=44100
       RI_RATE done
[submit] RESULT: PASS (agent e6320, session 1)
```

The lane was checked idle first (`status`: only IPrefs, ConClip, Decorator,
AROSTCP, ATCPBIN, Wanderer — no RIAPP), so nothing was contending for AHI.

## 3. The first run of the same binary, before the probe's own bug was fixed

Recorded because the bug reported a *plausible* wrong number rather than
crashing, which is the dangerous kind:

```
       RI_RATE req=48000 mode=0x003E0001 got=44100 bits=16 stereo=1 hifi=1 maxch=128 range=44100-192000 nfreq=5
       RI_RATE list[4]=192000
       RI_RATE req=48000 CONVERTED_TO=192000
```

The `got=` line said 44100 and the `CONVERTED_TO=` line, thirty milliseconds
later, said 192000 — because the frequency-list query wrote its last entry into
the same variable the mode readback had used. `192000` was the final element of
the driver's frequency list, not anything the mode had returned.

## 4. How the probe is invoked, and the one that does not work

AROS `Run` **spawns and returns**, so the child's console output arrives after
the agent's exec channel has already been read. Both of these returned no
output despite running correctly:

```
[exec] 'Run RAM:probe_rate' -> rc=0 (103 ms)
[exec] 'wait'                -> rc=0 (1122 ms)
[exec] 'RAM:probe_rate'      -> rc=0 (74 ms)      <- foreground: output appears
```

## 5. The other lane, at the same moment

`exec` on riqemu1 returned `rc=1` with no output while `ping` and `ui-windows`
worked — the recorded "RIAPP running kills the guest exec channel" defect,
reproducing on demand, with RIAPP live:

```
[exec] 'status' -> rc=1 (4 ms)
       (no output)
[ping] -> ok=True (3 ms)
[ui  ] windows 1280x1024 screen, 3 window(s) (3 ms)
       RIAPP live panel                         0,0 974x680 [active,close@5,0]
                                                0,18 1280x1006 [no-close]
       ReIncarnation agent (ATCPBIN)            311,27 640x180 [no-close]
```

No probe was run there: a second AHI client beside a live RIAPP risks the
contention that AHI cannot arbitrate.

## 6. Contrast, from the earlier verified riqemu1 run

The same readback on riqemu1, from the AHI-loader record's verified pass:

```
RI_PROBE alloc_audio: OK freq=48000 bits=16 stereo=1 hifi=1 maxch=128
```

`freq=48000` is a **readback** (from the `query_tags` result slots), not the
request. So the two lanes genuinely differ:

| lane | requested | AHI read back | QEMU/host plays |
|---|---|---|---|
| Dell | 48000 | **44100** | 44100 (no conversion) |
| riqemu1 | 48000 | **48000** | 44100 (QEMU resamples, 8.1 % slow) |

## 7. Gates

```
AROS STUB BUILD OK
AROS PROBE BUILD OK
AUDIT 0/0 PASS
```

New audit gates on the probe: exists, `__AROS__` guard, AROS-only `#error`, not
referenced by `ri_build_host.sh`, artifact present, reads the mode back via
`AHIDB_Frequency`, and carries the `CONVERTED_TO` resampling tell.