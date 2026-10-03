# AHI on riqemu1: the ELF loader ignores `sh_addralign`, and audio now works (2026-10-03)

- Source: ReIncarnation session, 2026-10-03 (opencode lane); the requester reports were operator-supplied
- Collected: 2026-10-03
- Published: 2026-10-03
- Prior: [The riqemu1 "Software Failure" is two faults](2026-10-02-riqemu1-software-failure-is-two-faults-wrong-abi-fixed-and-misaligned-movaps.md) — **its conclusion "the binaries on this image are miscompiled" is wrong, and this record corrects it.** Same evidence, opposite diagnosis.
- Raw: [three AROS requester dumps](../evidence/2026-10-03-riqemu1-ahi-loader-alignment-crash-reports.md)
- Fix: `~/Work/vms/fix-ahi-movaps.py` (host tool, outside the repo)

## The correction, first

The previous record concluded the guest's AROS binaries were **miscompiled**, and
called the fault "an upstream AROS build defect" that could not be fixed from this
repo. The mechanism was half-right (`movaps` on a misaligned address, `#GP`) and
the conclusion was wrong. **The binaries are fine. The loader places them wrong.**

The arithmetic closes exactly. In the shipped `ahi.device`, the faulting
instruction is:

```
1045: 48 b8 00 00 00 00 00 00   movabs $0x0,%rax
      1047: R_X86_64_64         .rodata+0x22a0
104f: 0f 28 00                 movaps (%rax),%xmm0
```

`0x22a0` is **16-byte aligned in the file**, so GCC was correct to emit `movaps`.
The observed runtime constant was `0x4bf4d88c`, and

```
0x4bf4d88c  -  0x22a0  =  0x4bf4b5ec     <- .rodata's runtime base
0x4bf4b5ec mod 16 = 12                   but .rodata sh_addralign = 16
```

**The AROS ELF loader placed a section whose `sh_addralign` is 16 at an address
that is 12 mod 16.** Every alignment-assuming SSE move the compiler proved aligned
at link time therefore faults, on every AHI open, before any driver is chosen.
Nothing is miscompiled. `readelf -SW` shows `.rodata`, `.ltext`, `.lrodata` and
`.lrodata.cst32` all declaring `Al 16`, in a module with **no program headers** —
all placement decisions belong to the loader, and it gets them wrong.

Two independent modules land on the same misalignment: `ahi.device` at
`0x4bf4d88c` and `ac97.audio` at `0xb444734c`, both 12 mod 16.

## Why this was not caught by the earlier "quarantine" work

Because it was never tested. Quarantining drivers changes *which* module faults,
not whether the loader misplaces sections, so every such experiment produced the
same class of result and was read as "driver selection is not the problem" —
which was true, and was then treated as the end of the enquiry rather than as
evidence that the problem was below the driver layer entirely.

## The fix

`~/Work/vms/fix-ahi-movaps.py` rewrites the alignment-assuming moves to their
unaligned twins, in place:

| | load | store |
|---|---|---|
| `movaps` | `0f 28` → `0f 10` | `0f 29` → `0f 11` |
| `movdqa` | `0f 6f` → `0f 10` | `0f 7f` → `0f 11` |

Both are the same 16-byte move without the alignment requirement, same length, no
prefix needed — which is what makes an in-place rewrite possible at all. Only
`.ltext` is touched, and only at offsets `objdump` reports as real instructions,
so a `0f 28` byte pair inside some other instruction's immediate is never
rewritten.

**159 single-byte changes in `ahi.device`, 17 in `ac97.audio`, size unchanged.**

This is a workaround for the loader, not a fix to it: the real fix is for the
AROS ELF loader to honour `sh_addralign`, which needs an AROS toolchain this host
does not have (`x86_64-aros-gcc` is absent).

### Two wrong answers reached first, both worth keeping

The patcher got the opcode mapping wrong twice, and each error was caught only by
checking the result rather than the reasoning:

1. **`movdqa` was skipped entirely**, because `0f 6f` was mistaken for the
   unaligned form. Result: the `movaps` patch worked, and 126 `movdqa` in
   `ahi.device` faulted exactly as before — a *different* crash that looked like
   progress. `movdqu` is F3-prefixed (`f3 0f 6f`).
2. **`0f 29` was rewritten to `0f 13`.** That is `movlps`, a **64-bit** store:
   it silently truncated a 16-byte store to 8. It shipped into the guest and
   produced a driver that faulted in `Mix()`. The store twin is `0f 11`.

`verify_twins()` now counts **mnemonics**, not opcode bytes, and fails on any
surviving `movaps`/`movdqa` or any `movlps`. Counting opcode bytes cannot tell
`movups` from `movlps` — that ambiguity is what let the bug through.

## Proof it worked

`probe_ahi` (`audio_io/probe_ahi.c`, **no ReIncarnation code**) run on the guest,
full pass, no requester:

