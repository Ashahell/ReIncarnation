# Beta exit — Task 14 (gate G14)

**Date:** 2026-09-20. **Branch:** work/ri-planexec.

**No beta ran in this environment: no beta users exist here.** Nothing
below claims a 20-user beta, an on-device GUI acceptance, or named-box
numbers. Each row is PROVEN (artifact in the tree, re-verified by
`ri_audit.sh` Phase 14 from clean) or DEFERRED with the exact method
that will close it.

## PROVEN (host)

| Claim | Artifact | Method |
|---|---|---|
| Worst-case DSP renders 4.9x realtime, deterministic | `tools/bench.c` + Phase 14 bench run | 45000 blocks (60 s audio) twice, FNV checksums equal; ratio 0.2025 host (frostmourne, GCC 16.2.1, -O2) |
| 5-min offline soak, switching under load, 0 underruns | `scripts/ri_soak.sh` + `docs/evidence/soak/soak-2026-09-20.md` | 300 s deadline → 200 iterations, per-render md5 vs first-pass pin; MODR prompt asserted every iteration (a 600 s run covered its full duration with no summary — process evidence only, see the soak file) |
| ARexx dispatch replies pinned | `tests/unit/t1_rel.c` PASS | all 5 commands + alias + 4 fail-closed cases |
| One translated string renders through the lookup | `gui/catalog.c` + `t1_rel` RENDERED-DE line | `ri_catalog_get("DE","MSG_PLAY")` = "Abspielen", printed at runtime, grepped in audit |
| Manual covers every control (33/33 + 5 ARexx) | `docs/ReIncarnation.guide` | Phase 14 greps all 33 panel-qualified rows + 5 command strings |
| Autodocs carry the exact shipped signatures | `docs/autodoc/*.doc` (8 files) | Phase 14 greps 30 signatures in header AND doc |
| Installer + icons exist as files | `Install/ReIncarnation-Install` + `Install/icons/*.png` | existence + stanza greps; AROS-run UNVERIFIED (see DEFERRED) |
| Task-13 deferred wiring owned | `project/arexx_aros.c`, `ri_camd_open/close`, `ri_*_datatype_reg` | AROS-only guards + host-exclusion + AROS compile-only, all gated in Phase 14 |
| Full audit green, no goldens touched | `ri_audit.sh` AUDIT 0/0 PASS | all 14 phases from clean; prior goldens re-verified byte-identical |

## DEFERRED (each with method — none may close without its artifact)

| Item | Method to close | Gate that holds it |
|---|---|---|
| Live full-graph render (303+808+909+mixer/FX through au_render_frames — currently first-light 303 only, tripwired by RI_LIVE_FULL_GRAPH_UNIMPLEMENTED in audio_io/audio.c) | Wire the shared engine core into au_render_frames (spec §5); extend the one-renderer file-vs-live diff past first-light songs, then remove the marker | Live-backend follow-up |
| 20-user beta (TC-2.16.x: zero crashers/data-loss, tutorial A/B ≥ 4/5) | Ship Install/ drawer + guide to 20 testers; collect sign-off sheets with build hash; file as `docs/evidence/beta/signoff-*.md` | TC-2.16.x |
| On-device GUI acceptance (LED lag, chase, zoom, silhouette) | Run `docs/evidence/gui/acceptance.md` checklist on the AROS box; tester + date + build per row | G12 device pass |
| M1.1 named-box numbers (AHI floor, device underruns, overnight soak) | `audio_io/probe_ahi.c` on the box; `ri_soak.sh` with an AHI-drain backend; AuQueryAttr AUQA_XRUN_COUNT must read 0 across the run; "overnight" only for an actually-overnight log | G5 + P-19 |
| CAMD live open (OpenMidiCluster on DEVS:MIDI/) | On-box: link proto/camd.h in camd_backend.c, open driver, loopback note→step; status must read OK | Task-14 follow-up |
| RexxMsg port loop (MsgPort + WaitPort + ReplyMsg) | On-box: create REINCARNATION port, drive all 5 commands from Rexx, assert reply texts equal the pinned dispatch strings | Task-14 follow-up |
| Datatype AddDataType registration | On-box: register both descriptors, double-click .rbng/.rbnm in Wanderer, assert probe+load+fallback/CPRG paths | Task-14 follow-up |
| Installer run + .info conversion | On-box: run Installer script, convert PNG masters to .info, screenshot installed drawer | G14 device pass |
| Catalog .catalog compile (CatComp) | On-box/AROS-SDK: compile .cd/.ct to .catalog, open GUI in DE locale, screenshot "Abspielen" | G14 device pass |

## Task-13 concern close-out

Task 13 concern 3 (CAMD open/pump wiring + RexxMsg glue + datatype
registration await Task 14): the ownable shells land in THIS task
(open/close state machine, string-level RexxMsg glue, registration
descriptors — all gated). The live-device halves move to the DEFERRED
rows above with methods. P-19 = 64 stands: the soak hammers the flood
cap path (max-density retrigger every block) with zero drops counted
against playback determinism.
