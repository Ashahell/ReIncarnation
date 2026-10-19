# R12 N1d — where netmidid exits: named (2026-10-19)

## Withdrawn: the "r12 defect" claim

Owner was right to challenge it, and the challenge is **settled against my
earlier report**.

The rule I cited ("r12 is reserved on x86-64; a `mov %rax,%r12` around a call
corrupts the library base") is an **ABIv11** rule. Nothing showed it holds on
ABIv1, and it does not:

| ABIv1 tree-built binary | `mov %rax,%r12` | `call *%r12` | runs on riqemu1? |
|---|---|---|---|
| `camd.library` (the one that PASSED the SELFTEST) | **7** | **8** | yes |
| `C:Version` | **5** | **5** | yes |

So ABIv1 does **not** reserve r12 the way ABIv11 does, the tree compiler
legitimately emits these sites, and binaries carrying them run fine. The
tree-built `netmidid`'s **3 r12 sites are not a defect**, and the claim is
**retracted**.

The hand-built replacement (v11 `x86_64-aros-gcc` + **v1** SDK) has been
**deleted** — it mixes two toolchains, which is precisely what the owner warned
against, and it changed nothing about the failure anyway.

## Which ABI, and which toolchain built what

- **riqemu1 runs ABIv1**: `Kickstart 51.51, Workbench 40.0`, from
  `src/abi/v1` (ADT lineage), toolchain `src/abi/v1/toolchain-core-x86_64`.
- The `camd.library` that passed the SELFTEST was built by **v1 tree mmake
  with the v1 toolchain** — the same combination as the `netmidid` under test.
- Every later build must match that: **tree mmake + v1 toolchain**, not a
  hand-rolled compiler line.

## The instrumentation (local debug build, never upstream)

Per owner direction the markers went into **netmidid's own `main()`**, not a
separate hello — a hello would only prove the link line works.

`nm_dbg()` uses `printf` + `fflush` deliberately, **before** the `freopen`
block that sends stdout to `NIL:`. A hand-rolled `PutStr` LVO call was written
first and **discarded**: it needed a hardcoded LVO offset, and the project
skill forbids both guessing offsets and hand-rolling inline-asm LVO calls.

Run as `Run >RAM:nm.con RAM:NM_DBG2` — never `--run-script`, which waits on a
daemon and wedged the agent previously.

### Result — the exit is named

```
netmidid: M0 main entered
netmidid: M1 before getenv
netmidid: M1b after getenv
netmidid: M1c no log name -> will freopen NIL:
```

(stops here)

- **M0 appears** → `main()` **is** reached. The failure is NOT before `main()`.
- **M1b appears** → `getenv()` succeeds and returns.
- **M1c appears, M2a does not** → it dies **inside `freopen()`**.

It is the `freopen` **call**, not the path: it stops at the same place both
when `netmidid.log` is set (→ `RAM:NMLOG`) and when it is unset (→ `NIL:`).
Two different targets, same crash point.

## What this means

`netmidid` reaches `main()`, gets its environment, and dies in the first
`freopen` — before any of its own logic (config load, bsdsocket open, UDP
bind, camd attach) runs. That is why **nothing** prints, including the
deliberately-invalid-config case: execution never gets near the `fputs` that
would have reported it. Every observation in the previous report is explained.

`freopen` resolves through **posixc** (`__freopen_PosixCBase_libreq` /
`__freopen_PosixCBase_wrapper` weak wrappers needing `PosixCBase`). The
working `MIDISEND` uses only the AROS `Printf` inline and has no posixc-stdio
startup dependency — netmidid is the only artifact here that does.

This makes the earlier "posixc stdio startup" hypothesis **the confirmed
region**, though not yet the precise line inside posixc.

## Library versions (per owner step 1)

On the guest:

| library | version |
|---|---|
| `posixc.library` | 0.40 (09/23/26) |
| `stdcio.library` | 0.11 (09/15/26) |
| `stdc.library` | 0.49 (09/22/26) |

The binary libreqs exactly: `posixc.library`, `stdcio.library`,
`stdc.library`, `dos.library`, `bsdsocket.library`. Nothing is missing.

**But there is a real mismatch worth naming:** my scratch build tree
(`r12netmidi-v1-build`) carries **its own** SDK copy under
`bin/pc-x86_64/AROS/Developer`, separate from the guest's `LIBS:`. The
`posixc`/`stdcio` `.a` archives netmidid was compiled and linked against come
from that scratch SDK, not from the tree that produced the guest's installed
`posixc.library 0.40`. So "the version the binary asks for" and "the version on
the guest" are **not the same artifact**, which is exactly the
rule-out-the-caller risk — a posixc built for a *newer* ADT master calling into
the guest's *older* 0.40 is a plausible source of this crash.

I have **not** proven that is the cause. It is the next thing to check, and it
is checkable: build netmidid in the same tree that produced the guest's
`LIBS:` (the v1 `core-pc-x86_64` tree), or install the scratch SDK's posixc
alongside and see whether the crash moves.

## Gate status

**Gate NOT met.** `netmidid` starts, reads its config and idles: not achieved.
It now dies in a **named, single call** — `freopen()` in `main()` — which is a
real narrowing, but the gate itself is unchanged.

Also corrected and withdrawn: the earlier claim that the 3 r12 sites were "a
genuine defect". Retracted above with the evidence.