# 2026-11-17 census — superseded (2026-11-21)

The first t243 run's tables (census.tsv, summary.txt, classify.md,
allowlist.tsv) were removed. They did not measure what they reported:

- D3 MIDI and D9 keyboard called `ri_panel_ctl_send` directly, so could only
  agree with D1;
- D8 "feedback" was a meter peak after 64 silent samples, so all 2010
  NOFEEDBACK verdicts were the check itself;
- the allow-list was generated from the verdicts it judged, and the gate ran
  only with `--gate`, which the audit never passed.

Current census: `tests/unit/t243_wiring_census.c`, baseline
`docs/evidence/wiring/2026-11-21-census/baseline.tsv` (unreviewed, pending
owner). Output goes to `$RI_TMP/run/t243`, never into the repo.
