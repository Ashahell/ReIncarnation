# R12 network MIDI: can upstream AppleMIDI connect the Dell to Ableton?

**Status:** assessment written 2026-10-19, before anything was built. It was
then built for ABIv11 and run on the Dell the same day: the AppleMIDI session
connected through `rtpmidid`; RIAPP's **R5 note in** and **R2 clock out** were
measured, and clock out was measured twice over 5.06 minutes — **56.0001
intervals/s, mean 17857.12 µs** against a 17857.14 µs nominal, **0 xruns,
0 dropped** (`docs/evidence/midi/r12/`). The guest→host note check used
**`MIDISEND`, not RIAPP**, so **RIAPP note/CC out is not tested**; its proven
paths are note in and clock out. The gate ran on a **local debug netmidid**
(128496 B, sha256 `dad3644b…`; 191 added lines of markers/bind logging plus
the build-only entropy shim, no session or protocol change), not on the clean
122448 B build from N0 (`c71fe8ff…`); the gate has not been re-run on the
clean build. The body below is kept as written, with a result note under the
table. What is still unproven is named in `r12-n3-clockout.md` and in the MIDI
ledger. Every statement below is either read from upstream source or PR text
(cited), or read from this tree (file:line); **unverified** marks a plan or
an expectation.

**Parent:** interop requirement `2026-09-29-interop-requirement.md`, **R12**:
*"RTP-MIDI / AppleMIDI, so an AROS machine and a DAW PC can exchange clock and
notes over Ethernet without MIDI hardware."* R12 was written as optional and
blocked on our own AROSTCP work. Upstream has now done most of that work.

## Short answer

The pieces are now there for **clock out and notes/CC in both directions**,
with no MIDI hardware. What is **proven on the Dell today** is RIAPP's **note
in** (R5) and **clock out** (R2); RIAPP note/CC out is not tested — the
guest→host notes came from the `MIDISEND` test tool. The Dell becomes an
AppleMIDI peer, and macOS (built in) or Windows (rtpMIDI) exposes the session
to Ableton as an ordinary MIDI port.

It does **not** help **clock in at 140 BPM** (P1, parked). That limit is how
CAMD delivers messages to our receiver, and the network path ends in CAMD
too.

Three things stand between us and a first test on the Dell, and none of them
waits on an upstream merge:

1. An **ABIv11 build** of four upstream pieces, because deadwood2/AROS (the
   Dell's ABIv11 base) has none of them.
2. A **camd.library rebuild** with #1585, because without it no `DEVS:Midi`
   driver is found on 64-bit. That includes the new ones.
3. A **one-setting app change**, because RIAPP sends and receives on the
   same CAMD cluster, and a network port has separate `in` and `out` clusters.

**All three are done (2026-10-19).** The four pieces build for ABIv11, the
camd rebuild is installed on the Dell, and the clock-out test uses the
no-change route (`RIAPP_MIDI_IN=applemidi.out.0`). `RIAPP_MIDI_OUT` itself
is still unimplemented — see the result note under "The app change".

## What upstream has, and where it is

All of this is in aros-development-team/AROS, by JJDSNT unless noted.

| piece | PR | state | what it is |
|---|---|---|---|
| CAMD drivers `applemidi`, `networkmidi2` | #1586 | **merged** 2026-10-18 | `DEVS:Midi` drivers with 16 ports each (`applemidi.in.0`–`.15`, `applemidi.out.0`–`.15`). They hold **no network code**: they publish adapter rings that a service attaches to. *"Alone, it is inert"*: without the service a port drops what it is sent. |
| CAMD driver scan at aligned addresses | #1585 | **merged** 2026-10-17 | *"On 64-bit targets the drivers are found only with #1585"* (from #1586). |
| CAMD expunge returns success | #1676 | **merged** 2026-10-18 | The original blocks at the second open after a flush. |
| RTP-MIDI payload library | #1587 | **merged** | Link library. |
| UMP / Network MIDI 2.0 library | #1588, #1692 | **merged** | Not needed for AppleMIDI. |
| **AppleMIDI sessions** (invitations, peers, clock sync) | **#1678** | **open**, review required | Link library. The review correction (backdated timestamps) is already in. |
| **`netmidid` service** | **#1679** | **open, draft**, depends on #1678 | The daemon that joins the network to the driver's ports. |
| Network MIDI 2.0 in `netmidid` | #1693 | open, draft | Not needed for Ableton. |
| mDNS / Bonami registration | #1591 | open | Discovery only. Peers can be configured by IP instead. |
| CAMD/netinfo SMP lifetime cleanup | #1677 | draft | Matters for unloading or restarting the service. Avoid doing that during tests. |

Upstream's own evidence is **hosted linux-x86_64 only**: node-rtpmidi peers,
notes/CC/SysEx, two peers, both invitation directions, stop and reconnect.
#1679 says plainly that it is *"not … a native latency bound, or proof of
complete RFC 6295 conformance and external journal interoperability"*. **No
one has run this on native x86-64 or real hardware, or against macOS or
Ableton.**

### How `netmidid` works (read from the #1679 head, `JJDSNT/AROS@db9e3073`)

- It is a plain program, `SYS:System/Network/Services/netmidid`, using
  `bsdsocket.library`. It is not a CAMD client: it attaches to the
  `applemidi` driver's adapters (found by a named semaphore).
- Configuration is `ENV:NetMIDI`: `local_port=5004`, `peer_ip=…`,
  `peer_port=…`, `accept=…`. It re-reads that file every 2 s and restarts its
  sessions when it changes.
- It is event-driven: it waits on the sockets **and** the CAMD port signal,
  so outgoing MIDI is forwarded on wake, not polled.
- Its clock is `gettimeofday()` in 10 kHz units. The RTP timestamp on an
  outgoing packet is the **send** time (`main.c` ~631), not the time the
  event was meant to happen.
- **Security:** it accepts any invitation unless `accept` says otherwise.
  Set `accept=<the DAW machine's IP>`.
- Upstream launches it from the AROSTCP **services framework** (`services.d/`,
  ADT `f93780cc5a`, 2026-10-09). The Dell's AROSTCP predates that framework,
  so on the Dell it is started by hand after AROSTCP, from `RIBOOT` or
  User-Startup. It stops with CTRL-C (`StopSig=12`). **Unverified** that it
  needs nothing else from the framework.

## What the Dell needs (ABIv11)

The Dell runs deadwood2/AROS: our tree is at `fbc242c104` (2026-09-04), and
deadwood2 `master` is at 2026-10-02. **Neither has the drivers, the RTP-MIDI
libraries, `netmidid`, or the services framework.** deadwood2's last sync
from ADT (`LAST_SYNC_TO_ADT`) is 2026-02-06. So for the Dell this is always
**our own build**, whether or not #1678/#1679 merge.

The route is the one M0 used for camd: a fresh build dir alongside the v11
tree (`core-pc-x86_64-m0`), with the ADT sources copied in.

1. **`camd.library`** with #1483 (already on the Dell, `41.1 (8.10.2026)`,
   53816 B), **plus #1585 and #1676**. Install to `LIBS:` and have the owner
   **reboot**. Never `avail flush` on the Dell.
