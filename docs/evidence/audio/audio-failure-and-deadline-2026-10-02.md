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

## A,B,B,A with the BUILD FLAG as the arm (2026-10-03)

Both arms built from the **same clean `HEAD` tree** (`a98691a`, via
`git archive HEAD | tar -x`), so the optimisation level is the only variable.
The working tree was deliberately not used: it holds the sibling lane's
uncommitted `NOAUDIO` change, which must not enter either build.

```
-O0   1,094,648 B   r12moves=41
-O2     874,744 B   r12moves=282
```

Protocol per cell (coordinates from the click-map record, each click verified
against the event it produces -- never a blind click):

```
delete RIAPP.LOG, RIAPP-EV.LOG
Run Vk4aros:ReIncarnation/<bin> PLAYLIST=.../songs/local/demos.rbpl
wait for window; confirm exactly one Process in `status`
click Play (513,80)
click SYNTH(56,166) DRUMS(122,166) LEVI(180,166) MIX(238,166) FX(288,166), 2 s apart
click Stop (560,80); settle; close; pull RIAPP.LOG + RIAPP-EV.LOG
```

Event verification, identical in all five cells:

```
TR PLAY TAB page=0 TAB page=1 TAB page=2 TAB page=3 TAB page=4 TR STOP
```

Results. A,B,B,A order with a third B added to resolve an outlier:

| cell | arm | buffers | **xruns** | **overloads** | `render_max` | `wake_max` | **5-tab total** | `full_avg` |
|------|-----|---------|-----------|---------------|--------------|------------|----------------|------------|
| A1 | `-O0` | 4,273 | **1,590** | **6** | 91,932 us | 5,820 us | 99,432 us | 2,275 us |
| A2 | `-O0` | 4,299 | **1,589** | **6** | 92,021 us | 5,816 us | 99,786 us | 2,274 us |
| B1 | `-O2` | 3,065 | **0** | **0** | 4,113 us | 471 us | 300,041 us * | 4,563 us |
| B2 | `-O2` | 3,084 | **0** | **0** | 4,101 us | 398 us | 241,791 us | 4,565 us |
| B3 | `-O2` | 6,064 | **0** | **0** | 4,117 us | 436 us | 241,632 us | 2,850 us |

\* B1's LEVI switch alone was 107,029 us against 52,475 and 52,545 in B2/B3.
**Excluded by name**, per the re-run rule for outliers on this guest, not
quietly dropped. B2 and B3 then agree to 0.07 %.

Per-tab costs (us):

| tab | A1 `-O0` | A2 `-O0` | B1 `-O2` | B2 `-O2` | B3 `-O2` |
|-----|----------|----------|----------|----------|----------|
| SYNTH | 27 | 29 | 27 | 25 | 25 |
| DRUMS | 22546 | 22644 | 51904 | 48530 | 48435 |
| LEVI | 18644 | 18654 | 107029 * | 52475 | 52545 |
| MIX | 34237 | 34374 | 81637 | 81428 | 81413 |
| FX | 23978 | 24085 | 59444 | 59333 | 59214 |

Play windows from the ev-log (`TR PLAY` -> `TR STOP`): 4299, 4301, 4646, 4738,
4831 ms -- comparable, so the xrun counts are not a duration artefact.

**What this settles, and what it does not.** `-O2` removes every xrun (0 across
12,213 buffers in three runs) and cuts `render_max` 22x and `wake_max` 13x, with
`overloads` going 6 -> 0. It also makes the five-tab repaint cycle **2.43x
slower** (99.6 ms -> 241.7 ms), reproducibly: the `-O0` pair agrees to 0.36 %
and the `-O2` pair to 0.07 %. So `-O2` is not a free win -- it trades audio
correctness for GUI latency, and which one matters is an owner call.

`r12moves=282` produced no functional problem across all three `-O2` runs, which
exercised the eight files the inlining touches (tab switches drive
`rsection.mcc.o`, the largest at 52; `skin_aros.o` loads at startup;
`audio_ahi_live.o` runs the live backend; `fs_aros.o` loads three songs; the 909
pack and the log are live). Every click was event-verified.

## Arm-disabled cells: is -O2's slow repaint the arm's fault? (2026-10-03)

The -O2 A,B,B,A left one coupling unresolved: at `-O0` six governor trips are
what yield CPU to Intuition, so the tab cycle may be fast *because* the audio
path is collapsing. Testing that needs the arm removed, and the accumulator left
running so `overloads`/`arm_us` still report what **would** have tripped.

