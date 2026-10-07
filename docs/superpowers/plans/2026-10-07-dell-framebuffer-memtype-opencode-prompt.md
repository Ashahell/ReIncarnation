# Prompt for OpenCode: what memory type does the Dell's VESA framebuffer have? (MTRR / PAT / page tables), and does write-combining fix the 220 MB/s screen path?

> Written 2026-10-07 by the Claude advisor session. The owner asked for
> "#1": read the Dell's processor cache settings for the display memory, to
> settle whether the slow screen path is an uncached framebuffer.
>
> The work has five phases (F0–F4). F0–F2 are **read-only** on the Dell. F3
> **changes CPU cache configuration** and runs **only with the owner's
> explicit "yes" in chat and the owner present**, because a mistake there
> needs the owner to reboot the Dell. F4 is the write-up.
>
> **You are done when the gates in §7 for the phases you reached read PASS,
> with evidence where the gate names it.** If you run short of context, stop
> at a phase boundary and write the handoff block (§8).

---

## 0. What is known, and the question

### 0.1 The measurement that started this (advisor, 2026-10-07)

- **Setup:** Dell E6320, ABIv11 AROS, VESA 1366x768, depth 24.
- **Probe:** `BLITPROBE`; its source is in
  `docs/evidence/gui/tab-switch/2026-10-07-b0-split.md`, in the addendum.
  Two runs gave the same figures:

```
RAM->RAM  BltBitMap        484 us/frame  4332 MB/s
RAM->WIN  BltBitMapRastPort 9530 us/frame  220 MB/s
WIN       RectFill full    9290 us/frame  225 MB/s
```

- **What it shows:** everything that reaches the screen runs at about
  220 MB/s, about 20× slower than RAM. This explains the 4–15 ms window blit
  per tab switch, and much of MUI's share of a switch. It slows every repaint
  on the Dell.

### 0.2 The AROS mechanism, from source (verify each pointer)

The Dell runs the **ABIv11** tree: `/home/miller/Work/projects/Vulkan4Aros/src/abi/v11/AROS`,
HEAD `fbc242c104` when this was written. The ABIv1 tree is `…/src/abi/v1/AROS`,
and it differs in places, as noted below.

1. **The draw path** (`rom/hidds/vesagfx/vesagfx_support.c`): the VESA driver
   draws into a RAM shadow. Then `UpdateRect` `CopyMem`s each changed
   rectangle to `hwdata->framebuffer`.
2. **Where the framebuffer address comes from:** `data->framebuffer =
   vi->FrameBuffer`, the bootloader's VESA info (around line 68). The
   fallback is `Find_PCI_Card` → `Enumerator` → `HIDD_PCIDriver_MapPCI` on
   BAR0. Which of the two is used on the Dell is **unknown**; find out (F1).
3. **The page tables** (`arch/x86_64-pc/kernel/mmu.c`, v11): the kernel's
   identity map sets `pwt = 0`, `pcd = 0` on every level. With the
   power-on PAT, that selects **PAT entry 0 = WB**, so the effective type is
   **whatever the MTRRs say** for that physical range.
4. **The ABIv1 tree differs:** `arch/all-pc/hidds/pcipc/pcipc_driverclass.c`
   maps BARs above `PCIPC_IDENTITYMAPPED` with `MAP_CacheInhibit` (PCD set,
   so UC regardless of MTRR). The v11 tree has no `PCIPC_IDENTITYMAPPED`.
   Confirm both statements.
5. **No MTRR or PAT setup in AROS:** a grep of both trees for `0x277`,
   `IA32_PAT`, `0x2FF`, `MTRR_DEF` finds nothing in the kernel or the drivers.
   AROS appears to leave the BIOS MTRRs and the power-on PAT untouched.
   Confirm.

### 0.3 The x86 rules you need

These come from the Intel SDM Vol. 3A, the chapter "Memory Cache Control".
Cite section and table numbers from the edition you use, and re-check every
value below against it before you rely on it.

