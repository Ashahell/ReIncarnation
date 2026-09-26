# Lane rule: a bare `Assign NAME:` probe REMOVES the assign (the "insert volume SYS" mechanism)

- Source: Vulkan4Aros commit `e0b43a4c` + record `llm-wiki/raw/articles/2026-09-24-assign-probe-removes-sys-dh0-boot-lane.md`
- Collected: 2026-09-26
- Published: 2026-09-24

## Verdict

In AmigaDOS/AROS, **`Assign NAME:` with no target REMOVES the assign**; it does
not display it. The recurring "required assigns aren't set when we boot from a
fresh hardfile" was not a boot defect: a fresh DH0 boot sets every assign
(`rom/dos/cliinit.c` does `AssignLock("SYS", lock)` on the boot volume, then
derives `C`, `LIBS`, `DEVS`, `L`, `S`, `FONTS` from `SYS:`). What removed SYS:
was the diagnostic probe itself — `Assign SYS:` printed nothing (rc 0) and
deleted SYS:. The assign lists before and after the probe differed only in the
`SYS AROS:` line. The next SYS: access raised the modal "Please insert volume
"SYS" in any drive" System requester, which also blocks the single-threaded
ATCP agent, so jobs time out and the agent's replies arrive out of step
afterwards.

## Permanent fix (Vulkan4Aros `scripts/spike_server.py`)

`submit` (exit 2, before anything is spooled) and `run_job` (server side, so
any client is covered) refuse the implicit-removal form. Allowed: plain
`Assign`, `Assign NAME: EXISTS` (rc 5 when missing), any assign with a target,
and an explicit `REMOVE`/`DISMOUNT`. Redirections are ignored when classifying,
so `Assign SYS: >RAM:x` is still refused. Other lanes' spike servers pick up
the server-side check at their next restart.

## Recovering a wedged lane

Dismiss the requester with the monitor `sendkey esc`. If replies are out of
step (a job's result carries the previous command's output), restart the lane's
spike server: the old session number stays attached until the server restarts.
A cold VM restart alone did not clear it.

## Consequences for ReIncarnation lanes

- The riqemu1 record asserts every assign resolves after the DH0 boot — that
  invariant is exactly what a bare-`Assign` probe would silently break. Never
  probe assigns with the bare form on riqemu1, the Dell, or aros_v1j; use
  `Assign NAME: EXISTS`, plain `Assign` to list.
- Modal requesters wedging the agent is already our standing lane hazard
  (2026-09-22: RequestChoice + foreground Sashimi wedge the agent; operator
  Enter/close-gadget, redial displaces). This mechanism adds one more producer
  of such requesters, and unlike a stuck GUI it is self-inflicted by the probe.
