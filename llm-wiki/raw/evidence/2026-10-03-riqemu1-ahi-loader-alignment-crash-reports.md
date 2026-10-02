# riqemu1 AHI crash reports — three faults, one loader bug (2026-10-03)

**Ingested:** 2026-10-03 into ReIncarnation `llm-wiki`
**Source:** AROS guest requester output on riqemu1 (`start_riqemu1.sh`), operator-supplied
2026-10-03, transcribed from report images.
**Provenance:** transcribed from the operator-supplied report images; values as read.
Marked `[?]` where the source image was ambiguous.
**Recorded in:** [AHI on riqemu1: the ELF loader ignores sh_addralign](../articles/2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md)

## Report 1 — `ahi.device ReadConfig`, the original fault

```
Software Failure!
Program failed
Task: 0x000000004B27D420 - RAM:probe_ahi
Error: 0x00000008 - Privilege violation error
PC   : 0x000000004B4F6EB6
Module ahi.device Segment 4 .text (0x0000004B4F5E20) Offset 0x000000000000104F
Function ReadConfig (0x0000004B4F6EE0) Offset 0x000000000000006F

Disassembly:
00000004b4f6ee0 <ReadConfig>:
 -12: eb29                - jmp    ReadConfig+1
 -10: 48b88c5d44fb0000    - mov    $0x4bf4d88c, rax
  0: 0f2800              - movaps (%rax),%xmm0
  3: 0f1183e8010000      - movaps xmm0, 0x1e8(%rbx)
 10: 48c783f801000000    - mov    $0x1, 0x1f8(%rbx)
```

## Report 2 — `probe_ahi`, NULL call at the session open (after the alignment patch)

```
Software Failure!
Program failed
Task: 0x00000004A6A62F0 - RAM:probe_ahi
Error: 0x00000003 - Illegal address access
PC   : 0x0000000000000000
RIP=0000000000000000 RFLAGS=000000000013246

Disassembly:
0x0000000000000000 ------------ <illegal address>
```

Cause established separately: a stale `NVHDMI.off` mode file left in
`DEVS:AudioModes/`, which `ahi.device` still scanned because it was resident and
held the file open, so the file could not be deleted until after a reboot.

## Report 3 — `ac97.audio Mix()`, the second alignment fault

```
Software Failure!
Program failed
Task: 0x000000004B4C959D - ac97.audio
Error: 0x00000008 - Privilege violation error
PC   : 0x000000004B554542
Module ac97.audio Segment 4 .text (0x000000004B547410) Offset 0x0000000000000132
Function Mix (0x00000004B5544D0) Offset 0x0000000000000072

Disassembly:
 -20: 0f1000              - movups (%rax),%xmm0
 -17: 0f1385 00ffffff     - movups %xmm0,-0x90(%rbp)
 -10: 48b84c7344b4000000  - mov    $0xb444734c,%rax
  0: 660f6f00             - movdqa (%rax),%xmm0
  4: 660f7f8550ffffff     - movdqa %xmm0,0xb0(%rbp)

Stack trace:
 ahi.device Function MixerFunc + 0x2D
 ac97.audio Function Slave + 0x1A9
 dos.library CallEntry
 dos.library TaskExitStub
 Kicstart
```

`0xb444734c` mod 16 == 12, the same misalignment as report 1.

## The two constants the diagnosis rests on

| report | module | constant | mod 16 | section offset |
|---|---|---|---|---|
| 1 | `ahi.device` | `0x4bf4d88c` | **12** | `.rodata+0x22a0` |
| 3 | `ac97.audio` | `0xb444734c` | **12** | — |

Report 1's arithmetic closes exactly, and it is what identifies the mechanism:

```
0x4bf4d88c  (observed constant)  -  0x22a0  =  0x4bf4b5ec   <- .rodata load base
0x4bf4b5ec mod 16 = 12           but .rodata sh_addralign = 16
```

`0x22a0` is 16-byte aligned in the file, so the compiler was right to emit
`movaps`. The loader put the section 4 bytes out.