- **CPUID:** `CPUID.01H:EDX[12]` = MTRR supported; `CPUID.01H:EDX[16]` = PAT
  supported.
- **MSRs** (read with `RDMSR`, which requires CPL 0):
  - `IA32_MTRRCAP` `0xFE`:
    - VCNT = bits 7:0 (the number of variable ranges);
    - FIX = bit 8;
    - WC = bit 10 (write-combining type supported).
  - `IA32_MTRR_DEF_TYPE` `0x2FF`: default type = bits 7:0, FE = bit 10,
    E = bit 11 (MTRRs enabled).
  - `IA32_MTRR_PHYSBASEn` = `0x200 + 2n` (type bits 7:0, base from bit 12);
    `IA32_MTRR_PHYSMASKn` = `0x201 + 2n` (V = bit 11, mask from bit 12), for
    n < VCNT.
  - The fixed-range MTRRs (`0x250`, `0x258`, `0x259`, `0x268`–`0x26F`) cover
    only the first 1 MiB and are **not relevant**. Read them only if FIX = 1,
    and for completeness.
  - `IA32_PAT` `0x277`: eight entries PA0–PA7, one per byte, using bits 2:0
    of each byte.
- **Memory type encodings:**
  - 0 = UC, 1 = WC, 4 = WT, 5 = WP, 6 = WB;
  - 7 = UC− (PAT only).
- **The power-on PAT** is PA0 = WB, PA1 = WT, PA2 = UC−, PA3 = UC, then the
  same again for PA4–PA7.
- **Page-table cache bits:**
  - PWT = bit 3, PCD = bit 4;
  - PAT = bit 7 in a 4 KiB PTE, and bit 12 in a 2 MiB PDE or 1 GiB PDPTE with
    PS = 1.
  - PAT index = `PAT*4 + PCD*2 + PWT`.
- **Combining PAT and MTRR** (the SDM's "effective page-level memory type"
  table; verify each row):

| PAT type | MTRR type | Effective |
|---|---|---|
| UC | any | UC |
| UC− | WC | WC |
| UC− | otherwise | UC |
| WC | any | **WC** |
| WB | UC | UC |
| WB | WC | WC |
| WB | WB | WB |

  - So write-combining is reachable either through an MTRR of type WC over
    the framebuffer (with a WB or UC− PAT entry selected), or through a PAT
    entry of type WC.
  - The variable MTRR rule: if a range has several matching types, UC wins,
    and WT combined with WB gives WT. Other overlaps are undefined.
- **Expectations, to test and not to assume:**
  - a BIOS normally covers RAM with WB and leaves PCI MMIO as UC, either as
    the default type or through an explicit UC range;
  - uncached writes to a framebuffer typically run at a few hundred MB/s, and
    write-combined writes are several times faster.

### 0.4 The question

What is the **effective memory type** of the physical range the Dell's VESA
driver writes to, and is UC the reason for 220 MB/s?

Secondary question, only if the owner approves F3: does making that range
write-combining raise `BLITPROBE`'s RAM→WIN rate, and by how much?

---

## 1. Read first

1. The tab-switch evidence and its addendum:
   `docs/evidence/gui/tab-switch/2026-10-07-b0-split.md` (the TAB split and
   `BLITPROBE`).
2. The llm-wiki: `llm-wiki/raw/articles/2026-10-07-drum-tails-are-polyphony-and-the-screen-is-the-bottleneck.md`.
3. The AROS sources named in §0.2, **in the v11 tree first** (that is what
   the Dell runs). Also read:
   - `rom/exec/supervisor.c` and the x86_64 exec/kernel code behind
     `Supervisor()`: how AROS runs a function at CPL 0, and what it requires
     of that function (register use, return path);
   - `rom/bootloader` (`bootloader_intern.h`, `struct VesaInfo`): how a
     program obtains the bootloader's VESA info (`GetBootInfo()` and its tag,
     if exposed);
   - the `KrnMapGlobal`/`KrnVirtualToPhysical` implementations, and whether
     the identity map covers the framebuffer's physical range (look for the
     identity-map top in `mmu.c`, "identity map trimmed …").
