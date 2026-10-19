# R12 N1c — netmidid on riqemu1: rc=20 isolated, cause not yet proven (2026-10-19)

## Correction accepted

**AROSTCP is running on riqemu1** — owner is right, and my `NET:` inference was
wrong. The agent holds a live TCP session to the host (guest connections appear
as localhost under slirp) and the agent *is* a bsdsocket client, so the stack is
up. A missing `NET:` volume does not imply a missing AROSTCP; `NET:` is just not
where this lane mounts it. I should not have promoted that hypothesis to a
cause. `rtpmidid` is likewise installed and running (PID 1891035, 5004/5005,
ALSA 128:0 [In/Out]) — the earlier premise that it was absent was stale.

## Step 1 — the toolchain trap: REAL, and fixed, but not the cause

Per owner direction I checked the ABIv1 `netmidid` first.

| binary | built by | `mov %rax,%r12` | unresolved |
|---|---|---|---|
| original ABIv1 `netmidid` (deployed) | AROS **tree mmake** (v1 clang) | **3** | 0 |
| my rebuilt `netmidid` | v11 `x86_64-aros-gcc` + `-ffixed-r12` | **0** | 0 |
| `MIDISEND` (known good, repo recipe) | v11 gcc | 0 | 0 |
| `MIDISEND` (clang, silent no-op) | v1 clang | 3 | 0 |

So the tree build inherits the same clang r12 behaviour as the broken MIDISEND,
and the tree-built `netmidid` **did** carry 3 r12 sites. Rebuilt with the recipe
that works — v11 `x86_64-aros-gcc` (GCC 16.1.0), `-ffixed-r12`, v1 SDK for the
riqemu1 ABI, all 4 service TUs plus the 9 `rtpmidi` TUs at **0 r12 sites**, then
linked with `-Wl,--start-group -lstdcio -lposixc -Wl,--end-group -lautoinit`
(the plain ordering leaves `__aros_getbase_StdCIOBase` unresolved because
`stdcio_getlibbase.o` is only pulled in on a second pass).

Rebuilt binary: **154344 B**, sha256 `8ed7f8f4…`, both repo gates clean.
Deployed and hash-verified on the guest.

**But the rebuild did not change the behaviour.** Both the 3-r12 tree build and
the 0-r12 rebuild return **rc=20** with no output. So the r12 sites were a real
defect worth fixing, but they are not what makes netmidid exit here.

## Step 2 — rc obtained, and it is netmidid's own

`run_script` returns an unreliable rc (a **nonexistent** absolute path also
returned `rc=0`), so I used it only comparatively. With a **known-good**
control through the identical path:

| command | rc | output |
|---|---|---|
| `MIDISEND r12ctl SELFTEST` (known good) | **rc=0** | 799 B |
| `ThisCommandDoesNotExist` | rc=0 | — |
| `RAM:NM_NEW` (rebuilt netmidid) | **rc=20** | 0 B |
| `SYS:System/Network/Services/netmidid` (tree build) | **rc=20** | 0 B |

MIDISEND returning rc=0 *with* output through the same path proves the harness
works, so **rc=20 is netmidid's own return code**, not a shell or timeout
artifact.

## Step 3 — why "zero output" is NOT the original mystery

`main.c:1157` explains it — the program discards its own streams by design:

```c
/* Started by the stack's service manager: no console, no arguments.
   What it would print goes nowhere, or to the file ENV:netmidid.log
   names, for whoever wants to see it. */
const char *log = getenv("netmidid.log");
if (!log || !*log || !freopen(log, "w", stdout))
    (void)freopen("NIL:", "w", stdout);
(void)freopen("NIL:", "w", stderr);
```

So **zero output is the correct, designed behaviour** and was never evidence of
a fault. Setting `netmidid.log` is the supported way to capture it. I set it to
`RAM:NMLOG`, `RAM:netmidid.log` and `ENV:netmidid.log` and the log stayed **0 B**
in every case — which is now the real signal: netmidid exits before reaching any
`fputs`, i.e. before its first diagnostic.

I also confirmed the config is **not** the cause. `ENV:NetMIDI` is populated
from `ENVARC:NetMIDI` (`ENV:` is RAM-backed and was wiped by the earlier reset),
and holds the right four values:

```
local_port 5004
peer_ip 10.0.2.2
peer_port 5004
accept 10.0.2.2
```

And a deliberately invalid `peer_ip=NOTANIP` — which must hit the
`peer must be an IPv4 address` / `invalid network preferences` prints — also
produced **0 B**. So the failure precedes config parsing output as well.

## Silent exit paths in `run()` — the candidates

Every early `return 20` in `main.c` is preceded by an `fputs` **except** one
shape: the paths that `goto cleanup` after a helper returns non-zero print
their own message *before* the goto, so they too would produce output. With
0 B on **all** paths, including a deliberately broken config, the exit is
happening **before `run()`'s first print** — most likely in CRT/libc startup,
not in netmidid's own logic.

Supporting evidence: the rebuilt `netmidid` resolves its stdio through
**posixc** weak wrappers that need `PosixCBase`
(`__printf_PosixCBase_libreq`, `__fputs_PosixCBase_wrapper`, `__getenv...`),
whereas the working `MIDISEND` uses only the AROS `Printf` inline
(`__inline_Dos_Printf`) and has no such dependency. netmidid is the only
artifact here that needs posixc stdio initialised at program start.

**This is a hypothesis, not a conclusion.** I have not proven where it exits,
and I am not going to report it as the cause.

## Also hit (my error)

Running netmidid through `--run-script` **wedged the agent**, because
`run_script` waits for the script and netmidid idles forever when it works.
Recovered with `system_reset` (same PID 1933951, 1280x1024 re-verified).
Lesson: never drive a long-running daemon through `run_script`.

## Gate status

**Gate NOT met.** `netmidid` starts, reads its config and idles: **not
achieved** — it exits rc=20 before printing anything, in both the tree build
and the 0-r12 rebuild.

What is established, and worth keeping:

- r12 defect in the tree-built netmidid: **real, fixed** (0 sites, gates clean),
  but not the cause here.
- rc=20 is netmidid's own, proven against a known-good control.
- "zero output" is designed behaviour (`freopen` to `NIL:`), not a symptom.
- Config is correct in both `ENV:` and `ENVARC:`.
- The unexplained part: netmidid exits before its first diagnostic, with no
  output even for a deliberately invalid config. posixc-stdio startup is the
  leading suspect and is **unverified**.