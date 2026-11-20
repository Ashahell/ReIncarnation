# Dell E6320 ABIv1 deployment

> Sources: ReIncarnation session, 2026-11-20
> Raw: [2026-11-20-dell-deployment-abiv1-binaries-atcpbin-riapp-midisend-midiclock.md](../../raw/articles/2026-11-20-dell-deployment-abiv1-binaries-atcpbin-riapp-midisend-midiclock.md)
> Updated: 2026-11-20

## Overview

The Dell E6320 (E6320) is an ABIv1 machine — it runs the aros-development-team-built toolchain+SDK (provisioned into `src/abi/v1`), not the ABIv11 v11 SDK. Previous v11 builds crashed at startup with "Software Failure!" before any log line. The v1 build (`ri_build_aros.sh`) uses the aros-development-team-built toolchain+SDK provisioned into `src/abi/v1`.

## Binaries deployed and verified

### ATCPBIN (v1) — 126,816 bytes
Built by `scripts/build_atcpbin_fix.sh` with `AROS_ABI=v1 AROS_TOOLCHAIN_FAMILY=llvm`. Zero unresolved symbols. Agent connects to spike server on port 9292 (pair name: e6320). Copied to `SYS:ATCPBIN` for persistence across reboots.

### RIAPP (v1 mixed build) — 1,266,384 bytes
Built by `scripts/ri_build_aros.sh riapp` against v1 SDK (library base in rdx). Mixed build: engine/ at -O2, app+GUI at -O0 (29 engine objects at -O2, 68 app/GUI objects at -O0). Build hash: 770f4bf. Runs from `SYS:RIAPP` — live panel window opens (1349x680 at 0,0). Audio: AHI low-level mode=0x003e0001, 48000 Hz, buffer=256 frames. Zero xruns: render_max=54 us, load=2-3/1000, overloads=0. Clean shutdown: buffers=12547, xruns=0, render_total=497 ms.

### MIDISEND (v1) — 70,392 bytes
Built by `ri_build_aros.sh sections` (includes MIDISEND target). Runs from `SYS:MIDISEND`. CAMD message-script sender for G7 remote-MIDI proof.

### MIDICLOCK (v1) — 31,856 bytes
Built manually with v1 toolchain + v1 SDK (clang 20, `--target=x86_64-unknown-aros`). Runs from `SYS:MIDICLOCK`.

## Deployment method

All four binaries copied to `SYS:` (USB stick SFS partition) for persistence:
```
Copy RAM:ATCPBIN SYS:ATCPBIN
Copy RAM:RIAPP SYS:RIAPP
Copy RAM:MIDISEND SYS:MIDISEND
Copy RAM:MIDICLOCK SYS:MIDICLOCK
```

The Dell boots from the USB stick (`SYS:` is the stick), so no mount needed.

## Key findings

- **Dell is ABIv1** — v11 binaries crash with "Software Failure!" at startup
- **V1 toolchain** — aros-development-team-built toolchain+SDK (clang 20) in `src/abi/v1`
- **Mixed build** — engine/ at -O2, app+GUI at -O0 is mandatory (owner-approved 2026-10-04)
- **Zero xruns** — RIAPP ran 12547 buffers with 0 xruns, render_max=54 us
- **ATCPBIN survives reboot** — `SYS:ATCPBIN` via `S:User-Startup`, agent redials after reboot

## See Also
- [MIDI push fix](../midi/midi-push-fix-t232.md)
- [Wiring census complete](../wiring/wiring-census-w0-w3-complete.md)