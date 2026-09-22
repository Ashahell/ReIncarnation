# 2026-09-22 — ABIv1 acceptance lane proven (P1/P2 reproduce Appendix A); full green blocked on post-fix void Stop rebuild

> Source: session evidence (guest result blocks, serial fault lines, Guru OCR, build artifacts), compiled by agent
> Collected: 2026-09-22
> Published: 2026-09-22

## Disposition
New. ~~Closes #3 as "lane proven, full-green scheduled" (NOT "Appendix A
reproduced end-to-end" — P3/P4 remain Stop-blocked, see below).~~
SUPERSEDED 2026-09-22 (same day): full green ACHIEVED — see
`2026-09-22-abiv1-full-green-one-byte-hookentry-patch.md`. The
"post-fix void rebuild" scheduled below was the WRONG fix (void
exonerated by disassembly + identical deadlock in both builds); the real
fix is the 1-byte HookEntry patch in the ahi.device shadow. This
article's P1/P2 proof, recipe proof, and failure chain 1–3 stand.

## TL;DR
The v1 toolchain path produces runnable ABIv1 binaries and P1/P2
reproduce Appendix A exactly on the QEMU v1 guest. P3/P4 cannot complete
with any currently available void driver (pre-fix Stop kill-no-death);
the post-fix void shadow rebuild is scheduled follow-up work, not this
turn.

## Lane proof (session 8, PASS, agent alive)
Recipe = `scripts/ri_build_aros.sh` probe section EXACTLY (v11 gcc +
v1 build-tree headers + v1 startup.o + SHIM libstdc.a symlinks for
libcrt/libstdlib/libcrtprog + `-lstdcio -lposixc -ldos -lexec`, `-O2`):
output `probe_ahi_v1b` is **byte-identical in size (29840 B) with
identical symtab shape** to the Sep21 green binary, task.resource=1,
r12-moves=0 (the script's own gate), UND=1.
```
RI_PROBE program=progdir_probe target=AROS-x86_64
RI_PROBE result: open_rc=0 io_Device=0x000000004a3d3080 io_Unit=0x0000000000000000
RI_PROBE program=probe_ahi m1.1 target=AROS-x86_64
RI_PROBE compiler=x86_64-aros-gcc-16.1.0 built=Sep 22 2026 11:13:29
RI_PROBE engine_block_frames=64 rate=48000
RI_PROBE ahi.session: OPEN unit=255 base=0x000000004a3d3080 version=6
RI_PROBE best_mode: id=0x001F0002
RI_PROBE alloc_audio: OK freq=5513 bits=32 stereo=1 hifi=1 maxch=128
RI_PROBE lowlevel frames=4096 playerfreq_hz=11 rc=0
RI_PROBE lowlevel frames=2048 playerfreq_hz=23 rc=0
RI_PROBE lowlevel frames=1024 playerfreq_hz=46 rc=0
RI_PROBE lowlevel frames=512 playerfreq_hz=93 rc=0
RI_PROBE lowlevel frames=256 playerfreq_hz=187 rc=0
RI_PROBE lowlevel frames=128 playerfreq_hz=375 rc=0
RI_PROBE lowlevel frames=64 playerfreq_hz=750 rc=0
RI_PROBE low_min_frames=64
RI_PROBE verify: SKIPPED (nop3 lane-isolation variant)
RI_PROBE dev_min_frames=SKIPPED (nop3 lane-isolation variant)
RI_PROBE SUMMARY low_min_frames=64 dev_min_frames=0 verify_obs=0 verify_exp=0 dev_open_rc=0
```
P1/P2 match Appendix A line-for-line (0x001F0002, 5513/32-bit/128ch,
ladder 7/7, low_min=64). Resident node identical (0x4a3d3080) =
shadow ahi.device serving. (Cosmetic wart, scratch only: the stock
`SKIPPED (no ladder size accepted)` else-line also prints before the
correct nop3 line.)

## Failure chain closed en route (each root-caused, none thrash)
1. **Hollow setcall stub → startup NULL-read (CR2=0, #PF).** A no-op
   `set_call_libfuncs` returning 1 kills LIBREQ processing; the binary
   dies before main. Lesson: startup init is load-bearing even for an
   argv-less probe. Real one links from `-llibinit` (found by sweeping
   all v1 SDK archives for the definition).
2. **Stock sb128 DriverInit movaps Guru.** Any reboot wipes the RAM:
   deploys (shadows + assigns); bare stock faults at DriverInit+0x135.
   Fix = redeploy (patched sb128 48984 B via `Assign DEVS: RAM:Devs`).
3. **Stock ahi.device ReadConfig movaps Guru.** My 12:14 shadow is
   objdump-proven fixed (0 movaps in ReadConfig); the faulting binary
   was stock — devlist only REPORTS, it never opens; the winning move
   is an explicit `PROGDIR:ahi.device` open (`progdir_probe`) while no
   resident exists (devlist NOT-FOUND on fresh boot; no boot-preload).
4. **Void Stop kill-no-death (BLOCKER, both void builds).** Playback
   starts, `AHIC_Play FALSE` → M50 `kill sent` → master waits forever
   (serial frozen, no Guru). Pre-fix void slave never services the
   death signal. Proven with shadow void AND stock void (M50 in both);
   stock-vs-shadow changes nothing here. P3/P4 unreachable until the
   post-fix void shadow is rebuilt (HookEntry h_SubEntry fix per the
   2026-09-21 record).

## Scheduled (not this turn)
- Rebuild void.audio with the HookEntry fix (wiki recipe: version.h,
  gatestubs reuse, per-driver library.o, `_set_call_funcs` stub, UND
  gates) → redeploy full set → full Appendix-A reproduction (P3 ticks +
  P4 err pattern).
- Deployment archaeology: DH0:Devs/AHI already carries patched sb128
  (48976 B) + stock void (18728 B) from install day — a future
  persistent-deploy lane could skip RAM: puts (assigns still needed).

## Files
- Scratch (outside repo): `/home/miller/Work/ri_build/probe_ahi_{v1,v1b,nop3,p1p2}{,.c,.o}`,
  `v1shim.{c,o}` (CallHookA + GetDataStream stubs KEPT; set_call no-op
  REMOVED in favor of `-llibinit`), `shim/` symlinks, quarantined stuck
  jobs. Repo `audio_io/probe_ahi.c` untouched (git confirms).
- v1 serve: port 9091, spool `/home/miller/Work/spike_spool_v1` (home
  fs; anonymous slot, matches history). QEMU aros_v1 on monitor 4447;
  v1c (other session, monitor 4448) untouched throughout.