# Trademark hygiene: patent/trademark assessment, alias + tutorial rename, sample rules

- Source: ReIncarnation session with the owner, 2026-09-27 (legal Q&A + rename work)
- Collected: 2026-09-27
- Published: 2026-09-27
- Commit: `489bd69` (12 files, audit green)
- Evidence: spec §1 (legal note, `[LEGAL REVIEW]` gate); `docs/superpowers/specs/2026-09-26-device-korg-esx1-requirement.md`

## Patent/trademark assessment (not legal advice; no search run)

- **Patents: no plausible exposure.** The hardware is 1980–83 and
  ReBirth itself is 1997 — any patents expired years ago — and
  clean-room DSP modeling doesn't implicate patents anyway.
- **Trademarks: careful but not sterile.** Product surface is all
  `RI-*`; no Roland in code; TB/TR designations appear only in
  comments describing hardware facts. Three touchpoints found:
  the `REBIRTHAROS` ARexx alias, the `ReBirth-101` tutorial name, and
  808/909 section + legend naming (functional terms, low risk;
  trade-dress exposure carried by the original-art policy).

## Rename (owner decision 2026-09-27)

- `REBIRTHAROS` → `REINCARNATIONAROS` (parser accepts new, refuses old —
  pinned in t1_formats + t1_rel); `ReBirth-101` →
  `ReIncarnation-101` (guide, acceptance rows, audit gate, tracker).
-Amends the spec §1 lock (recorded on the line). Dated plans and
  records keep the old names (history, not product).
- Mechanical note: `w0` is 32 B (`project/arexx.c:26`), fits the
  17-char token; the `test` target links stale objects, so rebuild
  `all` before trusting a rename test run.

## Open-source samples rule (owner Q&A)

- Yes, with the classic-01 pipeline: openly-licensed neutral samples
  (`reference/packs/classic-01` precedent), per-file source + checksum
  in `MANIFEST.txt`, license rows in voice ledgers (audit-enforced
  `CC0/RI` pattern).
- CC0 or CC-BY only (NC conflicts with the open Dist-exclusivity
  decision); "free" ≠ licensed; GPL audio stays out of tree (same rule
  as Open303/Hydrogen); never converted factory content (stays an
  OPEN-10-style legal item); neutral pack names.

## Korg ESX-1 legal read

- Same difficulty as ReBirth, not harder: 2003 hardware (patents
  expired/expiring), ROM/factory content fenced off, manual as
  facts-only E1, import gated. Korg is an active mark owner, so the
  requirement doc's "ESX-1 descriptively" line should tighten to the
  rename logic above: no Korg/Electribe/ESX in any user-visible string
  (UI, skins, chunk IDs, ARexx) — design docs only, nominative.
  Recommendation for the ESX-1 slice, not yet applied.