4. The Vulkan4AROS skill `aros-development` (ABI rules: the Dell is
   **ABIv11**, the r12 convention, build against the v11 SDK).
5. The lane rules: `docs/superpowers/plans/2026-09-26-g9-opencode-prompt.md`
   §4, and the Dell notes in the llm-wiki (lane articles of 2026-10-02 and
   2026-10-05).

---

## 2. Hard rules

- **Read-only means read-only.**
  - F1 and F2 may execute `CPUID`, `RDMSR` of the architectural MSRs listed
    in §0.3, `mov %cr3` (read) and page-table **reads**. Nothing else
    privileged.
  - **Gate every `RDMSR` on its CPUID bit and on `VCNT`.** A `RDMSR` of a
    non-existent MSR raises #GP at CPL 0, which crashes the machine.
  - **No `WRMSR`, no CR0/CR3/CR4 writes, no page-table writes in F1/F2.**
- **Prove the supervisor path before the Dell.** Before the first Dell run,
  run the probe on an AROS QEMU guest **that no other session is using**
  (ask the owner which one is free; never disturb riqemu1 if another lane
  holds it). KVM exposes MTRRs and PAT, so a guest proves the code path even
  though its values differ.
  - If no guest is free, ask the owner before running on the Dell: a fault
    at CPL 0 on the Dell means a crash and a reboot, which only the owner
    performs.
- **Dell lane:**
  - spool `/tmp/spike_spool_laptop`, agent `e6320`, via
    `python3 /home/miller/Work/projects/Vulkan4Aros/scripts/spike_server.py submit …`,
    with absolute local paths;
  - test binaries go to `RAM:` only, and you delete them afterwards;
  - check `status` first; never quit an RIAPP you did not start (an RIAPP may
    be running, and that is fine: the probe opens its own window or none);
  - prefer `status` over `--ui-windows` (it has reset the agent);
  - at most one `--ui-capture` per job;
  - only the owner reboots.
- **F3 is gated:**
  - nothing in F3 runs without the owner's explicit "yes" in chat for that
    run, with the owner at the Dell;
  - F3 changes are runtime-only and are undone by a reboot or by the restore
    step;
  - **no change is written to disk, ENVARC or the boot configuration.**
- **No upstream contact.** No AROS issue, PR or mailing-list post: F4 writes
  a draft only. **The stealth rule:** never name ReIncarnation on public
  trackers.
- **Repo rules:**
  - commit only your own files; tag `[§12.11 G9]`; trailer
    `Co-Authored-By: OpenCode <noreply@opencode.ai>`;
  - `bash scripts/ri_audit.sh` must read `AUDIT 0/0 PASS` before each commit
    (isolated worktree if the tree has foreign WIP);
  - **do not push.**
  - Probe sources go under `lane/` (Dell-only plumbing) or into the evidence
    file. Not `tools/`, which is gated against Amiga paths, and not
    `scripts/`, which is gated to a fixed set.
- **Clean-room:** this is OS and CPU work; no ReBirth, ASM or Korg content is
  involved.

---

## 3. Phases

### F0: source answers (no lane)

For the **v11** tree (and note any v1 difference), answer with `file:line`
citations:

1. **Where `vesagfx` gets `framebuffer` on a bootloader-VESA boot**, and
   whether that address is ever passed through `MapPCI` or `KrnMapGlobal`.
2. **The identity-map flags** (P/RW/US/PWT/PCD/PAT, page size) that cover a
   physical address in the 3–4 GiB PCI hole, and how high the identity map
   reaches. Is the framebuffer inside it?
