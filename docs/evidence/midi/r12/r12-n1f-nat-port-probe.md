# R12 N1f — host-only NAT probe: the source ports settle it (2026-10-19)

> **CORRECTION — see `r12-n1i-probe-artifact-and-nat.md`.** This file's
> conclusion was **WRONG**, and N1i is the finding. The probe was still bound
> to UDP 5004 (`SO_REUSEADDR`) during later tests, so it, not rtpmidid,
> answered the guest. N1f concluded that the NAT was not the cause, that the
> control+1 pairing held, and that a `hostfwd` was not justified; **all three
> are withdrawn.** N1i measured the real behaviour: slirp breaks AppleMIDI's
> control+1 data-port pairing, and the reply goes to control+1 while the
> guest's data flow is mapped elsewhere. The body below is kept as the record
> of that wrong turn, not as a finding.

## Labels, corrected (owner)

The previous commit (`110f5d3`) said **"GATE MET"**. **Retracted.** Only the
**peerless** check passed — "netmidid starts, reads its config and idles".
The **N1 gate is NOT met**: the session has not been observed connected on both
sides, `MIDISEND` to `applemidi.out.0` has not appeared in `aseqdump`, and
`aplaymidi` has not moved `NOTEIN played=`. Kept apart from here on.

## Setup (host-only, no root, no guest change)

`rtpmidid` stopped and its log moved **off `/tmp`** to
`~/.worktrees/r12-logs/rtpmidid-20261019.log` — it had grown to **70 MB**, and
`/tmp` filling is what knocked the Dell lane over earlier today.

A read-only python responder (`~/.worktrees/r12-probe/applemidi_probe.py`) took
over UDP 5004/5005: it logs the exact source address:port of every datagram and
answers **to the exact source it arrived from** (never to a recomputed port).
That is the whole point — AppleMIDI assumes "MIDI port = control port + 1", and
slirp does not have to honour that.

## THE ANSWER: the source ports

| port | source seen |
|---|---|
| **CONTROL udp/5004** | `127.0.0.1:43631` — **fixed**, repeated, ~20 packets |
| **MIDI udp/5005** | **nothing at all** |

Every packet netmidid sends in this test is a **control-port** packet. **It
never sends on its data port.**

Two controls make this trustworthy:

1. **The 5005 listener works.** A test datagram sent to it from the host was
   logged immediately (`MIDI 127.0.0.1:34035 x1`). So the absence is netmidid's,
   not a dead socket.
2. **slirp does NOT remap the port.** The source is a single fixed `43631`
   across ~20 packets, so the NAT is not shuffling ports on us — the
   "AppleMIDI's port+1 assumption doesn't survive NAT" hypothesis is **not**
   supported by this measurement. Port pairing holds; the guest simply is not
   sending there.

## The wire format is correct — this is not an encoding bug

I first read `ffff 494e` as a UMP/MIDI-2.0 marker and nearly recorded it as a
protocol mismatch. **That was wrong.** `0xffff` *is* the AppleMIDI version
marker. From `rtpmidi/applemidi.c`:

```c
if (!data || !packet || length < 4 || read16(data) != 0xffff)
    return APPLEMIDI_NOT_APPLEMIDI;
...
write16(data, 0xffff);
write16(data + 2, (uint16_t)packet->command);   /* 0x494e = 'IN' */
write32(data + 4, 2);                            /* version 2            */
write32(data + 8, packet->token);
write32(data + 12, packet->ssrc);
```

Captured: `ffff 494e 00000002 a1bdbacb 3d2b89ea …` — version marker, `IN`,
version 2, token, SSRC. **A well-formed AppleMIDI invitation**, retried every
couple of seconds with a fresh token.

And it is answered: the probe replies `ACK` **to the exact source**
(`127.0.0.1:43631`) on the control socket, every time. netmidid still logs
`invitation timed out` and starts over.

## What that points at

netmidid sends a correct `IN` on the control port, gets an `ACK` back to the
address it sent from, and does not advance to the data port. Its own session
state machine (`applemidi_session_receive` / `applemidi_peers_retry`) is where
an `ACK` should move the phase from `INVITING_CONTROL` to the data-port
`IN`.

So the failure is most likely **in netmidid's receive path on the control
port** — it is not seeing or not accepting the `ACK` — rather than in NAT, the
port pairing, or the packet encoding. **That is a hypothesis; it is not
proven**, and the next step is to check whether the `ACK` actually arrives at
netmidid's control socket at all.

Note the probe answers `IN` with `ACK` (cmd 4). If netmidid expects something
else there, or expects it on the data port, that would explain the same
symptom — which is exactly why the next step is to log what netmidid receives,
not to guess.

## rtpmidid's side, for the record

Its log shows a far more complete exchange than "just listening": control `IN`
accepted, MIDI-port `IN` accepted (`CONNECTED`, ALSA port created), the same
MIDI-port `IN` re-sent ~10 times ("already connected ... accept anew", 816
times), then `BY` and a new token — **938 invitations**, with **3649 leaked
`arosbox` ALSA ports** on client 128. That last number is a real defect in its
own right (ports not reaped when a session ends), independent of ours.

## Not proven

- Whether netmidid receives the `ACK` at all.
- Whether a `hostfwd` would help: the measurement above suggests it would
  **not**, since the control path already works end to end and the data port is
  simply unused. A fixed `5004->5004, 5005->5005` forward is therefore **not
  justified by this evidence** — and the Dell, which has no NAT, is the better
  place for the peer test either way.
- Nothing in the N1 gate has been re-checked.