# 2026-09-21 — Third-party consultant analysis: HUNK alignment mechanism, SendIO gate, lddemon cache, P4 ladder

> Source: third-party consultant Q&A (via session operator), verbatim with formatting cleaned only
> Collected: 2026-09-21
> Published: Unknown

This is now much more interesting, because the evidence you have changes the diagnosis substantially. I would not treat P4 as an AHI/VOID problem yet. The strongest evidence points to two separate AROS x86-64 ABI/loader realities:

- SendIO() is almost certainly being entered with the right request pointer, but your interpretation of +0x28 matters enormously.
- The apparent 8-byte hunk alignment is not simply an AllocMem alignment failure. The AROS HUNK loader deliberately puts an 8-byte segment header in front of the HUNK payload. On x86-64 that makes the payload base + 8, which explains your exact mod16 = 8 observation.

I searched the current AROS sources/docs for the relevant loader, allocator, Exec I/O and AHI behavior. The conclusions below distinguish source-confirmed facts from inference from your traces.

## 1. Exec_77_SendIO: RSI is the IORequest, and +0x28 is io_Unit

This one is actually quite clean.

The AROS/Amiga SendIO() contract takes the IORequest * as its sole argument. The AROS documentation describes SendIO(IORequest) and says that it internally calls the device's BeginIO() entry point.

On 64-bit AROS, the relevant structure layout is effectively:

IORequest / IOStdReq

- +0x00 io_Message (Node, mn_ReplyPort, mn_Length)
- +0x20 io_Device
- +0x28 io_Unit
- +0x30 io_Command
- +0x32 io_Flags
- +0x33 io_Error

The documented structure confirms the ordering: `struct Message io_Message; struct Device *io_Device; struct Unit *io_Unit; UWORD io_Command; UBYTE io_Flags; BYTE io_Error;`

Therefore: at `mov 0x28(%rsi),%rax`, RSI is the IORequest *, and [rsi+0x28] is io_Unit. It is not r1/r2 by value at +0x28, io_Device, or io_Command.

Diagnostic value: if the trace before the fault shows RSI = 0x4a41eb50 and *(0x4a41eb50 + 0x28) = sane unit pointer, then the gate itself is not confusing io_Device and io_Unit. If RSI = 0x4a41eb50 and [rsi+0x28] = 0, that isn't necessarily bad either — io_Unit is initialized by OpenDevice(), and the AROS device documentation explicitly says Open() initializes io_Unit for later requests.

What to probe next: right before SendIO(r1): r1, r1+0x20 (io_Device), r1+0x28 (io_Unit), r1+0x30 (io_Command), r1+0x32, r1+0x33. Then at the faulting instruction: RIP, RSI, RSI+0x20, RSI+0x28, RSI+0x30. If RSI != r1/r2: smoking gun. If RSI == request but [RSI+0x28] faults: the request itself became invalid/corrupted between call and gate — a very different problem.

## 2. The HUNK alignment mystery: you found the explanation

This is the most important result from the source dive. Observation: HUNK allocation appears 16-byte aligned, but actual .rodata begins at mod16 = 8. That is exactly what the current AROS AOS HUNK loader does. The loader defines `GETHUNKPTR(x) ((UBYTE*)(BADDR(hunktab[x]) + sizeof(BPTR)))`, allocates `hunksize = lcount * 4 + sizeof(ULONG) + sizeof(BPTR)` via `ilsAllocVec(hunksize, req | MEMF_31BIT)`, stores `hunktab[i] = MKBADDR(hunkptr)`. So on x86-64: AllocVec result +0x00 segment bookkeeping, +0x08 actual HUNK payload. If AllocVec() = ...00 then HUNK payload = ...08. Exactly the observation.

This is NOT evidence that AllocMem only provides 8-byte alignment. Current AROS documentation says allocation is aligned to sizeof(struct MemChunk), and on 64-bit systems struct MemChunk is deliberately pointer-sized (sizeof(IPTR)). Distinction: allocator alignment ≠ HUNK payload alignment. The loader deliberately shifts the usable HUNK address.

## 3. Therefore: movaps from HUNK section data is indeed unsafe

HUNK allocation base mod16 = 0, HUNK payload mod16 = 8. A compiler/linker sees `.rodata ALIGN(16)` and can reasonably emit `movaps .rodata(%rip), %xmm0`, because within the ELF object the section really is 16-aligned. But the HUNK loader doesn't preserve that as absolute runtime alignment: payload = allocation + 8. So `.rodata`: expected ...00, actual ...08, and movaps faults. Matches sb128.audio and ahi.device (.rodata Align 16, #GP, 0F28→0F10 then success) almost perfectly.

