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

## Trap: the single-instance requester does not enforce

Launching a second copy with one already up produced a requester ("Another
instance is already running…") whose OK button **did not stop it**: after
dismissing it, two full live panels existed side by side, both writing the
same prefs file. A guard that only warns is worse than none once two
instances can write shared state — use a lock file or `prevent` on the
shared file, or make the second instance exit after the requester.

## Facts worth keeping

- `SYS:WBStartup` is empty on a stock AROS install — nothing else will
  collide with a script you add there.
- Wanderer runs startup scripts with the CWD set to the startup directory,
  so use absolute paths inside the script.
- A running image does not hold a write lock: `Copy RAM:X SYS:Y` succeeds
  while `Y` is executing. Copy first, restart second, and read the file
  back with a hash — the copy is the step worth verifying.