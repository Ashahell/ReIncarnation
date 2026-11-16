# Prompt for OpenCode: an ABIv1 SMP e1000.device that drives the Dell's 82579LM

> Written 2026-11-16 by the Claude advisor session.
>
> **Goal:** the Dell E6320 boots the ABIv1 **SMP** USB stick, but `net0` fails:
> *"DEVS:networks/e1000.device unit 0 could not be opened"*. The stick carries the
> stock v1 `e1000.device`, which has **no 82579LM (PCI 8086:1502) support**. Build
> an ABIv1 SMP `e1000.device` from the recovered e1000e port, prove it in VMs, and
> stage it for the stick. The owner does the Dell boot.
>
> **Phases E0–E4.** Each phase ends with a commit (Vulkan4Aros) and a short report.
> **Do not push.** If you run short of context, stop at a phase boundary and
> write a handoff.

---

## 0. Facts (verified 2026-11-16; check each before relying on it)

- **The working Dell driver** is a private e1000e port: `$VER: e1000.device 1.1
  (21.7.2026)`, plus the 2026-09-25 txpool binary patch. Binary:
  `Vulkan4Aros/artifacts/e1000-txfix-20260925/e1000.device.dell-e6320-txpool`
  (423,168 B, sha256 `b98be161…`). It is **ABIv11** (library calls set `r12` =
  base, e.g. `mov %r11,%r12; call *-0x990(%r11)`), so it **cannot** be dropped
  onto the ABIv1 stick.
- **The port's source was not on GitHub or in any local tree.** It was recovered
  2026-11-16 from the old Windows WSL disk (`Ubuntu-20.04/ext4.vhdx`, tree
  `/root/vulkan4aros/src/AROS`, origin `deadwood2/AROS`). Saved in
  `~/Work/wslold/`:
  - `e1000-port-src/` — the driver directory as it stood (incl. untracked
    `e1000_mbx.c`, `e1000_vf.c`), 1.4 MB;
  - `0001-e1000e-port-3e4f72904.patch` — commit `3e4f72904` (2026-08-06, *"e1000:
    port e1000e family (82574L/82579) core from FreeBSD, add FAT-partition
    logging"*), never pushed;
  - `0002-e1000e-uncommitted.diff` — the later uncommitted edits
    (`e1000_write_pcie_cap_reg`, log to `RAM:e1000.log`, `e1000_vf`/`e1000_mbx`
    in the mmakefile);
  - `fbsd124/` — the FreeBSD 12.4 shared code the port came from.
  - `ext4.vhdx` (317 GB) — the replayed **copy** of the WSL disk. The original on
    the Windows drive is untouched. Don't delete either; that's the owner's call.
- **Target tree:** `Vulkan4Aros/src/abi/v1/AROS` (ADT master + our series). Its
  e1000 is still upstream's (no 82579). The path is unchanged:
  `workbench/devs/networks/e1000`, though `a4e40456ae` edited its mmakefile.
  Read that commit first.
- **Build dir:** `src/abi/v1/core-smp` (configured `--enable-target-variant=smp`,
  llvm toolchain). The stock SMP driver is
  `core-smp/bin/pc-x86_64-smp/AROS/Devs/Networks/e1000.device` (150,264 B,
  sha256 `1734eba4…`).
- **The Tx wedge fix is NOT in the v1 tree.** It exists as
  `Vulkan4Aros/patches/e1000-tx-no-alloc-in-interrupt.v1.patch` (commit
  `b78bd3a47a`). Root cause: Tx frames were AllocMem'd in the soft interrupt and
  FreeMem'd from the hardware interrupt → `tlsf_freevec` faults. The Dell driver
  only has it as a binary patch. **The new driver must carry it.**
- **The stick (ABIv1 SMP, PHILIPS 60 GB):** p1 = 100 MB FAT `EFI` (GRUB, UEFI +
  legacy core in the MBR since 2026-11-16), p5 = SFS `AROS64` (SYS:). The agent is
  already installed (`SYS:ATCPBIN`, `S:User-Startup`, AROSTCP `db/interfaces` =
  `net0 DEV=DEVS:networks/e1000.device UNIT=0 IP=192.168.1.60`).
  - **The Dell does not mount `EFI:`**; a VM booting the stick does.
  - This stick's flash returned unstable reads in September.

## 1. Lane rules

- **Never `avail flush` on the Dell.** A new `.device` loads only after a
  **reboot**. A copy that "succeeds" in a running system proves nothing.
- **Hash-verify every binary you stage**, and record its sha256 and size in each
  report.
- **ABI check before any boot.** Compare the call-site pattern of the new
  driver against the stock v1 SMP `e1000.device`: base in the last C argument
  (`rdx`/`rsi` per arity), not `r12`. Prove your grep finds a known positive
  first (the ABIv11 Dell binary is one). A zero from an untested pattern proves
  nothing.
- **Before blaming AROS, rule out the caller:** constants, return conventions,
  NULL locks, late `IoErr()`.
- Writing to the stick needs the owner: it's attached to the Dell or the host
  as they choose. Ask before any write, and never write outside the files named
  in E3.
- Patch-series conventions are in `src/abi-patches/v1/aros/SERIES` and `PREREQS`.
  Read the header before adding anything. Grep for an unused number first.

## 2. Phases

### E0 — reconcile the source (host only)

1. Diff `~/Work/wslold/e1000-port-src/` against the v1 tree's e1000 directory and
   against `3e4f72904`'s parent. Classify every hunk: **port** (new family code,
   device IDs, `e1k_log`), **base drift** (deadwood2 vs ADT differences), or
   **lane-only** (debug logging).
