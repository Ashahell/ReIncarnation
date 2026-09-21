# 2026-09-21 — ahi.library absent from the v1 tree: P1–P3 unavailable (no-go)

> Source: session evidence (tree scans, ISO listing, build-file inspection), compiled by agent
> Collected: 2026-09-21
> Published: 2026-09-21

Assessment for unlocking probe P1–P3 (low-level API: BestAudioID, AllocAudio,
PlayerFreq ladder, 5 s PlayerFunc verify) on the codec-less guest.

> Status (2026-09-21): Outdated — the "P1–P3 unavailable" premise was wrong.
> The platform contract is device-as-library (`OpenDevice("ahi.device",
> AHI_NO_UNIT)` → base from `io_Device`; no `LIBS:ahi.library` by design),
> and P1–P3 now run crash-free (see `2026-09-21-device-as-library-p1-p3.md`:
> BestAudioID `0x1f0002`, AllocAudio OK, ladder all rc=0, verify 0 ticks).
> The tree-scan facts below (no file/dir/build target) remain true; only the
> conclusion drawn from them is superseded.

## Finding: the OS build has no ahi.library at all

- `isoinfo` over the boot ISO: no `Libs/ahi.library` on the CD.
- Whole v1 tree search (`src/abi/v1`): no `ahi.library` binary, no
  `workbench/libs/ahi` source dir, nothing named `ahi.library` under the
  built `AROS/Libs/`.
- The AHI subtree (`workbench/devs/AHI/AHI/`) builds only prefs-program
  executables (`AHI_BGUI`/`AHI_CA`/`AHI_MUI` = `ahi.o + support.o +
  ahiprefs_Cat.o` + one GUI TU each) plus catalogs — not the library.
- No `ahi.conf`, no libinit source, no `%build_module` reference to an AHI
  library anywhere in the tree (`workbench/*/mmakefile*` searched).
- `translations/` is empty (no catalog source for `ahiprefs_Cat.o`).
- An LVO table would have to be hand-built from `Include/SFD/ahi_lib.sfd`
  (~40+ API functions) plus a ROMTag/resident/libinit layer written from
  scratch — a multi-hour port, not a shadow rebuild.

## No-go rationale (two independent reasons)

1. Cost: porting a whole library (ROMTag, LVO table, init, catalog,
   GUI-toolkit exclusions) dwarfs the proven shadow flows (device/drivers
   reused existing init scaffolding: `Common/library.c`,
   `libaddroutines.a` equivalents, generated `gatestubs`).
2. Value: on VOID the low-level numbers would be fake-timing artifacts —
   the slave spins unclocked calling PlayerFunc as fast as the CPU allows,
   so a 5 s verify would report ticks orders of magnitude above expected.
   P1–P3 would execute but measure nothing meaningful about audio.

## Standing

- M1.1 on codec-less stays honestly worded from P4: device opens,
  CMD_WRITE accepted and completed (`dev_min_frames=64`), aborts clean,
  CloseDevice returns, rc=0. P1–P3 are environmentally unavailable (no
  library in the OS build), not a test failure.
- If a future OS build ships `ahi.library`, the probe's P1–P3 path is
  already written and gated (it cleanly reports ABSENT today); revisit then.
