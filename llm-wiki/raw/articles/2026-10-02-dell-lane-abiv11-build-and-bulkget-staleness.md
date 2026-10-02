# The Dell lane has two silent traps: the audit's link gate builds ABIv1, and `--get` serves stale bytes for a file the guest still has open

- Source: ReIncarnation session, 2026-10-02 (opencode lane, the P9 Leviasynth slice + the Dell xrun interlude)
- Collected: 2026-10-02
- Published: 2026-10-02
- Prior: [2026-10-01-dell-deploy-abiv11-usb-stick-layout.md](2026-10-01-dell-deploy-abiv11-usb-stick-layout.md) (how a binary gets onto `RAM:` at all), [2026-10-01-dell-zn-telemetry-song-load-diagnosis.md](2026-10-01-dell-zn-telemetry-song-load-diagnosis.md), [2026-10-01-dell-xruns-governor-arm-and-repaint-policy.md](2026-10-01-dell-xruns-governor-arm-and-repaint-policy.md)
- Type: lane / method record. Nothing here is a product finding; both items are ways to be *wrong* about the target machine without noticing.

Two traps, both of which cost real time in this session, and both of which fail **silently** — that is what makes them worth writing down.

## Trap 1: a green AROS link gate does not mean the binary runs on the Dell

The repo's `scripts/ri_build_aros.sh` cross-compiles against the **ABIv1** SDK: the old calling convention, zero `mov %rax,%r12` in the emitted code. The audit runs it and reports a pass, including a "AROS RIAPP link gate" that links the whole application.

That gate is real, and it proves exactly one thing: *the v1 lane compiles and links*. The Dell is an **ABIv11** machine (register-based calling convention, the callee-saved-register shuffle around every non-trivial call). A v1 binary there **does not run**: it faults during start-up and shows a `Software Failure!` requester (I measured one at 354,212 658x344). It writes **no log line at all**, so a lane that only reads `RAM:RIAPP.LOG` sees silence and concludes the app never started, rather than "this binary is for the wrong machine".

The three sizes that identify each lane at a glance:

| build | size | on the Dell |
|---|---|---|
| deployed good `RAM:RIPP9` | 1074888 B | runs |
| v11 `-O0` rebuild of the `cdcf85c` tree | 1074856 B (683453 bytes differ from the deployed one) | runs |
| v11 build of the xrun fix (labelled `RI_V11_OPT=-O2` — **that label is wrong, see the Status block**) | 1075600 B | runs |
| accidental **v1** build | 1082416 B | `Software Failure!` requester, no log line |

> **Status: Outdated** (2026-10-02)
> **The optimisation label on the third row is wrong, and size does not identify the lane.** Rebuilt and measured on one tree:

