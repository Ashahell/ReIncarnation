# Building an AHI probe for either ABI — verbatim compile and link transcript, 2026-10-03

**Ingested:** 2026-10-03 into ReIncarnation `llm-wiki`
**Source:** `audio_io/probe_rate.c`, written and built this session against both
the v1 build-pc SDK (`scripts/ri_build_aros.sh`) and the v11 toolchain
(`/home/miller/Work/vms/ri-p9/build_probe_v11.sh`).
**Provenance:** verbatim compiler and linker output, plus the header greps that
located each declaration. Nothing here is reconstructed.
**Recorded in:** [the AHI probe build contract](../articles/2026-10-03-the-ahi-probe-build-contract.md)

## 0. Why this exists

`probe_rate.c` is the second standalone AHI executable in this project. The first
is `probe_ahi.c`. Both are one translation unit that opens `ahi.device`, asks it
questions, and prints answers through `Printf`. Everything below is the cost of
writing the second one, and none of it is discoverable from the header — the
declarations are all present, which is exactly what makes each failure a
detour rather than a dead end.

## 1. Where `Printf` actually comes from

The first build:

```
./scripts/../audio_io/probe_rate.c: In function 'put':
./scripts/../audio_io/probe_rate.c:72:5: error: implicit declaration of function 'Printf'; did you mean 'SNPrintf'? [-Wimplicit-function-declaration]
   72 |     Printf("RI_RATE %s=%lu\n", (STRPTR) a, v);
      |     ^~~~~
./scripts/../audio_io/probe_rate.c: In function 'main':
./scripts/../audio_io/probe_rate.c:124:24: error: array size missing in 'alloc_tags'
  124 |         struct TagItem alloc_tags[];
      |                        ^~~~~~~~~
./scripts/../audio_io/probe_rate.c:160:13: error: implicit declaration of function 'AHI_FreeAudioA'; did you mean 'AHI_FreeAudio'? [-Wimplicit-function-declaration]
  160 |             AHI_FreeAudioA(actl);
      |             ^~~~~~~~~~~
```

Locating the declarations:

```
$ grep -rn "Printf" <v1 SDK include tree>
.../include/clib/dos_protos.h:39:LONG Printf (CONST_STRPTR format,  ...) __stackparm;
.../include/clib/dos_protos.h:40:LONG FPrintf (BPTR file, CONST_STRPTR format,  ...) __stackparm;
.../include/clib/utility_protos.h:23:LONG SNPrintf(STRPTR buffer, LONG buffer_size, CONST_STRPTR format, ...) __stackparm;
```

`Printf` is a **dos.library** function. `SNPrintf` is utility's — which is why
the compiler suggested the wrong one. The fix is `#include <proto/dos.h>`, which
is how `probe_ahi.c` gets it too, and there is no other header that would have.

Two more errors arrived in the same pass:

- **`struct TagItem alloc_tags[];` has no size.** The tag list is filled by
  index (`alloc_tags[0].ti_Tag = ...`), which an unsized array cannot accept.
  It needs an explicit compile-time size.
- **`AHI_FreeAudioA` does not exist.** The compiler named the real one:

```
$ grep -n "FreeAudio\|AllocAudio" <v1 SDK AHI header>
(no output for FreeAudio in the SDK AHI header)
```

The only free call in `probe_ahi.c` is `AHI_FreeAudio(actl)`.

## 2. `DOSBase` must not be defined when `proto/dos.h` is included

The next build, after adding the include:

```
AROS STUB BUILD OK
./scripts/../audio_io/probe_rate.c:62:17: error: conflicting types for 'DOSBase'; have 'struct Library *'
   62 | struct Library *DOSBase;
      |                 ^~~~~
In file included from ./scripts/../audio_io/probe_rate.c:56:
/home/miller/Work/projects/Vulkan4Aros/src/abi/v1/core-pc-x86_64/bin/pc-x86_64/AROS/Developer/include/proto/dos.h:20:31: note: previous declaration of 'DOSBase' with type 'struct DosLibrary *'
   20 |     extern struct DosLibrary *DOSBase;
      |                 ^~~~~~~~
```

`<proto/dos.h>` already declares it, and as a **different type**:

```
   20 |     extern struct DosLibrary *DOSBase;
```

The storage comes from `startup.o`, which is why the link supplies it. The rule
that falls out, and which `probe_ahi.c` already follows:

