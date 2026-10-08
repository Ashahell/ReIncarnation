# riqemu1 M0 proofs (2026-10-08, ABIv1 lane, agent anon)

Lane: QEMU `-boot c` DH0, 1280x1024 (monitor screendump verified after
every reboot below), spooler port 9295, monitor 4477. No other session's
lane touched (a `gpf` VM runs alongside on 4481).

## Deploy + reboot

- Before: `camd.library` 41.1, 53408 B (= the unpatched tree binary).
- Backup: existing `camd.library.orig` (G7 era, 53296 B) kept; current
  copied to `camd.library.m0orig`.
- After: fixed ABIv1 build, 53472 B, SHA-verified on PUT.
- Rebooted via monitor `system_reset`; agent reconnected; screendump
  still 1280x1024. (One self-inflicted kill: a monitor `quit` sent by
  a scratch script — relaunched, recipe now uses a quit-free helper.)

## G7 proof re-run (fixed library)

`RISECT remote` (fresh ABIv1 build, 653864 B) + `MIDISEND ri.remote`
(34152 B) + `2026-09-26-remote1.mid.txt`, all PUT to RAM:.

- `MIDISEND: 41 messages to ri.remote`, all through real CAMD.
- Trace (`riqemu1-m0-reremote-trace.txt`, from `T:RISECT.LOG` — the PAL
  temp path, not `RAM:`): `BD8 x...x...x...x...` (steps 1/5/9/13),
  `MIDI 41/1` (channel-2 Play ignored once), `N 69` Play starts
  transport (`ST 1`, head advances to `PH 15/15` and wraps),
  `CAMD 41/248.0` (41/41 delivered), focus `SEL8 1 P909 7`.
- Screenshot `riqemu1-m0-g7.png`: 808 BD steps lit, transport playing
  at 120 BPM, `MIDI 41/1`, `CAMD 41/248.0`.
- CC 38/17 knob laws are pinned host-side by t73 (unchanged path);
  routing — the load-bearing part for M0 — is proven above.

## debugdriver un-park test: PASS

- `DEVS:Midi/` was empty; driver parked at `SYS:debugdriver.parked`.
- Disk image backed up first (`riqemu1_dh0.m0bak.img` in /tmp/opencode).
- Un-parked (`Copy` to `DEVS:Midi/debugdriver`), rebooted: boot did
  NOT block, agent reconnected on first boot.
- `MIDISEND m0dbg SELFTEST` with the driver present: `connected: 1,
  1`, readable cluster names (`72 69 2E 72` = `ri.r…`, `6D 30 64 62`
  = `m0db…`), echo `90 45 64` + `B0 19 40` received (`got 2`, rc=0).
  The exact inverse of the Dell's broken output.
- Lane state left: debugdriver UN-parked (the fix removes the reason
  it was parked); `camd.library.m0orig` backup kept.

## ABIv11 build (for the Dell)

Fresh build dir `core-pc-x86_64-m0` configured against the installed
16.1.0 toolchain (the Sep-14 tree is pinned to the gone 10.5.0):
`make workbench-libs-camd` EXIT 0 with the v11 carriage patches
applied (tree sources reverted after, pristine).

- Binary: 53816 B, `$VER: camd.library 41.1 (8.10.2026)`, sha256
  `29b47ce2…`. `mysprintf` calls `VNewRawDoFmt`; nested calls pass
  the base in R12 — the v11 convention (v1 binary uses RDX).
- Staged to Dell `RAM:camd-fixed` (SHA OK); Dell `LIBS:` swapped
  (backup `camd.library.m0orig`, 54904 B); awaiting owner reboot,
  then the SELFTEST re-probe (`connected: 1, 1`, readable names,
  `got 2`, rc=0 expected).
