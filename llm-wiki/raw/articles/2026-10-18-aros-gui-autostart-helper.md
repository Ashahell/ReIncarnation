# Autostarting an AROS GUI app when Wanderer's hook does nothing

- Source: Dell E6320, three reboots, 2026-10-18 (ReIncarnation RIAPP)
- Collected: 2026-10-18
- Published: 2026-10-18
- Related: `2026-10-18-aros-file-semantics-single-instance-lock.md`, `2026-10-18-atcp-bounded-exec-dead-lane.md`
- Evidence: `docs/evidence/gui/hw-look/README.md` (boot section)

The goal: RIAPP starts by itself when the machine boots. What follows is the
route that actually works, and the two that look right and do not.

## The ordering fact that decides the design

`S:Startup-Sequence` runs `S:User-Startup` **before** `WANDERER:Wanderer`.
So a GUI app launched from `User-Startup` has no Intuition to open a window
on. The wait has to happen after the desktop is up — which is exactly what
Wanderer's startup hook is for.

## Two paths that look right and are not

- **`SYS:WBStartup/`** (AmigaOS's Wanderer startup directory). Installed,
  present, correct, never executed.
- **`S:WBStartup/`** (the startup volume, where `User-Startup` and
  `Startup-Sequence` live). Also never executed.

Reading the binaries ended the guessing:

```
strings SYS:System/Wanderer/Wanderer        -> "Wanderer:Tools/ExecuteStartup"
strings SYS:System/Wanderer/Tools/ExecuteStartup
  -> "sys:WBStartup/"  "sys:WBStartup/?#?.info"  "Couldn't examine sys:WBStartup/"
```

So AROS *does* have the hook, the first location was right, and it expects a
matching `.info` next to each script. But running it by hand starts
**nothing** on this install — verified twice: with a bare script, and with an
icon copied from `SYS:Utilities` (if the icon were honoured, a Utilities
window would have appeared). Neither did anything.

The transferable part: **an installed-looking autostart that silently does
nothing is worse than none.** Those files were deleted rather than left as
decoration for the next person to trust.

## What works: a helper that waits, then executes

40 lines of C, `SYS:Classes/ReIncarnation/RIBOOT`:

```c
Delay((LONG)TICKS_PER_SECOND * secs);        /* secs = 12, say */
rc = Execute("SYS:Classes/ReIncarnation/RIAPP", NULL, NULL);
```

and one line in `S:User-Startup`:

```
Run >NIL: SYS:Classes/ReIncarnation/RIBOOT 12
```

`Execute` blocks until the app exits, so the helper stays alive as its parent
(hence a `RAM:RIBOOT.LOG` you cannot read while the app runs — the file is
held open by the very process that logs it; that is the platform's locking
rule again, not a bug).

Proven in two steps: by hand first (`RIBOOT 6` brought the app up as a new
process), then across a real reboot, where the app came up as **Process 1** —
the first process on the machine — launched by the helper.

## Two smaller facts from the same session

- **`SYS:ATCPBIN` survives a reboot** — verified by reading it back and
  hashing after two reboots. An earlier session had seen it revert; whatever
  did that, it does not now. Verify by hash after a boot rather than by
  trusting either story.
- **`Seek()` does not reposition a buffered dos handle here.** A lock file
  rewritten with `Seek(0)` + `Write` appended instead: 8 bytes per refresh,
  unbounded, and the file's own timestamp never advanced, so a directory
  listing showed a plausible-looking size for hours. The fix was to stop
  writing: the file's contents were never read by anyone — its only job was
  to be *open*. **A file that is written but never read is doing something
  you have not asked it to do.**