Built from a second clean `git archive HEAD` tree (`/tmp/opencode/tree2`), so
again the flag and the arm are the only variables. **One line, at the one place
that consults the flag** -- `audio_io/audio_ahi_live.c`, where the render task
drops below the UI:

```
-        if (ri_livedrv_overloaded(&lv->drv) != yielding) {
+        if (ri_livedrv_arm_enabled() && ri_livedrv_overloaded(&lv->drv) != yielding) {
```

```
int ri_livedrv_arm_enabled(void) { return 0; }
```

`governor()` itself is untouched. Binaries (both from the same tree, so the arm
gate is the only difference from their controls):

```
-O2  arm ON   RIAPP-f2        874,744 B   r12moves=282
-O2  arm OFF  RIAPP-f2noarm   874,896 B   r12moves=282
-O0  arm ON   RIAPP-f0      1,094,648 B   r12moves=41
-O0  arm OFF  RIAPP-f0noarm 1,095,592 B   r12moves=41
```

Same protocol, same click verification (`TR PLAY TAB page=0..4 TR STOP` in every
cell).

| arm | cell | xruns | `overloads` | `render_max` | `wake_max` | 5-tab total |
|-----|------|-------|-------------|--------------|------------|-------------|
| `-O0` ON | A1 | 1,614 | 6 | 91,932 us | 5,820 us | 99,432 us |
| `-O0` ON | A2 | 1,601 | 6 | 92,021 us | 5,816 us | 99,786 us |
| `-O0` OFF | T2 | **11,852** | -- | 9,464 us | 5,820 us | **38,214,843 us** |
| `-O0` OFF | T4 | **11,955** | -- | 9,499 us | 5,823 us | **27,701,899 us** |
| `-O2` ON | B2 | **0** | 0 | 4,101 us | 398 us | 241,791 us |
| `-O2` ON | B3 | **0** | 0 | 4,117 us | 436 us | 241,632 us |
| `-O2` OFF | T3 | **0** | 0 | 4,098 us | 483 us | 240,873 us |
| `-O2` OFF | T5 | **0** | 0 | 4,125 us | 485 us | 260,392 us |
| `-O2` OFF | T1 | **0** | 0 | 4,110 us | 414 us | 514,356 us * |

\* T1's MIX tab alone was 357,611 us against 85,516 in T5. **Excluded by name.**
T3 and T5 then agree to 8 % -- looser than the `-O2` arm-on pair's 0.07 %.

`-O0` arm-OFF per tab, which is where the number comes from (T2):

```
TAB page=0 us=28          xruns+0
TAB page=1 us=8411253     xruns+490
TAB page=2 us=24221363    xruns+1552
TAB page=3 us=5368346     xruns+290
TAB page=4 us=213853      xruns+0
```

**And no `hb:` heartbeat line at all** in either `-O0` arm-OFF cell -- the
heartbeat is printed from the GUI task, and the GUI was too starved to print it.
`RIAPP closed:` is present, so the process finished normally:

```
RIAPP closed: buffers=27485 xruns=11852 render_max=9464 us render_total=202914 ms period=5333 us wake_max=5820 us wake_total=68710 ms wake_n=27483
RIAPP closed: buffers=27080 xruns=11955 render_max=9499 us render_total=203419 ms period=5333 us wake_max=5823 us wake_total=68744 ms wake_n=27078
```

Note `render_max` *falls* when the arm is disabled (91,932 -> 9,464 us): the
render task no longer yields below the UI, so it never accumulates a 92 ms
single-buffer stall -- it simply never gets to run, and the xruns and the GUI
latency both go up instead. That inversion is the clearest single statement of
what the arm is for.

## Where the -O2 tab cost actually is -- and a correction (2026-10-03)

Follow-up to the two sections above, prompted by checking the claim that the
damage-box full-rebuild fix "is on the critical path for shipping `-O2`".
Three measurements say that claim is wrong.

