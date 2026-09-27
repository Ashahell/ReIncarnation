# Dell lane bridge recovery: reboot-proof spooler (2026-09-27)

- Source: ReIncarnation session, 2026-09-27 (frostmourne reboot recovery)
- Collected: 2026-09-27
- Published: 2026-09-27
- Lane canonical: Vulkan4AROS `llm-wiki/raw/articles/2026-09-22-laptop-abiv11-real-hardware-ahi-probe.md` (serve `--port 9292`, pairs `e6320`, redial `SYS:ATCPBIN agent 192.168.1.81 9292 e6320`, ufw/nft notes)

## What happened

Two frostmourne reboots (iGPU crash) wiped `/tmp`: the spike bridge,
the spool (`/tmp/spike_spool_laptop`), the v11 build tree, and the
`/tmp/ri/build_v11.sh` helper. The Dell agent dials `:9292` with pair
name `e6320` — a bridge on the default `:9091` serves the right spool
through the wrong door (zero connection attempts; owner-reported errno
61). The wiki named the right port; ufw already allows the Dell
(`192.168.1.60` full pass + `9292/tcp`); NordVPN was disconnected (no
nft interference).

## Recovery recipe (now in place)

- Bridge: `serve --port 9292 --spool /tmp/spike_spool_laptop --pairs
  /tmp/spike_spool_laptop/pairs.json` (`[{"name":"e6320","spool":"/tmp/spike_spool_laptop"}]`).
- Boot persistence: `~/.config/systemd/user/spike-laptop.service`
  (linger enabled) with `ExecStartPre` recreating spool dirs + pairs
  (both live in `/tmp`, wiped every boot); `Restart=always`.
- Build script: persistent copy `~/bin/build_v11.sh`, symlinked as
  `/tmp/ri/build_v11.sh` (recreated twice before learning).
- Repo rule reinforced: everything load-bearing lives in git; `/tmp`
  holds only regenerables (binaries, objects, captures, audit logs).
  Committed evidence (`docs/evidence/`, wiki records) survived both
  reboots intact; uncommitted lane screenshots did not.

## Known wart

Two bridge starters race for `:9292` at boot (EADDRINUSE + burst
backoff observed once); exactly one correct instance serves. Do not
run a second serve on the same spool. If the lane goes quiet, check
`ss -tln | grep 9292` and the agent redial before anything else.
