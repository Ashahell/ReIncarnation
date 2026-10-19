# R12 N1g — full packet diagnostics: where the session stalls (2026-10-19)

> **PARTLY WITHDRAWN — see `r12-n1i-probe-artifact-and-nat.md`.** The peer in
> this capture was my Python probe (still bound to UDP 5004), not rtpmidid.
> The `action=0` readings and the token hypothesis below describe the probe's
> behaviour. The diagnostics themselves are sound and reusable.

## Labels

**Peerless check**: met (netmidid starts, reads config, idles).
**N1 gate**: **NOT met.** No established session on both sides, no `MIDISEND`
in `aseqdump`, no `NOTEIN played=` movement. The two are kept apart.

## What the diagnostics say

Instrumented netmidid's own receive/send path (local debug build, never
upstream). Two attempts at the sink were needed and both failures are worth
recording:

1. `Open()` + `WriteChars` to `RAM:nm.marker` — the file was **created** but
   received **zero bytes**. `VPrintf` is not declared in this SDK; same failure
   class as the earlier stdout sink.
2. Plain `printf` + `fflush`, which is **proven** to work here. The reason
   post-`freopen` markers were invisible before is that `main()` sends stdout
   to `NIL:` unless `netmidid.log` names a file — so **post-freopen
   diagnostics are silently discarded without it.** That is exactly what
   happened before, and it is why "no output" kept meaning nothing.

With `netmidid.log=RAM:NMLOG` set, everything lands there.

### Counters, against the real `rtpmidid` peer

| | |
|---|---|
| TX to peer | 78 packets, all to **10.0.2.2:5004**, cmd `IN` (18766) |
| RX from peer | 75 packets, **all on the control port**, cmd `IN` (18766) |
| RX **data** port (5005) | **0** |
| `MATCH find_control` | index 0, `configured` = 1 → **the peer matches** |
| `MATCH configured_index` | 0 → the configured peer is found |
| `PEERS_RECEIVE` | reached every time, **action = 0** (nothing actionable) |
| `netmidid` local SSRC | 4081032358 |
| rtpmidid peer SSRC | 2842019580 — **no collision** |
| outcome | `invitation timed out`, repeatedly |

## What this rules out

- **Not NAT.** The control path works end to end: replies arrive from the
  configured `10.0.2.2:5004`, the peer matches by address **and** port, and the
  packet decodes cleanly. The earlier source-port reading (`127.0.0.1:43631`)
  is the slirp-mapped view from the host side; inside the guest it is
  `10.0.2.2:5004`, exactly as configured. **A `hostfwd` is not justified.**
- **Not the SSRC collision** path — the two SSRCs differ, and that branch is
  not taken.
- **Not encoding.** The packets on the wire are well-formed AppleMIDI
  (`ffff 494e`, version 2, token, SSRC), per `rtpmidi/applemidi.c`.
- **Not a missing library, not a bind failure, not a pre-`main()` fault.**

## Where it actually stalls

The session never leaves the control phase. `applemidi_session_receive`
(`rtpmidi/session.c`) requires, for a control-port `IN` while
`INVITING_CONTROL`, that:

- `in->token == session->token` — the peer's `IN` must **echo our token**, and
- `in->ssrc != session->local_ssrc`.

Otherwise it falls through to `return 0`, which is precisely the observed
`action = 0`. The likely cause is the **token** condition: rtpmidid answers
with its own token rather than echoing ours. That is consistent with every
observation, but **it is not proven** — I did not log netmidid's own token
alongside the received one, which is the one measurement that would settle it
and the obvious next diagnostic.

## Also: my own probe was broken for most of this

The hand-written python responder sent **zero** replies for most of its life
because it used small-integer command codes while AppleMIDI commands are
**two ASCII characters** (`IN` = `0x494E`). When fixed it revealed the real
behaviour above. It then replied `ACK`, which `exchange_command()` rejects
(only IN/OK/NO/BY are exchange commands) — RFC 6295 answers an invitation with
`IN`. The probe is a diagnostic, not a peer; `rtpmidid` is the real one and is
what these numbers come from.

## Gate status

**Peerless: met. N1 gate: not met** — unchanged. The diagnostics have narrowed
this from "silently exits" to "session established control-side, never advances
to the data port", which is a specific, located defect.

## Not proven

- The token hypothesis above (needs netmidid's own token logged next to the
  received one).
- Whether the Dell behaves the same — it is rebooted and available, and has no
  NAT, so it is the natural place to confirm the token question with the
  control path taken out of the picture.
- N3's clock-out spread: untouched.