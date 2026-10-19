# Prompt for OpenCode: R12 network MIDI on the lanes (plumbing + clock-out spread)

> Written 2026-10-19 by the Claude advisor session. Background and every
> source fact: `docs/superpowers/specs/2026-10-19-r12-network-midi-assessment.md`.
> Read it first.
>
> **Goal:** the Dell talks AppleMIDI (RTP-MIDI) to the Linux host, and we
> **measure the timing spread of RIAPP's clock out at the far end**. The
> interop spec calls that spread unmeasurable today, because our receiver
> stamps messages when it drains them.
>
> **Phases N0–N4.** Each phase ends with a commit and a short report. If you
> run short of context, stop at a phase boundary and write a handoff.
>
> **Not in scope:** any change to RIAPP's code (including `RIAPP_MIDI_OUT`),
> Ableton, Network MIDI 2.0 (#1693), mDNS (#1591), and pushing anything
> upstream.

---

## 0. Owner decisions and lane rules

- A **draft upstream branch on the Dell is approved**, pinned to a commit
  (owner, 2026-10-19). Pin these and record the hashes in every report:
  - #1678 `review/applemidi-sessions` @ `31d1ffdde5d4af3aa4139ac071d407afb6961645`
  - #1679 `review/netmidid-service` @ `db9e3073c3a0645a50dabf388092cece9902e344`
    (JJDSNT/AROS; this branch already contains #1678)
  - ADT `master` for #1585, #1586 and #1676 (all merged).
- **Never `avail flush` on the Dell.** A new `camd.library` reaches `LIBS:`
  only through an **owner reboot**. Stage it, then ask.
- **riqemu1** restarts must come back at **1280x1024**. Verify with a monitor
  `screendump`.
- **Hash-verify after every boot** (you learned this with `SYS:ATCPBIN`).
- **Before blaming AROS, rule out the caller:** constants, return-value
  conventions, NULL locks, late `IoErr()`. The last three "AROS does X"
  findings were all ours.
- Only one RIAPP at a time. Use `-i` with the instance grep.

## 1. Facts you will need (verified 2026-10-19)

- The **Dell** is ABIv11 = deadwood2/AROS. Our tree is at `fbc242c104`.
  **None** of the network MIDI code is in it, and neither is AROSTCP's
  `services.d` framework. Build in a fresh dir alongside, as M0 did
  (`core-pc-x86_64-m0`), with the ADT sources copied in.
- The Dell's `camd.library` today is `41.1 (8.10.2026)`, 53816 B. It has
  #1483 but **not #1585**, so **no `DEVS:Midi` driver loads** on 64-bit until
  camd is rebuilt.
- **riqemu1** is ABIv1 = ADT master (`src/abi/v1/AROS`, `6f62b30830`,
  2026-10-12). Pull to get #1585/#1586/#1676.
- **`netmidid`** is a plain bsdsocket program. Its config is `ENV:NetMIDI`
  (`local_port`, `peer_ip`, `peer_port`, `accept`), it stops on CTRL-C, and
  it attaches to `DEVS:Midi/applemidi`'s adapters. On both lanes it is
  started **by hand**, after AROSTCP. **Always set
  `accept=<host IP>`**: by default it takes anyone's invitation.
- Driver ports are clusters `applemidi.in.N` and `applemidi.out.N`. RIAPP
  uses one cluster (`RIAPP_MIDI_IN`) for both directions. For a clock-out
  test, set `RIAPP_MIDI_IN=applemidi.out.0`. No code change is needed.
- The host is `192.168.1.81` and the Dell is `192.168.1.60`.
- **Host peer:** `rtpmidid` (AUR, not installed; `yay` is available). It
  exposes AppleMIDI sessions as ALSA sequencer ports. `aseqdump` is
  installed. **Ask the owner before installing anything on the host.**

## 2. Phases

### N0 — builds (no lane)

Build `camd.library` (#1585 + #1676 on top of what the Dell has),
`DEVS:Midi/applemidi`, the `rtpmidi` link library and `netmidid`:
- for **ABIv1** (riqemu1);
- for **ABIv11** (the Dell).

Record the size, sha256 and `$VER` of each. If a source does not build
against the ABIv11 SDK, **stop and report the exact error**. Don't patch
upstream protocol code to make it fit.

### N1 — riqemu1 plumbing

Install the pieces (camd needs a restart), configure `peer_ip=10.0.2.2`, and
start `netmidid`. Start `rtpmidid` on the host.

QEMU user networking: `netmidid` invites **outward**, which is expected to
pass the NAT. **That is unverified, so test it**, and add a UDP `hostfwd`
for 5004–5005 only if it fails.

**Gate N1:**
- the session reports connected on both sides;
- `MIDISEND` to `applemidi.out.0` (a note and a CC) appears in `aseqdump`;
- `aplaymidi` from the host moves RIAPP's `NOTEIN played=` counter.

### N2 — Dell plumbing

Stage everything in `RAM:`. Ask the owner to install camd to `LIBS:` and
reboot. Hash-verify after the boot. Then run the N1 test with
`peer_ip=192.168.1.81`.

**Gate N2:** the same three checks as N1, on the Dell.

### N3 — the measurement

RIAPP on the Dell with `RIAPP_MIDI_CLKOUT=1` and
`RIAPP_MIDI_IN=applemidi.out.0`, a song at 140 BPM, and **at least 5 minutes**
of play. On the host, a small script reads the `rtpmidid` ALSA port and
timestamps every `0xF8` on arrival, using a monotonic clock in µs.

Report:
- count, mean, standard deviation, min, max, p99 and the worst gap of the
  intervals (theory: 17857.14 µs);
- the same run's sender-side `mean_us` from the RIAPP log, next to it;
- the xruns from the RIAPP heartbeat lines (clock out must not cost audio).

Run it **twice**. One run is not a result.

**Gate N3:** two runs with all of the numbers above. **No pass/fail
threshold is set.** This phase produces the number, and the owner judges it.

### N4 — write-up

- Evidence goes to `docs/evidence/midi/r12-*.md`, with the raw logs beside it.
- Update the assessment's status line and the interop spec's R12 entry.
- Add a ledger entry.
- Name everything that was **not** proven. In particular: inbound clock (P1)
  stays parked, and nothing here tests a real DAW (T3 Ardour is a separate,
  later step).

## 3. Stop and ask the owner when

- the camd install needs the reboot;
- anything has to be installed on the host;
- an ABIv11 build fails;
- a result contradicts the assessment. Say which statement it contradicts.
