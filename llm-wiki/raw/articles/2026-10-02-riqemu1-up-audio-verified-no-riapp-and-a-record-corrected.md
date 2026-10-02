# `riqemu1` is up and audio is verified — but no RIAPP runs on it, and the reason corrects a record I had already pushed (2026-10-02)

- Source: ReIncarnation session, 2026-10-02 (opencode lane)
- Collected: 2026-10-02
- Published: 2026-10-02
- Prior: [r12moves is an inlining counter, not an ABI hazard](2026-10-02-r12moves-is-an-inlining-counter-not-an-abi-hazard.md) (**corrected by this session's work — see below**), [five sections, one culprit](2026-10-02-five-sections-one-culprit-levi-is-32-percent.md)
- Commit: unpushed at collection. No ReIncarnation device numbers in this record: **riqemu1 cannot run RIAPP**, and the Dell belongs to another session.

## Why riqemu1, and why not the Dell

Another opencode session is working the Dell. That makes it not merely contended but
**actively unsafe for me to touch**: `stg_run.sh`'s pre-flight is
`--ui-close "#0"`, and with a live RIAPP on `#0` that closes *theirs*. A RIAPP
window was indeed live and frontmost on the Dell when I checked. So the Dell lane
was ruled out on correctness grounds, not etiquette.

`riqemu1` is the project's **private** lane (`start_riqemu1.sh`: *"riqemu1 —
private ReIncarnation lane (ABI v1)"*), so it is the right target: no sharing, no
possibility of closing someone else's window.

## Bringing it up

Three prerequisites, all pre-existing and all still true:

```
riqemu1_dh0.img       4 294 967 296 B     installed system on DH0
riqemu1_test.iso          626 688 B     file-delivery CD
DISPLAY=:0   pulse socket /run/user/1000/pulse/native present   /dev/kvm present
```

The agent dials **out**, so the spooler has to be listening *before* the guest
boots — the same ordering `vm_restart.sh` uses:

```
python3 scripts/spike_server.py serve --port 9295 --spool /tmp/spike_spool_riqemu1
bash ~/Work/vms/start_riqemu1.sh          # -daemonize, so the launcher returns
```

Result: `qemu-system-x86_64 -name riqemu1 -m 2048 -smp 2 -enable-kvm -cpu host`,
monitor on 4477, and the agent connected in **~10 s** as identity `anon` (the
spooler accepts anonymous agents, so no pairs file is needed). Guest: **Kickstart
51.51, Workbench 40.0**.

**`vm_restart.sh` must not be used for this**, for three checkable reasons: its
`REPO=/root/vulkan4aros` and `HDD=/root/aros_hdd.img` do not exist here; it runs
`pkill -f qemu-system-x86_64`, which would kill the other session's running VM; and
it launches `-cpu qemu64,+sse3,+sse4.1,+sse4.2` with **no `-enable-kvm`**, i.e.
pure TCG software emulation, where any CPU-cost number would be meaningless.

## Audio: yes, it works — verified, not assumed

The question was whether audio works under QEMU. It does, and the evidence is a
host-side observation rather than an inference:

```
$ pactl list sink-inputs
Sink Input #250
        application.name = "riqemu1"
        application.process.id = "46984"        <- the qemu pid
        application.process.binary = "qemu-system-x86_64"
        media.name = "pa0"                      <- the -audiodev pa backend
        s16le 2ch 44100Hz                      <- AC97's native rate
```

So `riqemu1`'s AC97 is open and streaming into the host's audio stack. That is the
end-to-end answer.

**On the suggested `-audio driver=sdl,model=es1370`: this guest has no ES1370.**
`DEVS:AudioModes` on riqemu1 contains:

```
ac97  NVHDMI  CMI8738  HDAUDIO  VIA-AC97  SB128
```

No ES1370 entry, so `-model es1370` would present a card the guest has no mode for
and produce silence. The suggestion is generic-correct — an emulated ES1370 does
work on a current AROS build — but it does not match this image. `start_riqemu1.sh`
already documents why, and the evidence has since improved: it says *"AC97 only —
sb128.audio faults in DriverInit (quarantined with its mode), hdaudio.audio faults
too, and only DEVS:AudioModes/ac97 is left"*, which was true on 2026-09-30. Today
the mode directory holds **six** modes, all dated at boot — the image has been
updated since, as *"make sure that riqemu1 has all up-to-date software"* implies.
AC97 remains the choice the guest is known to drive.

## But no RIAPP runs on it

Uploaded the current v11 build (`1092656 B`, `sha_ok=True`) and ran it:

```
[exec] 'Run RAM:RIAPP' -> rc=0
[ui  ] windows 1024x768 screen, 3 window(s)
       Software Failure!    231,289 563x191 [active,no-close]
```

The requester reads **`Type: Illegal instruction (?!)`**. riqemu1 is a genuinely
different ABI lane and the v11 binary faults on first execution.

Building for v1 gets further and then stops at link:

```
x86_64-aros-ld: cannot find -lstdlib
x86_64-aros-ld: cannot find -lcrt
```

Not a missing library — **a naming difference between the lanes' C runtimes**:

| | v1 `AROS/Developer/lib` | v11 `AROS/Development/lib` |
|---|---|---|
| C runtime | `libstdc.a`, `libstdcio.a`, `libstdc_rel.a` | `libstdlib.a`, `libcrt.a`, `libcrtprog.a` |
| Amiga libs | `libamiga.a`, `libdos.a`, `libexec.a`, `libautoinit.a`, `libutility.a` | same names present |
| startup | `startup.o`, `elf-startup.o` | `startup.o` |

The v11 cross-driver injects `-lstdlib -lcrt` by name; the v1 tree has no such
names, so the link cannot close. Note also that the SDK roots differ —
v11 is `sdk/Developer/{include,lib}`, v1 is `AROS/Developer/{include,lib}` — which
is why the first v1 attempt failed on `exec/types.h` before any of this.

**`ri_build_aros.sh` only ever built a stub library and `probe_ahi` for v1**, never
a full program, so this lane has never produced a linked RIAPP from this tree.
Bringing one up means pairing the v1 compiler driver with the v1 runtime names —
a real task, not a flag, and not part of this one.

## This is what corrected a record I had already pushed

Going to build a binary for a *different lane* is what exposed the error. Asking
"which SDK does this lane use?" is answered by one line of
`ri_build_aros.sh`:

```sh
V1SDK="../Vulkan4Aros/src/abi/v1/core-pc-x86_64/bin/pc-x86_64/AROS/Developer/include"
```

**The audit builds with the v1 SDK and gates what it builds at zero `r12moves` —
and v1 builds genuinely count zero.** Only **v11** ships an arch-specific
`aros/x86_64/libcall.h`, and it uses r12 as the library base by design:

```
v11/core-pc-x86_64/.../aros/x86_64/libcall.h   r12=45  rdx=0
v11/core-pc-x86_64/gen/.../aros/x86_64/libcall.h r12=45  rdx=0
v1  -- no aros/x86_64/libcall.h exists --
```

So my [r12moves record](2026-10-02-r12moves-is-an-inlining-counter-not-an-abi-hazard.md)
was wrong in its central claim. It said the audit gate's invariant was
"unachievable in this tree" and that `r12moves` "must stop being cited as evidence
of ABI conformance". Both are false:

- The gate is a **v1-lane gate** and is coherent for v1 — v1 has no arch-specific
  x86_64 libcall header at all, which is why it passes at zero.
- `r12moves` is a reliable **ABI fingerprint**: zero means v1, non-zero means v11.
  That is *stronger* than the claim I replaced it with, and it is exactly how the
  size-heuristic record used it all along, which is why it worked for weeks.
- What genuinely remains: **within** the v11 lane the count is an inlining counter
  (`-O0` 0 in our objects, `-O2` 245, confined to the eight AROS-only files that
  call AHI/Intuition/dos, no engine or DSP file affected), because `-O2` inlines the
  SDK's r12 trampoline into our code.

The error was mine, it shipped to `origin/main`, and nothing prompted the discovery
except needing a different lane. The `scripts/ri_audit.sh` comment I added has been
corrected to match, and the record now carries a CORRECTION section.

## A mutation run that was interrupted left a mutant in the tree

Found while rebuilding after the cold reboot: `t156` failed on a law about clock
independence, and the cause was **not** the code. `mut.sh` writes a mutant, runs the
test, then restores the original ~20 lines later — so an interrupt in between
(a cancelled tool call, a timeout, Ctrl-C) leaves the **mutant** in the working
tree. Here the interrupted run had parked *"the session clock cascades into the
engine again"* in `engine/live.c`.

Fixed: the restore now runs in a `finally`, on `SIGINT`/`SIGTERM`/`SIGHUP`, and via
`atexit`, with the in-flight `(path, original)` held at module scope. Verified by
running a mutant and interrupting it deliberately — the tree came back clean.

This is the same shape as the wedge in
[two Dell-lane traps](2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md):
**a lane that cannot be interrupted safely is a lane that will eventually hand you
a result you did not ask for.** A harness that leaves state behind on cancellation
is the same defect as one that hangs.

## Lane differences worth recording before anyone automates here

| | Dell | riqemu1 |
|---|---|---|
| guest `wait` | ~1.06 s | **~8–29 ms** |
| screen | 1366×768 | **1024×768** |
| `--ui-capture` max | larger | **scale 2 (512×384)**; scale 1 refused "screen too large" |
| click map | verified, record-held | **does not apply** — different resolution |
| ABI | v11 | v1 |

The `wait` difference alone invalidates the whole settle-by-N-waits harness idiom:
twenty waits here is 0.4 s, where on the Dell it is the reason the window is open
at all. And the capture ceiling makes reading an AROS requester unreliable — the
guru text was legible enough to identify "Illegal instruction (?!)" and not much
more, which is why the ABI conclusion rests on the toolchain evidence rather than
on the screenshot.

## What is built and tested but still unmeasured

The Levi sub-split is complete and proven: `RI_ENGINE_ST_ARPA / LEVSEQ / LEVVOICE /
LEVMIX` inside `SLEVI`, each gated on the section test, with `SLEVI` keeping its own
timestamp (`tv`) because it now wraps four stages. `TOTAL` moved to the end of the
table since it is the outermost wrapper. `t156` plus `mut_fixM6` (14/14) and
`mut_fixM7` (25/25), every kill behavioural; audit 0/0; ASan clean.

**No device number exists for it**, and there is now nowhere to take one: the Dell
is another session's, and riqemu1 cannot run RIAPP. Recorded as blocked rather than
skipped.

## Standing gaps

- A v1 link for riqemu1: pair the v1 driver with `libstdc.a`/`libstdcio.a`, or
  provide the v1 runtime under the v11 names. Until then riqemu1 is an AROS work
  VM, not a ReIncarnation measurement lane.
- Whether `-model es1370` works if an ES1370 mode is added to the image — the
  suggestion may be right for a future riqemu1 revision and is untested here.
- The Levi sub-stage split needs one Dell run. Everything else above is done and
  waiting only for a window.
- The interrupted-mutation hazard existed for the whole lane's history; other
  harnesses (`build_ri.sh`, `stg_run.sh`) may deserve the same interrupt-safety
  review.

## See Also

- [r12moves is an inlining counter, not an ABI hazard](2026-10-02-r12moves-is-an-inlining-counter-not-an-abi-hazard.md) (carries the CORRECTION)
- [Five sections, one culprit: LEVI is 32 % of the render](2026-10-02-five-sections-one-culprit-levi-is-32-percent.md)
- [Two Dell-lane traps: the bare-path launch and a hanging capture](2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md)
- [The e1000 wedge root cause](2026-09-25-e1000-wedge-root-cause-freemem-in-irq.md) (the `rtl8139`-for-VM-lanes rule, which riqemu1 follows)