# 2026-09-25 — Lane wedge root cause: e1000.device frees Tx buffers from the interrupt handler

> Source: code review of e1000.device (v1 + v11 AROS trees, Dell binary disassembly) + reproduction on a private QEMU e1000 lane (rie1k) + host A/B runs
> Collected: 2026-09-25
> Published: 2026-09-25
> Artifacts: Vulkan4Aros `artifacts/e1000-txfix-20260925/` (SHA256SUMS), patches `patches/e1000-tx-no-alloc-in-interrupt.v1.patch`, `.v11.patch`, `patches/e1000-dell-txpool/`

## Disposition
New + Disputed. Supersedes the "single ~786 KB Send()" theory
(2026-09-24-spike-lane-wedge-fix) and the m42 lesson "transfer size, not
driver"; disputes Vulkan4Aros `arostcp-outbound-wedge.md` ("lives in QEMU
8.2.2's e1000-family emulation").

## Root cause (proven)
`e1000.device` allocates a frame buffer with exec `AllocMem()` for EVERY
outgoing packet in its Tx soft interrupt (`e1000func_TX_Int`) and frees it
with `FreeMem()` from the hardware interrupt handler
(`e1000func_IntHandler` → `e1000func_clean_tx_irq` →
`e1000func_unmap_and_free_tx_resource`). exec's memory lists are guarded by
`MEM_LOCK` = `Forbid()` on non-SMP builds (`rom/exec/memory.h`), which does
not stop interrupts, so the handler's `FreeMem()` races task allocations and
corrupts the TLSF lists. rtl8139/pcnet32 never allocate in the packet path —
which is why the 2026-08-14 NIC matrix only ever saw e1000 wedge, and why the
upstream e1000 rebuild did not help (same code).

Same pattern in: the v1 tree driver (QEMU lanes), the v11 tree driver, and the
Dell's deployed `e1000.device 1.1 (21.7.2026)` e1000e port (disassembly:
AllocMem stub LVO 33 at TX_Int `0x4460`, FreeMem stub LVO 35 at
`0x992f`, reached from `e1000func_clean_tx_irq`).

## Reproduction (private lane rie1k: QEMU `-device e1000`, v1 live ISO, agent on 9297)
- Original driver, captures only (400x300 frames): 20/20 PASS (no wedge).
- Original driver + guest `memstress` (AllocMem/FreeMem hammer, pattern
  check) + capture + 845 KB bulk get per round: **wedged at round 14**.
  Serial log:

```
Software Failure!
IRQ : #43 - 0x0000000049BADF70
Error: 0x80000008 - Privilege violation error
...
Kickstart ELF Function tlsf_freevec + 0x0000000000000520
Kickstart ELF Function nommu_FreeMem + 0x00000000000001CE
Kickstart ELF Function Exec_35_FreeMem + 0x000000000000007C
e1000.device Function e1000func_clean_tx_irq + 0x00000000000000DA
e1000.device Function e1000func_IntHandler + 0x0000000000000139
```

  followed by a `core_IRQHandle(13)` storm (335 lines), no guru on screen,
  agent + ping dead — the exact lane-wedge signature.
- Fixed driver, same stress: **45/45 PASS**, 0 faults; memstress
  `allocs=1025202706 fails=0 corrupt=0` over 600 s.
- Fixed driver + new agent: 25/25 PASS, 0 faults.

## Fixes
1. **Driver source (v1 + v11 trees):** Tx frame buffers preallocated per
   descriptor at ring setup (task context), never freed in interrupt
   context; ring-full guard (no overrun of `next_to_clean`); a failed
   request no longer advances past an unwritten descriptor; statistics
   updated before `ReplyMsg()`; the interrupt handler re-`Cause()`s the Tx
   soft interrupt when it frees descriptors and writes are queued. v1 build
   compiles clean (`e1000.device` 150264 B). v11 build tree currently fails
   for an unrelated toolchain path reason.
2. **Dell driver (source of the 21.7.2026 e1000e port not on this machine):**
   binary patch — `ri_txpool.o` (1024 × 1536 B lock-free pool in `.bss`, no
   exec calls) linked into the original object; the two relocations
   redirected to `ri_txpool_alloc` / `ri_txpool_free`. Reproducible:
   `patches/e1000-dell-txpool/build.sh` checks the input sha256 `90f5dfbe…`
   and yields `b98be161…`. Not VM-tested: the v11 live ISO does not boot
   under QEMU here (unmodified ISO also black).
3. **Agent (`arostcp.c`):** a frame that cannot be sent completely now drops
   the connection and redials instead of answering the next job on a
   desynchronised stream (`g_send_broken`).

## Dell observations (before any change)
- 3 scale-2 captures PASS; TCP 10 data packets retransmitted of 1775 sent,
  31 duplicate acks.
- `netstat -i` net0: Ipkts 0 vs IP "3760 total packets received" (driver/stack
  input counter never updated; cosmetic).

## Deployment status
- **Dell E6320: DEPLOYED 2026-09-25 (owner-approved).** Original kept as
  `DEVS:networks/e1000.device.pre-txpool` (422448 B); installed
  `e1000.device` 423168 B, read-back sha256 `b98be1610dec9cc4…`. Remote
  `C:Reboot` 09:11; ping back 09:13:05, agent back 09:13:26 (session 22);
  `RAM:e1000.log` shows the 82579LM (`device_id 0x1502`) online. 10/10 scale-2
  captures PASS; TCP 20 data packets retransmitted of 5811 sent.
- **Agent on the Dell: DEPLOYED 2026-09-25.** New `SYS:ATCPBIN` 117952 B
  (v11 build of commit `b78bd3a4`, sha256 `a576a88f3b76c0b4…`, read back);
  previous kept as `SYS:ATCPBIN.pre-sendbroken` (117664 B), original
  `SYS:ATCPBIN.BAK` untouched. Reboot → agent back 10:13:37 (session 23);
  `Status` shows `SYS:ATCPBIN`; 5/5 scale-2 captures PASS.
- Rollback (at the Dell): `copy DEVS:networks/e1000.device.pre-txpool DEVS:networks/e1000.device` + reboot.
- `aros_v1j` (e1000, shared) not yet updated (busy with another session);
  riqemu1 uses rtl8139 (unaffected).
