# Verbatim evidence — 2026-10-05: the three latency numbers, and two instrument defects the self-check missed

Source: ReIncarnation session, 2026-10-05 (opencode lane, Dell E6320, ABIv11, mixed build:
`engine/` at `-O2`, app+GUI at `-O0`). Collected 2026-10-05.

## 1. The latency line, first real run (RIPP-LAT, mixed build 52fc452)

```
RIAPP lat: ticks=158 late=155 late_max=2759 us hist=57/101/0/0 hs=158 | input=300 in_max=1112 us in_avg=31 us ihist=299/1/0/0 is=300 | cyc=0 cyc_avg=0 us cyc_max=0 us per_tab=29241/31512/24398/47497/32841 sumtab=165489 | span=16000432 us expect=15800000 us drift=200432 us SELFCHK=ok
```

Note `cyc=0` beside five non-zero `per_tab` values.

## 2. After seeding the first tick and making per_tab a guarded mean (RIPP-LAT2)

```
RIAPP lat: ticks=149 late=117 late_max=2805650769 us hist=74/69/1/5 hs=149 | input=299 in_max=50413 us in_avg=675 us ihist=286/8/0/5 is=299 | cyc=1 cyc_avg=165489 us cyc_max=165489 us per_tab=29241/31512/24398/47497/32841 sumtab=165489 | span=2820903567 us expect=14900000 us drift=2806003567 us SELFCHK=ok
```

`late_max=2805650769 us` and `span=2820903567 us` while SELFCHK still reads ok.

## 3. After drift became a checked equality (RIPP-LAT3)

```
RIAPP lat: ticks=162 late=157 late_max=2867 us hist=61/101/0/0 hs=162 | input=300 in_max=2655 us in_avg=40 us ihist=298/2/0/0 is=300 | cyc=0 cyc_avg=0 us cyc_max=0 us per_tab=0/0/0/0/0 sumtab=0 | span=16405418 us expect=16200000 us drift=205418 us latesum=205418 us SELFCHK=ok
RIAPP lat: ticks=159 late=152 late_max=2759 us hist=63/96/0/0 hs=159 | input=300 in_max=3836 us in_avg=52 us ihist=297/3/0/0 is=300 | cyc=0 ... span=16100926 us expect=15900000 us drift=200926 us latesum=200926 us SELFCHK=ok
```

`per_tab=0/0/0/0/0` beside `cyc=0` — consistent now.
`drift == latesum` exactly, in both windows.

## 4. The stamps and the switch

```
RIAPP LOG build=25587fc diag=0
release   (diag=0):  part_avg 151.0 us
diagnostic(diag=1):  part_avg 157.5 us | build 32.9 | gap 21.7
partition with diag=1: rows 26 | mismatches beyond truncation 0
```

## 5. Tab switches actually recorded (RIAPP-EV.LOG, RIPP-LAT3 run)

```
TAB events: 1
TAB page=4 us=41608
```

Five of the six tab clicks did not register. `TAB` events are in the EV log, not
RIAPP.LOG.

## 6. The comparison that aims the next cut

```
  click -> repaint      in_max  2655-3836 us, in_avg 40-52 us
  full repaint          full_avg 2291 us
  tab switch            TAB page=4 us=41608
```

41608 / 2291 = 18.2x.

The per-tab spread, from the one complete cycle recorded (run 2):

```
  per_tab=29241/31512/24398/47497/32841   sumtab=165489
```

## 7. Mixed build, in-repo script, gates

```
AROS RIAPP v11 BUILD OK (/tmp/opencode/RIAPP-lat3.v11, 1042384 bytes, build=52fc452, r12moves=193)
  mixed: engine/ at -O2, rest at -O0 (29 TUs optimised)
```

Negative controls on the two new audit gates:

```
FAIL: ri_build_aros.sh must route engine/ to -O2; a single flag for every file is the regression this gate exists for
FAIL: scripts/ri_stray.sh is not on the shared-script allowlist (ri_audit.sh ri_build_aros.sh ri_build_host.sh ri_build_v11.sh ri_fuzz.sh ri_soak.sh)
```
