# The AHI probe build contract: what a second AHI executable costs, and which mistakes look like different ones (2026-10-03)

- Source: ReIncarnation session, 2026-10-03 (opencode lane, `audio_io/probe_rate.c`, both ABIs)
- Collected: 2026-10-03
- Published: 2026-10-03
- Raw: [build transcript, verbatim](../evidence/2026-10-03-ahi-probe-build-transcript.md)
- Related: [the Dell hands back 44100 for every rate](2026-10-03-dell-ahi-hands-back-44100-for-every-rate.md), [M1.1 on real hardware, ABIv11](2026-09-22-m1-1-real-hardware-abiv11-e6320.md), [two silent traps on the Dell lane](2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md)

`probe_rate.c` is this project's **second** standalone AHI executable. The first
is `probe_ahi.c`. Both are one translation unit that opens `ahi.device`, asks it
questions, and prints the answers. This is what writing the second one cost.

It is worth recording because every failure below had a *plausible wrong
explanation attached*, and two of them are not AHI problems at all.

## `Printf` is dos.library's, and the compiler suggests the wrong one

```
audio_io/probe_rate.c:72:5: error: implicit declaration of function 'Printf'; did you mean 'SNPrintf'? [-Wimplicit-function-declaration]
```

`Printf` and `SNPrintf` live in different libraries:

```
.../include/clib/dos_protos.h:39:LONG Printf (CONST_STRPTR format,  ...) __stackparm;
.../include/clib/utility_protos.h:23:LONG SNPrintf(STRPTR buffer, LONG buffer_size, CONST_STRPTR format, ...) __stackparm;
```

The suggestion is utility's, and taking it would have been a second wrong turn.
The fix is `#include <proto/dos.h>`, and there is no other header that would have
supplied it. `probe_ahi.c` gets `Printf` the same way.

Two more errors rode along in the same pass, and both are compile-time
arithmetic rather than AHI:

- **`struct TagItem alloc_tags[];` — array size missing.** Tag lists handed to
  `AHI_AllocAudioA` are built by index, which an unsized array cannot accept.
  Needs an explicit size.
- **`AHI_FreeAudioA` does not exist.** The compiler named the real one,
  `AHI_FreeAudio`. It is the only free call in `probe_ahi.c` too.

## `DOSBase` must not be defined, and the type in the header is not the obvious one

```
audio_io/probe_rate.c:62:17: error: conflicting types for 'DOSBase'; have 'struct Library *'
   62 | struct Library *DOSBase;
In file included from audio_io/probe_rate.c:56:
.../include/proto/dos.h:20:31: note: previous declaration of 'DOSBase' with type 'struct DosLibrary *'
   20 |     extern struct DosLibrary *DOSBase;
```

`proto/dos.h` declares it, as `struct DosLibrary *` — **not** `struct Library *` —
and `startup.o` provides the storage. So the rule is:

> **`AHIBase` is ours to define. `DOSBase` is ours only to assign.**

```c
struct Library *AHIBase = NULL;                          /* ours */
DOSBase = (struct DosLibrary *) OpenLibrary("dos.library", 0);  /* assigned */
```

`probe_ahi.c` already documents this at the top of its globals block, which is
why it never hit the error. Worth noting the ordering trap: this error only
appeared *after* `proto/dos.h` was added to fix `Printf`. Including the header to
get one thing is what created a second problem.

## The two ABIs need different link lines, and the v11 one is shorter

```
$ /home/miller/Work/vms/ri-p9/build_probe_v11.sh
.../x86_64-aros-ld: cannot find -lstdcio
.../x86_64-aros-ld: cannot find -lposixc
```

| | v1 build-pc lane | v11 (Dell) |
|---|---|---|
| `libstdcio` / `libposixc` | present, behind a rename shim | **absent** |
| working link | `-lstdcio -lposixc -ldos -lexec` | **`-ldos -lexec`** |
| `devices/ahi.h` | in the SDK | **absent** — add `-I` to the source tree |