Revised hypothesis (stronger): "The x86-64 AROS HUNK loader currently places the HUNK payload at AllocVec()+sizeof(BPTR), i.e. +8 bytes. Consequently, a 16-byte-aligned allocation produces an actual HUNK payload that is only 8-byte aligned. ELF section alignment is not preserved as an absolute runtime alignment guarantee. movaps/movdqa against HUNK data therefore cannot currently be assumed safe." (Not: "AllocMem only guarantees 8-byte alignment".)

## 4. This also explains why the Clang rebuild worked

Rebuilt mixer.o with `-fno-vectorize -fno-slp-vectorize` → 0 vector ops. That removes aligned vector loads/stores. The resulting ahi.device loads/opens and the DB scan completes — exactly expected if the fault was invalid alignment assumptions in generated module code rather than HUNK corruption. Causal chain demonstrated: HUNK data @ +8 → compiler assumes 16 → movaps/movdqa → #GP.

## 5. Do NOT call movaps "illegal in AROS modules"

Nuance: the instruction isn't illegal, and aligned SIMD accesses aren't categorically forbidden. The rule: an x86-64 HUNK module cannot currently assume arbitrary HUNK section data is 16-byte aligned merely because its ELF section declares sh_addralign = 16. So `movaps (%rax),%xmm0` / `movdqa (%rax),%xmm0` into ordinary HUNK data are dangerous; `movups`/`movdqu` are safe w.r.t. 16-byte alignment; or deliberately allocate an independently 16-byte-aligned runtime buffer.

## 6. Nasty secondary implication

Not just .rodata. Any compiler-generated code can make this assumption for: constants, vector globals, static tables, local arrays with alignment, constant pools, structure fields, SIMD temporaries, memcpy/memset expansions, auto-vectorized loops. So "No movaps/movdqa from HUNK memory" should probably become an x86-64 AROS module build rule until the loader is fixed.

## 7. The one-byte movaps → movups experiment is particularly valuable

movaps → #GP, movups → scan proceeds: exactly the experiment to prove alignment causality. Suggested follow-ups: (A) patch movaps→movups only; (B) at runtime print `&rodata_symbol` and `&rodata_symbol & 0xf`; (C) print HUNK allocation base: `BADDR(seg)`, `BADDR(seg) & 0xf`, `(BADDR(seg)+8) & 0xf` — expected 0 and 8. If confirmed, case closed.

## 8. ld.lld -r: section count is not the important thing

35 vs 20 ELF sections is not inherently a loader problem. The HUNK conversion (elf2hunk) iterates ELF section headers turning allocatable sections into HUNK code/data/BSS objects. Section count is a build artifact; HUNK structure/relocations/flags/layout are the runtime contract.

## 9. UND symbols: the HUNK path has no general dynamic resolver

elf2hunk explicitly handles SHN_UNDEF: for x86-64 relocation processing, an undefined symbol is accepted only when the relocation type is zero, otherwise "SHN_UNDEF symbol ... type ... unsupported". The converter does not generally turn arbitrary ELF undefined references into dynamic load-time resolution, and the AOS HUNK loader states HUNK_EXT not implemented. So "leave Exec symbols UND and expect load-time binding" is No — not under this HUNK loading path. Hand stubs are therefore required, not cosmetic.

## 10. Classify every stub before keeping all seven

Separate: (A) true external ABI dependencies, (B) libc conveniences, (C) linker-generated support, (D) functions eliminable by changing compilation. E.g. memcpy/memset/strlen/strcpy may be compiler lowering; HookEntry/NewList may be AROS/Exec ABI expectations. Classify each: symbol, origin object, relocation type, caller, why required, replacement.

## 11. Stale PC after UnLoadSeg: treat as UAF

AmigaDOS: UnLoadSeg() releases LoadSeg() segments to the system pool. AROS docs warn freed executable memory must not be executed; after FreeVec/UnLoadSeg it may disappear and execution faults. After UnLoadSeg(), the segment is no longer yours — "it still reads" (allocator/MMU state) doesn't make access valid.

## 12. hdaudio.audio DriverInit+0x125 after expunge is highly suspicious

First question: was the PC actually inside the released segment? Correlate PC against segment start/end immediately before UnLoadSeg(). Inside after unload = use-after-unload; outside = attribution problem or stale metadata.

