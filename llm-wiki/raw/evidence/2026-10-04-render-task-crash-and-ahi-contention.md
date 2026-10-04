# A render-task privilege violation in utility.library, and a stale AHI holder that silently changes the render path — verbatim, 2026-10-04

**Ingested:** 2026-10-04 into ReIncarnation `llm-wiki`
**Source:** riqemu1, ABIv1, while instrumenting `levi_voice_render_sum_stereo`
with work counters. Two runs failed before producing a measurement, and the
reason is a guest crash, not an instrumentation problem.
**Provenance:** verbatim requester text from the owner-supplied screenshot, and
verbatim log lines.
**Recorded in:** [the guest crashed in utility.library, and AHI contention silently changes the render path](../articles/2026-10-04-the-guest-crashed-in-utility-library.md)

## 1. What was being built, and why it counts work instead of timing it

The next split after LEVI is inside `levi_voice_render_sum_stereo` — 81 % of LEVI
on riqemu1, 91 % on the Dell. Its per-sample loop cannot be **time**-split:

- the function averages about **3 µs per sample** (`lev-voice` 189 µs over 64
  frames on riqemu1);
- a clock read on this lane costs **4 µs** (Dell `lev-probe`) to **6 µs**
  (riqemu1 `lev-probe`).

**A clock read costs more than the code it would measure**, and a per-block region
split is impossible because the regions interleave inside the sample loop. So
`struct RILeviSet` gained six work counters instead — `vc_samples`,
`vc_lfo_samples`, `vc_lfo_iters`, `vc_voice_calls`, `vc_voice_active`,
`vc_fx_samples` — reset per block by the engine and logged as `RIAPP vcount:`.

The counters are **correct**, and the host contract is pinned in
`t136_levi_stereo`:

```
RI_ASSERT(B.vc_samples == N, ...)
RI_ASSERT(B.vc_voice_calls == N * RI_LEVI_NVOICES, ...)
```

On the guest, the last block of each run:

```
('64', '0', '0', '512', '0', '0')
```

`samples 64` and `voice_calls 512 = 64 × 8` — the contract holds on the target,
not just the host.

## 2. Both runs produced no measurement

```
run 5:  last stg: playing=3906  stopped=20183    (3906 x 256 / 48000 = 20.8 s)
run 6:  last stg: playing=3071  stopped=32532    (3071 x 256 / 48000 = 16.4 s)
```

`playing` climbed and then froze while `stopped` climbed without bound, and
restarted from the beginning — the transport was cycling, not playing through:

```
   playing=1511     stopped=3
   playing=3040     stopped=3
   playing=3856     stopped=687
   playing=3856     stopped=2199
   ...              (stopped climbs forever, playing frozen)
   playing=3906     stopped=20183
   playing=1519     stopped=2          <- started again
```

And **not one block in either run had a single active Levi voice**:

```
vcount rows: 16 ; rows with any activity: 0
max voice_active over the run: 0
max lfo_iters   over the run: 0
```

Meanwhile the song never loaded. `dir` confirms it is present:

```
[exec] 'dir SYS:Classes/ReIncarnation/Songs' -> rc=0 (10 ms)
         zombie-nation.rbng
```

but the log contains **no `RIAPP song` line and neither demo-song message**:

```
=== the demo-song log lines in run 6 ===
(none)
```

so neither `"RIAPP demo song %s not found; built-in demo only"` nor
`"RIAPP demo song: no songs volume"` was reached. The ~20 s run length matches the
built-in demo pattern, not a 151-bar song.

`RIAPP_DEMO` and `RIAPP_DEMO_SONG` are both unset on the guest:

```
[exec] 'Getenv RIAPP_DEMO' -> rc=5
       Getenv: object not found
[exec] 'Getenv RIAPP_DEMO_SONG' -> rc=5
       Getenv: object not found
```

## 3. The crash

The owner supplied a screenshot of the guest showing two requesters.

**`Software Failure!`:**

```
Program failed
Task: 0x00000004BEFC980 - RIAPP render
Error: 0x00000008 - Privilege violation
PC: 0x000000001D12B72
Module utility.library Segment 5.text (0x000000001D11A0) Offset 0x0000000019D2
```

**And, behind it, RIAPP's own audio requester:**

> Cannot start audio
>
> The sound card could not be opened. Another program may already be using it.
>
> RIAPP will run without sound (offline render only).
> Close the other program and start RIAPP again.

**These two requesters together are the explanation for section 2.** The render
task died, so the transport produced a few buffers and stopped. And the audio
requester is not incidental: the documented fallback is *"RIAPP will run without
sound (offline render only)"*, so an instance that cannot get AHI **silently
takes a different render path** — and that is the path that crashed.

## 4. Why privilege violation in utility.library is the same class as a known bug

This guest already carries a documented, patched class of fault: the AROS ELF
loader places sections at **12 mod 16** despite `sh_addralign = 16`, so any
alignment-assuming SSE instruction faults. Three binaries were patched guest-side
for it — `ahi.device`, `ac97.audio`, `void.audio`.

**`utility.library` was never patched.** `Error: 0x00000008` in a library segment
is the same shape, and this crash is in `utility.library` rather than in any of
the three that were fixed. The offset is fixed
(`Offset 0x0000000019D2` in `Segment 5.text`), which is what an
alignment-sensitive instruction site looks like rather than a data-dependent one.

## 5. The requester will not dismiss from the lane

```
$ sendkey tab ; sendkey ret
$ sendkey tab ; sendkey tab ; sendkey ret
$ sendkey spc
  Software Failure windows: 1
```

All three left it up:

```
[ui  ] windows 1280x1024 screen, 4 window(s) (3 ms)
       Software Failure!                        228,17 656x786 [active,no-close]
       RIAPP                                    888,44 328x191 [no-close]
```

This **corrects** the lane record, which had `sendkey tab` then `ret` dismissing a
`Smart Filesystem request`. A `Software Failure!` requester has not been
dismissible this way, consistent with its own record's note that such a requester
needs a human click. **The lesson generalises: `sendkey` dismissed one kind of
requester once, which is not evidence it dismisses requesters.**

`--ui-close` was not tried here because that record states it returns ok and the
requester reappears.

## 6. Lane state

- riqemu1 blocked on a modal requester: **needs a human click on Suspend, or a
  guest reboot.**
- `exec` returns `rc=1` throughout — something still holds the lane.
- The v11 Dell binary carrying the same counters is built and waiting:
  `AROS RIAPP v11 BUILD OK (/tmp/opencode/RIAPP-vcount.v11, 1100264 bytes,
  build=6541259, r12moves=41)`.
- `/tmp/opencode/start_riqemu1_visible.sh` had to be recreated again — the sixth
  `/tmp` loss on this lane.

## 7. What is NOT established

- **Whether the counters caused the crash.** The fault is in `utility.library`,
  which the counters do not call, and the host build passes `AUDIT 0/0 PASS` with
  `t136` pinning the counter contract. But the crash is unexplained and the
  bisect needs a guest that boots.
- **Why the song did not load**, given the file is present and both demo-song log
  lines are absent. That is a genuine gap in the logging: a path that reaches
  neither the success nor the documented miss message.
- **Which instance held AHI.** Several RIAPP runs were started and closed in this
  session; a crashed one can survive a `--ui-close`.