The probe never needed `-lstdcio` on either lane, because `Printf` is a
dos.library function. That is why the shorter v11 link is not a workaround but
the correct one.

The v11→v1 runtime rename difference is already recorded in
[the Dell's two silent traps](2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md):

```
| C runtime | `libstdc.a`, `libstdcio.a`, `libstdc_rel.a` | `libstdlib.a`, `libcrt.a`, `libcrtprog.a` |
```

## The three tags that make a rate queryable

```
#define AHIDB_Frequencies	(AHI_TagBase+115)
#define AHIDB_FrequencyArg	(AHI_TagBase+116)
#define AHIDB_Frequency		(AHI_TagBase+117)
```

A frequency query needs all three, and the middle one is the one that is easy to
miss: `AHIDB_Frequency` is the **output** slot, `AHIDB_FrequencyArg` is the
index into it. `AHIDB_MinMixFreq`/`AHIDB_MaxMixFreq` give the range and
`AHIDB_MaxChannels` the channel count, all from the same query.

## ELF type and size identify nothing

```
  probe_ahi    Advanced Micro | REL (Relocatable | 29840
  probe_rate   Advanced Micro | REL (Relocatable | 27856
```

Both "executables" are `REL` files on this toolchain, and their sizes differ by
1984 B for no structural reason. So neither `Type:` nor size tells you whether a
probe is sound — the discriminators are `r12moves` for the lane and *running it*
for correctness. This lines up with the existing finding that no size band
separates v1 from v11.

The new tool therefore asserts `r12moves > 0` rather than merely reporting it:

```
[ - "$R12" -gt 0 ] || { echo "FAIL: r12moves=0 -- this is a v1 binary, wrong lane for the Dell"; exit 1; }
```

`r12moves=24` on this build.

## A standalone probe needs its own build script

`build_v11.sh` is RIAPP-only: it hardcodes a 65-file application source list and
links Amiga/MUI/CyberGraphics. A probe needs none of that, and routing it through
that recipe would make every probe a full rebuild — and, worse, would leave the
probe's lane implicit in a script named for the application.

`build_probe_v11.sh` exists separately for that reason **and** because the
audit's build compiles the **v1** lane. A probe that builds green in the audit is
by construction the wrong binary for the Dell, which faults before its first log
line. The `r12moves > 0` assertion turns that silent failure into a build error.

## The runtime half: `Run` backgrounds the process

```
[exec] 'Run RAM:probe_rate' -> rc=0 (103 ms)
[exec] 'wait'                -> rc=0 (1122 ms)
[exec] 'RAM:probe_rate'      -> rc=0 (74 ms)
```

Only the third returned any output. AROS `Run` spawns and returns, so a child's
console output arrives after the agent has already read the channel — and its
`rc=0` means *"spawned"*, not *"ran and succeeded"*. A measurement probe that
reports through stdout has to be invoked in the foreground.

## Method

- **A compiler suggestion is a hint about a name, not about a library.**
  `did you mean 'SNPrintf'?` pointed at utility when the answer was dos.
- **One query, one output slot.** Two AHI queries writing through the same
  variable is how the first `CONVERTED_TO` reading came back as `192000` — the
  same class of mistake as an unsized tag array, at the other end of the
  pipeline.
- **Fixing one error can create the next.** `proto/dos.h` solved `Printf` and
  caused the `DOSBase` conflict; they were separated by a build, not by thought.
- **Assert the lane, do not report it.** A build that prints `r12moves` and
  continues is a build that will one day ship a binary to the wrong machine.

## See Also

- [the Dell hands back 44100 for every rate](2026-10-03-dell-ahi-hands-back-44100-for-every-rate.md) — what this probe was built to find
- [M1.1 on real hardware, ABIv11](2026-09-22-m1-1-real-hardware-abiv11-e6320.md) — the v11 include path for `ahi.h`
- [two silent traps on the Dell lane](2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md) — the v1/v11 runtime rename table
- [AHI on riqemu1: the loader ignores `sh_addralign`](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md) — why AHI is reachable on either lane at all