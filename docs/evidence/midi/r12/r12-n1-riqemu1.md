# R12 N1 — riqemu1 plumbing (2026-10-19)

## What went in (all hash-verified on the guest)

| item | host | guest | size |
|---|---|---|---|
| `DEVS:Midi/applemidi` | `r12netmidi-v1-build/.../Devs/Midi/applemidi` | `DEVS:Midi/applemidi` | 20768 B, `$VER: AppleMIDI CAMD driver 1.0 (12.10.2026)` |
| `SYS:System/Network/Services/netmidid` | `.../System/Network/Services/netmidid` | same | 127120 B |
| `camd.library` (**staged only, NOT installed**) | `.../Libs/camd.library` | `RAM:CAMDLIB` | 53528 B, `$VER: camd.library 41.1 (19.10.2026)` |
| `ENV:NetMIDI` | — | `ENV:NetMIDI` | `local_port=5004`, `peer_ip=10.0.2.2`, `peer_port=5004`, **`accept=10.0.2.2`** |

Every `--put` returned `sha_ok=True`, and the one file read back
(`ATCPBIN2`) matched the host sha256 exactly.

`accept=10.0.2.2` is the plan's security rule (§1): without it netmidid takes
anyone's invitation.

`SYS:System/Network/Services` did not exist on riqemu1 and was created — the
Dell has the same layout question and the same answer is needed there.

**`camd.library` is deliberately NOT installed.** The running one is
`41.1 (8.10.2026)`, i.e. the pre-#1585 build, so **no `DEVS:Midi` driver is
found on 64-bit** until camd is replaced and the machine rebooted. That is the
owner-reboot gate in the plan (§3). Installing it was not attempted, so the N1
gate **cannot be reached yet** even with a healthy agent.

## Host peer — ready and verified

`rtpmidid v24.12~2~gb306a` installed from AUR (`yay`), running:

- RTP-MIDI listening on `:::5004` / `:::5005`, name `ReIncarnation-peer`.
- ALSA sequencer port present: `aseqdump -l` shows
  `128:0  ReIncarnation-peer  ReIncarnation-peer`, and
  `/proc/asound/seq/clients` lists client 128 as `[In/Out]`.

So the far end of the measurement exists and `aseqdump` can read it. One
harmless startup error: the control socket dir `/var/run/rtpmidid` does not
exist, so `--control` bind fails. It is not needed for this test.

## Lane state

- **riqemu1 is running** at **1280x1024**, verified by monitor `screendump`
  before any work and again after (the lane rule). Re-verified at 10:58 after
  the wedge: still 1280x1024, `VM status: running`, 99.7% non-black — a live
  Workbench, not a Guru screen.
- The **agent is wedged**, not the machine. See below.

## The wedge — my mistake, and the rule it teaches

I ran `ping 10.0.2.2 > RAM:ping.txt` over the spike exec lane. `ping` **blocks
until it answers**, so the agent's exec child never returned and the agent
stopped answering anything:

```
[submit] error: ... a command that never finishes stops it answering anything.
       Confirm with the serial log: it will have stopped advancing.
       C:Reboot will NOT help (it is itself an exec). Needs an owner
       reboot in the Workbench window.
```

That message is the lane's own correct diagnosis (`ATCP_bounded_exec_lane.md`
T1), and the guest confirms it: QEMU is alive and the desktop renders. Only
the agent is gone. The serial log is empty, but that is expected for this
image — it does not route kernel `bug()` to serial — so "serial stopped
advancing" is not a usable signal here; the monitor screendump is.

**This is my error, and it is avoidable.** I used a *blocking* network command
through an exec channel that the project explicitly documents as
wedge-sensitive, on a lane whose recovery is an owner reboot. Two rules:

1. **Never issue a blocking command over the spike exec lane.** `ping` is the
   obvious one. The `aros-development` skill already warns that AROS `ping`
   "returns a WARN-level RC even on a SUCCESSFUL reply — don't gate a script
   on its return code"; that is a different trap from the blocking one, and I
   walked into the second while reading past the first.
2. **This lane's recovery costs an owner action.** A QEMU restart is cheap and
   I could do it, but the agent is launched by `S:User-Startup`; recovering
   needs the owner at the Workbench window, or a lane restart they have not
   asked for.

## Gate status

**N1 gate NOT reached.** None of the three checks ran:

- session reports connected on both sides — not shown;
- `MIDISEND` to `applemidi.out.0` appears in `aseqdump` — not run;
- `aplaymidi` moves RIAPP's `NOTEIN played=` — not run.

Two independent blockers, either of which alone is enough:

1. **`camd.library` not installed** — needs the owner reboot (plan §3). Until
   then `DEVS:Midi/applemidi` is never opened, and it is inert alone by design
   (it holds no network code; the service attaches to its adapters).
2. **The agent is wedged** — needs an owner reboot in the Workbench window.

Nothing about the *builds* is in doubt: all four ABIv1 artifacts are built and
staged, the host peer is live, and the config is correct. This phase is
blocked on two reboots, not on a defect.