**1. The bounded build is already shipped.** `bb1c385` ("bound the display-list
build to the damage box") is an ancestor of HEAD:

```
git merge-base --is-ancestor bb1c385 HEAD   ->  yes
```

and `gui/widgets/rsection.mcc.c:233` passes the damage box:
`build_dl(&d->brp, d, 0, 0, &dl, x0, y0, x1, y1)`. There is no unbounded
full-rebuild left to fix.

**2. `build_avg` is CHEAPER at `-O2`, not dearer** -- the metric that would move
if the build were the cost:

| cell | arm | `full_avg` | `build_avg` | `blit_max` | `part_max` | 5-tab total |
|------|-----|-----------|-------------|------------|------------|-------------|
| A1 | `-O0` | 2,275 us | 179 us | 9,914 us | 82,049 us | 99,432 us |
| A2 | `-O0` | 2,274 us | 483 us | 9,819 us | 82,419 us | 99,786 us |
| B2 | `-O2` | 4,565 us | 195 us | 27,249 us | 4,227 us | 241,791 us |
| B3 | `-O2` | 2,850 us | 62 us | 4,118 us | 3,860 us | 241,632 us |
| T3 | `-O2` off | 4,897 us | 194 us | 30,620 us | 4,200 us | 240,873 us |
| T5 | `-O2` off | 4,839 us | 195 us | 28,033 us | 4,224 us | 260,392 us |

The bounded build is 195 us at `-O2` against 179-483 us at `-O0`. It is not the
cost, and at `-O2` there are **zero** `box_none` entries in most cells -- the
unattributed full-repaint bucket the older record blamed is empty.

**3. The section repaint is 1-2 % of the tab-switch cost.** The whole section
full repaint at `-O2` averages 4,565 us against a five-tab total of 241,791 us:

```
  A1: 5 tabs = 99432 us; section full_avg = 2275 us  -> section is 2% of the tab cost
  B2: 5 tabs = 241791 us; section full_avg = 4565 us  -> section is 1% of the tab cost
```

So the 2.43x is **not** in the section repaint, and cannot be fixed by bounding a
build that is already bounded and already 1-2 % of the cost.

**4. And not CPU contention either.** `render_total` -- the CPU the render task
actually consumed -- is *lower* at `-O2`:

| arm | buffers | xruns | xruns % | `render_total` | per buffer | `wake_total` |
|-----|---------|-------|---------|----------------|------------|--------------|
| `-O0` | 5,537 | 1,614 | 29 % | 31,540 ms | 5,696 us | 11,547 ms |
| `-O0` | 5,540 | 1,601 | 28 % | 31,478 ms | 5,681 us | 11,515 ms |
| `-O2` | 6,068 | 0 | 0 % | 15,933 ms | 2,625 us | 116 ms |
| `-O2` | 6,067 | 0 | 0 % | 16,293 ms | 2,685 us | 117 ms |
| `-O2` off | 5,879 | 0 | 0 % | 15,557 ms | 2,646 us | 112 ms |
| `-O2` off | 5,975 | 0 | 0 % | 15,933 ms | 2,666 us | 115 ms |

The render task does **half** the work at `-O2` and the GUI is still 2.43x
slower. A busier render task cannot be what is slowing the GUI down.

**Where that leaves it.** `-O2` only changes *our* 81 TUs; MUI, Intuition and
`graphics.library` are prebuilt and byte-identical in both arms. The tab-switch
cost is 98 % outside the section repaint, and the render task is idle-er at
`-O2`, not busier. The remaining candidates are our own code around the page
switch (`SetAttrs(MUIA_Group_ActivePage)`, the five `MUIA_RArt_Active` writes,
`rail_for_tab`) and the per-tab bimodality already recorded. **Not chased here**,
and the correction below is the load-bearing output of this pass.

## The page switch, split three ways (2026-10-03, after the host+guest reboot)

The named probe for the unlocalised `-O2` repaint cost: instrument the three
things `tab_switch()` does, with the same `ReadEClock` arithmetic that
surrounding `TAB` line already uses. **`app/riapp.c` in the repo is untouched** --
the instrumentation lives only in a clean `git archive HEAD` tree, exactly as the
arm-disabled build did.

```c
SetAttrs(s_pages, MUIA_Group_ActivePage, (IPTR)g, TAG_DONE);      -> page_us
for (k = 0; k < RI_TAB_COUNT; k++)
    if (s_tabs[k])
        SetAttrs(s_tabs[k], MUIA_RArt_Active, ...);               -> tabs_us
rail_for_tab();                                                    -> rail_us
```

Binaries: `RIAPP-p0` 1,099,544 B (`r12moves=41`), `RIAPP-p2` 878,528 B
(`r12moves=285`). Both from `HEAD`, flag the only variable.

```
ev 11 1925 TABP page=0 page_us=25    tabs_us=5   rail_us=6      split_us=37
ev 13 2686 TABP page=1 page_us=17543 tabs_us=365 rail_us=4593   split_us=22502
ev 15 2992 TABP page=2 page_us=15056 tabs_us=364 rail_us=3209   split_us=18630
ev 17 3447 TABP page=3 page_us=27142 tabs_us=306 rail_us=6788   split_us=34237
ev 19 3749 TABP page=4 page_us=23694 tabs_us=321 rail_us=5      split_us=24021
```

Totals over the four real switches (page 0 is the no-op first click):

| arm | cell | `page_us` | `tabs_us` | `rail_us` | `split_us` |
|-----|------|-----------|-----------|-----------|------------|
| `-O0` | A1 | 83,435 | 1,356 | 14,595 | 99,390 |
| `-O0` | A2 | 83,423 | 1,360 | 14,584 | 99,371 |
| `-O2` | B1 | 218,735 | 1,322 | **259,907** | 479,966 |
| `-O2` | B2 | 200,469 | 4,791 | 40,937 | 246,200 |

Three results, one of which rules a candidate out and one of which corrects a
claim this lane made earlier the same day.

**1. `tabs_us` is IDENTICAL at both flags** -- 1,356 / 1,360 against 1,322.
The five `MUIA_RArt_Active` writes are **ruled out**: they cost the same at
`-O0` and `-O2`, and they are ~1 % of the tab cost at either. One of the three
named candidates is eliminated.

**2. The bulk is `page_us` -- and it is a prebuilt-library call.**
`SetAttrs(s_pages, MUIA_Group_ActivePage, ...)` is MUI's own code. **`-O2` does
not change MUI**: the flag applies only to our 81 TUs, and
`MUI_MakeObject`/`Intuition` come from the SDK unaltered. Yet the call costs
**83.4 ms -> 200-219 ms, 2.4-2.6x**, with the `-O0` pair agreeing to 0.014 %
(83,435 / 83,423). Identical code, 2.5x the time.

**3. The section repaint is nowhere near it.** Damage-box repaints
(`box_steps`, format n/sum/max) cost **613-620 us** across ~530 samples at `-O0`
and **4,373-4,374 us** across ~400-460 at `-O2` -- so ~1 us and ~11 us each,
against a `page_us` of 21 ms and 50 ms per switch. That is **three orders of
magnitude** short of the cost. Unattributed full repaints (`box_none`) are 139-600
samples at `-O0` and **0** at `-O2`; `build_avg` is 157-520 us against 238-324 us.
So the earlier finding stands and sharpens: the section repaint is not the cost,
and at `-O2` the full-repaint path does not run at all.

**Why identical code is 2.5x slower -- and this CORRECTS this lane's earlier
claim.** The previous pass concluded "not contention" from `render_total` being
*half* at `-O2` (15,933 ms against 31,540 ms). `render_total` measures CPU
**consumed**; it says nothing about **preemption pressure**. The right measure
is how late the render task is, and `wake_max` inverts completely:

```
-O0  wake_max = 5,820 us / 5,816 us      <- the audio task is the one WAITING
-O2  wake_max =   398 us /   436 us      <- it never waits; it finishes in 2.6 ms
                                            of a 5,333 us period and preempts the GUI
```

At `-O0` the render task overruns its period, the governor arm trips six times,
and the task is deliberately pushed **below** the UI -- so MUI runs unimpeded and
the tab cycle is 99 ms. At `-O2` the task finishes early, **never yields**, and
takes the CPU away from the GUI roughly 187 times a second. The GUI's own work
is identical; the *scheduling* is what differs.

**So `-O2`'s tab cost is preemption of prebuilt MUI code by an audio task that
is no longer late enough to be told to yield.** Not more work, not slower work,
not the section repaint, and not a fixable build. It is the arm's whole purpose
being correctly unnecessary -- and the cost of that showing up somewhere it was
never measured.

**4. The bimodality is localised, and it is the rail.** `rail_us` is 5 us on page 4
in **every** cell, and 3,209-6,812 us at `-O0` against 9,949-239,355 us at `-O2`.
B1's single 239,355 us is the outlier behind its 259,907 total; B2, without it,
totals 40,937. The spikes are in `rail_for_tab()` -- five
`SetAttrs(s_devbtn[d], MUIA_ShowMe, ...)`. B2 also shows a `tabs_us` spike
(3,835 against a ~350 baseline), so **the spikes are in `SetAttrs` on our own
widgets generally, not in one call.**
