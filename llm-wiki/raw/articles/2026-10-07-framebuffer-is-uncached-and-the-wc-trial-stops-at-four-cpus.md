# The Dell VESA framebuffer is uncached (MTRR default UC), and the WC trial stops at four CPUs (2026-10-07)

- Source: ReIncarnation opencode lane, 2026-10-07.
  - Opencode commits: `f31d2f3` (F0), `97dbd04` (F1a), `4c806c9` (F1b),
    `486e314` (F1c+F2), `2d4e6eb` (F4), `1459e4b` (F3), `ef329a4` (sent upstream).
  - Dispatch prompt: `docs/superpowers/plans/2026-10-07-dell-framebuffer-memtype-opencode-prompt.md`.
  - Upstream: AROS issue #1508 (proposal only, no code, no project names).
- Evidence:
  - `docs/evidence/gui/framebuffer/2026-10-07-f0-source.md`;
  - `docs/evidence/gui/framebuffer/2026-10-07-f1-memtype.md`;
  - `docs/evidence/gui/framebuffer/2026-10-07-f2-speed.md`;
  - `docs/evidence/gui/framebuffer/2026-10-07-f3-wc-trial.md`;
  - `docs/evidence/gui/framebuffer/2026-10-07-f4-upstream-draft.md`.
- Probe sources: `lane/memtype/` (`memtype.c`, `memtype_decode.*`, `cpucount.c`,
  `suptest6.c`, `build.sh`), `lane/blitprobe/`, host test `tests/unit/t177_memtype_decode.c`.
- Collected: 2026-10-07
- Published: 2026-10-07
- Related:
  - [2026-10-07-drum-tails-are-polyphony-and-the-screen-is-the-bottleneck.md](2026-10-07-drum-tails-are-polyphony-and-the-screen-is-the-bottleneck.md), whose "likely uncached, MTRR/PAT unread" this closes (see Status there);
  - [2026-10-03-the-ahi-probe-build-contract.md](2026-10-03-the-ahi-probe-build-contract.md), whose foreground-exec rule this reuses;
  - [2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md), the last known-good probe shape.

## The answer

- **Type:** the Dell VESA framebuffer (`0xD0000000` + 67043328 B, bootloader
  source — the range `UpdateRect` writes) is **UC**: no variable MTRR covers
  it so the BIOS default (UC, E=1) applies, while the page tables select
  power-on PAT entry 0 (WB) — the §0.3 WB+UC→UC row. Two identical Dell runs.
- **Speed:** yes, UC explains 220 MB/s. Same-session BLITPROBE: 480 µs
  RAM→RAM (4369 MB/s) against 9509 µs RAM→WIN (220 MB/s), matching B0.
- **WC:** not measured. The trial stopped at its own gate (below).

## How it was proven (F0–F2)

- **F0 (source):** vesagfx takes `vi->FrameBuffer`, PCI fallback only when
  NULL; identity map is PWT=0 PCD=0 2M pages (→ PAT[0]=WB → MTRRs decide);
  no MTRR/PAT writes anywhere in `arch/`, `rom/`, `workbench/hidds/`; v11
  `MapPCI` is an identity pass-through while v1 remaps past 128 GiB with
  `MAP_CacheInhibit`; `Supervisor()` is `int $0xFE`; `GetBootInfo(BL_Video)`
  yields `struct VesaInfo` (size in KBytes).
- **F1 (probe):** `MEMTYPE` reads CPUID, every variable MTRR n < VCNT, fixed
  MTRRs iff FIX, PAT, bootloader FB, a 3-point page walk, and prints one
  `FB effective=` line via the tested decoders (`t177`, every §0.3 row, plus
  a PCD/PWT-swap mutant that FAILs). Dell MTRRs: RAM WB to `0xCC000000`,
  one UC carve-out, RAM above 4 GiB; FB matches nothing → default UC;
  PAT byte-exact power-on; index 0 at base/mid/end.
- **F2:** baseline above; verdict stated, no further measurement needed.

## F3: the trial that did not run (stop rule, not a failure)

- Every Method-A precondition held: two free MTRRs (8, 9 V=0), PCD=0, no UC
  overlap, `WC`=1, and one 64 MiB WC range `[0xD0000000, 0xD4000000)` would
  cover the whole framebuffer.
- `CPUCOUNT` (`KrnGetCPUCount`, kernel.resource LVO 40 from the generated v11
  headers, owned `KernelBase` symbol like `BootLoaderBase`): QEMU control
  `n=2`, Dell **`n=4`**. The phase requires identical MTRRs on every CPU.
- No user-mode vehicle exists: the IPI LVO is reserved (`kernel.conf:92`),
  `core_DoCallIPI` is kernel-internal, and `KrnScheduleCPU` only reschedules
  other CPUs. So per the phase's own rule the trial is NOT DONE — no trial
  binary built, no write executed anywhere. A WC change needs kernel-side
  code or a patched AROS (owner calls).

## Lane lessons (each cost a cycle or a reboot)

- **`Supervisor()` with an `iret` worker hangs the QEMU agent** — every
  binary calling it wedged the exec with no output and dropped the guest
  agent (4 cycles, 2 reboots; kernel entry/exit quad counts verified
  statically, handler registered — cause unpursued). **`SuperState()` /
  `UserState()` on the same guest reads MTRRCAP and CR3 cleanly**, so all
  privileged work runs inside one SuperState window (integer asm, no calls,
  verified XMM-free). The `int $0xFE` mechanism is fine; the
  `core_Supervisor` jmp-to-worker return path is what never comes back.
- **`startup.o` leaves `DOSBase` NULL: open `dos.library` manually**
  (open/close, `probe_ahi.c` shape) or the first `Printf` faults with no
  Guru and wedges the exec. Found by bisecting against known-good probe_ahi.
- **`/tmp` is wiped every host boot, and `/tmp/ri` is shared**: another
  session cleaned it mid-work. Rebuild lane binaries from the repo into a
  private dir; never stage irreplaceables under `/tmp/ri`.
- **A wedged agent's `jobs/*.json` must move aside before reboot**, or the
  fresh agent re-runs the poison job. Foreground exec + `>RAM:*.LOG` +
  `--get` for probe output; `delete` + `list RAM:` afterwards.
- **The Dell agent does not always reconnect after a bridge drop** — check
  for a session before submitting; the owner restarts it on the Dell.
- **Stale MTRR entries are real**: the Dell's MTRR8 holds a WC base with
  V=0 — decode must gate on V, not on nonzero content.
