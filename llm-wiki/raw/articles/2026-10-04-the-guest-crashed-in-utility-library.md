# The guest crashed in utility.library, and an AHI holder silently changes the render path (2026-10-04)

- Source: ReIncarnation session, 2026-10-04 (opencode lane, riqemu1, ABIv1)
- Collected: 2026-10-04
- Published: 2026-10-04
- Raw: [verbatim](../evidence/2026-10-04-render-task-crash-and-ahi-contention.md)
- Related: [the sub-split charges LEVI 6 µs per stage it contains](2026-10-03-the-sub-split-charges-levi-6us-per-stage-it-contains.md), [AHI on riqemu1: the loader ignores `sh_addralign`](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md), [the riqemu1 lane cannot be driven by injection](2026-10-03-riqemu1-cannot-be-driven-by-injection.md)

The next split after LEVI is inside `levi_voice_render_sum_stereo` — 81 % of LEVI
on riqemu1, 91 % on the Dell. Building the instrumentation succeeded and its
contract holds on the target. **Producing a measurement failed, and the reason is
a guest crash that is very probably the same bug class this lane has already paid
for once.**

## A time split here is impossible, so count work instead

The function averages about **3 µs per sample**. A clock read on this lane costs
**4 µs** on the Dell and **6 µs** on riqemu1 — measured, by the controls.

> **A clock read costs more than the code it would measure**, and a per-block
> region split is impossible because the regions interleave inside the sample
> loop.

So `struct RILeviSet` carries six work counters — `vc_samples`, `vc_lfo_samples`,
`vc_lfo_iters`, `vc_voice_calls`, `vc_voice_active`, `vc_fx_samples` — reset per
block by the engine and logged as `RIAPP vcount:`. `t136_levi_stereo` pins the
contract, and it holds **on the guest**, not just the host:

```
('64', '0', '0', '512', '0', '0')
```

`samples 64`, `voice_calls 512 = 64 × 8`. The obvious next target is already
visible in the source: `RI_LEVI_NLFO`(5) × `RI_LEVI_NVOICES`(8) LFO iterations per
sample against only 8 voice calls — and `vc_lfo_iters` is the counter that would
have settled it.

## Both runs produced no measurement, and the transport was cycling

```
run 5:  playing=3906  stopped=20183     (20.8 s of audio)
run 6:  playing=3071  stopped=32532     (16.4 s of audio)

   playing=1511     stopped=3
   playing=3040     stopped=3
   playing=3856     stopped=687
   playing=3856     stopped=2199      <- playing frozen, stopped climbing forever
   ...
   playing=1519     stopped=2         <- started again from the top
```

**Not one block in either run had a single active Levi voice** — `max
voice_active = 0`, `max lfo_iters = 0`. And the song never loaded, although it is
present on the guest:

```
[exec] 'dir SYS:Classes/ReIncarnation/Songs' -> rc=0
         zombie-nation.rbng
```

with **no `RIAPP song` line and neither demo-song message** — neither
`"not found; built-in demo only"` nor `"no songs volume"`. The ~20 s run length is
the built-in demo pattern, not a 151-bar song.

## The crash, and why it is probably the same bug this lane already fixed

```
Program failed
Task: 0x00000004BEFC980 - RIAPP render
Error: 0x00000008 - Privilege violation
PC: 0x000000001D12B72
Module utility.library Segment 5.text (0x000000001D11A0) Offset 0x0000000019D2
```

**This guest already carries this bug class.** The AROS ELF loader places sections
at **12 mod 16** despite `sh_addralign = 16`, so alignment-assuming SSE faults —
`ahi.device`, `ac97.audio` and `void.audio` were patched for it guest-side.
**`utility.library` was never patched.** `Error 0x00000008` in a library segment
is the same shape, and a *fixed* offset (`0x19D2`) is what an
alignment-sensitive instruction site looks like rather than a data-dependent one.

**Not established:** whether the counters caused it. They do not call
`utility.library`, and `AUDIT 0/0 PASS` with `t136` pinning the counter contract.
But it is unexplained, and the bisect needs a guest that boots.

## The more reusable finding: AHI contention silently changes the render path

Behind the crash requester was RIAPP's own:

> Cannot start audio — The sound card could not be opened. Another program may
> already be using it. **RIAPP will run without sound (offline render only).**

**A stale AHI holder does not fail the launch. It silently substitutes a different
render path**, and the only warning is a requester most of this lane's tooling
cannot dismiss. That is how a run can look healthy — a live uncorked stream, a
transport that responds — while measuring something else entirely.

Two operational consequences:

- **A crashed RIAPP survives `--ui-close`.** Several instances were started and
  closed in this session; the next one's audio requester is the evidence that one
  of them held the card.
- **`sendkey tab` then `ret` does not dismiss a `Software Failure!` requester.**
  All of `tab;ret`, `tab;tab;ret` and `spc` left it up. This **corrects** the lane
  record, where that sequence dismissed a `Smart Filesystem request`. The
  generalisation: **one requester dismissing once is not evidence that requesters
  dismiss.**

## Lane state

- riqemu1 is **blocked on a modal requester** — needs a human click on Suspend, or
  a guest reboot.
- The **v11 Dell binary carrying the same counters is built and waiting**:
  `AROS RIAPP v11 BUILD OK (1100264 bytes, build=6541259, r12moves=41)`. That is the
  lane where the split matters most, since `lev-voice` is 91 % of LEVI there
  against 81 % here.
- `start_riqemu1_visible.sh` had to be recreated again — the sixth `/tmp` loss on
  this lane, and the standing argument for the systemd unit that still does not
  exist.

## Method

- **When a clock read costs more than the code, count work.** The split was still
  answerable; it just could not be answered in microseconds.
- **A live audio stream is not proof that the render path is the one you think.**
  The offline-render fallback is silent, and it crashed.
- **A log line that proves a path was taken is worth more than the absence of an
  error.** Here neither demo-song message appeared, which is a real gap: a code
  path that reaches neither success nor its documented miss.

## Open

- **Why the song did not load**, with the file present and both documented log
  lines absent.
- **Whether the counters caused the crash** — needs a bootable guest.
- **Whether `utility.library` needs the same guest-side patch** as the three
  binaries already patched. If the alignment fault is in the loader's placement,
  every unpatched SSE-using library on the guest is a candidate.
- **The LFO split itself**, which `vc_lfo_iters` was built to answer.

## See Also

- [the sub-split charges LEVI 6 µs per stage it contains](2026-10-03-the-sub-split-charges-levi-6us-per-stage-it-contains.md) — the 4 µs and 6 µs clock costs, and `lev-voice` as the target
- [AHI on riqemu1: the loader ignores `sh_addralign`](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md) — the alignment bug class, and the three binaries patched for it