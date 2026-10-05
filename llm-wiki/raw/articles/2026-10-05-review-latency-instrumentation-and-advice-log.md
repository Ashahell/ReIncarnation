# Review of the latency instrumentation (530f662) and the advisor guidance of 2026-10-02..05

- Source: ReIncarnation session (advisor lane), 2026-10-02..05. Fix commit `e1008dc` on local branch `claude/latency-review` (worktree `../ReIncarnation-claude`), based on `6e84fd7`; not pushed, not merged.
- Collected: 2026-10-05
- Published: 2026-10-05
- Related: [2026-10-01-clipping-comp-limiter-tab-stall-priority.md](2026-10-01-clipping-comp-limiter-tab-stall-priority.md), [2026-10-05-render-stage-close-out-three-floors-and-my-own-clock.md](2026-10-05-render-stage-close-out-three-floors-and-my-own-clock.md)

## Defects found in 530f662 (fixed in e1008dc)

1. **The tab cycle counted switches, not tabs.**
   - `cyc_fill` was a count. Five switches between two tabs therefore "completed" a cycle.
   - That cycle's sum included three per-tab slots left stale from an older cycle.
   - Fix: `cyc_fill` is a bit mask of the tabs visited. A cycle closes only when all `RI_TAB_COUNT` bits are set, and the slots are cleared afterwards.
2. **The SELFCHK equality held only by luck.**
   - The check `drift == late_sum` compared microsecond figures rounded on every tick.
   - An early tick (`d < per`) added 0 to `late` but shortened `span`.
   - Both sides also came from the same samples, so the check was close to a tautology.
   - Fix: exact sums in EClock units (`span_ec`, `late_ec`, `early_ec`), with the identity `span + early == ticks * (efreq/10) + late`. This can genuinely fail.
3. **"Input → repainted" sampled every main-loop pass.**
   - The 10 Hz timer passes made up most of the population.
   - The heartbeat pass, which writes the log file, set the maximum (the 2.7–3.8 ms max).
   - Fix: sample only wakes with a signal other than the timer bit and `SIGBREAKF_CTRL_C`, and drop the heartbeat pass.
   - Still not true input-to-pixel time. For that, stamp the IntuiMessage time in rsection's `MUIM_HandleEvent` and compare it at the end of that canvas's `draw_frame`.

- **Verification:** AUDIT 0/0 PASS. The audit ran with `/tmp/ri` redirected in the worktree; the redirects were reverted before the commit. v11 build OK: 1043168 B, build=6e84fd7, r12moves=193, 29 TUs at -O2.

## Accepted as sound

- **Bounded `ri_ctlreg_find` scan.** Guarded by t170 (adjacency plus exhaustive equivalence). Weakness: the section bounds are literals, so every new Levi control needs a manual edit, and t170 fails loudly if one is missed.
- **Item cull (t169), panel trim and the `disc_grad` row clamp.**
- **Housekeeping:** timing gated on `RIAPP_DIAG` (off by default), build hash in `RIAPP.LOG`, `ri_build_v11.sh` in the repo, mixed build (engine/ -O2, app+GUI -O0).

## Open review points

- **Mixed-build gate:** it greps the script text. Prefer checking the objects, e.g. built with `-frecord-gcc-switches` and read from `.GCC.command.line`.
- **Misleading build-script echo:** the `NO2` count in `ri_build_aros.sh` matches `*/gui_*.o`, which no object is named, so the echo is wrong.
- **Wrong rationale comment:** it says -O0 "keeps GUI latency down", which contradicts the governor finding. The real reason for -O0 on app+GUI is the owner's debuggability rule.
- **`ri_dlist_set_clip`:** it does not reset `n`; make it reset or assert rather than document a trap.
- **Comment length:** the narrative comments are far longer than the surrounding code's.
- **`RIAPP_DIAG=1`:** still set in the Dell's ENVARC from a positive control.

## Advisor guidance given (2026-10-02..05, condensed)

- **Governor:** `4167e32` (stall cap 1200‰) and opencode's `3dadda7` (2 s continuous arm) coexist on main. Neither was meant to replace the other. Recommendation: keep the arm as the one mechanism and drop the cap, keeping the t88 stall case.
- **Unverified build identity:** an earlier xrun report rested on a build whose identity was not logged (`build=?`). Require the build hash per run and an A,B,B,A scripted protocol before drawing conclusions.
- **riqemu1 start (wiki 2026-10-02):**
  1. Start the spooler first: `python3 scripts/spike_server.py serve --port 9295 --spool /tmp/spike_spool_riqemu1`, from the Vulkan4Aros tree.
  2. Then run `bash ~/Work/vms/start_riqemu1.sh` (AC97 on PulseAudio, `-vga vmware`, HMP 4477).
  3. Verify the screendump is 1280x1024.
  4. Never use `vm_restart.sh`: it kills all QEMU and runs without KVM.
- **Levi performance:**
  - Measure on the host with controlled sweeps (k voices, k operators, Morph on/off) at -O2, not on an overloaded Dell at -O0.
  - First make bit-identical cuts: skip a Morph bank whose weight is exactly 0, and skip unrouted or zero-level operators.
  - Then, with the owner's approval and a ledger entry: control-rate envelopes/LFOs/matrix, kernel approximations, a polyphony cap or quality switch.
  - Cutting operators (8 to 4/6) is the last resort, offered as a "lite" mode.
- **GUI responsiveness:**
  - Close the box-repaint path: 150 µs at ~9–10 advances/s is about 0.15 % of a core.
  - Measure user-facing latency instead.
  - Leave the AROS blit path alone (upstream). The in-repo alternative is coalescing damage boxes per canvas per loop.
  - Next target: the tab switch (41.6 ms vs a 2.3 ms full repaint). Count `MUIM_Draw` calls per switch before cutting.