3. **Whether anything in AROS writes MTRRs or the PAT.** Grep for `wrmsr`
   and the MSR numbers in §0.3 across `arch/`, `rom/` and `workbench/hidds/`.
   If you find a hit, quote it.
4. **How `Supervisor()` works on x86_64:** the CPL it runs at, the calling
   convention for the function, and how to return. Alternatively, establish
   whether ordinary tasks already run at CPL 0 on this AROS. Reading `%cs`
   is unprivileged, and its low 2 bits are the CPL, which settles it at run
   time.
5. **How a program reads the bootloader's VESA info** (the `GetBootInfo` tag
   or equivalent), so the probe can print the framebuffer base and size the
   driver used.

**Evidence:** `docs/evidence/gui/framebuffer/2026-10-07-f0-source.md`.

### F1: the read-only memory-type probe (`MEMTYPE`)

**Source:** `lane/memtype/memtype.c`.

**Build:** for ABIv11 with the `ri_build_v11.sh` flags (`-mcmodel=large
-mno-red-zone -ffixed-r12 -fno-builtin -fno-stack-protector`, the v11 SDK),
linked against `startup.o -lamiga -ldos -lexec -lautoinit`. Use the same
recipe as `BLITPROBE`. Also build an ABIv1 variant for the QEMU check.

The probe prints, in a fixed, parseable format:

1. `CPL=<n>` (from `%cs`).
2. `CPUID` vendor, family/model/stepping, the MTRR and PAT feature bits, and
   the processor brand string.
3. `MTRRCAP`, decoded (VCNT, FIX, WC); `MTRR_DEF_TYPE`, decoded (default
   type, FE, E).
4. **Every variable MTRR n < VCNT** as raw hex, plus the decoded `[base, base
   + size)` and type, with the V bit. Derive the size from the mask with
   `MAXPHYADDR` from `CPUID.80000008H:EAX[7:0]`, and print `MAXPHYADDR`.
5. `IA32_PAT` raw, plus the eight decoded entries.
6. **The framebuffer:** the base and size the driver uses (from the
   bootloader VESA info or the PCI BAR, per F0; print which). Also print the
   PCI display device's BARs if obtainable, as a cross-check.
7. **The page-table walk** for the framebuffer base, its last byte, and one
   2 MiB step in between:
   - read `CR3` at CPL 0;
   - walk PML4 → PDPT → PD (→ PT);
   - at each level print P, RW, US, PWT, PCD, PS, and the PAT bit where
     applicable, and the final page size;
   - stop the walk, printing why, at any not-present level.
8. **The answer line:** `FB effective=<type> via MTRR=<type>(<which range or
   default>) PAT[<idx>]=<type>`, computed with the §0.3 table, with the table
   rows quoted in a comment beside the code.

**Mechanics:**
- Run the privileged parts through `Supervisor()`, or directly if CPL = 0.
- Keep the CPL-0 function minimal: it fills a plain struct and returns.
  Printing happens afterwards, in user mode.

**Tests** (host, pure logic). Put the decoders in a small pure C file that
the host can compile, with a gated or ungated unit test as fits the audit's
rules for `lane/` code. If the audit cannot reach `lane/`, put the decoders
and their test under `tests/unit/` as a pure module. Test:
- the MTRR mask-to-size decode, with known BIOS-style examples;
- the PAT byte decode;
- the effective-type table, every row of §0.3;
- the PAT index from PWT/PCD/PAT for both the 4 KiB and the large-page bit
  positions.

Mutation proof: swap PCD and PWT in the index → FAIL.

**Runs:**
1. **QEMU guest first** (per §2), ABIv1 build. Record the output and confirm
   that it did not fault.
2. **Dell (ABIv11)**, twice. The values must be identical across runs.
   Delete `RAM:MEMTYPE` afterwards.

**Evidence:** `docs/evidence/gui/framebuffer/2026-10-07-f1-memtype.md`, with
the verbatim output and the decode reasoning.

### F2: tie the type to the speed (read-only)

