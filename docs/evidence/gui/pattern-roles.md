# Pattern role bubbles (owner request, 2026-09-27)

- Pattern-block backgrounds now bubble `303A BASS / 303B BASS / 808 DRUMS /
  909 DRUMS` (empty-area hover; per-control bubbles unchanged).
- Single source: `role` field on `RIPanelDesc` (`gui/panels.{h,c}`) next to
  the wiring `name`/`device` — title bars (`bg_pat` device tags) and
  bubbles (`ri_panel_role_label`) read the same table.
- Tests: t29 pins all four labels + refusals (bad instance, NULL/tiny
  buffer). Mutant (`DRUMS`→`DRUM `): `FAIL role 2` (killed).
- AROS: `rsection.mcc.c` background branch only; RISECT/RIAPP re-link.
  Bubble visibility needs an owner hover on the Dell (no remote hover
  instrument: `ui-move` never produced IDCMP on this lane).
