# ATCP dead lane: the bounded-exec switch is the discriminator (Dell, 2026-10-18)

- Source: Dell E6320 lane sessions e6320/e6320x, 2026-10-18 (ReIncarnation S6 follow-up)
- Collected: 2026-10-18
- Published: 2026-10-18
- Related: `2026-09-29-s6-closed-s7-proven.md`, `2026-09-29-exall-acceptance-forensics.md`

## The symptom that lies

- Reboot, agent dials, `--ping` answers, file `get`/`put` transfers fine — and **every** `--exec`/`--ui-*` job fails: `ProtocolErr: exec_result carries no 'proto' field: this agent predates the bounded exec lane (needs proto>=2)`.
- Two agents, two pairs, two ports, all identical: `SYS:ATCPBIN` 121,864 B (`556c72c3…`) and a click-fixed 119,136 B build (`fe1fc3ea…`). Both predate the lane.
- So "the agent is wedged" and "the reboot broke the lane" are both wrong conclusions available at zero cost. The split **file transfer works / exec refused** is the signature of a *build* difference, not a runtime one.

## The one-line fix

- Build from current main **with** the switch: `ATCP_EXTRA_CFLAGS="-DATCP_BOUNDED_EXEC=1" bash scripts/build_atcpbin_fix.sh OUT` (0 unresolved GLOBAL UND gate holds). Default build has the switch **off** and looks perfectly healthy.
- Verify by behaviour, not size: `exec "List SYS:"` succeeds only on a bounded-exec build. (The same tree also carries the injected-click fixes, so one binary serves input + exec.)

## Boot path facts (Dell, USB stick)

- `S:Startup-Sequence` executes `S:User-Startup`, whose last line is `Run SYS:ATCPBIN agent 192.168.1.81 9292 e6320` — the boot-visible name is `SYS:ATCPBIN` on the system volume, **not** a file on `S:`. Replace that path and the next boot is fixed with no hand work; `S:` needs no rename.
- Copy it while the old agent runs: `Copy RAM:ATCPX SYS:ATCPBIN` succeeded (the loaded image does not hold a write lock). Read the file back and hash it — copying is the step worth verifying.
- One agent can start the other: `Run >NIL: SYS:ATCPBIN agent <host> <port> <pair>` over the exec lane, after draining the old session with `--bye`.

## Hazard: a directory `get` wedges the command lane

- `--get SYS:` (a directory, not a file) never returned; the agent then stopped answering `ping`. Only an owner restart recovered it. Same family as the known malformed-`--exec` wedge.
- On these agents: plain file `get`/`put` only for transfer; enumerate with `exec "List …"` (bounded-exec build). Treat any non-file path as a lane-ending mistake.

## Proof that followed the repair (evlog-pinned)

- Tabs `TAB page=1 → 0 → 3`; rail `VIS dev=2 show=0 mask=1b → show=1 mask=1f`; knob drag `CTL 021b=8`; renders follow the toggles.
- Coordinates are per-build: tabs y≈167 (Synths 70, Drums 130, Mix 250), rail y≈128, 808 CUT OFF ≈(420,256). Re-ground from a capture, never from a previous build's numbers.