1. Re-run `BLITPROBE` (source in the B0 addendum) in the same Dell session as
   F1, as the baseline.
2. **If F1 says the framebuffer is UC,** state it plainly: 220 MB/s is what
   UC writes over this path deliver, and 4332 MB/s RAM to RAM shows the CPU
   is not the limit.
3. **If F1 says WC or WB already,** the cause is elsewhere. Candidates:
   - the `CopyMem` implementation for this target (is it byte-wise or SSE?
     read `CopyMem` for x86_64);
   - `UpdateRect` granularity;
   - bus speed.

   Write down what to measure next, and **stop**: F3 does not apply.

**Evidence:** `docs/evidence/gui/framebuffer/2026-10-07-f2-speed.md`.

### F3 (gated): a runtime write-combining trial

**Only if F1 found UC, and only with the owner's "yes" in chat for this run,
with the owner at the Dell.**

**Pick the least invasive method F1 permits:**

- **Method A, a variable MTRR (preferred when F1 shows):**
  - a free variable MTRR (V = 0) exists;
  - the framebuffer is mapped with PCD = 0 (PAT entry WB or UC−);
  - no explicit UC MTRR overlaps the range (UC would win);
  - `MTRRCAP.WC` = 1.

  The framebuffer range must be programmed as power-of-two-sized,
  naturally aligned WC range(s) covering exactly the framebuffer. If the
  size is not a power of two, cover the visible part, or use two ranges if
  enough are free.

- **Method B, PAT:** reprogram a PAT entry to WC and point the
  framebuffer's mapping at it. Riskier, because it changes every mapping
  that already uses that index. **Do not use it** unless F1 proves the index
  is otherwise unused, and the owner approves the extra risk.

**Procedure:**
- Follow the SDM's MTRR update procedure. Every logical CPU must hold
  identical MTRRs:
  1. disable interrupts;
  2. set CR0.CD, clear NW, `WBINVD`;
  3. flush the TLB (reload CR3);
  4. clear `MTRR_DEF_TYPE.E`;
  5. write PHYSBASE/PHYSMASK;
  6. set E;
  7. `WBINVD`, flush the TLB;
  8. clear CR0.CD;
  9. restore interrupts.
- **Determine first how many CPUs AROS runs on the Dell.** If more than one
  is active, the change must run on each one (find out how AROS runs code on
  each CPU, e.g. its SMP/IPI facilities), or the trial is **not done**. Write
  that down and stop.
- Before anything runs on the Dell, the trial code is reviewed by the
  advisor (send the diff, wait for the OK).

**Measure:**
1. Re-run MEMTYPE: it must show the range as WC.
2. Run BLITPROBE twice: compare RAM→WIN and RectFill with F2's baseline.
3. Then **restore** the original PHYSBASE/PHYSMASK pair with the same
   procedure.
4. Re-run MEMTYPE: it must show the original value.

**Abort rule:** if anything misbehaves (hang, artefacts), stop. Tell the
owner that a reboot restores the BIOS MTRRs, and do not attempt more.

**Evidence:** `docs/evidence/gui/framebuffer/2026-10-07-f3-wc-trial.md`.

### F4: the write-up and the upstream draft

1. One paragraph each answering:
   - **the type:** what the framebuffer's memory type is, and why;
   - **the speed:** whether that explains 220 MB/s;
   - **WC (F3, if run):** what write-combining bought.
2. **A draft upstream proposal**, in the evidence file only:
   `docs/evidence/gui/framebuffer/2026-10-07-f4-upstream-draft.md`.
   - It covers where in AROS a fix belongs: options include the kernel
     programming a WC PAT entry at boot (as other OSes do) plus `vesagfx`
     mapping the framebuffer through it, or `vesagfx` setting a WC MTRR when
     PAT is unavailable.
   - It states the risks: overlaps, SMP consistency, older CPUs without PAT,
     and the interaction with other gfx drivers.
   - It cites the source lines.
   - No PR, no issue, no project name. Whether and when this goes upstream is
     the owner's decision.
