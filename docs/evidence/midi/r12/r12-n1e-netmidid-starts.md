# R12 N1e — netmidid starts, reads its config and idles: GATE MET (2026-10-19)

## The root cause was mine, not the build's

`ENV:NetMIDI` on the guest had **spaces where `=` should be**:

```
0000000   l o c a l _ p o r t     5 0 0 4  \n
          p e e r _ i p     1 0 . 0 . 2 . 2 \n
```

`config.c` parses with `equal = strchr(key, '='); if (!equal) ...`, so every
line failed to parse, `netmidi_config_load` returned -1, and `run()` took:

```c
if (config_result < 0) {
    fputs(TAG "invalid network preferences\n", stderr);
    return 20;                      /* rc=20, and stderr is NIL: */
}
```

The `=` was eaten **in transit through the shell exec layer**, not by netmidid
and not by the build. `TYPE` had been showing `local_port 5004` all along and
I read it as normal `TYPE` formatting instead of checking the bytes — a
`TYPE` rendering I did not verify. Fixing it: write the file from the host and
`--put` it, rather than composing `echo "k=v"` through the agent exec.

That is why **nothing** ever printed, including the deliberately-invalid-config
test: the failure was a config *parse* rejection, and the message goes to
stderr, which `main()` has already sent to `NIL:`.

## Two of my own conclusions, corrected

**1. "It dies inside freopen" — WRONG, and I caused it.** The stdout sink made
this unprovable: after `freopen("NIL:")` stdout is gone, so markers placed
after it cannot print *even on success*. Output stopping at `M1c` is exactly
what a **working** `freopen` looks like. Re-run with `netmidid.log` pointed at
a RAM file, every post-`freopen` marker appeared:

```
netmidid: M2a stdout redirected
netmidid: M2 after freopen
netmidid: M3 before run
netmidid: R0 run entered
netmidid: R1 after ENV: load
netmidid: R2 after ENVARC: load
netmidid: RX exit: invalid network preferences      <-- the real exit, named
```

So the earlier "posixc stdio startup" theory was wrong too. `freopen` is fine;
the parse rejection is the whole story.

**2. The "posixc version mismatch" was a bad comparison.** I compared
`libposixc.a` (415378 B, static SDK archive) against `posixc.library`
(312952 B, built runtime library) and called it a mismatch. Different artifact
types — not comparable. Both scratch trees' `libposixc.a` hash identically.

## What the lane tree did and did not tell us

Rebuilt netmidid from the lane's own source commit `6f62b30830` (a fresh
worktree + fresh build dir; the lane tree itself was not touched), with the
same v1 toolchain that produced the guest. It behaved **identically** — same
exit — which is consistent with the cause being the config file rather than the
toolchain, though by itself it proves nothing about toolchains.

Recorded, as asked:
- **riqemu1 runs ABIv1** — `Kickstart 51.51, Workbench 40.0`, from `src/abi/v1`.
- The `camd.library` that passed the SELFTEST was built by **v1 tree mmake with
  the v1 toolchain**.
- Every later build must match that. No hand-rolled compiler lines.

The **r12 claim stays retracted**: ABIv1 tree-built binaries that demonstrably
run carry 7 (`camd.library`) and 5 (`C:Version`) `mov %rax,%r12` sites.

## GATE: MET

`netmidid` starts, reads its config and idles.

```
netmidid: listening on UDP 5004/5005
netmidid: invitation timed out
```

Run with `Run RAM:NM_M` — never `--run_script`, which waits on a daemon (I
wedged the agent twice that way; recovered each time with `system_reset`,
same PID 1933951, 1280x1024 re-verified by screendump).

**Host corroboration:** `rtpmidid` logged a real peer event —
`Peer event=CONNECTED_ROUTER from=1564`, `Connect 1564 -> 1565`. So the
AppleMIDI session came up over QEMU user NAT **without any `hostfwd`**, which
the plan flagged as unverified. `invitation timed out` is the service idling
and re-offering on its own timer, which is its normal behaviour when a peer
does not complete.

## Instrumentation note

Markers are a **local debug build, never upstream**, and live in a scratch
worktree. Sink is `Open()`/`WriteChars` to `RAM:nm.marker`, deliberately
independent of stdout — the first two attempts used stdout and both produced
false readings. `WriteChars` is core exec and needs no varargs machinery, which
avoids both the hand-rolled LVO offset and `VPrintf`, which is not declared in
this SDK.

## Still not proven

- The full N1 gate (session connected both sides, `MIDISEND` to
  `applemidi.out.0` visible in `aseqdump`, `aplaymidi` moving `NOTEIN played=`)
  is **not** run — that needs the peer to complete the session, and the
  invitation is timing out.
- The host peer event is real evidence of connectivity, but a
  `CONNECTED_ROUTER` on the host is not the same as an established AppleMIDI
  session with clock sync.
- N3 (the clock-out spread measurement) is untouched.