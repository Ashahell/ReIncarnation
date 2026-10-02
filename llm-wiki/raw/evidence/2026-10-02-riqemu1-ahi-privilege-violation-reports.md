# AROS crash reports from riqemu1 — two privilege violations from misaligned `movaps`

**Ingested:** 2026-10-02 into ReIncarnation `llm-wiki`
**Source:** AROS guest requester output on riqemu1 (`start_riqemu1.sh`, ABI v1 lane),
operator-supplied 2026-10-02, captured as two screenshots and transcribed here.
**Provenance:** transcribed from the operator-supplied report images; values are as read
from those reports. Where a field was ambiguous in the source image it is marked `[?]`.
This file exists so that the compiled article's literals are grounded in a durable
artefact rather than only in a conversation.
**Recorded in:** [the riqemu1 Software Failure record](../articles/2026-10-02-riqemu1-software-failure-is-two-faults-wrong-abi-fixed-and-misaligned-movaps.md)

## Report 1 — faulting module `sb128.audio`

Taken with all six audio modes present and the guest running the v1-built RIAPP.

```
Software Failure!

Program failed
Task : 0x000000004A3E40F0 - RIAPP render
Error: 0x00000008 - Privilege violation error
PC   : 0x000000004BFA8DE5
Module sb128.audio Segment 4 _text (0x00000004BFA8720) Offset 0x00000000000006C5
Function DriverInit (0x00000004BFA8CB0) Offset 0x0000000000000135

CPU context:
RAX=000000004BFC1730 RBX=000000004BF504E0 RCX=0000000000000000 RDX=000000004BFA867C
RSI=000000004BFAE4B8 RDI=000000004BFC1720 RSP=0000004BF4A5B0 RBP=000000004BF4A600
R8=000000004BF4A538 R9=00000000102A8B0 R10=0000000000000004 R11=0000000000000004
R12=000000004BFAE318 R13=0000000001AB0000 R14=000000004BFAE000 R15=000000004BFAEAA8
RIP=000000004BFA8DE5 RSP=0000004BF4A5B0 RFLAGS=000000000013202
CS=002B SS=0023 DS=0023 ES=0023 FS=0023 GS=0030

Disassembly:

00000004bfa8cb0 <DriverInit>:
<skipping 297 bytes>

00000004bfa8d9:
-12: 8b0e                - mov    (rsi), ecx
-10: 48ba7c86fab0000    - mov    $0x4bfab87c, rdx
 0: 0f2802              - movaps (%rdx), xmm0
 3: 0f1f00              - movaps xmm0, (%rax)
 6: 8d4104              - lea    0x4(%rcx), eax
 9: 8906                - mov    eax, (rsi)
11: c783c801000000      - mov    $0x0, 0x1c8(%rbx)

Stack trace:
0x000000004BFA89D sb128.audio _LibInit_ + 0x000000000000000D
0x000000001AA55A9 Kicstart ELF _Init_ + 0x0000000000000139
0x000000001D25B4 ld demon.resource Segment 5 .text + 0x0000000000000474
0x000000001D257F8 ld demon.resource Function Ld demon_O_OpenLibrary + 0x0000000000000038
0x000000004BFA5B93 sb128.audio Segment 4 .text + 0x00000000000000A3
0x000000004FB5B49 ahi.device __AHI_LoadModeFile + 0x00000000000000E9
0x000000004FB53723 ahi.device __DevOpen + 0x00000000000000F3
```

## Report 2 — faulting module `ahi.device`

Taken after quarantining the five non-AC97 drivers in `DEVS:AHI/` and running the
repo's own `audio_io/probe_ahi.c` (which contains no ReIncarnation code).

```
Software Failure!

Program failed
Task : 0x000000004B27D420 - RAM:probe_ahi
Error: 0x00000008 - Privilege violation error
PC   : 0x000000004B4F6EB6
Module ahi.device Segment 4 .text (0x0000004B4F5E20) Offset 0x000000000000104F
Function ReadConfig (0x0000004B4F6EE0) Offset 0x000000000000006F

CPU context:
RAX=000000004B4F5D8C RBX=0000004B28B640 RCX=0000004B287C50 RDX=0000000008000064
RSI=000000004B28B640 RDI=0000000000000009 RSP=0000004B4F6E60 RBP=0000004B4F7B0
R8=0000000000000009 R9=00000000102A8B0 R10=0000000000000004 R11=0000000000000004
R12=000000000000000F R13=0000000000000000 R14=0000000000000000 R15=0000004B28B740
RIP=0000004B4F6EB6 RSP=0000004B4F6E60 RFLAGS=000000000013246
CS=002B SS=0023 DS=0023 ES=0023 FS=0023 GS=0030

Disassembly:

00000004b4f6ee0 <ReadConfig>:
<skipping 99 bytes>

00000004b4f6e63:
-12: eb29                - jmp    ReadConfig+1
-10: 48b88c5d44fb0000    - mov    $0x4bf4d88c, rax
 0: 0f2800              - movaps (rax), xmm0
 3: 0f1183e801000000    - movaps xmm0, 0x1e8(%rbx)
10: 48c783f801000000    - mov    $0x1, 0x1f8(%rbx)

Stack trace:
0x000000004BF66DF ahi.device Function __DevOpen + 0x000000000000017F
0x000000001AA6290 Kicstart ELF _Init_ + 0x0000000000000080
0x000000001D25C0 ldemon.resource Function Ldemon_O_DevOpen + 0x0000000000000060
0x000000004B52073 probe_ahi Function main + 0x0000000000000113
0x000000004B22C4D probe_ahi Function __startup_main + 0x000000000000004D
0x000000004B3CD6 probe_ahi Function __initcormain + 0x0000000000000406
0x000000004B41D6 probe_ahi Function __startup_init + 0x00000000000000A6
0x000000004B40C8 probe_ahi Function __startup_fromwb + 0x00000000000000A8
0x000000004B2BD8 probe_ahi Function entry_body + 0x00000000000000ED
```

## The two literals the diagnosis rests on

Both faults are the same instruction, and both source addresses are **not 16-byte
aligned** while `movaps` requires 16-byte alignment:

| report | instruction | address | ends in |
|---|---|---|---|
| 1 | `mov $0x4bfab87c, %rdx` then `movaps (%rdx), %xmm0` | `0x4bfab87c` | `c` |
| 2 | `mov $0x4bf4d88c, %rax` then `movaps (%rax), %xmm0` | `0x4bf4d88c` | `c` |

`0x...c` is 12 mod 16, so both are misaligned by 4 bytes. On x86-64 a misaligned
`movaps` raises **#GP**, which is error code `0x00000008` — the error both reports
carry. The faulting module changes between reports (`sb128.audio` → `ahi.device`)
because the second was taken with the drivers quarantined, but the mechanism does
not: **this image's AROS binaries contain misaligned SSE loads.**