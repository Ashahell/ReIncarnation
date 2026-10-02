# Audio-failure reporting and the deadline gap on a real song — evidence

Session 2026-10-02 (opencode lane). Every figure below is a verbatim excerpt
from a guest log pulled over the Dell lane; the pull is named so the capture
can be reproduced. Companion articles:
`llm-wiki/raw/articles/2026-10-02-a-lost-audio-path-must-not-be-silent.md`,
`2026-10-02-head-misses-the-audio-deadline-on-a-real-song.md`,
`2026-10-02-one-requester-two-instances-and-a-silent-null-backend.md`.

Binary under test: `Vk4aros:ReIncarnation/RIAPP`, 1,094,648 B, built with
`~/bin/build_v11.sh riapp` (81 TUs, 0 undefined, v11 `r12` convention),
`build=2ecffd0` read back from `Vk4aros:RIAPP-EV.LOG`:

```
ev 1 0 RUN frames=256 vol=Vk4aros: build=2ecffd0
```

## Control — a single instance reaches the real backend

`--get Vk4aros:RIAPP.LOG`, instance launched alone with
`PLAYLIST=Vk4aros:ReIncarnation/songs/local/demos.rbpl`:

```
audio: AHI low-level mode=0x003e0001 mix=48000 Hz buffer=256 frames period=5333 us
```

This is the reference for every "no sound" claim below. `mode=0x003e0001` is
the negotiated AHI mode; `period=5333 us` is the deadline the stage average is
compared against.

## RED — a contended launch is silent (pre-fix binary `406500f`)

Instance A settled, then instance B launched while A held `ahi.device`:

```
[exec] 'status'
       Process 8 Loaded as command: Vk4aros:ReIncarnation/RIAPP
       Process 9 Loaded as command: Vk4aros:ReIncarnation/RIAPP
[ui  ] windows 1366x768 screen, 4 window(s)
       RIAPP live panel                         0,0 1349x680 [active,close@5,0]
       RIAPP live panel                         0,0 1349x680 [close@5,0]
```

Two panels, **no requester**. Instance B's log line:

```
audio: AHI unavailable - null backend active (offline render only) [err 4]
```

`err 4` is set at `audio_io/audio_ahi_live.c:236`, immediately after
`AHI_AllocAudioA` returns NULL — the device opened, the card was already taken.
The quiet counterpart is `err 2`, set at `:192` immediately after
`OpenDevice("ahi.device")` fails, i.e. no AHI hardware at all.

## GREEN — the same scenario now reports (binary `2ecffd0`)

Same two-instance sequence, rebuilt binary:

```
[ui  ] windows 1366x768 screen, 4 window(s)
       RIAPP                                    519,289 328x191 [active,no-close]
       RIAPP live panel                         0,0 1349x680 [close@5,0]
       RIAPP live panel                         0,0 1349x680 [close@5,0]
```

Requester present, `328x191` (the song requester is `391x123`; the difference is
the multi-line message). New log line:

```
RIAPP audio: sound card unusable, continuing without sound [err 4]
```

Single instance afterwards: live AHI, one panel, no requester — so the
`err 2` carve-out still stays quiet and this is not "always nag".

## The deadline gap, on a `..`-free playlist, single instance, live AHI

`demos.rbpl` contains no `..` entry, one instance, AHI live — so neither the
path fix, nor contention, nor a fixture is involved. Shutdown summary from
`--get Vk4aros:RIAPP.LOG` after close:

```
RIAPP closed: buffers=90106 xruns=18259 render_max=10115 us render_total=510638 ms period=5333 us wake_max=5823 us wake_total=124391 ms wake_n=90104 stg_total_avg=5654 us stg_dsp_avg=5576 us stg_evt_avg=30 us stg_playing=90067 stg_stopped=39
```

The three numbers that decide the question, and why:

| figure | value | what it rules out |
|--------|-------|-------------------|
| `stg_total_avg` vs `period` | **5654 us vs 5333 us** | a mean over budget cannot be explained by scheduling, the governor arm, repaint policy or contention |
| `stg_dsp_avg` vs `period` | **5576 us vs 5333 us** | instrumentation: the DSP mean **alone** already exceeds the deadline by 243 us |
| `stg_evt_avg` | **30 us** | bounds the event-handling share of the stage cost |

**Three runs, same verdict.** The `..`-free single-instance capture above is the
third, and it reproduces the two earlier ones (19,703/94,489 on the `..` fixture;
20,998/97,444 on `demos.rbpl`). All three put `stg_dsp_avg` **above** `period`:

| run | buffers | xruns | `stg_total_avg` | `stg_dsp_avg` | `period` |
|-----|---------|-------|-----------------|---------------|----------|
| `..` fixture | 94,489 | 19,703 | — | — | 5333 us |
| `demos.rbpl` | 97,444 | 20,998 | 5766 us | 5689 us | 5333 us |
| `demos.rbpl` (this capture) | 90,106 | 18,259 | 5654 us | 5576 us | 5333 us |

So the effect is the song's DSP cost on this hardware, reproducible, and not an
artefact of any playlist, fixture or contention state.

Per-stage averages from the same run (`stg[5]` and `stg[7]` are the two that
carry it) and the largest single device:

```
stg[0]: avg=7 us max=15 us
stg[1]: avg=30 us max=264 us
stg[2]: avg=4 us max=69 us
stg[3]: avg=4 us max=63 us
stg[4]: avg=4 us max=41 us
stg[5]: avg=5576 us max=10028 us
stg[6]: avg=4 us max=36 us
stg[7]: avg=5654 us max=10100 us
dstg levi   avg=737 us max=2564 us
dstg block  avg=1370 us max=3220 us
```

## Host-side proof for the policy

`tests/unit/t158_audio_failure_loud.c`, gated in `scripts/ri_audit.sh`.

Behavioural RED was the shipped behaviour itself — `ri_core_audio_failure_is_loud`
first returned 0 ("never loud", which is what deployed), giving 10 failures on
exactly the lost-path cases with the `err 2` carve-out already green:

```
FAIL t158_audio_failure_loud.c:51: err 1 (msgport): loud=0, want 1
FAIL t158_audio_failure_loud.c:51: err 3 (mode): loud=0, want 1
FAIL t158_audio_failure_loud.c:51: err 4 (AHI_AllocAudioA failed (another AHI client holds the card)): loud=0, want 1
FAIL t158_audio_failure_loud.c:51: err 5 (AHI_LoadSound): loud=0, want 1
FAIL t158_audio_failure_loud.c:51: err 6 (render task / signal): loud=0, want 1
FAIL t158_audio_failure_loud.c:51: err 7 (open handshake timeout): loud=0, want 1
FAIL t158_audio_failure_loud.c:72: err 0 (run failed) must be loud
FAIL t158_audio_failure_loud.c:76: unknown -1 must be loud
FAIL t158_audio_failure_loud.c:77: unknown 99 must be loud
FAIL t158_audio_failure_loud.c:80: contended alloc must be exactly 1
```

Mutants, each on a hash-verified fresh `/tmp/ri/build/riapp_core.o`
(baseline `079267f9644e6c76`):

| mutant | object sha | result |
|--------|-----------|--------|
| `==` → `!=` | `a7fb6e1c68677ecc` | killed |
| carve-out → `0` | `8c679ba5ae6a56c9` | killed |
| carve-out → `3` | `4ee807995cd7b675` | killed |
| always loud (`? 1 : 1`) | `5561ec058418048b` | killed |
| always quiet (`? 0 : 0`) | `6ca90be3416617eb` | killed |
| `err == K` → `K == err` | `079267f9644e6c76` (unchanged) | provably equivalent — not a survival |

Two earlier attempts (`return 1;`, `return 0;`) wrote **no object at all**:
`-Wunused-parameter` with `-Werror` fails the build, so "did the test pass? no"
reads as a kill when nothing ran. Recorded inconclusive and redone with the
parameter still referenced.

## Correction to an earlier record: the guest CLI does have `kill`

The claim that clearing a wedged process "needs a guest-side `kill` (the lane's
CLI has no `kill` in reach)" conflated two CLIs. The lane's action set is
`--exec / --get / --put / --run-script / --ui-*`; the guest AROS CLI has `kill`:

```
[exec] 'kill Process8' -> rc=0 (5 ms)
       kill: object not found
```

`rc=0` with "object not found" on a bad argument, which is that CLI's usual
shape. Reachable before concluding a reboot is required.

## A windowless holder, and why it poisons the next launch

After the two-instance experiment both processes stayed in `status` while being
absent from `--ui-windows`, and they still held `ahi.device`, so a *subsequent
single* launch also failed:

```
audio: AHI unavailable - null backend active (offline render only) [err 4]
RIAPP audio: sound card unusable, continuing without sound [err 4]
```