2. **`DEVS:Midi/applemidi`** from #1586 (`workbench/devs/camd/applemidi` +
   `common/`).
3. The **`rtpmidi` link library** (#1587 + #1678) and **`netmidid`** (#1679),
   built from the #1679 branch.
4. `ENV:NetMIDI` on the Dell: `local_port=5004`, `peer_ip=<DAW machine>`,
   `peer_port=5004`, `accept=<DAW machine>`.

**Unverified:** that these sources build against the ABIv11 SDK unchanged.
They build for hosted x86-64 and m68k upstream, and the driver interface
(`midi/camddevices.h`) is the one M0 already built against.

### The app change (ours, small)

RIAPP uses **one** CAMD cluster for everything: `RIAPP_MIDI_IN` (default
`riapp`) feeds the receiver ([riapp.c:3276](../../../app/riapp.c)), the
clock/device sender task ([riapp.c:3251](../../../app/riapp.c)) and MMC out
([riapp.c:772](../../../app/riapp.c)). That works app-to-app, where one cluster
carries both directions. A driver port does not: we receive on
`applemidi.in.N` and send to `applemidi.out.N`.

Proposal: add **`RIAPP_MIDI_OUT`**, defaulting to the `RIAPP_MIDI_IN` value
so every existing setup is unchanged. R4 already lists "MIDI in and out port
selection", so this finishes R4 rather than adding scope.

**For a clock-out-only first test, no change is needed:** set
`RIAPP_MIDI_IN=applemidi.out.0`. The receiver then listens on a cluster
nothing sends to, which is harmless for that test.

**Result (2026-10-19):** not implemented. The clock-out test used exactly the
no-change route above, so the app still has one cluster setting. This stays
the first thing to add when a DAW needs both directions at once (R5 + R6).
**Known trap for that work:** `ri_pal_midi_send_start()` ignores its `port`
argument and sends to the cluster the input open selected, so a real
`RIAPP_MIDI_OUT` must split the send cluster first; not changed here.

## What it gives the interop requirement

| item | over AppleMIDI | why |
|---|---|---|
| **R2 clock out** (Dell leads, Ableton follows) | **expected to work** | Ableton smooths incoming clock as it does for USB-MIDI. Jitter sources are our sender task, CAMD, the `netmidid` wake and the LAN. Untimed RTP timestamps (send time) mean nothing compensates for that jitter. **Measure it, don't assume.** |
| R3 MMC out | expected to work | Same path. Live's MMC support is limited, so clock Start/Stop is what Live follows. |
| R5 note input (Ableton plays the 303/808/909) | expected to work | Inbound notes reach our receiver like any CAMD message. |
| R6 note/CC out | expected to work | Same path as clock out. |
| G7 remote CC in | expected to work | Same as R5. |
| **P1 clock in at 140 BPM** | **no improvement** | The limit is our receiver seeing CAMD deliveries in 10 ms system-tick batches, stamped at drain time. A network hop ends in the same CAMD delivery, with more jitter before it. Stays parked. **Unverified** whether `netmidid` passes RTP timestamps into CAMD at all. |
| R10 Ableton Link | unrelated | Link is its own UDP multicast protocol. AppleMIDI doesn't stand in for it. |

**Measured on the Dell (2026-10-19):** R2 clock out — two 5.06-minute runs,
56.0001 intervals/s, mean 17857.12 µs against 17857.14 nominal, 0 xruns,
0 dropped (`r12-n3-clockout.md`). R5 note input — RIAPP's `played` and `seen`
move on every `aplaymidi` run. The **guest→host note path** was exercised by
the `MIDISEND` test tool: 33 messages, 33 events at the host; that is not
RIAPP's R6. **Still unverified:** RIAPP note/CC out (`RIAPP_MIDI_DEVOUT`),
CC out, MMC out, a real DAW, and P1 clock in (parked; the limit is CAMD
delivery, not the network).

**The measurement the interop spec says is missing becomes possible.** The
spec records that *"the wire spread is not measurable with this
architecture"* because our receiver stamps at drain time. With the far end a
Linux host, the host timestamps each arriving clock at the far end. That
measures the spread **as Ableton would see it**, which is the number that
matters for R2.

## What we can test now, in order

None of this waits on #1678/#1679 merging. It uses the draft branch, which
is acceptable for a lane test and not for shipping.

**T1 — functional, riqemu1 (ABIv1, ADT master).** The ABIv1 tree tracks ADT
directly (`6f62b30830`, 2026-10-12), so pull to get #1585/#1586/#1676 and
apply the #1678/#1679 branch. Build camd, `applemidi`, `rtpmidi` and
`netmidid`. On the Linux host, run an AppleMIDI peer (`rtpmidid`, which
exposes sessions as ALSA sequencer ports; not installed today), with
`aseqdump` on the other side.
- riqemu1 is on QEMU user networking. With `peer_ip=10.0.2.2`, `netmidid`
  **invites outward**, which should pass the NAT without a port forward.
  **Unverified.** The other direction needs `hostfwd` for UDP 5004–5005, and
  that means restarting riqemu1 (and checking 1280x1024 by screendump).
- Pass: the session connects, `MIDISEND` to `applemidi.out.0` shows up in
  `aseqdump`, and `aplaymidi` from the host moves our `NOTEIN` counters.
- riqemu1 has no sound and its clocks are not trustworthy, so this proves
  plumbing only.

**T2 — the Dell (ABIv11).** Build the pieces above in the fresh ABIv11 build
dir. Stage them in `RAM:` (camd to `LIBS:` only via the owner's reboot) and
start `netmidid` by hand. The peer is the Linux host (`192.168.1.81`; the
Dell is `.60`).
- Repeat T1's pass criteria.
- Then the **clock-out measurement**: `RIAPP_MIDI_CLKOUT=1`,
  `RIAPP_MIDI_IN=applemidi.out.0`, the song at 140 BPM, and a small host
  script that timestamps each `F8` from the ALSA port. Report the mean
  (expected 17857 µs), the spread and the worst gap, next to the existing
  `mean_us=17853` schedule figure, which is the sender side only.

**T3 — a DAW end to end: deferred (owner, 2026-10-19).** There is no
Ableton, and the owner chose to **stick to `aseqdump` for now** rather than
install a Linux DAW. A DAW following the Dell's clock is therefore **not
tested**. If it is wanted later, Ardour (GPL, Arch `extra`) is the candidate:
it is expected to follow external MIDI Clock as a transport master
(**unverified**).

## What actually waits on the open PRs

- **#1678 / #1679 merging** means upstream maintains the code we run. Until
  then we carry a draft whose protocol code may still change under review.
  Pin the branch commit in whatever we deploy.
- **#1677** (SMP lifetime) means we don't unload the driver or restart the
  service in a test.
- **#1591** (mDNS) means automatic discovery. Not needed: configure by IP.
- **#1693** (Network MIDI 2.0). Not needed for Ableton.
- **Not a PR, and slower than any of them:** a deadwood2 sync from ADT, which
  would put this in the ABIv11 tree itself. We should not wait for it.

## Owner decisions (2026-10-19)

1. **No access to Ableton.** Use `aseqdump` only; no Linux DAW for now, so T3
   is deferred.
2. **A draft upstream branch on the Dell is acceptable**, pinned to a commit,
   until #1678/#1679 merge.
3. **Priority:** **R7 (SMF) goes first.** Without Ableton, R12's headline
   payoff can't be shown, and R7 can be verified entirely on the host. R12
   runs alongside it as lane work, scoped to **T1/T2: plumbing plus the
   clock-out measurement**. The brief is
   `docs/superpowers/plans/2026-10-19-r12-network-midi-opencode-prompt.md`.
   The `RIAPP_MIDI_OUT` app change waits
   until T2 has shown that the path works.