## 13. AROS process cleanup does UnLoadSeg(me->pr_SegList[3])

Expected lifecycle: LoadSeg → execute/use → stop all users → UnLoadSeg → memory returned. No "keep executing because pages remain" phase.

## 14. devs: shadowing — put caching ahead of current-dir/homedir

DEVS: → RAM:Devs verified, yet LDLoad("ahi.device","devs") gets stock CD bytes while OpenDevice("PROGDIR:ahi.device") gets the shadow. Strong evidence against ordinary DOS CWD resolution (devs: is explicit, CWD should not win). More likely: lddemon object cache / load record / already-registered module / path canonicalization / resident lookup / pre-resolved loader object. The LDObjectNode hypothesis deserves serious attention.

## 15–17. Experiments to distinguish caching from path resolution

Don't change contents — change the NAME (e.g. RAM:Devs/ahi-shadow.device or ZZTEST.device with unique marker) and ask lddemon to load it via LDLoad(name,"devs"). If the shadow-by-new-name loads correctly, ahi.device is likely already represented in lddemon's object/load cache. If instrumenting lddemon, log: requested name, directory, canonical name, cache hit/miss, resolved pathname, LoadSeg argument, returned segment — one line deciding HIT vs MISS.

## 18–19. P4 double-buffering is conceptually valid — but wrong first test

AHI docs specify double buffering via request1 → SendIO, request2 → ahir_Link = request1, request2 → SendIO, wait, etc. CMD_WRITE is the raw audio-output command. But P4 tests SendIO + CMD_WRITE + AHIRequest + VOID + double buffer + ahir_Link + completion + reply port simultaneously — too many variables. Historical warning: some AHI implementations (Jazz² docs) accepted linked requests but never played/replied them. Treat ahir_Link as a separate compatibility test, not the fundamental CMD_WRITE proof.

## 20. Recommended P4 ladder

- P4.0 — Exec I/O sanity: already-open request, CMD_WRITE, ahir_Link = NULL, one SendIO + WaitIO. Don't care about audibility.
- P4.1 — second independent request: r1→SendIO→WaitIO, r2→SendIO→WaitIO (both NULL link).
- P4.2 — documented linking only then.

## 21–26. The crash occurs too early to blame VOID; fault tree

[PDBG] SendIO r1 done means Exec_77_SendIO returned once — distinguish SendIO(r1)→success then SendIO(r2)→fault. Dependency chain dies at "SendIO ABI", before VOID executes. Make P4's first diagnostic deliberately stupid (single request, NULL link, serial-dump r/device/unit/cmd, SendIO, "returned", WaitIO, "returned"). Specifically check r1->io_Unit after OpenDevice, and whether r2 was built by copying r1 (AHI docs recommend copy; memset+partial-memcpy second requests may be invalid). Ranked fault tree: H1 request register/field corruption (high), H2 malformed second request (high), H3 SendIO gate bug (medium/high — jumps up if RSI != r1/r2), H4 stale io_Unit (medium), H5 VOID bug (low for current crash), H6 ahir_Link (low now, relevant later).

## 27–30. Formalize the loader finding; architectural question

Document: allocation mod16=0, payload mod16=8, source GETHUNKPTR + sizeof(BPTR); consequence (ELF alignment not preserved); workaround (no-vectorize flags / unaligned SIMD); long-term options (A: toolchain contract never assuming >8-byte HUNK alignment; B: linker/converter padding; C: loader allocating so payload is 16-aligned — needs care, don't casually patch). Note the loader comment: "On 64bit systems, Hunk segments are ALWAYS allocated in the 32bit address space using MEMF_31BIT" — a deliberate compatibility decision, so this affects every x86-64 HUNK module with modern compiler assumptions, not one driver. The ReadConfig/sb128/Mixer/SendIO crashes may share this broader ABI root plus a separate SendIO issue.

## 31. Ordered next steps

Exp-1: prove SendIO register identity (RSI vs r1/r2 + fields, no code changes). Exp-2/3/4: P4 ladder (single → independent → linked). Exp-5: prove HUNK offset (base & 0xf). Exp-6: ZZTEST.device load path. Exp-7: stale PC vs segment range. Bottom line: (1) RSI=IORequest*, +0x28=io_Unit; (2) HUNK mechanism confirmed as loader +8; (3) DEVS: shadow → suspect object cache; (4) section count irrelevant, UND needs stubs; (5) UnLoadSeg = dead; (6) P4 valid but ladder it — current crash is in SendIO, VOID not convicted.
