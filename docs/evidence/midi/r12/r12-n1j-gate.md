# R12 N1j — N1 gate MET on the Dell: both directions through rtpmidid (2026-10-19)

## Labels

**Peerless check**: met (unchanged; riqemu1, separate run). **N1 gate**:
**MET** on the Dell e6320 over the real LAN. The two are different runs and
different evidence; nothing here reuses the peerless capture.

## The run

- Dell e6320 (ABIv11, `192.168.1.60`): `LIBS:camd.library` 41.1 (8.10.2026),
  `DEVS:Midi/applemidi` 11880 B, `SYS:System/Network/Services/netmidid`
  128496 B, both from the #1679 branch.
- `ENV:NetMIDI` on the Dell:

  ```
  local_port=5004
  peer_ip=192.168.1.81
  peer_port=5004
  accept=192.168.1.81
  ```

- Host (`192.168.1.81`): `rtpmidid 24.12.2`, started as

  ```
  rtpmidid --port 5004 --name ReIncarnation-peer --rtpmidi-discover false
  ```

  Discovery is off on purpose: with it on, a local test ALSA client got
  exported over mDNS and its removal removed the Dell's alsa peer (see
  "Corrections").
- RIAPP started with `RIAPP_MIDI_IN=applemidi.in.0`, after `netmidid`.

Session, host peer log:

```
Connected from ::ffff:192.168.1.60:5004
Got connection request from remote_name="arosbox" ... at port=CONTROL_PORT
status change to CONTROL_CONNECTED
Got connection request from remote_name="arosbox" ... at port=MIDI_PORT
status change to CONNECTED
Created alsapeer arosbox, port 2
```

Guest:

```
netmidid: R4 udp pair bound
netmidid: R5 camd adapters attached
netmidid: listening on UDP 5004/5005
netmidid: session connected port=0
```

Gate check 1 (session connects): met.

## Gate check 2 — guest to host

Guest: `RAM:MIDISEND applemidi.out.0 RAM:R5-NOTEIN.MID.TXT`

```
MIDISEND: 33 messages to applemidi.out.0
```

Host: `aseqdump -p 128:2`

```
128:2   Note on                 9, note 36, velocity 100
128:2   Note off                9, note 36
128:2   Note on                 9, note 38, velocity 100
...
128:2   Note on                 1, note 36, velocity 127
128:2   Note off                1, note 36, velocity 0
```

Totals: **17 Note on / 16 Note off** (33 messages sent, 33 events seen).

## Gate check 3 — host to guest

Baseline (`Vk4aros:RIAPP-EV.LOG`, fresh instance listening on
`applemidi.in.0`):

```
ev 8 2 NOTEIN bound 2 drums_implicit=1
ev 9 2 NOTEIN played=0 refused=0 unclaimed=0 dropped=0 seen=0
```

Host: `aplaymidi -p 128:2 gate-notes.mid` (two notes: channel 2 and channel 10;
four events):

```
ev 24 3469 LEVI note 36 on vel 100
ev 27 3469 NOTEIN played=1 refused=0 unclaimed=0 dropped=0 seen=1
ev 28 3570 LEVI note 36 off vel 0
ev 30 3570 NOTEIN played=3 refused=0 unclaimed=0 dropped=0 seen=3
ev 32 3668 NOTEIN played=3 refused=1 unclaimed=0 dropped=0 seen=4
```

Repeat, same live session (after check 2):

```
ev 33 21381 LEVI note 36 on vel 100
ev 35 21381 NOTEIN played=4 refused=1 unclaimed=0 dropped=0 seen=5
ev 36 21475 LEVI note 36 off vel 0
ev 38 21475 NOTEIN played=6 refused=1 unclaimed=0 dropped=0 seen=7
ev 40 21573 NOTEIN played=6 refused=2 unclaimed=0 dropped=0 seen=8
```

`played` and `seen` move on every run; the melodic channel adds nothing to
`refused`. One event per file — the channel-10 note-off — lands in `refused`:
the implicit drum map claims note-ons. Recorded for N2; it does not affect the
gate (played > 0 is the criterion).

netmidid's receive log for the same window (delta between before/after
snapshots) contains the four data packets themselves:

```
sock=1  src=192.168.1.81:5005  len=16   (x4; decoded = NOT_APPLEMIDI = RTP-MIDI DT)
```

plus the expected `CK`/`RS` keepalive traffic. No journal, no errors.

## Corrections to the working hypothesis

- "rtpmidid's ALSA input never reaches the router" was **wrong**. The DT
  packets were arriving at netmidid all along. rtpmidid sends a DT packet for
  every ALSA message; the app-side counter stayed at zero because the running
  RIAPP had been started by RIBOOT at boot with the default cluster `riapp`,
  and a `setenv RIAPP_MIDI_IN` in a later agent job cannot change an
  already-running process. Restarting RIAPP with the variable set made
  `NOTEIN` move on the first note.
- The netmidid diagnostic log prints `RX cmd` with a stale command word for
  any packet that is not AppleMIDI. Reading the histogram as "no DT" was a
  measurement error, not a defect. The reliable discriminator is
  `len=16` with `decoded = 18446744073709551615` (`APPLEMIDI_NOT_APPLEMIDI`).
- rtpmidid with discovery enabled discovers its own mDNS announcements of
  local ALSA clients; removing such a client removes the connected Dell alsa
  peer (`local_alsa_listener` deletes connected `local_alsa_peer_t`s). Run
  the peer with `--rtpmidi-discover false`, and do not create test ALSA
  clients while a live session is up.
- An earlier attempted `Run >RAM:RIAPP.OUT ...` while another instance held
  the claim left a blocked "another instance is running" requester and no log
  line. Start RIAPP once, after stopping the previous instance with
  `Break NAME=SYS:Classes/ReIncarnation/RIAPP C`.

## Artifacts

`/home/miller/.worktrees/r12-logs/r12-n1j/`:

- `aseqdump-gate.log` — check 2, 17 Note on / 16 Note off
- `riapp-ev-before.txt`, `riapp-ev-after.txt` — check 3 event log
- `netmidid-before.txt`, `netmidid-after.txt`, `netmidid-delta.txt` — guest
  session and receive log
- `rtpmidid-peer.log` — host session, CONNECTED, alsapeer

## Not proven

- A DAW (Ableton or any other) end to end; the far peer here is rtpmidid
  24.12.2.
- Clock out (N3) — not measured in this run.
- Whether the channel-10 note-off `refused` accounting matters musically (it
  is a one-shot percussion map; N2 detail).