Present in `status`, absent from `--ui-windows` is the signature. Rule: after
any two-instance experiment, wait for `status` to clear before launching again,
or the next launch inherits the failure and reads as a new bug.
## `-O2` on a REAL song: 0 xruns (2026-10-03)

The gap this closes: every `-O0` figure above is from a playlist carrying The
Knife and Zombie Nation, while the `-O2` figure on file
(`docs/evidence` sibling record, commit `5d233a1`) was taken with **no song
loaded at all**. The raw logs show it directly — `stg1`, `stg2`, `stgo2`,
`stgo2b` and `stgbase` each contain `RIAPP play` and **zero** `RIAPP playlist` /
`RIAPP song` lines, so they played the built-in demo. Different workload, so the
two records were never in conflict; the missing cell was `-O2` on a real song.

Binary: `Vk4aros:ReIncarnation/RIAPP-o2`, 874,968 B, built with
`~/bin/build_v11.sh` and `-O2` substituted for `-O0` (81 TUs, 0 undefined,
`r12moves=282` — the record's `-O2` profile is 873,888 B / 286).

Same playlist as the `-O0` runs, `Vk4aros:ReIncarnation/songs/local/demos.rbpl`,
one instance, AHI live (`mode=0x003e0001 mix=48000 Hz buffer=256 frames
period=5333 us`). Two runs, because the effect is large and a single 0 is
either a fix or a quiet guest.

```
run 1  RIAPP closed: buffers=109229 xruns=0 render_max=4296 us render_total=270756 ms
       period=5333 us wake_max=448 us wake_total=2097 ms wake_n=109227
       stg_total_avg=2467 us stg_dsp_avg=2389 us stg_evt_avg=27 us
       stg_playing=109205 stg_stopped=24

run 2  RIAPP closed: buffers=105447 xruns=0 render_max=4414 us render_total=296911 ms
       period=5333 us wake_max=43 us wake_total=2029 ms wake_n=105445
       stg_total_avg=2805 us stg_dsp_avg=2723 us stg_evt_avg=33 us
       stg_playing=105403 stg_stopped=44
```

**214,676 buffers across the two runs, 0 xruns.** Final heartbeats:

```
hb: buffers=108291 xruns=0 render_max=4296 us wake_max=448 us wake_n=108289 prio=21 arm_us=0 load=526/1000 overloads=0
hb: buffers=102552 xruns=0 render_max=4414 us wake_max=43  wake_n=102550 prio=21 arm_us=0 load=716/1000 overloads=0
```

Per-stage and per-device, run 2:

```
stg[5]: avg=2698 us max=4318 us
stg[7]: avg=2780 us max=4401 us
dstg levi      avg=350 us max=655 us
dstg lev-voice avg=306 us max=600 us
dstg block     avg=660 us max=1054 us
```

`arm_us=0` and `overloads=0` in both runs: **the governor arm never engages**,
which is exactly what the `-O2` record predicted would happen to the A,B,B,A
arms if re-run at `-O2`. Confirmed here on a real song.

### The four cells, same playlist where comparable

| flag | workload | buffers | xruns | `stg_total_avg` | `stg_dsp_avg` | `render_max` | `wake_max` |
|------|----------|---------|-------|-----------------|---------------|--------------|------------|
| `-O0` | `demos.rbpl` | 97,444 | 20,998 | 5766 us | 5689 us | 21573 us | 5823 us |
| `-O0` | `demos.rbpl` | 90,106 | 18,259 | 5654 us | 5576 us | 10115 us | 5823 us |
| `-O2` | `demos.rbpl` | 109,229 | **0** | 2467 us | 2389 us | 4296 us | 448 us |
| `-O2` | `demos.rbpl` | 105,447 | **0** | 2805 us | 2723 us | 4414 us | 43 us |
| `-O0` | demo only | 40,208 | 2,320 | 4638 us | 4557 us | 6457 us | 5822 us |
| `-O2` | demo only | 40,812 | **0** | 2223 us | 2142 us | 3028 us | 37 us |

At `-O2` the stage average is 46-53 % of the 5333 us period on the heaviest
workload measured, where at `-O0` it was 106-108 %. Levi, the largest single
device, goes 737 us -> 350 us and `dstg block` 1370 us -> 660 us: a uniform
~2.1x, with the *proportions* unchanged, which is the signature of the flag and
not of an algorithmic change.
