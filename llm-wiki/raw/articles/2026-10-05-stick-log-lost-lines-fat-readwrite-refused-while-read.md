# Lost log lines on the stick: FAT refuses a read-write open while a reader holds the file — RIAPP.LOG now keeps one handle (2026-10-05)

- Source: ReIncarnation session (advisor lane), 2026-10-05, Dell E6320 (ABIv11). Commit `fdcea19` (pushed), deployed to `Vk4aros:ReIncarnation/RIAPP` (previous `0628f4e` kept as `RIAPP.prev`).
- Collected: 2026-10-05
- Published: 2026-10-05
- Related: [2026-10-05-rail-page-group-and-log-newlines.md](2026-10-05-rail-page-group-and-log-newlines.md), [2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md](2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md)

## Owner check of the old bugs (build 0628f4e)

- **Owner's test:** the owner opened the right-click menus during Zombie Nation (hold over Songs/View, quick clicks) and switched tabs. Menus worked, no freeze, and the music played fine.
- **Scripted tab cycles at the start:** 21–33 ms per switch, xruns+0.
- **The odd log.** The stick log of that run ended with `RIAPP closed: buffers=18037` (96 s of audio). Its heartbeats stopped about 96 s in, although the app ran for about 9 minutes. The owner confirmed no restart and no second instance.
- **Repro with `RIAPP_LOG=RAM:`.** A heartbeat every 15–16 s across the menu window (owner opened the menus around 22:22):
  - buffers 2934 → 35767;
  - xruns 0, overloads 0;
  - render_max 4101 µs, wake_max 73 µs;
  - prio 21, load 312–730/1000.
- **Conclusion:** the earlier gap was lost log lines, not a stall.

## Two traps in reading logs live

1. **`--exec "Type <log>"` output is capped by the agent.** A live poll that greps the tail of `Type` sees a frozen log. Use `--get` (bulk, sha-verified) for anything that matters.
2. **Lines were lost on the stick.**
   - The old `rlog` did `Open(MODE_READWRITE)` / `Seek(END)` / `FPuts` / `Close` per line, and returned silently when the open failed.
   - Probes on the Dell (small ABIv11 test programs):
     - `APPTEST`: 3000 open-append-close cycles to `Vk4aros:` and to `RAM:`. All 312000 bytes read back on both, so plain appends work.
     - `HOLD` (reader, `MODE_OLDFILE`) holding `Vk4aros:apptest.log`, then `TIMED` (one append): `OPEN FAILED after 0 ticks`. The same on `RAM:`: `ok`.
     - `WHOLD` (writer holds `MODE_READWRITE`, writes and `Flush`es a line every 0.5 s) while `HOLD` opens the file: the reader opens (`hold ...: open`) and all 20 held writes land (21 lines with the header).
   - Rule: on this stick's FAT handler, a read-write open is refused while another process has the file open. A reader may open a file a writer already holds.
   - Every `Type` or evidence pull of `RIAPP.LOG` during a run therefore dropped lines. A `Type` blocked behind the agent's output cap could hold the file for minutes.

## Fix (fdcea19)

- **One handle.** `rlog` opens `RIAPP.LOG` once, `Flush`es every line and keeps the handle (`s_logfh`).
- **Lines wait instead of dropping.** If the open is refused, lines wait in a 16 KB buffer and go out on the next successful open. Overflow is counted and written as `RIAPP log: N lines lost while the log was busy`.
- **Recovery.** A failed write or Flush (stick pulled) closes the handle, so the next line reopens. A changed `RIAPP_LOG` path also reopens.
- **The roll note.** `log_roll_if_big`'s decision note ("session start ... keeping/ROLLING") used a separate open and was lost the same way. It now goes through `rlog`'s pending path.
- **Closing the handle.** `atexit` is not in the v11 link (`U atexit`), so `main` wraps the old body (`riapp_main`) and closes the handle on every return path.
- **Wall clock.** The heartbeat now writes `RIAPP hb: t=hh:mm:ss` from `DateStamp`, so a gap shows as a gap.
- **A -Werror trap in the audit's v1 build:** `strncpy` into a same-size buffer triggered `-Werror=stringop-truncation`. Use `snprintf(dst, sizeof dst, "%s", src)`.

## Verification (Dell, log on the stick)

- **During a run with readers:** two 40 s `HOLD`s plus a full `Type` of the log. Heartbeats arrived every 16 s (t=05:00:56 … 05:02:32) and none were missing; `closed: buffers=22004`.
- **Reader holding the log before RIAPP starts:** the first open is refused. Every startup line still arrives (build, audio, panel, then heartbeats), and so does `RIAPP log: session start, 164396 B of 262144 -> keeping`.
- **Audit:** AUDIT 0/0 PASS (isolated paths).
- **Deploy:** the log reads `RIAPP LOG build=fdcea19 diag=0`.
- **Cleanup:** the test programs and test files were removed from the Dell.

## Implication for earlier records

Any Dell `RIAPP.LOG` on the stick taken while something read the log mid-run may be missing lines. That covers live `Type` polls and mid-run evidence pulls. Absence of a line in such logs proves nothing before `fdcea19`. `RIAPP-EV.LOG` always kept one handle (`s_evfh` with `Flush`) and was not affected.