> | build | bytes | `mov %rax,%r12` | runs? |
> |---|---|---|---|
> | v1 (`ri_build_aros.sh`, the audit's link gate) | 1088384 | **0** | no |
> | v11 at `-O0` | 1081512 | 41 | yes |
> | v11 at `-O2` (the recipe's documented default) | 867320 | 285 | yes |

> So the 1075600 B binary above is an **`-O0`** build, and so was `RAM:RIPP9F` (`r12moves=41` is the `-O0` signature). The default `-O2` yields ~867 KB, outside every band this table ever recorded. Two consequences: **the A/B must run both arms at the same optimisation level**, and the baseline stays `-O0` to match what was proven to run — an `-O2` deploy changes CPU cost by ~24 % by size alone and would confound the metrics being measured. And the v1 / v11-at-`-O0` gap is only 6872 bytes (0.6 %), so **no size band can separate the lanes**; the discriminator is the register convention — `r12moves == 0` means the build-pc SDK leaked in and the binary faults on the Dell's first LVO call, `r12moves > 0` is the v11 lane. Full record: [2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md](2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md). This article's other two findings — that the audit's link gate builds the wrong lane, and that `--get` serves stale bytes for an open file — are unaffected and still hold.

Note the second row: the deployed `RAM:RIPP9` is 32 bytes larger than a clean rebuild of the tree it claims to be built from, and 683453 bytes of it differ. So a deployed binary's embedded `build=` label is evidence of *intent*, not of *provenance* — always compare bytes before trusting a label on the target.

**The rule: the Dell is a separate lane with its own recipe.** It lives at `/home/miller/Work/vms/ri-p9/build_v11.sh`, deliberately outside the repo, parametrised by `RI_V11_OPT` (default `-O2`), taking `[source-tree] [out-binary]`. The shape of it, so it can be rebuilt from memory if that file is ever lost:

```
toolchain  Vulkan4Aros/src/abi/v11/toolchain-core-x86_64
SDK        Vulkan4Aros/src/abi/v11/sdk/Developer
CFLAGS     -std=gnu99 <opt> -mcmodel=large -mno-red-zone -mno-ms-bitfields
           -fno-strict-aliasing -ffixed-r12 -fno-builtin -fno-stack-protector
           -DPCF_TABLE_VERIFIED=1 -Wa,-W
INCLUDES   -I$ROOT -I$SDK/include -I$SDK/include/aros/posixc -I$SDK/include/aros/stdc
LINK       -nostartfiles -no-pie  $SDK/lib/startup.o -lamiga -lmui -lintuition
           -lgraphics -lutility -ldos -lexec -lautoinit -lcybergraphics -lcamd
```

`-ffixed-r12` is not decoration: it stops GCC from using r12 as a scratch register, which in ABIv11 is callee-saved. A single emitted `mov %rax,%r12` where the caller did not expect one is enough to fault. The lane's own sanity check is to count them — the good fix build reports `r12moves=41` in the ev log's `RUN` line, which is the number of legitimately emitted shuffles, not a threshold to tune.

**Corollary for the audit:** a green `ri_audit.sh` and a green Dell run are independent facts. Never let one stand in for the other, and never let the audit's link gate be quoted as evidence that the change was proved on target.

## Trap 2: `--get` on a file the guest still has open returns stale bytes, silently

I spent time convinced that the running app was not logging. It was logging. The reader was lying.

Three successive reads of `RAM:RIAPP.LOG` while the app was running returned **exactly** 16630 bytes each time. After quitting the app, the same read returned **19107** bytes — precisely 16630 + 2477, the bytes that had been appended in the meantime. So `--get` hands back a cached copy for a path that is currently open on the guest, and it does not say so.

`RAM:RIAPP-EV.LOG`, also held open, behaved differently and more honestly: it failed loudly with

```
REFUSED: bulk_get_begin failed: cannot open file
```

Two different failure modes for the same class of problem, which is the worst combination: one that shouts and one that whispers a lie. The lesson is not "the ev log is more reliable" but **never conclude "the app is not writing" from a bulk read taken while it is running**. Read after the process exits, or use `--put` and a `sha_ok` comparison when byte identity is what is being proved.

This is worth stating as a general rule for the lane: *evidence about a running guest must come from a channel that fails loudly when it cannot see the truth.* `--ping` and `--ui-windows` do; `--get` on an open file does not.

## Two smaller lane facts, for the same reason

- **`--ui-close <window>` is window management, not pointer injection.** It works and it is clean (I used it to quit a running RIAPP). It does **not** dismiss a crash requester: the call returns ok and the requester reappears. A `Software Failure!` requester needs a human click on Suspend — which is why the leftover v1 process from Trap 1 can only be cleared by the owner or by a reboot. A reboot also costs the agent dial-in: the owner has to re-run `SYS:ATCPBIN agent 192.168.1.81 9292 e6320`, and the guest dials in once per boot.
- **The guest Shell accepts only `Run`, `SetEnv`, `echo`, `dir`, `status`, `wait`, `Break`, `Getenv`.** Two things that look like they should work and do not: the `>FILE cmd` prefix form returns rc=205, and `Break 8` returns rc=0 without killing a stuck process. Neither fails loudly in a way that reads as "this did nothing" if you are watching only the exit code — check the process list after a `Break`, not the return code.

## What I would check first next time

1. Is the binary the right ABI for this machine? (Size and `r12moves` before anything else.)
2. Did I read the evidence after the writer exited?
3. Is the silence I am interpreting actually a refusal?

All three of today's wrong conclusions came from silence that meant something other than "nothing happened".
