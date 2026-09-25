# riqemu1 boots its InstallAROS DH0 with a visible agent console; aros_v1j moved to rtl8139

- Source: ReIncarnation session work 2026-09-25 (lane operations, no repo code)
- Collected: 2026-09-25
- Published: 2026-09-25
- Cross-post: Vulkan4AROS llm-wiki `raw/articles/2026-09-25-dh0-installed-lane-visible-agent-startup.md`

## riqemu1 (private RI lane, ABIv1, spool /tmp/spike_spool_priv, port 9295, monitor 4477)

- InstallAROS was run by the owner onto `riqemu1_dh0.img`. DH0:S/Startup-Sequence is byte-identical to the Live CD's, so no assign was missing; after the DH0 boot every assign (SYS:, C:, S:, LIBS:, DEVS:, Developer:, BIN:, INCLUDE:, LIB:, ENVARC: …) resolves to `AROS:` (the installed volume).
- Installed on DH0: `C:ATCPBIN` (= `Vulkan4Aros/artifacts/e1000-txfix-20260925/ATCPBIN.v1`, 114400 B, the session-desync fix), `NET/{NETUP.AROS,AGENT.AROS,AROSTCP_IF,AROSTCP_ROUTES}` (rtl8139, 10.0.2.15, gateway 10.0.2.2), new `S:User-Startup`; the original is kept as `S:User-Startup.cd`.
- `S:User-Startup`: `Run <NIL: >"CON:0/400/640/180/ReIncarnation agent (ATCPBIN)" QUIET Execute DH0:NET/AGENT.AROS`; `AGENT.AROS` runs NETUP then `RAM:ATCPBIN agent 10.0.2.2 9295` in the foreground, so its `[atcp] …` lines stay visible in that window. Falls back to `CD1:AGENT.AROS` when the DH0 copy is missing.
- Launcher: `/home/miller/Work/vms/start_riqemu1.sh` (`-boot c`, Live CD no longer attached so SYS: is always DH0; test ISO kept for file delivery).
- Verified: agent back after `system_reset`; Status shows AROSTCP + RAM:ATCPBIN + Wanderer; 4 MB put PASS; screendump shows the agent window on a clean desktop.
- Installed system runs 1280x1024: agent `ui-capture` at scale 1 is refused ("screen too large at this scale") — use scale 2 or monitor `screendump`.

## aros_v1j (juggler lane, spool /tmp/spike_spool_jug, monitor 4467)

- Switched e1000 → rtl8139 after the idle watcher reported it idle (3 h since the last result). `DH0:NET/AROSTCP_IF` now names `rtl8139.device` (backup `AROSTCP_IF.e1000`); QEMU relaunched with identical args except `-device rtl8139,netdev=net0`.
- Verified: agent back (session 41), processes normal, 4 MB put arrived at full size. No launch script references aros_v1j, so a manual relaunch must keep `-device rtl8139`.
