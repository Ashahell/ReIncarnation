# R12 N1i — CORRECTION: the stall was my probe, not rtpmidid; the real blocker is slirp port pairing (2026-10-19)

## Labels

**Peerless check**: met. **N1 gate**: **NOT met.** Kept apart.

## N1h is WITHDRAWN

`r12-n1h-root-cause.md` claimed netmidid and rtpmidid wait for opposite
handshakes. **That conclusion is wrong and is retracted.**

The "peer" in every stall capture was **my own Python probe**,
`/home/miller/.worktrees/r12-probe/applemidi_probe.py`. It was still running
and bound to UDP 5004 with `SO_REUSEADDR`; it answers an invitation with `IN`
by design. rtpmidid was also running, but never received the packets.

Proof, a synthetic invitation to `127.0.0.1:5004` with a known token:

```
reply cmd=0x494E(IN) token=0x12345678 name=b'probe\0'      <-- before
reply cmd=0x4F4B(OK) token=0x12345678 name=b'ReIncarnation-peer\0'  <-- after killing the probe
```

`name='probe'` identifies it. After `kill` on the probe and a clean restart of
rtpmidid, the same packet is answered with **`OK`**, correctly echoing the
token. So:

- rtpmidid's responder path is standard: `parse_command_in()` replies `OK`.
- netmidid's inviter path is correct: it requires `OK` (`session.c:131`).
- The token hypothesis was also wrong, but for the trivial reason that the
  responder was never rtpmidid.

## The real blocker on riqemu1: slirp breaks AppleMIDI's port pairing

With the real rtpmidid, the control half now completes:

```
RX OK   token matches
PHASE   [0]=1 (INVITING_CONTROL)
peers_receive action=1     <-- ACCEPTED
TX      IN to 10.0.2.2:5005   (data-port invitation, repeated)
```

rtpmidid's side agrees it is connected — and then re-accepts the data
invitation over and over ("This peer is already connected. But OK, I will
accept it anew"), creating a new ALSA peer each time.

The data-port `OK` never reaches the guest. Cause, from rtpmidid's
`lib/rtpserver.cpp:165-173`:

```cpp
if (port == rtppeer_t::MIDI_PORT) {
    address.set_port(remote_base_port + 1);   // control source port + 1
    midi.sendto(packet_t(pb.start, pb.size()), address);
}
```

AppleMIDI assumes the data port is the control port plus one. That holds on a
LAN and **fails under QEMU user-mode networking**, which translates the two
flows independently:

```
qemu UDP sockets for riqemu1:
  0.0.0.0:43631   <- guest 5004 (control)
  0.0.0.0:51239   <- guest 5005 (data)
rtpmidid replies to: 43631 + 1 = 43632   (nothing there)
```

Measured directly, with a control socket on 5008 and a data socket on **5010**
(not the paired port):

```
control reply -> got on 5008
data reply    -> NOT on 5010 (the socket that sent it)
data reply    -> arrived on 5009 = control+1
```

So the reply is sent to control+1 regardless of where the data packet came
from. Under slirp that is an unrelated, unbound port. Nothing is wrong with
netmidid or with rtpmidid; the NAT destroys the port relationship the protocol
relies on.

## Consequence for the gate

The N1 gate **cannot pass on riqemu1 as configured** (user-mode NAT), no matter
how long we wait. The same code should connect on a real LAN, where the
source ports are 5004/5005 and control+1 is the actual data port. The Dell
e6320 is that LAN.

## What this changes

- N1h's "opposite handshakes" reading: **withdrawn**.
- N1g's "peer's IN does not echo netmidid's token": **withdrawn** — the peer
  was the probe.
- The earlier "rtpmidid leaked ALSA ports / re-accepts" observation is real,
  but it is *caused by* our repeated data invitations, which only repeat
  because the NAT eats the reply. Do not read it as an independent defect.
- The peerless check stands unchanged.

## Next

1. Dell e6320 (no NAT): one run, one instance, expect `CONNECTED`.
2. Restart rtpmidid first so the leaked ALSA peers from the NAT retries are
   cleared.
3. Then the three N1 gate checks.

## Not proven

- That the Dell connects — that is the next measurement, not an assumption.
- Whether any other NAT mode (tap/bridge) would also work; untested.
