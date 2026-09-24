# m43 — Chase beat runs: NewInput exonerated-blind, manual-Wait + MICROHZ measured

**Date:** 2026-09-24 (QEMU private lane riqemu1, ABIv1 build; Dell untouched)
**Slice:** m43 (follows m41 live-chase vehicle + m42 lane incident)
**Disposition:** New (first measured chase-beat fix; production loop-shape change in a vehicle)

## What m41 left pending

m41's stepproof armed a `timer.device` MICROHZ one-shot and seeded its signal bit into
`MUIM_Application_NewInput`, but STEP never advanced on Dell. Session-13/14 work added
`io_Flags` hygiene and setup-bits diagnostics (`STEP=100+bits`), which read `107`
(open ok, first SendIO ok) while the loop stayed frozen. Two hypotheses survived:
setup-silence (device never delivers in-app) vs NewInput-seed blindness (loop never
sees it). Everything before this slice was observed *through* the suspected-blind
instrument, so no verdict was possible.

## diag13: the device delivers everywhere (rc 1/11/21/31)

New scratch vehicle `/home/miller/Work/ri_build/diag13.c` (never committed): one 86 ms
MICROHZ one-shot with an EClock-bounded poll, in four graduated contexts:

| stage | context | exit rc (= 10×stage + replies) |
|---|---|---|
| 0 | bare task | **1** |
| 1 | + intuition/muimaster/graphics open | **11** |
| 2 | + Intuition window | **21** |
| 3 | + MUI application + open window | **31** |

Timer replies arrive in **all** contexts, including beside a live MUI app. Setup-silence
is dead. The NewInput loop is the blind instrument: Zune never surfaces seeded user
signal bits (and `InputBuffered` never returns on Zune — drain-spin, separately
established). Trial record also holds a bare-`OpenDevice`-only probe (DIAG12, rc=0):
the device opens fine in isolation.

## Production shape (app/stepproof.c only)

- Event loop is now manual `Wait(tsig | SIGBREAKF_CTRL_C)` — no `NewInput`, no drain.
  Clicks/close are NOT served here (documented tradeoff, proven separately on NewInput
  builds); exit is Ctrl-C (`Break N C` used 5× this session, clean every time).
- Timer setup moved FIRST THING, before any MUI object exists (port + req + open +
  first SendIO); chase-mark and readouts stay late (need objects).
- `UNIT_MICROHZ` kept. `UNIT_VBLANK` was tried (one-line flip) and rejected on
  measurement (below) — not on liveness.
- Grid origin fixed off-by-one: due(k) = t0+(k+1)·period (t0 just before first SendIO,
  so k=0 is due one full period out). Before this, lag0 read ~86 ms and poisoned maxlag.
- TEMP-DIAG file logging (`RAM:chase.log`, open-append-close per fire) used for
  measurement, then fully stripped: no `log_u64`, no `logfh`, no `proto/dos.h`.
  Committed tree contains zero diag residue (verified by grep).

## Measured (QEMU, file log, 255 fires)

MICROHZ + manual Wait, requested period 86206.9 µs (174 BPM 16ths):

- fires: 255 (k = 0..254), zero drops
- interval mean **86554.2 µs** (+347 µs/fire, +0.4%), min 86501, max 89869 (one 3.3 ms
  scheduling blip), stdev 218 µs (outlier-dominated; body jitter ±0.1 ms)
- cumulative grid drift +347 µs/fire (≈174 ms behind wall-grid after 22 s).
  Per-fire scheduling is tight; the drift is crystal/latency rate error, not jitter.
  A shipping engine should re-anchor the grid (relative rearm or periodic correction);
  the proof vehicle reports it honestly via LAG/maxlag.

VBLANK + same loop (rejected): fires continuously but every interval quantizes to the
blank grid — measured **100.18 ms** (≈6×16.67 ms) against an 86.2 ms request, i.e.
+14 ms/fire unbounded lag growth. Correct tempo is unachievable on VBLANK ticks for
174 BPM 16ths; AROS treats both units' `tr_time` as seconds+microseconds (shared
`ADDTIME` path in `rom/timer/lowlevel.c`, separate wait lists), so the unit flip was
always safe — just out of tune.

## Evidence quality notes (honest)

- The deployed-and-measured binary (QSTEPL4, md5 `764f4ce5…`) carried per-fire
  open-append-close logging; the committed binary is identical minus logging (strictly
  less per-fire work) plus the 1-period grid shift (readout math only). Loop shape,
  rearm path, and SetAttrs path are unchanged.
- Chase-glow motion has no standalone pixel proof this session: window-capture slot
  mapping proved unstable (`,0`/`,2` returned scaled workbench thumbnails; 8–10 px
  diffs = FIRES counter repainting, which DOES prove the fire handler's SetAttrs path
  repaints live). Glow uses the same SetAttrs mechanism three lines up in the same
  handler, on the owner-approved RStp chase art (m38/m41). Final visual: Dell
  hands/feel on ration.
- `microhz rearm stalls` (session-13 hypothesis) is STRUCK as unproven: every stuck
  observation ran through the blind NewInput loop; the one manual-Wait microhz run
  (QSTEPM) was judged via wrong-region crops. Both units fire on manual Wait.
- Lane hygiene: 9 captures hit the wedge budget; lane reset via monitor (`system_reset`,
  session 14→15), subsequent verification by `Type`/counter-diffs only. Dell lane and
  all jug state untouched.

## Gates

- `ri_audit.sh` **0/0** (final log `ri_build/audit_chase_final.log`), V11 `-Werror`
  clean, V1 link clean. spirv-val vacuous (no shader content).
- Deployed QSTEPF (final binary, md5 `b6cbdcb6…`): window opens, Break-exit rc=0,
  lane left clean (Status shows no corpses).
- Sibling-session file `gui/widgets/rknb.mcc.c` (2-line ReplyPort) left uncommitted —
  not this slice's work.

## Next

Owner Dell run: advancing-glow eyeball + LAG/FIRES numbers on a fresh instance
(Dell ration ~1 capture). Then M2.6 engine scheduling (grid re-anchor decision) and
the remaining frontier (accent/flam glows, fader travel, MCC typography).
