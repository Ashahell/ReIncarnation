# 2026-09-22 — ABIv1 full green: 1-byte HookEntry patch fixes Stop; Appendix A reproduced end-to-end

> Source: session evidence (guest result blocks, disassembly, serial markers), compiled by agent
> Collected: 2026-09-22
> Published: 2026-09-22

## Disposition
New. Closes the Stop-blocker item from the ABIv1-lane record and
reproduces Appendix A in full on ABIv1 (session 9, PASS, agent alive).

## TL;DR
The Stop kill-no-death was NEVER in void.audio (either build) — it is
the ahi.device shadow's hand `HookEntry` stub jumping to `h_Entry`
(+0x10, itself) instead of `h_SubEntry` (+0x18). One byte patched
(`ff 67 10` → `ff 67 18` at file offset 0x5ee5), disassembly matches the
documented fixed form exactly, full probe run goes green with
Appendix-A numbers.

## Root-cause chain (evidence, not theory)
- `objdump` of the deployed shadow (`ahi.device.new`, 12:14 build):
  `HookEntry` at 0x5ea0 contains `jmp *0x10(%rdi)`; NO `jmp *0x18`
  anywhere; 4 relocation sites install it as hook entries. Pre-fix,
  proven.
- The deployed void.audio (13:36) contains NO +0x10/+0x18 hook jump at
  all — its slave trampoline (`SlaveEntry` 0xd80) jumps via reloc to
  `Slave+0xda0` (correct). Void exonerated by disassembly on top of the
  behavioral proof (both void builds deadlocked identically → the
  common factor was always the device).
- No relocation targets the patched instruction bytes (verified via
  `.rela.ltext`), so the 1-byte edit is load-safe.

## Full green run (session 9, 12088 ms, rc=0)
Deploy set: patched `ahi.device` (225,353 B, 1 byte over shadow) via
`PROGDIR:` first-open (resident 0x4a3de720, devlist FOUND same node) +
patched sb128 (48984 B) + void shadow (12760 B) under redirected
`DEVS:` + canonical `probe_ahi_v1b` (29840 B).
```
RI_PROBE best_mode: id=0x001F0002
RI_PROBE alloc_audio: OK freq=5513 bits=32 stereo=1 hifi=1 maxch=128
RI_PROBE lowlevel frames=4096..64 all rc=0 (11/23/46/93/187/375/750 Hz)
RI_PROBE low_min_frames=64
RI_PROBE verify frames=64 window_s=5 observed=50577849 expected=3750 shortfall=-50574099
RI_PROBE device: unit 0 PRESENT
RI_PROBE device frames=4096..64 err1=0 err2=-2 (all rungs)
RI_PROBE dev_min_frames=0
RI_PROBE SUMMARY low_min_frames=64 dev_min_frames=0 verify_obs=50577849 verify_exp=3750 dev_open_rc=0
```
Appendix A line-for-line (VOID unclocked spin 50.6M in the 50–50.5M
family; r1 natural/r2 abort-clean; dev_min=0 by design).

## Standing
- The patch is a SCRATCH binary edit on a shadow device (load-bearing
  byte documented above). The source-faithful fix (rebuild with fixed
  `clib_stubs.c`, lost with `/tmp/ri/ahibuild/`) remains the durable
  path if the shadow ever needs rebuilding — the disassembly match is
  the bridge between them.
- ABIv1 acceptance evidence is now COMPLETE: P1/P2 (earlier session-8)
  + full P1–P4 here. G5 closure (spec OPEN-09 row + commit) is a
  separate, explicitly requested step — not taken here.

## Files
- `/home/miller/Work/ri_build/ahi.device.fixed` (225,353 B; pristine
  `/tmp/ri/aros/ahi.device.new` untouched).
- Quarantined stuck-job JSONs under `/home/miller/Work/ri_build/`.
- Guest left idle with the green set deployed (RAM: intact until next
  reboot); v1 serve on 9091 stays up.