3. Add the outcome to the `Dell screen write bandwidth` item in
   `docs/2026-09-24-improvement-todo.md`: tick sub-step (1), and record what
   F3 showed if it ran.
4. Commit as
   `framebuffer memtype: Dell VESA FB is <type> (MTRR/PAT/PTE read), <WC trial result or not run> [§12.11 G9]`.

---

## 4. Traps

- **`RDMSR` of an MSR the CPU lacks is a #GP at CPL 0,** which is a crash.
  Gate on CPUID and VCNT, and read only the architectural MSRs listed in
  §0.3.
- **Do not assume the framebuffer address.** The driver may use the
  bootloader's address or a PCI BAR; F0/F1 must show which.
- **Variable-MTRR overlap rules are not "last one wins":** UC beats
  everything, and WT plus WB gives WT. Other overlaps are undefined.
- **A 2 MiB or 1 GiB page keeps its PAT bit at bit 12, not bit 7.**
- **The QEMU guest's MTRR and PAT values say nothing about the Dell.** The
  guest only proves the code path.
- **`Type`ing long output over the agent is capped;** redirect the probe's
  output to a file in `RAM:` and `--get` it.
- **The Dell agent has hung on `--ui-capture` and `--ui-windows` before.**
  This work needs neither.

---

## 5. Out of scope

- Any change to RIAPP or the engine.
- Shipping a WC change: persistent configuration, boot scripts, or a patched
  AROS on the Dell.
- The app-side "avoid MUI backfill under canvases" item. It is separate and
  stays in the todo.

---

## 6. Evidence layout

`docs/evidence/gui/framebuffer/2026-10-07-f{0,1,2,3,4}-*.md`, plus the probe
source under `lane/memtype/`. Every number is verbatim tool output, or derived
with its components shown.

---

## 7. Success gates

| Gate | PASS condition | Evidence |
|---|---|---|
| GF0 | The five F0 questions answered with v11 `file:line` citations (and v1 differences noted) | F0 evidence |
| GF1a | Decoder unit tests, including every effective-type row, with a mutation proof | test output, commit |
| GF1b | MEMTYPE ran on a QEMU guest without fault before the first Dell run (or the owner approved skipping it in chat, quoted) | F1 evidence |
| GF1c | Two identical Dell runs; framebuffer base/size and source named; MTRR ranges, PAT and the page walk printed; one `effective=` answer line | F1 evidence |
| GF2 | BLITPROBE baseline in the same session; the type-to-speed conclusion stated, or the next measurement named if not UC | F2 evidence |
| GF3 (if run) | Owner's yes quoted; advisor review OK quoted; the CPU count handled; MEMTYPE shows WC during and the original after restore; BLITPROBE before/after | F3 evidence |
| GF4 | Answers, the upstream draft (no project name, not sent), todo updated | F4 evidence, todo diff |
| G-all | `AUDIT 0/0 PASS` at each commit; own files only; nothing pushed; `RAM:` cleaned; no persistent change on the Dell | `git status`, `dir RAM:` |

---

## 8. Reporting and handoff

After each phase, report in at most 10 lines: the gates passed with the key
value each (e.g. "FB 0xE0000000+0x01000000, MTRR default UC, PAT[0]=WB →
UC"), the commit hash, and anything unproven.

If you stop mid-way:

```
HANDOFF framebuffer-memtype
  last green phase : F?
  last commit      : <hash>
  next step        : <one line>
  open questions   : <list>
  lanes touched    : <QEMU guest? Dell?> and state left in
```

## 9. Owner decisions (list them; do not decide)

1. Whether to run F3 (a runtime WC trial on the Dell, owner present).
2. Whether and how to take the fix upstream to AROS, using the F4 draft.
3. If a fix lands upstream: whether to run a patched AROS build on the Dell.
