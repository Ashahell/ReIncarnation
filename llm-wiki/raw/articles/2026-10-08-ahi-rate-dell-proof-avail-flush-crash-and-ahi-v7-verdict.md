# AHI honest rate proven on the Dell; `avail flush` crashes AROS in a ROM expunge; the framebuffer follow-up went upstream; the AHI v7 verdict (2026-10-08)

- Source: ReIncarnation session (advisor lane), 2026-10-07 – 2026-10-08.
- Evidence:
  - `docs/evidence/audio/ahi-rate/2026-10-07-h2-fix.md` (§"Dell proof", verbatim probe output);
  - `docs/evidence/gui/framebuffer/2026-10-07-f1-memtype.md` and `…-f4-upstream-draft.md` (review addenda);
  - `docs/AhiV7.md` (Draft 3, Appendix E).
- Collected: 2026-10-08
- Published: 2026-10-08
- Related:
  - [2026-10-07-ahi-honest-rate.md](2026-10-07-ahi-honest-rate.md);
  - [2026-10-07-framebuffer-is-uncached-and-the-wc-trial-stops-at-four-cpus.md](2026-10-07-framebuffer-is-uncached-and-the-wc-trial-stops-at-four-cpus.md);
  - [2026-10-07-drum-tails-are-polyphony-and-the-screen-is-the-bottleneck.md](2026-10-07-drum-tails-are-polyphony-and-the-screen-is-the-bottleneck.md).

## 1. AHI honest rate: Dell proof done

**Setup:**
- The patched `hdaudio.audio` (ABIv11, 62904 B, md5
  `43b41b7f4b1e47e82422af317ddbea3a`) was installed on a **fresh boot, before
  any audio use**. The first open then loaded it from disk, so no expunge was
  needed.
- The original (62200 B, md5 `4c7f0199e9c6a20ef58a8cfdecbd586d`, version
  6.37) was backed up to `Vk4aros:ahi-backup/hdaudio.audio.orig`, and the
  backup was read back with a matching checksum.

**`probe_rate_hw`** (each loop timed with EClock):

| request (Hz) | before: M | before: H | after: M | after: H |
|---|---|---|---|---|
| 50000 | 50000 | 48027 | **48000** | **48049** |
| 60000 | 60000 | 48029 | **48000** | **48030** |
| 100000 | 100000 | 96054 | **96000** | **96098** |

- The listed-rate and below-floor rows are unchanged (44100/48000/96000;
  32000/22050 → 44100).
- **M = H within 0.1 % on every row** (permille 1000–1001).
- **Regression:** RIAPP `521ab7e` with Zombie Nation on the patched driver
  logs `mix=48000 Hz buffer=256 frames period=5333 us`, with `xruns=0` and
  `overloads=0` over 11825 buffers.
- **Restored:** the original driver file was put back from the stick and
  md5-verified.
- **Still open:** the upstream branch `ahi-report-selected-rate` (six driver
  commits plus an optional autodoc commit) is unpushed, and no PR is open.
  That is the owner's call.

## 2. `avail flush` crashes this AROS: a ROM-library expunge bug, not ours

- **What happened:** the first install attempt began with `avail flush`. It
  raised a Software Failure:
  - task `avail`, error `0x80000003`, illegal address access, function
    `tlsf_freevec`;
  - module: Kickstart ELF segment 11 `.text`.
- **Backtrace,** read from a half-scale capture, cropped and upscaled:
  `LDFlush` → a Kickstart library's `*_Expunge_Lib` → Kickstart segment code
  → `FreeVec` → `nommu_FreeMem`.
- **Mechanism:** Avail's `FlushMem` (`workbench/c/Avail.c`) recursively calls
  `AllocMem(0x7ffffff0)` until it fails. That fires lddemon's low-memory
  `LDFlush`, which expunges every unused library inside the `avail` task. One
  ROM library's expunge frees memory badly.
- **Aftermath:**
  - the crashed task held lddemon, so the Dell **could not load any new
    command** (`copy`, `delete` and `wait` all returned rc=1 through the
    agent) until a reboot;
  - the old agent hung on the job, and the server dropped the session on a
    recvn timeout.
- **Rules that came out of it** (saved to memory):
  - **never `avail flush` on the Dell;**
  - to load a replaced driver or library, install the file, then use an
    owner reboot or a fresh boot before first use.
