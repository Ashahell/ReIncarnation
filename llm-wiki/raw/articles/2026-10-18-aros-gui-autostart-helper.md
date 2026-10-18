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
- **A lock file that grew instead of being rewritten — and the cause was a
  caller constant, not the platform.** `hb` was refreshed with
  `Seek(s_hb, 0, SEEK_SET)`, and the two vocabularies disagree:
  `dos/dos.h` defines `OFFSET_BEGINNING/CURRENT/END` as **-1/0/1**, while
  `SEEK_SET/CUR/END` are the **stdio** names and values **0/1/2**. So the
  call was `Seek(fh, 0, OFFSET_CURRENT)` — zero bytes from the current
  position — and every refresh appended: 8 bytes each, unbounded, with the
  file's own timestamp never advancing, so a directory listing showed a
  plausible size for hours. All three stdio names are wrong for dos.library,
  and not in the same way: `SEEK_CUR` (1) means *end of file* there, and
  `SEEK_END` (2) is not a valid mode at all. The fix was to stop writing —
  the file's contents were never read by anyone, its only job was to be
  *open* — and `scripts/ri_audit.sh` now fails on any of the three stdio
  names in a file that calls dos.library `Seek()`.

  **This was the third time in one session** that a caller mistake was
  recorded as an "AROS does X" fact (the others: `Rename`'s return value,
  and `Lock()` appearing to block). The rule that now goes with it: before
  claiming the platform did something surprising, rule out the caller —
  constants from the wrong header, return-value conventions, a NULL lock
  passed where a lock was expected, an `IoErr()` read after other calls have
  already overwritten it. Every one of the three had a dull, local cause.