```
$ grep -n "DOSBase\|AHIBase\|struct Library" audio_io/probe_ahi.c
73:/* AHIBase/DOSBase are the extern globals declared by <proto/ahi.h> and
74: * <proto/dos.h>; the inline calls below resolve through them. SysBase and
75: * DOSBase are provided by startup.o; the AHI base is ours to define —
76: * taken from the P1 session request's io_Device (device-as-library),
77: * never via OpenLibrary (there is no LIBS:ahi.library by design). */
78:struct Library *AHIBase = NULL;
117:    DOSBase = (struct DosLibrary *) OpenLibrary("dos.library", 0);
```

**`AHIBase` is ours. `DOSBase` is not.** One is defined, the other only assigned.

## 3. The v11 toolchain has neither `libstdcio` nor `libposixc`

```
$ /home/miller/Work/vms/ri-p9/build_probe_v11.sh
/home/miller/Work/projects/Vulkan4Aros/src/abi/v11/toolchain-core-x86_64/x86_64-aros-ld: cannot find -lstdcio
/home/miller/Work/projects/Vulkan4Aros/src/abi/v11/toolchain-core-x86_64/x86_64-aros-ld: cannot find -lposixc
```

The v1 lane links `-lstdcio -lposixc` because its SDK ships them (behind the
`libcrt`/`libstdlib`/`libcrtprog` rename shim). The v11 link that works is
`-ldos -lexec` alone — consistent with `Printf` being a dos.library function,
so `-lstdcio` was never needed for this probe on any lane.

The v1→v11 runtime name difference is already recorded elsewhere:

```
| C runtime | `libstdc.a`, `libstdcio.a`, `libstdc_rel.a` | `libstdlib.a`, `libcrt.a`, `libcrtprog.a` |
```

## 4. The v11 SDK ships no `devices/ahi.h`

Handled by an explicit include of the source-tree header:

```
AHIH="$VK/src/abi/v11/AROS/workbench/devs/AHI/Include/C"
[ -f "$AHIH/devices/ahi.h" ] || { echo "FAIL: v11 ahi.h absent ($AHIH)"; exit 1; }
```

Already recorded in `2026-09-22-m1-1-real-hardware-abiv11-e6320.md`.

## 5. The AHI read tags used by the probe

```
#define AHIDB_Frequencies	(AHI_TagBase+115)
#define AHIDB_FrequencyArg	(AHI_TagBase+116)
#define AHIDB_Frequency		(AHI_TagBase+117)
#define AHIDB_MinMixFreq	(AHI_TagBase+112)	/* Min mixing freq. supported */
#define AHIDB_MaxMixFreq	(AHI_TagBase+113)	/* Max mixing freq. supported */
#define AHIDB_MaxChannels	(AHI_TagBase+111)	/* Max supported channels */
```

The three that make a rate queryable at all: `AHIDB_Frequencies` for how many,
`AHIDB_FrequencyArg` as the index, `AHIDB_Frequency` as the output.

## 6. Both lanes, final

```
$ ./scripts/ri_build_aros.sh
AROS STUB BUILD OK
AROS PROBE BUILD OK

$ /home/miller/Work/vms/ri-p9/build_probe_v11.sh
PROBE v11 BUILD OK (/tmp/opencode/probe_rate.v11, 21576 bytes, r12moves=24, und=0)

$ ./scripts/ri_audit.sh
AUDIT 0/0 PASS
```

## 7. ELF shape: type and size identify nothing

```
  probe_ahi    Advanced Micro | REL (Relocatable | 29840
  probe_rate   Advanced Micro | REL (Relocatable | 27856
```

Both "executables" are `REL (Relocatable file)` on this toolchain, and their
sizes differ by 1984 B for no structural reason. So `Type:` and size are useless
for telling whether a probe is sound — the discriminator is `r12moves` for the
lane, and running it for correctness. This agrees with the existing finding that
no size band separates v1 from v11 either.

## 8. The runtime half: `Run` is the wrong invocation

```
[exec] 'Run RAM:probe_rate' -> rc=0 (103 ms)
[exec] 'wait'                -> rc=0 (1122 ms)
[exec] 'RAM:probe_rate'      -> rc=0 (74 ms)
```

Only the third returned the probe's output. AROS `Run` spawns and returns.