2. Write the reconciliation as `aros/00NN-e1000e-pch2.diff` on top of the v1 tree,
   **plus** the Tx fix from `patches/e1000-tx-no-alloc-in-interrupt.v1.patch`, as
   a separate entry. Keep `e1k_log` writing to `RAM:e1000.log` only. The old
   `DH1:DebugLog` path caused a boot-blocking requester.
3. Report every base-drift hunk you had to adapt, and why.

**Gate E0:** both entries apply to a clean v1 tree, and the tree diff equals
the two entries exactly.

### E1 — build (host only)

`make workbench-devs-networks-e1000-quick` in `core-smp` (check that the target
name exists in the regenerated mmakefile). Then:
- record size, sha256 and `$VER`, and **bump `$VER`** so the two binaries can be
  told apart on the guest;
- `strings` must show `82579LM (e1000e)`;
- run the ABI check from §1.

**Gate E1:** builds clean, the device-ID table contains `0x1502`, and the ABI
check passes against a proven-positive pattern.

### E2 — VM proof (no stick, no Dell)

Boot an SMP v1 DH0 image under SeaBIOS (`-vga std`, `cache=directsync`), with
the new driver installed in `SYS:Devs/Networks/`. Reboot after installing it.
Run it three times:
1. `-device e1000e` (82574L, `0x10d3`): exercises the new family code. The
   interface comes up, and the agent connects and moves a ≥20 MB file both ways.
2. `-device e1000` (82540EM, `0x100e`): a regression check against the old
   family, with the same transfer.
3. The **Tx stress** from `b78bd3a47a`: `memstress` running during sustained
   transfers. This must pass with no `tlsf_freevec` fault, compared against the
   fault the original driver produced.

QEMU cannot emulate the 82579LM. **Say so in the report**: E2 does not prove
the PCH2 path.

**Gate E2:** all three pass, with `RAM:e1000.log` pulled for each.

### E3 — stage for the stick (needs the owner)

Ask the owner to attach the stick to the host. Then:
1. Put the driver on the stick's **FAT** partition as
   `EFI:ATCP/e1000.device`, and extend `EFI:ATCP/INSTALL` with
   `Copy EFI:ATCP/e1000.device TO SYS:Devs/Networks/e1000.device`. Keep a backup
   of the stock driver as `SYS:Devs/Networks/e1000.device.stock`.
2. Boot the stick in a host VM: SeaBIOS, a writable raw device, no network,
   `-display gtk`. The owner runs `Execute EFI:ATCP/INSTALL` there.
3. Verify from the host, with the stick unmounted, by reading the SFS: the
   driver's sha256 under `SYS:Devs/Networks/` must equal E1's. The
   `sfs_nav.py` reader needs two shims on a raw device: size via `seek(0,2)`, and
   the superblock at absolute block **206136**. A working wrapper is
   `…/scratchpad/stickfix/sfsdev.py` from the advisor session; recreate it if it
   is gone.

**Gate E3:** the hash matches on the stick, and the stock driver is backed up.

### E4 — the Dell (owner) and write-up

The owner boots the Dell from the stick. Report:
- `net0` up or not, plus `RAM:e1000.log` (the agent can `--get` it once it's
  connected);
- the agent on `192.168.1.81:9292` as `e6320` (`ss -tn state established
  '( sport = :9292 )'` on the host);
- one ≥20 MB transfer each way.

Then write the evidence and a Vulkan4Aros wiki entry: where the source was
recovered from, and the E2 gap (82579 untestable in QEMU). Record the source
recovery prominently, so the port is never again only on one disk. **Commit
`~/Work/wslold/e1000-port-src` + both patches into Vulkan4Aros** (not the
vhdx).

## 3. Stop and ask the owner when

- anything needs the stick or the Dell;
- E0 finds base drift that changes behaviour (not just names);
- a result contradicts this brief. Say which statement it contradicts.
