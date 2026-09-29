# Post-reboot lane recovery record (spooler + worktrees)

- Source: Host reboot recovery session, 2026-09-29
- Collected: 2026-09-29
- Published: 2026-09-29
- Related: `2026-09-29-zoom-guard-proof-spooler-fix.md`

## What survives a host reboot

- `spike-laptop.service` (enabled): recreates `/tmp/spike_spool_laptop`, serves :9292. Survived because the unit + `~/.config/spike/pairs.e6320.json` master live outside `/tmp`.
- `spike-s6.service` (now enabled too): same for the x lane (:9294, pair e6320x). Was `disabled` before — a second reboot would have silently dropped the proof lane.
- Bulk port 9092 opens on demand; nothing to start.

## What does not survive (rebuild checklist)

- `/tmp/opencode/*` worktrees and binaries (v4s6, probes, staged Dell binaries). The v4 commits survive in the main repo object store even when the worktree dir is wiped — recreate with `git worktree add --detach <dir> <sha>`.
- Uncommitted work in wiped worktrees is LOST (the atcp_ui agent fix had to be re-applied from context once; the rawkey-qualifier CLI twice). Commit early.
- Dell RAM: is wiped (staged binaries, probe, logs). `SYS:ATCPBIN` on disk is the fallback; its hash is on file (`8d7bc8b6…`).
- The Dell agent never redials on its own after a reboot — the owner runs `SYS:ATCPBIN agent 192.168.1.81 9292 e6320` by hand.

## S6/S7 tooling state after this recovery

- v4s6 at `4f03fd7d`: --ui-press/--ui-release, agent WRITEEVENT+qualifier fix, both JSON bool-parser fixes, --ui-rawkey qualifier (`CODE[,up][,qQ]`). Unit-tested 32/32, still unpushed (push rides with lane proof).
- Dell agent binary rebuilt with all fixes (`ATCPBIN.new`, 119136 B), awaiting deploy.
