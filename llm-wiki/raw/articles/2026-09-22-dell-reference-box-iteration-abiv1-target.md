# 2026-09-22 — Decision: Dell E6320 is the reference box for ITERATION; ultimate target is ABIv1

> Source: session decision (user, 2026-09-22) + prior-session evidence (measurement article, cross-post build rule), compiled by agent
> Collected: 2026-09-22
> Published: 2026-09-22

## Disposition
New. Decision record. Stands until superseded by a newer decision article;
measurement articles keep their own Status notes pointing here.

## Decision
- **Reference box for iteration: the Dell Latitude E6320** (ABIv11 boot
  stick, agent `e6320`, 192.168.1.60). Day-to-day probe runs, timing
  characterization, and driver-behavior questions are settled on this box.
- **Ultimate target: ABIv1.** Acceptance gates pass on ABIv1 (the v1/QEMU
  guest lane stays the acceptance lane). Dell numbers are
  iteration-reference, not acceptance.

## Why this split
Real hardware kills VOID fiction during development (real codec
negotiation 44100/16-bit, real best_mode `0x003E0001`, honest player-rate
timing), while the ship target's ABI is v1 — and a binary built for one
ABI faults on the other (demonstrated v1→v11: `exec_83_OpenResource`
privilege violation; v11→v1 untested, not claimed). A single-lane
reference would therefore certify numbers the target cannot reproduce, or
gate iteration on emulator fiction. Iteration on Dell, acceptance on
ABIv1.

## Consequences (binding on future work)
1. **Dual-build discipline.** Every probe/harness must build under BOTH
   SDKs: v1 SDK for the ABIv1 target, v11 SDK + gcc-16.1.0 for Dell
   iteration (recipe in the cross-post build rule). One binary never
   covers both lanes.
2. **Source-portable, not binary-portable.** ExecBase layout is identical
   across v1/v11 but field names are privatized on v11
   (`ThisTask→Private1` etc.) — code must not depend on the privatized
   names or on either lane's startup object. Offset-based access ports;
   name-based access does not.
3. **Thresholds are lane-scoped.** Dell-derived thresholds (e.g. the
   55-vs-3750 verify window) are iteration thresholds. Acceptance
   thresholds come from ABIv1 runs. Never copy a threshold across lanes
   without a same-day run on the accepting lane.
4. **Characterize on Dell, confirm on ABIv1.** Open items keep this shape:
   the ~11 Hz-vs-750 Hz player-rate question is characterized on the Dell
   (frames=128/256 follow-up), then confirmed on ABIv1 before any gate
   text cites it.
   **Status 2026-09-22 (refinement from the 11 Hz case):** confirm-on-ABIv1
   applies where the ABIv1 vehicle can observe the phenomenon. The
   driverless QEMU guest cannot confirm driver timing (VOID spins
   unclocked by design) — driver-timing verdicts stand on Dell
   characterization alone, labeled as such; ABIv1 confirmation is
   required for everything the guest CAN observe (negotiation, ladder,
   device IO).
5. **QEMU/VOID lane is retained** as the cheap pre-gate and the ABIv1
   acceptance vehicle — not retired.

## Pinned box identity (iteration reference) — PINNED 2026-09-22, session 750
Collection record: this article's decision authorized the pinning; the
values below are box-measured unless labeled otherwise. Evidence files on
host: `/home/miller/Work/spike_spool_laptop/pcitoolinfo.txt` (11,606 B
full PCITool save, sha-verified GET) + `hdaudio.config` (2,182 B,
ENVARC copy, sha-verified GET) + `/tmp/ri/HDAUDIO_mode` (126 B mode
file); durable originals persist on the box (`SYS:pcitoolinfo.txt`,
`ENVARC:hdaudio.config`).
- **BIOS rev A19** — operator screen-read (no software path exists on the
  stick: ShowConfig/Identify carry no BIOS field, no SMBIOS reader).
- **AROS build** — `Version FULL`: Kickstart 51.51, Workbench 40.0
  (07/17/26); ShowConfig: AROS 41.3, Exec 51.8 [64bit], GRUB 2.12,
  `ARGS: vesa=1024x768 ATA=32bit`, CPU i5-2520M.
- **Stick hash** — sha256
  `dbbc8fadf77f74bb025a74f2b0da1f4a14a0607197839be2a2ff5f7b972e522f`
  (`sfs_p5.img`, 8,589,837,312 B); MBR `mbr.bin` md5
  `029629803558167b0b501115ca5334ca`.
- **HDA controller (box-measured, two independent sources)** — Intel 6
  Series/C200 (Cougar Point, QM67) HDA: VendorID `0x8086`, ProductID
  `0x1C20`, Rev `0x04`, Subsystem Dell `0x0492`, Class `04:03:00`,
  IRQ 22, BAR0 mem `0xe2e60000` size `0x4000` (PCITool save); driver
  `ENVARC:hdaudio.config` match list contains `0x8086, 0x1C20`
  (`;QUERY` scan mode). Driver `hdaudio.audio 6.37 (11/15/25)`;
  mode file binds driver `hdaudio` ↔ mode `0x003E0001`
  ("HiFi 16 bit stereo++").
- **HDA codec — HYPOTHESIS (verb-level OPEN): IDT 92HD90.** Legs: Dell
  E6320 Owner's Manual (P12S) states "Audio Controller IDT 92HD90";
  Dell's IDT 92HDxxx driver page lists E6320 compatible; the driver
  carries an IDT branch (`misc.c:1094`, vendor `0x111d`/`0x8384`) and
  audio demonstrably works. Against: no verb-level read exists on the
  stick — `DumpDebugBuffer` absent, Sashimi opens no window via
  `Run` (attempted 2026-09-22, ui-windows shows no Sashimi window), no
  HDA-verb tool ships in C:. Vendor `0x111D` by elimination+factory doc;
  exact device/rev stays OPEN until a verb path exists.
- **M1.1 reproduced post-reboot (session 750)** — full `probe_ahi_v11`
  rerun after the reboot returned every number identical (ladder 7/7,
  verify 55/3750, unit 0 PRESENT err1=0/err2=-2); lib base now
  `0x0000000101376c80` (was `0x0100eed720` — placement varies per boot,
  always above 4 GB). Reference numbers are boot-stable.

## Files
- Measurement: `2026-09-22-m1-1-real-hardware-abiv11-e6320.md` (Status
  note appended pointing here).
- Build rule: Vulkan4Aros
  `llm-wiki/raw/articles/2026-09-22-laptop-abiv11-real-hardware-ahi-probe.md`
  (Status note appended pointing here).