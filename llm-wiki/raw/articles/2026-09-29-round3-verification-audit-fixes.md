# GUI round 3 close-out verified; audit made green again (`6358792`)

- Source: ReIncarnation session, 2026-09-29: verification of OpenCode's round-3 final report, and fix commit `6358792`
- Collected: 2026-09-29
- Published: 2026-09-29
- Related: `2026-09-29-gui-round3-s1-s3-and-interop-requirement.md`, `2026-09-29-s6-closed-s7-proven.md`, `2026-09-29-s7abc-s6-research.md`

## OpenCode's final report and what checked out

OpenCode reported GUI round 3 closed:
- **S6** (lane clicks reach the app) is closed;
- **S7** (per-section skins) is proven on the Dell;
- "all granted work done", everything pushed.

**Verified against the repos:**
- **S6:** Vulkan4AROS `72a3fe4a`…`4f03fd7d` (agent and server fixes) and the cross-post `77b1b25a` are on `origin/main`.
  - The ReIncarnation README section "S6 lane clicks + S7 skin proof" records the three causes:
    1. pointer moves sent inside `IND_ADDEVENT` were ignored;
    2. the button qualifier was missing;
    3. JSON `true` releases were read as presses.
  - It also records the no-owner acceptance: tab switch (`TAB page=1`); rail power for 303B (`VIS dev=1 show=0 mask=1d` / `show=1 mask=1f`); knob drag (`CTL 0104=0 → 25`).
- **S4/S5:** evidence is present (`dell-s4-master.png`, `dell-s4-mix.png`, `dell-s5b4-*`, `dell-s5guard-clamp.png`).
- **S7:** commits `ea11f38` (S7a core), `e60f911` (S7b registry, canvas, Ctrl+M) and `eb81381` (S7c song chunk **SKAS**, minor 3).
  - The owner's grant for the format change is recorded in `2026-09-29-s7abc-s6-research.md` ("owner granted both").
  - The 808-RI captures show per-part Classic fallbacks beside skinned strips.
- **Host tests green at HEAD:** t60, t61, t70, t71, t92, t93, t111 (legend face), t112 (partial redraw).

**Not as reported:** the full audit on `origin/main` (`af76d5a`) was **red twice**. It was run in a clean worktree with a private `/tmp/ri`, so stale objects from other sessions were ruled out.
1. **t75** failed with `808-RI bg03.png is 1460x468: wrong size for the panel` and "no stale parts".
   - Cause: `945065f` (owner visual pass) padded the 909 row from 1460 to 1472 Q, but the shipped 808-RI 909 backdrop stayed 1460 wide.
   - OpenCode's report listed this as a remaining "t75 asset (sibling)" item, not as a red audit.
2. **Portable build (T10 gate):** once t75 was skipped in a scratch copy, the link failed with `undefined reference to ri_levi_matrix_eval` / `ri_levi_matrix_init` from `levi.c`.
   - Cause: the Levi session added `engine/dsp/levi_matrix.c` to `ri_build_host.sh` `MOD_dsplevi` but not to `build/portable.mk` `CORE_TU`.

## The fix (`6358792`)

- `build/portable.mk`: added `engine/dsp/levi_matrix.c`. Result: `PORTABLE BUILD OK`, t84 `PASS pal_thread`, headless render OK (0 xruns).
  - `engine/fx/reverb.c` is in the host lists but is not needed by the portable link.
- `skins/808-RI/bg03.png`: regenerated with `tools/mkskin` (1472x468). t75 `PASS skin`.
- `AUDIT 0/0 PASS` (clean worktree, private `/tmp/ri`). Committed locally; not yet pushed.

## Findings

- **mkskin build line is out of date.** The header comment of `tools/mkskin.c` omits `/tmp/ri/build/knob_logic.o` and `-lm`, and linking fails on `ri_knob_pointer_mdeg`. The working line:

  ```
  gcc -std=c99 -O2 -Wall -Wextra -I. -o /tmp/ri/mkskin tools/mkskin.c \
    /tmp/ri/build/panelgeo.o /tmp/ri/build/ctlreg.o /tmp/ri/build/knob_art.o \
    /tmp/ri/build/knob_logic.o /tmp/ri/build/skin.o /tmp/ri/build/sha256.o -lpng -lm
  ```

- **mkskin now also emits Levi parts.** Regenerating adds `bg18.png` (levi, 1464x560), `bg19.png` (pat-levi, 284x464) and `bg20.png` (mix-levi, 284x464), plus five manifest lines (`BACKGROUND.levi`, a `PART.levi` knob, `BACKGROUND.pat-levi`, `BACKGROUND.mix-levi`, a `PART.mix-levi` knob).
  - They were **not** committed: skinning the Levi panel under 808-RI is a look decision for the owner or the Levi lane.
  - All other 808-RI files regenerate byte-identical, so the generator is deterministic.
- **The Dell's installed skin is stale too.** `SYS:Classes/ReIncarnation/Mods/808-RI/bg03.png` is still the 1460-wide image, so the 909 part keeps falling back to Classic there until the new file is deployed.
- **Two sessions editing the same wiki at once loses work.** A Claude ingest wrote the index and log lines plus an untracked raw file; a parallel OpenCode wiki commit swept in the index and log lines but not the untracked raw file. The pushed index then linked to a missing file until `1d00ba9`.
  - The Vulkan4AROS worktree used for the cross-post (`scratchpad/v4main`) disappeared before it was committed; it was recreated and pushed as `2722fd82`.
  - Rule: commit an ingest (raw file + index + log together) in the same turn it is written, or keep it in a private worktree until committed.
- **"Done" reports need an audit check.** A handoff that says "all granted work done" can still leave the main audit red, both from known carry-overs (t75) and from a sibling lane's build list (portable.mk). Verify with a full audit at `origin/main` in a clean worktree with a private `/tmp/ri` before accepting a close-out.
