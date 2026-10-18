# Autostarting an AROS GUI app: WBStartup, not User-Startup

- Source: Dell E6320, ReIncarnation RIAPP install, 2026-10-18
- Collected: 2026-10-18
- Published: 2026-10-18
- Related: `2026-10-18-atcp-bounded-exec-dead-lane.md`, `2026-10-18-s7-skin-cycle-closed.md`

## The ordering fact that decides everything

`S:Startup-Sequence` runs, in order: assignments/patches → `S:User-Startup`
(executed near the end) → **`WANDERER:Wanderer`**. A GUI app launched from
`User-Startup` therefore runs *before there is an Intuition to open a
window on*. Wanderer executes the scripts in `SYS:WBStartup` after the
desktop is up — that directory is the supported autostart hook for a GUI
app, and it exists by default (empty on a stock install).

## Recipe that survives reboots

1. Put the binary somewhere persistent: `SYS:Classes/ReIncarnation/RIAPP`
   next to its data, never `RAM:`.
2. `SYS:WBStartup/RIAPP` (an AmigaDOS script):

   ```amiga
   If NOT EXISTS "ENV:ReIncarnation"
       MakeDir "ENV:ReIncarnation"
   EndIf
   Run >NIL: SYS:Classes/ReIncarnation/RIAPP
   ```

3. Persisted prefs need nothing: the startup-sequence copies `ENVARC:` into
   `ENV:` every boot (`Copy ENVARC: ENV: ALL …`), so a file written to
   `ENVARC:MyApp/…` is there next boot. The `MakeDir` guard only matters on
   a first boot that has no archive yet.
4. Verify by **running the boot path itself** rather than rebooting:
   `Execute SYS:WBStartup/RIAPP`. Then break any instance you were running
   by hand and confirm the survivor answers input — that proves the script,
   the path and the binary together.

## Trap: a box that mentions the cause is not a guard

Launching a second copy with one already up produced a requester, and after
dismissing it two full live panels existed side by side. The obvious
conclusion — "the app has a single-instance guard, and it only warns" —
was **wrong**: reading the source showed no such guard exists. The box was
the *audio* failure: the second instance opened its window and then could
not open `ahi.device` exclusively. Two lessons, both general:

- A requester that **names the real cause** is not a guard. Read the source
  before writing down what the app enforces; a confident wrong answer here
  cost a whole hardening cycle's worth of wrong reasoning.
- The hazard is still real wherever the exclusive resource is *not*
  exclusive: a null audio backend, or a device that permits sharing, lets
  the second instance run to completion, and two instances then write the
  same prefs. A real guard is an atomic claim of its own — directory
  creation is atomic (`CreateDir` on AROS, `mkdir` elsewhere), a heartbeat
  keeps a crashed holder from locking the instrument until the next boot,
  and the app must **exit** rather than warn. Run anyway when the claim is
  undecidable: an unopenable lock must never be the reason the instrument
  will not open.
- **Implemented and lane-proven** (ReIncarnation `9395d0d` plus the
  corrections after review): `ri_pal_claim_acquire/refresh/release` in the
  PAL, a claim on `RAM:`, and the app refusing to start on a busy claim.
  The AROS file semantics that shape it — including the two of my own
  claims that a raw-value probe had to retract — are written up in
  `2026-10-18-aros-file-semantics-single-instance-lock.md`.

## Facts worth keeping

- `SYS:WBStartup` is empty on a stock AROS install — nothing else will
  collide with a script you add there.
- Wanderer runs startup scripts with the CWD set to the startup directory,
  so use absolute paths inside the script.
- A running image does not hold a write lock: `Copy RAM:X SYS:Y` succeeds
  while `Y` is executing. Copy first, restart second, and read the file
  back with a hash — the copy is the step worth verifying.