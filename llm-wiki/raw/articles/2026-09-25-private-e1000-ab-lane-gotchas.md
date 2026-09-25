# 2026-09-25 — Private e1000 A/B test lane: recipe and gotchas

> Source: session work building the rie1k QEMU lane for the e1000 wedge root cause
> Collected: 2026-09-25
> Published: 2026-09-25
> Companion: [2026-09-25-e1000-wedge-root-cause-freemem-in-irq.md](2026-09-25-e1000-wedge-root-cause-freemem-in-irq.md)

## Disposition
New (method + lane gotchas; no product code).

## Recipe (v1 ABI, reproducible A/B of a NIC driver)
- QEMU: blank raw disk at IDE index 0, v1 live ISO (`distfiles/aros-pc-x86_64.iso`) at index 1, test ISO at index 2, `-boot d`, `-device e1000` (or `rtl8139`), `-display none`, monitor on a private TCP port, serial to file.
- The v1 live ISO's `S:User-Startup` runs `CD1:AGENT.AROS` when present; the test ISO carries `AGENT.AROS`, `NETUP.AROS`, `AROSTCP_IF`, `AROSTCP_ROUTES`, `ATCPBIN`, the driver under test, and tools.
- Private spike server in tmux (`serve --port 9297 --bulk-port 9298 --spool /tmp/spike_spool_e1k`); `AGENT.AROS` dials 10.0.2.2:9297.
- Stress that exposes allocator races: guest `memstress` (AllocMem/FreeMem hammer with fill-pattern verification, `patches/e1000-dell-txpool/memstress.c`) run detached, plus per round one scale-2 `ui_capture` and one 845 KB bulk `--get`.

## Gotchas (each cost a boot)
- **No disk at IDE index 0 → CDs not detected** ("Waiting for bootable media"; serial `ata_setup_unit: setup failed`). A 64 MB blank raw disk fixes it.
- **Driver file name must stay `e1000.device`.** Loading it as `CD1:E1K.device` or `DEVS:networks/e1k.device` fails because the module registers under its resident name; AROSTCP then reports a misleading "Fatal error in NetDB file interfaces … Memory exhausted" at the end of the `net0` line. Working form: `copy CD1:E1K.device RAM:e1000.device` + `DEV=RAM:e1000.device`.
- **v11 live ISO does not boot under QEMU here** (the unmodified `src/abi/v11/.../distfiles/aros-pc-x86_64.iso` stays black, empty serial) — v11 binaries could not be VM-tested; the Dell is the only v11 runtime.
- **v11 build tree:** `make workbench-devs-networks-e1000-quick` fails with "No rule to make target …/toolchain-core-x86_64/lib/gcc/x86_64-aros/…" — unrelated to the e1000 change.
- `pkill -f "qemu … -name rie1k"` from a compound shell kills the shell itself (pattern matches its own command line); select the PID by `ps` and `$2 ~ /qemu-system-x86_64/`.
- Remote reboot of the Dell works: `Run >NIL: C:Reboot` via the agent (job stays "busy"; delete it from the spool); ping returned in ~90 s, agent in ~110 s.
- `version DEVS:networks/e1000.device FULL` returned normally on the Dell 2026-09-25 (earlier record said `Version <ELF-binary>` killed the agent 3/3 — not reproduced this time).
- `netstat -i` on the Dell took 70151 ms (use `netstat -n`-style numeric options or avoid).
