# 2026-09-24 — Wiki doc-sync: strike compact lock, acceptance earns MCC boxes (Update)

> Source: repo sweep (geometry doc, acceptance ledger, test comments)
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
Update. No behavior, no measurements, no device work — the code
moved (m19 v2 geometry, m22 MCC proof) and two evidence docs
still told the old story. Cascade corrections only.

## What
- `docs/evidence/gui/panel-909-geometry.md`: the E0→compact lock
  note now carries visible `~~strikethrough~~` + SUPERSEDED markers
  pointing at the v2 section (70 px pitch, custom RKnB art,
  MUIC_Knob retired). Per doc-lock discipline: old text struck
  visibly, never silently rewritten. The dedicated "Superseded
  compact lock" section already held the history; now the lock
  note agrees with it.
- `docs/evidence/gui/acceptance.md`: the checked centers box cited
  the superseded 33 px compact numbers — now cites the v2 table +
  the m22 re-measurement (0 px x-error net of chrome). Two boxes
  newly checked with device evidence: custom-knob art render,
  click down/up safety. Drag/fine/commit/right-click stay
  unchecked (honest: untestable remotely / no app listeners).
- Checked and left alone: `tests/unit/t29_knobart.c` header
  ("replaces MUIC_Knob stock visuals" — historically accurate);
  log.md history lines (immutable); ref909 reference images
  (measurements already in m19/m22 articles).

## Files
- env: `docs/evidence/gui/panel-909-geometry.md`,
  `docs/evidence/gui/acceptance.md`
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

Audit gates on acceptance content (P-18, TC-2.9/2.10/2.11,
ReBirth-101, Tester lines, checkable boxes) — all preserved.
",
