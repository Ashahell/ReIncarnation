# 2026-09-21 — ahi.library on upstream and deadwood: neither fork ships it

> Source: session evidence (GitHub API listings, tree searches, Makefile inspection), compiled by agent
> Collected: 2026-09-21
> Published: 2026-09-21

Follow-up to the no-go record: is there an `ahi.library` module on either
major AROS fork that the v1 tree assessment missed?

## Upstream (`aros-development-team/AROS`, master)

- `workbench/libs/` listing via GitHub API: 60 entries, no `ahi`
  directory (amigaguide → asl → asyncio → bullet …).
- No build rule producing `ahi.library` anywhere in `workbench/`, `rom/`,
  or `arch/` mmakefiles; no `.conf`/libinit source.
- (Local v1 worktree origin IS this repo, so the earlier tree search
  already covered its content.)

## Deadwood (`deadwood2/AROS`, master)

- `workbench/libs/` listing via GitHub API: no `ahi` directory either.
- `workbench/devs/AHI/AHI/Makefile.in`: same prefs-executables-only
  structure (`AHI_BGUI`/`AHI_CA`/`AHI_MUI`); `mmakefile.src` has no
  library target.
- One genuine delta: deadwood's `AHI/` directory DOES contain the catalog
  sources (`ahiprefs_Cat.c/.h`, `ahiprefs.cd/.ct`) absent from the v1
  worktree (empty `translations/`). This removes one of the three port
  blockers (catalog); ROMTag/LVO/init scaffolding would still have to be
  written from scratch, so the no-go stands.

## Standing

Neither fork ships or builds `ahi.library`. The device-as-library path
remains the only interface (already proven: P1–P3 green). If a port is
ever attempted, deadwood is the better source tree for the catalog files.
