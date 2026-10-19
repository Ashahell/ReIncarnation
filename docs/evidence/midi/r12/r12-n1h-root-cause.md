# R12 N1h — ROOT CAUSE: netmidid and rtpmidid wait for opposite handshakes (2026-10-19)

> **RETRACTED 2026-10-19 — see `r12-n1i-probe-artifact-and-nat.md`.**
> The "peer" in this capture was my Python probe, which was still bound to
> UDP 5004 and answers `IN` by design. rtpmidid was never answered-from.
> With the probe killed, rtpmidid replies `OK` and the control half of the
> session completes. The real blocker is slirp breaking AppleMIDI's
> control+1 data-port pairing. Everything below is kept as the record of a
> wrong turn, not as a finding.

## Labels

**Peerless check**: met. **N1 gate**: **NOT met.** Kept apart.

## The finding

The session never advances past the control phase because **the two ends
expect opposite halves of the RFC 6295 handshake**.

`rtpmidi/session.c:131`, the inviter path:

```c
if (in->command != APPLEMIDI_OK || in->token != session->token)
    return 0;                       /* <-- rtpmidid's reply dies HERE */
if (!on_data_port && session->phase == APPLEMIDI_SESSION_INVITING_CONTROL) {
    session->peer_ssrc = in->ssrc;
    session->phase = APPLEMIDI_SESSION_INVITING_DATA;
    make_packet(session, APPLEMIDI_IN, out);
    *out_data_port = 1;             /* the data-port invitation */
    return 1;
}
```

To move `INVITING_CONTROL → INVITING_DATA`, netmidid requires the peer to
answer with **`OK` (`0x4143`)**.

**Observed from `rtpmidid` on the Dell: command `18766` = `0x494E` = `IN`.
Never `OK`.** So the reply is discarded and netmidid retries forever
(`invitation timed out`).

The asymmetry: RFC 6295 has the *responder* answer an invitation with `IN`,
while netmidid — which is the **initiator** here — is waiting for `OK`. Both
ends are holding the handshake shape of the other role.

## The measurement that settles it

On the **Dell** (no NAT), with the token instrumentation:

```
netmidid SENT tokens    : 418751277 (repeated)
netmidid RECEIVED tokens: 418751277, 2563294745
INTERSECTION            : 418751277   <-- tokens DO match
PHASE before            : (index 0, phase 1, token 418751277)
PEERS_RECEIVE           : (action 0, index 0, event 0)   every time
RX                      : control port only, cmd 18766 (IN)
RX data port            : zero
```

Phase `1` is `APPLEMIDI_SESSION_INVITING_CONTROL`, unchanged. The peer is found
(index 0, not `<0`), the token matches, `PEERS_RECEIVE` is reached — and it
returns **0** every time, which is precisely the `return 0` at the line above.

**So the token hypothesis is WRONG and is withdrawn.** Tokens match; the block
is the **command check**.

## Why the Dell was the right place to test

riqemu1 (QEMU user NAT) produced the same `action=0`, but with a NAT in the
picture every hypothesis was confounded. On the Dell there is no NAT, the
control path is trivially correct, and only one condition remains — the one
that turned out to be it.

## Also found on the Dell

`netmidid` **cannot bind UDP 5004/5005**: `RX exit: cannot bind UDP port
pair`. With `local_port=5010` it binds fine (`listening on UDP 5010/5011`), so
5004/5005 are already taken on the Dell by something else. Not yet identified;
it may simply be an AROSTCP service. **Unverified.**

## What this is and is not

This looks like an **interop defect between two implementations**, not a bug in
our build and not a NAT problem. Two readings, and I cannot yet choose between
them without checking upstream:

1. `rtpmidid` does not implement the `OK` reply netmidid requires, or
2. netmidid's inviter state machine expects a command the RFC does not
   prescribe for this role.

**Upstream's own evidence is only node-rtpmidi against node-rtpmidi**, so this
pair may simply never have been tested together. That is exactly the gap the
assessment flagged. **I have not verified either reading** — it needs the
#1679 head's stated protocol intent and rtpmidid's RFC 6295 behaviour checked
side by side, and it is **upstream code, which is out of scope** per the plan.

## Gate status

**Peerless: met. N1 gate: not met** — unchanged, but the blocker is now a
specific, reproducible protocol mismatch rather than a mystery.

## Not proven

- Which of the two implementations is at fault.
- Why the Dell cannot bind 5004/5005.
- N3's clock-out spread: untouched, and it depends on this session existing.