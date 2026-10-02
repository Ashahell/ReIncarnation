# Where RIAPP.LOG lives, and why file size cannot tell you which lane you built (Dell, 2026-10-02)

- Source: ReIncarnation session, 2026-10-02 (opencode lane; sticky log destination, deployed and proven on the Dell)
- Collected: 2026-10-02
- Published: 2026-10-02
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P9 interlude)
- Prior: [2026-10-02-dell-xrun-proof-result-and-the-cost-per-repaint.md](2026-10-02-dell-xrun-proof-result-and-the-cost-per-repaint.md) (the logs this work exists to stop losing), [2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md](2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md) (the size heuristic corrected below), [2026-10-01-dell-deploy-abiv11-usb-stick-layout.md](2026-10-01-dell-deploy-abiv11-usb-stick-layout.md) (the stick's volume name and partition)
- Commit: `0c2ba7f` (unpushed at collection)

## The canonical location

> **`RIAPP.LOG` goes to the first mounted USB/stick volume, which on the Dell is `Vk4aros:` on device `USBSCSI0P1:` (30.0G VFAT).** `RIAPP-EV.LOG` already did this; the main log did not.

`RAM:` is the **fallback only**, used when no such volume is mounted. `RIAPP_LOG=<vol>` pins the main log and `RIAPP_EVLOG=<vol>` pins the ev-log, both accepting `RAM:` deliberately as an escape hatch (the spike hunt of 2026-09-27 needed the ev-log forced off the stick because the Dell boots system parts from it and shells will not open without it).

The list, in preference order, is `platform/pal/ri_pal_sticky.h`:

```
Vk4aros:   USB0:   USB1:   UMSD0:   UMSD1:   USBDISK0:      fallback: RAM:
```

It is one header, not two copies, because `app/riapp.c`'s ev-log and the main log must agree on where output goes, and two lists that have to agree are a list that will not.

## Why this was not already true

`RIAPP.LOG` was `RAM:RIAPP.LOG`, and the 2026-10-02 xrun session lost its telemetry twice over:

- I pulled `RAM:RIAPP.LOG` to the host at `/tmp/opencode/fix_f.log` (346925 B), and **the host restarted at 07:48**, which is where that copy died.
- The guest's own copy was then lost when the owner rebooted the Dell, because `RAM:` is wiped by every reboot.

So the figures from that session survive only in a wiki record and a commit message, and the artefacts are gone — every number in the record is un-re-checkable. The ev-log, which had preferred the stick since 2026-09-27, was the one file that would have survived; the main log was not. The `dir Vk4aros:` after the owner's reboot shows exactly that asymmetry: `RIAPP-EV.LOG` present, `RIAPP.LOG` absent.

## On-target proof

Deployed as `RAM:RIPLOG` (a new name per binary; `RAM:` was clean after the reboot and nothing else was running — never two RIAPPs, they contend for the sound card).

```
ev 1 0 RUN frames=256 vol=Vk4aros: build=0c2ba7f
```

`RIAPP.LOG` appeared on the stick, and **was pullable while the app was still running** — 413 B, sha-verified, no quit required. This is the property that was missing: the log is now both durable *and* live, instead of locked and then transient.

Close line, a clean idle run:

```
RIAPP closed: buffers=20204 xruns=0 render_max=44 us render_total=498 ms period=5333 us wake_max=36 us wake_total=383 ms wake_n=20202
```

That is `render_total` 24.6 µs per buffer and `wake_total` 19.0 µs per wake at idle, `xruns=0`. Both new fields are populated and sane, and the draw line carries the new reason split (`box_steps=0/0/0 box_bar=0/0/0 box_other=0/0/0` at idle, as expected — nothing is playing). Durable copies of all three pulls are in `~/Work/vms/ri-p9/logs/`.

## The correction: file size does not identify the lane

The ABIv11 lane article records *"Sizes identify the lane at a glance (v1 1082416 B = broken; v11 1074856..1075600 B = runs)"*. Measured on **one tree** (`0c2ba7f`), all three ways the recipe can be invoked:

| build | bytes | `mov %rax,%r12` | runs on the Dell? |
|---|---|---|---|
| v1 (`ri_build_aros.sh`, the audit's link gate) | 1088384 | **0** | no — `Software Failure!` |
| v11, `RI_V11_OPT=-O0` | 1081512 | 41 | yes |
| v11, default `-O2` | 867320 | 285 | yes |

Two things follow, and the second is the trap.

1. **The previously deployed `RAM:RIPP9F` was built at `-O0`, not the recipe's documented `-O2`.** 1081512 B / `r12moves=41` is an `-O0` build; `-O2` gives 867320 B / `r12moves=285`. The recorded "1075600 B, r12moves=41" is an `-O0` figure. This matters beyond bookkeeping: optimisation level changes CPU cost by ~24 % by size alone, so deploying an `-O2` build onto a machine being measured for GUI repaint cost would confound the very metrics the previous commit added. **The A/B must run both arms at the same optimisation level, and the baseline must stay `-O0` to match what was actually proven to run.**

2. **Size cannot separate the lanes, and never could.** The v1 and v11-at-`-O0` builds differ by **6872 bytes** — 0.6 % — and the old recorded figures were similarly adjacent. A size band is also not a stable property: it moves with every code change, and a 24 % swing from `-O` level alone would put an `-O2` build outside any band recorded from an `-O0` one. **The reliable discriminator is the register convention:** `r12moves == 0` means the build-pc SDK leaked in and the binary will fault on the Dell's first LVO call; `r12moves > 0` is the v11 lane. That is the same check the audit's ABIv1 gate applies, and it is the one to quote.

> **Status: Outdated** (2026-10-02) — on the size heuristic, in [2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md](2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md). Its claim that sizes identify the lane at a glance is replaced by the `r12moves` rule above. Its other findings — that the audit's link gate builds the wrong lane, and that `--get` serves stale bytes for an open file — are unaffected and still hold.

## Method findings

- **A log that lives on `RAM:` is a log that will be lost, and the loss is silent.** Nothing warns you; the run simply ends and the evidence is gone. Anything a run will later be *judged from* belongs on persistent storage, and the destination should be decided by the platform layer rather than by whoever wrote the first call site.
- **Two lists that must agree will eventually disagree.** The ev-log already probed for the stick and the main log did not; consolidating on one list in `platform/` is what keeps them from diverging again.
- **A test that mirrors production data proves nothing about it.** `t153` first carried its own copy of the volume table because `fs_aros.c` is AROS-only and `#error`s a host build. All eight mutants of the real file survived that mirror — a mutation set of pure theatre. Moving the table into a shared header (`ri_pal_sticky.h`) is what made 5/5 behavioural kills possible. This is the second time in two days that an admission-control failure quietly removed coverage (the repaint-reason codes had the same problem in `rsection.h`), and the tell in both cases is the same: the test could not include the thing it was testing.
- **Verify a lane recipe's recorded constants by rebuilding, not by re-reading.** `r12moves=41` looked like a v11 property and was in fact an `-O0` property. Two minutes of rebuilding at each optimisation level turned an assumption into a measurement.

## Standing gaps

- The reboot-survival half of the claim is **not yet demonstrated**: the log was proven to be on the stick and to be live, but the guest has not been rebooted since. That is the owner's call.
- `RIAPP_LOG` is honoured but untested on-target; only the default path was exercised.
- The A/B is still undesigned beyond "both arms at `-O0`". The in-memory ring dumped at quit is still not built, for the reason recorded in the previous article: live per-line writes can perturb what they measure, and now that `RIAPP.LOG` is on a **USB stick** that risk is higher, not lower — stick writes are slower than RAM: writes and this heartbeat writes a line per 30 s while the draw path is timing itself.
## See Also

- [The Dell lane has two silent traps: the audit's link gate builds ABIv1, and `--get` serves stale bytes for a file the guest still has open](2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md)
- [The xrun fix worked and the symptom did not close: repaint count fell 98.6 %, repaint cost did not move](2026-10-02-dell-xrun-proof-result-and-the-cost-per-repaint.md)