```
RI_PROBE ahi.session: OPEN unit=255 base=0x000000004b4c4960 version=6
RI_PROBE best_mode: id=0x00390004
RI_PROBE alloc_audio: OK freq=48000 bits=16 stereo=1 hifi=0 maxch=128
RI_PROBE lowlevel frames=4096 playerfreq_hz=11  rc=0
RI_PROBE lowlevel frames=2048 playerfreq_hz=23  rc=0
RI_PROBE lowlevel frames=1024 playerfreq_hz=46  rc=0
RI_PROBE lowlevel frames=512  playerfreq_hz=93  rc=0
RI_PROBE lowlevel frames=256  playerfreq_hz=187 rc=0
RI_PROBE lowlevel frames=128  playerfreq_hz=375 rc=0
RI_PROBE lowlevel frames=64   playerfreq_hz=750 rc=0
RI_PROBE low_min_frames=64
```

Host side, QEMU is genuinely streaming to PulseAudio:

```
Sink Input #221   Corked: no
  application.name = "riqemu1"
  media.name = "pa0"
```

The low-level API opens, allocates at 48 kHz/16-bit/stereo/128 channels, and
every buffer size negotiates. **Audio on riqemu1 works.**

## Not yet proven

**RIAPP playing audible audio end-to-end on riqemu1.** The AHI layer is proven
and the host sink is live, but the transport was never started (no Play click),
and a 6-second `parec` capture off the monitor read **peak 0, rms 0.0, 100 %
zeros** — consistent with a stopped transport, but not a measurement of sound.

The guest is also currently **halted at boot** on:

```
Smart Filesystem request
Device DH0: (data.device, unit 0)
Has an unfinished transaction which will be loaded now.
```

from the resets used to reload the patched `ahi.device`. This needs a host click
on **OK**. It could not be dismissed from the lane: `sendkey ret` does not
activate the gadget, and this host has no mouse-injection tool (`ydotool` and
`xdotool` are absent; QEMU's `mouse_move` is relative and did not land).
**Someone must click it, or the guest stays down.**

> **Correction 2026-10-03: `hyprctl` is *not* absent — it is installed and its
> Lua dispatcher namespace is empty on this build.** Enumerating it returns an
> empty list:
>
> ```
> $ hyprctl eval '... enumerate pairs(hl.dsp) ...'
> ok
> ```
>
> So `hyprctl clients`, `monitors` and `eval` all work and are used elsewhere,
> but there is no `exec` dispatcher to launch an app and no cursor dispatcher to
> move the pointer. The conclusion above stands — a requester still needs a human
> — while the stated reason was wrong. Also relevant to the same host: **nothing
> launched from the agent's shell can map a window at all**, proved with `foot`,
> which also rules out launching anything via `hyprctl` as a workaround. Full
> record: [proving the VM window is actually visible](2026-10-03-proving-the-vm-window-is-actually-visible.md).

## The two requesters, resolved

Neither was a ReIncarnation bug.

1. **`RIAPP` / "Cannot start audio"** — `audio_fail()`, from the other session's
   `2ecffd0`. Correct: AHI genuinely failed. Its five text lines and `OK` gadget
   were identified by matching the requester's pixel text against the two formats
   the binary can produce and confirming all five strings with `strings` on the
   built binary.
2. **`Software Failure!`** — the guest's own crash handler.

Two correct reports for one fault, and together they made the lane unusable,
because the modal box hides the log behind it. `NOAUDIO` (committed `58bb3e7`)
skips the attempt and logs its own line *before* the panel, which is what makes
it verifiable. It is now the escape hatch rather than the fix.

## Method

- **Get the crash report as text, and when you cannot, get the binary.** The
  reports were unreadable at this guest's capture ceiling (`1280x1024` will not
  fit the 2 MiB frame at scale 1; `1024x768` does not either). Pulling
  `DEVS:ahi.device` and disassembling it answered in one step what four
  experiments on the guest could not: the constant's section offset, and hence
  the load base.
- **Arithmetic beats inference.** "Miscompiled" was an inference from "the
  constant is not 16-aligned". Computing the load base from the constant and the
  section offset turned a guess into a proof.
- **A guard on the output, not on the reasoning.** Both wrong mappings were
  caught by disassembling the result. Neither was caught by re-reading the
  argument.
- **Count mnemonics, not bytes.** Two of the three checks in this task were
  wrong in ways that only a mnemonic-level check exposed.
- **An earlier "no requester" reading was premature** — the window list was read
  before the requester appeared, and I reported it as a pass. It was a timing
  artefact, and the owner's follow-up screenshot showed the fault still there.

## See Also

- [The riqemu1 "Software Failure" is two faults](2026-10-02-riqemu1-software-failure-is-two-faults-wrong-abi-fixed-and-misaligned-movaps.md) — same evidence, wrong conclusion; superseded here
- [riqemu1 up, audio verified, no RIAPP](2026-10-02-riqemu1-up-audio-verified-no-riapp-and-a-record-corrected.md)
- [One requester, two instances, and a silent null backend](2026-10-02-one-requester-two-instances-and-a-silent-null-backend.md)