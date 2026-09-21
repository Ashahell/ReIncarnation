# 2026-09-21 — ZZTEST verdict: fresh names load via DEVS:; stock-bypass unproven

> Source: session evidence (guest List/devlist/open outputs, host ISO/package scans), compiled by agent
> Collected: 2026-09-21
> Published: 2026-09-21

Consultant Exp-6, executed. Single hypothesis: "LDLoad serves stock bytes
for the name ahi.device due to per-name lddemon state, while fresh names
resolve through the DEVS: assign normally."

## Protocol (5 minutes, no new markers needed)

1. Fresh boot → `devlist_probe` → `ahi.device: NOT-FOUND` (baseline).
2. Deploy shadow bytes (newsafe ahi.device, 177480 B) as BOTH
   `RAM:Devs/ahi.device` and `RAM:Devs/ZZTEST.device`; `Assign DEVS:`.
   Both Lists resolve (177480).
3. `OpenDevice("ZZTEST.device", 99, req, NOMODESCAN)` (zztest_probe):
   open_rc=-1 (expected: the resident inside is named "ahi.device", so the
   exec-layer lookup of "ZZTEST.device" fails), agent alive, no crash.
4. `devlist_probe` again → `ahi.device: FOUND node=0x000000004a44b7a0`.

## Verdict

CONFIRMED: `LDLoad("ZZTEST.device", "devs")` loaded the shadow file through
the DEVS: assign and registered its `ahi.device` resident. A follow-up plain
`OpenDevice("ahi.device")` then served that resident with the M20 marker.
The assign path, LoadSeg, LDInit, and registration all work for fresh
names. Vulkan4AROS wiki's staged open-diagnosis pattern
(`FindName → Lock → Resident check`) informed the design; no V4A wiki page
covers lddemon caching specifically.

## What this does NOT prove (honesty note)

Whether fresh-boot plain `"ahi.device"` opens EVER bypassed the shadow is
now UNCERTAIN (was: assumed proven). The evidence audit:

- Refuted: kickstart resident — zero `ahi.device` strings in ALL boot
  packages (kernel.xz, aros-base/bsp/acpi/fs, poseidon, legacy).
- Impossible on fresh boot: per-name LDObjectNode cache (empty at boot).
- Exhausted: DOS curdir/homedir theories (every resolvable path yields
  shadow bytes; lowercase `devs:` resolves; case-insensitivity proven).
- Contaminated: the original "no M20 + movaps fault" observations came
  from a multi-boot appended serial (lines unattributable to boots) across
  sessions that deployed MULTIPLE intermediate device builds (guest Lists
  showed 179816 and 179424; the 179424 build's marker/fix content is
  unrecoverable). The "stock served" attribution cannot be verified —
  those crashes may have been an unmarked/unfixed intermediate shadow.
- The clean sessions all used PROGDIR-first (resident = mine thereafter).

## Standing

- The bypass question is MOOT for testing (PROGDIR forced-load is
  deterministic and used for all green runs) and re-answerable in
  5 minutes any time: fresh boot → devlist → deploy → open "ahi.device" →
  M20 present? (never skipped the baseline again).
- Practical rule stands: deploy shadows, force the FIRST open via
  `PROGDIR:ahi.device`, verify M20, then all later opens (any path) serve
  the resident.
