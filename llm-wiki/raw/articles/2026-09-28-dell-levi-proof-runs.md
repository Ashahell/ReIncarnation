# Dell Levi proof runs (2026-09-28, E6320 lane)

- Source: ReIncarnation session, 2026-09-28 (opencode lane ops)
- Collected: 2026-09-28
- Published: 2026-09-28
- Lane recipe: [2026-09-27-dell-lane-bridge-recovery.md](2026-09-27-dell-lane-bridge-recovery.md)

## Deployments (all `~/bin/build_v11.sh`, 72 TUs, 0 UND; hash verified on-device via `Search RAM:RIAPP <hash>`)

- `657a990` (demo part): broke old `ram:riapp` clean (5.6M buffers,
  30 xruns over ~23 min), put to `RAM:RIAPP`, ran as process 10.
  Session healthy (0 xruns, render_max ~75 us vs 5333 us period, 909
  pack 11 voices bound) — but owner reported play→~6 ms sound→silence
  (the NaN crash; see crash record).
- `bc9237c` (NaN fix): broke poisoned instance, redeployed, verified
  at offset 627. Idling 0 xruns. Owner re-proof APPROVED (sustained
  play, Levi part audible).
- `12dd8b2` (mixer strip): lane exec channel wedged first (fresh
  heartbeat, every exec `rc=-1` in ~29 ms); Dell rebooted; agent back
  on session 5 with clean slate; deployed, verified at offset 627,
  idling 0 xruns render_max 68 us. Strip proof (fader/meter/inserts)
  pending owner hands.

## Findings

- `RIAPP_EVLOG=ram:` persists in the Dell's `ENVARC:` (spike-hunt
  leftover): ev-log lands in `RAM:RIAPP-EV.LOG`, NOT the USB stick,
  and stays locked while RIAPP runs (bulk get `REFUSED`, `Copy`
  rc=20). Pull after close. `RAM:RIAPP.LOG` (status) is per-line
  closed and pulls fine mid-run.
- `Vk4aros:RIAPP-EV.LOG` left stale from the prior session is
  expected when the override is set — not a logging bug.
- Dell clock is garbage (`Sunday 11/17/13` in listings): volume dates
  mean nothing; use content (`RUN build=` line) for attribution.
- Single-instance discipline holds: always break the old `ram:riapp`
  before deploy (duplicate-instance AHI contention froze the Dell
  before). Sibling's `RIAPPPWR` left untouched (not ours to break).
- Reboots wipe `RAM:` (binary + logs) but not `ENVARC:` or the USB
  stick; the agent needs a manual redial after every reboot
  (`SYS:ATCPBIN agent 192.168.1.81 9292 e6320`); host `/tmp/ri`
  binaries survive (host never rebooted).
- Full-screen `ui-capture` at scale 1 is refused (screen too large);
  scale 2 (683x384) is too coarse to read tabs/rail — visual proof
  needs owner eyes. Never inject `ui-move`/`ui-click` into a live
  owner session (moved the owner's mouse once; no harm, no repeat).
- First play after deploy spiked render_max 3693 us once (inside the
  period, no dropout) — consistent with cold caches + the NaN blowup
  (inf handling is slow).