- **Upstream candidate:** naming the exact ROM library needs a full-scale read
  of the function line. It was not pursued.

### Lane techniques that worked

- **Reading a crash requester without the owner typing it:**
  1. the owner presses **More** and restarts the agent;
  2. `--ui-close` any window overlapping the requester;
  3. `--ui-capture …,2` (scale 1 is refused at 1366×768);
  4. crop the requester and upscale 4–6× with Pillow. The backtrace function
     names become readable.
- **A second agent while the first is wedged:** the owner runs
  `Run >NIL: SYS:ATCPBIN agent 192.168.1.81 9292 e6320` from a Shell. There
  is no singleton guard, and a queued job runs as soon as the new session
  connects.
- **Park a poison job before the agent reconnects:** move it out of
  `jobs/`, as recorded before. The in-flight job's file being missing made the
  old session end cleanly.
- **The `Software Failure` "Log" button writes to serial only**
  (`rom/exec/useralert.c`: `RAWFMTFUNC_SERIAL`). It is useless on the Dell.
- **A host reboot wipes `/tmp`,** which included `/tmp/opencode` and the
  scratchpad. The patched driver survived in the v11 gen dir
  (`core-pc-x86_64/bin/pc-x86_64/gen/workbench/devs/AHI/Drivers/HDAudio/`),
  with an identical md5. Probes rebuild with
  `~/Work/vms/ri-p9/build_probe_v11.sh`.

## 3. Framebuffer follow-up: two facts, posted upstream

- **MTRR8 is a staged, disabled WC range over the framebuffer:**
  - raw: `base=0x00000000D0000001 mask=0x0000000FFC010000 V=0`;
  - decoded: type WC, base `0xD0000000` (the framebuffer), 64 MiB, valid bit
    clear;
  - the mask carries a **stray bit 16**, so enabling it needs a clean mask,
    not just V;
  - something before AROS prepared it and left it off.
- **No other display driver for the Dell:** the IntelGMA monitor driver has
  the Sandy Bridge IDs (`0x0102/0x0106/0x0112/0x0116/0x0122/0x0126`)
  commented out, upstream too. `vesagfx` is the only driver for the HD 3000.
- **No single-CPU workaround:** there is no `nosmp` boot switch. `noapic`
  only skips probing when ACPI found nothing, and x86-64 panics without an
  APIC. The IPI LVO is reserved in `kernel.conf`. So a runtime WC trial needs
  kernel code.
- **Posted, on owner instruction, as a comment on AROS #1508** (no project
  name): https://github.com/aros-development-team/AROS/issues/1508#issuecomment-6044926251

## 4. AHI v7: Draft 3, and the verdict

- **Draft 3** (`docs/AhiV7.md`) corrected Draft 2 against the tree:
  - G1/G2 were false: AHI v6 has 32-bit types, HiFi, and `AHIST_L7_1`;
  - G9 was false: about 20 drivers ship;
  - broken primitives were fixed: multi-producer `PostControl` on an SPSC
    ring; tearing params; a `ULONG Position` carrying an error; a 0-valued
    format bit; the C8/C9 contradiction; a zero-tolerance SRC test;
  - the `Cause()` hop was dropped;
  - the PIT-based `ReadEClock` cost (~2.2 µs) was priced into attribution;
  - Phase 1 was split into 1a (v7 spine, v6 untouched) and 1b (one
    library).
  - A false "44100 for every rate" claim in Draft 3 was then withdrawn (see
    the honest-rate article), and Q11 was answered from the driver survey.
- **The advisor's verdict:**
  - **the architecture is right; the plan is unlikely.** It is 20–30 kLOC and
    12–24 months for perhaps two to five part-time people, with a social
    problem bigger than the technical one;
  - the premise is overstated: AHI v6 plays at a 5.3 ms buffer with 0 xruns
    on the Dell;
  - **recommended path:** take most of the value as small AHI v6 patches:
    1. honest rate (now done and Dell-proven);
    2. a latency query tag;
    3. task-context `PlayerFunc`;
    4. a driver conformance tool.
  - **For this project, AHI v7 is a distraction:** nothing on the roadmap is
    blocked by AHI v6.
