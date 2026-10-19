# R12 N1b — riqemu1 after the camd reboot (2026-10-19)

## Reset method

`system_reset` through the QEMU monitor on port 4477, **not** quit-and-relaunch,
per owner direction. QEMU kept PID **1933951** across the reset, so the machine
config is unchanged. (A `aros_v1smp` lane also appeared during this window; it
is a different lane and was not touched.)

Resolution verified **before anything else** after the reset:
`screendump` → **1280x1024**, 99.6% non-black (live Workbench). The lane rule is
satisfied.

## camd.library — hash-verified after the reboot

Copied `RAM:CAMDLIB` → `LIBS:camd.library`, then read the **booted** file back
off the guest and compared:

```
host   baca61a17193d06647657ab671ef086712c7066ca66b0aabe58d0e7c8afd152a
guest  baca61a17193d06647657ab671ef086712c7066ca66b0aabe58d0e7c8afd152a
```

53528 B, `$VER: camd.library 41.1 (19.10.2026)`. The old one was 53472 B
(`d4a7f97d…`), backed up to `~/.worktrees/r12-backup/camd.library.pre1585`
before the swap, so the change is reversible.

## Both fixes confirmed — #1585 AND #1676

The owner's point was right that naming only #1585 was incomplete. Both are
now verified, by **behaviour** on the booted system, not by claiming a build
flag carried them.

### #1585 (aligned `MidiDeviceData` scan) — CONFIRMED

`MIDISEND <app> SELFTEST` on the booted system:

```
SELFTEST connected: early sender 1, late sender 1
SELFTEST rx err 0
SELFTEST cluster 0 name applemidi.in.0
SELFTEST cluster 1 name applemidi.out.0
...
SELFTEST rx node ... link ... sig 16 signalled 1 got 2
```

`connected: 1, 1` with 32 named applemidi clusters is only possible if camd
**found the driver's `MidiDeviceData`** — which is precisely what #1585 makes
possible on 64-bit. Static corroboration: the new binary has the
`AROS_ROUNDUP2` idiom (`lea 0x7(%r14),%eax`) in
`Camd_34_OpenMidiDevice`; the pre-#1585 build has none.

### #1676 (expunge returns success) — CONFIRMED

#1676's symptom is "blocks at the second open after a flush". Ran the SELFTEST
twice back to back — the second open is exactly the failing case:

```
run 1: connected: 1, 1 ... got 2
run 2: connected: 1, 1 ... got 2
```

Second open succeeds, so expunge cleaned up and returned success.

Static corroboration, and this is the part that needed care: the registered
expunge **callback** differs between builds. In
`~/.worktrees/r12-backup/camd.library.pre1585` the `__EXPUNGELIB_LIST__`
entry relocates to the symbol `UninitCamd`; in the new build it relocates to a
`.ltext+0x5a70` local — the `Expunge` wrapper. Source confirms what it is
(`workbench/libs/camd/camd_init.c`):

```c
/* Expunge callbacks return success after releasing their resources. */
static int Expunge(struct CamdBase *CamdBase)
{ UninitCamd(CamdBase); return TRUE; }
ADD2EXPUNGELIB(Expunge, 0);      /* was: ADD2EXPUNGELIB(UninitCamd, 0); */
```

Note the honest limit: `strings` shows **no** `Expunge` symbol in either
binary and the callback *name* is loader-resolved, so a string comparison
cannot distinguish them. I also could not cleanly disassemble the `.ltext+0x5a70`
addend (it lands inside `GetMidiLinkFromOwnerNode`, so the addend is not a
function entry in this layout). **The behavioural test is the load-bearing
evidence for #1676**; the relocation difference corroborates but does not by
itself prove it.

## Peerless checks — both requested, one PASS one BLOCKED

### 1. CAMD lists the applemidi ports — **PASS**

Needed a rebuilt probe: the shipped one capped enumeration at 12 clusters and
printed only 4 name bytes, so the applemidi ports were invisible. Raised the
cap to 40 and print the full name (`app/midisend.c`). Result on the guest:

- **16** `applemidi.in.N` (N=0..15)
- **16** `applemidi.out.N` (N=0..15)
- plus the pre-existing `debugdriver.in.N` / `debugdriver.out.N`

Exactly the cluster set the plan depends on: `RIAPP_MIDI_IN=applemidi.out.0`
is a real cluster.

### 2. netmidid starts, reads `ENV:NetMIDI`, idles cleanly — **NOT REACHED**

`SYS:System/Network/Services/netmidid` runs and exits immediately with **zero
output**, foreground or detached. Not a crash signature (no guru, machine
stays up), and it is not a config rejection — the source prints
`invalid network preferences` and returns 20 on a bad file, which would have
produced output.

`ENV:NetMIDI` had to be rewritten in **`ENVARC:`**, because `ENV:` on this
lane is RAM-backed and the reset wiped it. `main.c` reads `ENV:NetMIDI` first
and falls back to `ENVARC:NetMIDI`, so `ENVARC:` is the correct persistent home:

```
local_port 5004
peer_ip 10.0.2.2
peer_port 5004
accept 10.0.2.2
```

**Suspected cause, not yet proven: `NET:` does not exist on riqemu1.** A
`list NET:` probe raised a blocking `Please insert volume "NET:" in any drive`
requester, which is the DOS `Lock()` requester the `aros-development` skill
documents; dismissed with `sendkey esc` through the monitor, after which the
agent answered again. `NET:` is AROSTCP's volume, so AROSTCP is very likely
not running — and `bsdsocket.library` only exists while AROSTCP runs, so
netmidid would fail at `OpenLibrary("bsdsocket.library")` and exit before
printing. That is a **hypothesis**, not a measurement; I have not yet confirmed
whether AROSTCP is up, and I should not report it as fact.

### Correction to the owner's premise

`rtpmidid` **is** on the host and **is** running (PID 1891035, UDP 5004/5005,
ALSA port `128:0 ReIncarnation-peer [In/Out]`). It was installed earlier in this
session with owner approval. So the stated blocker "rtpmidid isn't on the host"
does not hold. That does not unblock the gate — the guest side is what is
missing — but the premise should not be relied on.

## A toolchain trap worth recording

My first MIDISEND rebuild used the **clang** cross-compiler
(`toolchain-core-x86_64/bin/clang`). It linked cleanly, had **0 unresolved
symbols**, and passed a loose r12 check — but produced **3** occurrences of the
`mov %rax,%r12` pattern that the repo's own gate requires to be **0**, and it
produced **zero output** on the guest. Both the v1 `clang` and the v1
`x86_64-aros-cc` (which is clang) produce the same 3 — including from
**unmodified** `midisend.c` (verified by stashing my edit and rebuilding), so
it is not my change. The repo's own recipe
(`AROS_ABI=v11 bash scripts/ri_build_aros.sh sections`) uses the **v11**
`x86_64-aros-gcc`, which accepts `-ffixed-r12`; that build gives **0**, and
**that** is the binary that runs correctly on the guest.

Lesson: for AROS guest binaries, build with the repo's recipe
(`scripts/ri_build_aros.sh`) rather than hand-rolling a compiler line. The
`x86_64-aros-cc` in the v1 tree is clang and silently lacks the r12 treatment;
the gate catches it only if you actually run the gate.

## Tool change made

`app/midisend.c`: SELFTEST cluster cap 12 → 40, and print the full cluster
name instead of 4 hex bytes. Needed to see the applemidi ports at all. This is
a diagnostic tool change, not product code, and is gated by the audit.