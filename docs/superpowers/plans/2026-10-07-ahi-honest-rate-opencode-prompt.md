# Prompt for OpenCode: AHI honest-rate patch (HDAudio and drivers report the rate they actually run)

> Written 2026-10-07 by the Claude advisor session. The owner asked for
> "patch 1" from the advisor's AHI v7 review: *AHI tells the application the
> rate it actually gets*.
>
> **The work has six phases (H0–H5).**
> - H0 corrects our own records first.
> - H1–H2 prove the bug and the fix on the host and the Dell.
> - H3 surveys the other drivers.
> - H4 prepares the upstream pull request.
> - H5 is the write-up.
>
> Every code phase ends with a commit and a short report. If you run short of
> context, stop at a phase boundary and write the handoff block (§8).
>
> **You are done when the gates in §7 for the phases you reached read PASS,
> with evidence where the gate names it.**

---

## 0. What is known, and what turned out to be wrong

### 0.1 A finding of ours is wrong, and it has spread

On 2026-10-03 we recorded *"The Dell hands back 44100 for every rate it is
asked for"*:
- llm-wiki article `2026-10-03-dell-ahi-hands-back-44100-for-every-rate.md`;
- evidence `llm-wiki/raw/evidence/2026-10-03-dell-ahi-rate-probe.md`;
- `audio_io/probe_rate.c`.

**That reading is an API misuse, not a fact about the hardware.**

- **What the probe read:** after `AHI_AllocAudioA`, `probe_rate.c` called
  `AHI_GetAudioAttrsA(AHI_INVALID_ID, actl, … AHIDB_Frequency …)` and
  reported the result as the rate it "got".
- **What that tag means:** per the AHI autodoc
  (`workbench/devs/AHI/Device/modeinfo.c`, the `AHI_GetAudioAttrsA` doc,
  around lines 297–305), `AHIDB_Frequency` returns **the frequency at index
  `AHIDB_FrequencyArg`**, which defaults to **0**.
  - The HDAudio driver's `AHIsub_GetAttr(AHIDB_Frequency)` returns
    `card->frequencies[argument].frequency`
    (`Drivers/HDAudio/main.c` ~line 483).
  - Index 0 on this codec is 44100. So "44100 for every rate" is "entry 0 of
    the rate list, every time". The tag never was a current-rate query.
- **The correct query** is `AHI_ControlAudioA(actl, AHIC_MixFreq_Query, &f)`:
  "Get the current mixing frequency" (`Device/audioctrl.c`, around line
  888). That is what the live backend uses (`audio_io/audio_ahi_live.c`
  ~line 275), and it logs `mix=48000 Hz` on the Dell. Pitch on the Dell has
  always sounded right.

**This claim was carried into:**
1. the wiki article above, and its index line;
2. `docs/AhiV7.md` (Draft 3): G3, G7, §3.4, §15.2, §15.5, Q11, and the
   Appendix E row "Two observed silent rate conversions cited".
3. The **QEMU AC97 8.1 % slow** finding is a different case: the guest runs
   48000, the host plays 44100. That one is real, and it is not an AHI
   driver bug. Keep it, and say precisely what it is.

### 0.2 The real AHI bug the code shows

**HDAudio never writes the rate it selected back to the AudioCtrl.** In
`Drivers/HDAudio/main.c`, `_AHIsub_AllocAudio` (around lines 118–160):

1. It clamps `AudioCtrl->ahiac_MixFreq` **up** to `frequencies[0]` if the
   request is below the lowest rate. That write-back is correct.
2. It picks the **nearest** supported rate and stores only the *index*
   (`card->selected_freq_index`). The hardware stream format is programmed
   from that index (`main.c` ~lines 326–336).
3. It **never** sets `ahiac_MixFreq` to that selected rate.

**Consequences:**
- A request that is on the codec's list (44100, 48000, 88200, 96000, 192000
  on the Dell) works.
- **A request above the lowest rate but off the list does not.** For example,
  50000 Hz:
  - the hardware runs 48000;
  - AHI's mixer runs 50000;
  - `AHIC_MixFreq_Query` says 50000;
  - **audio plays about 4 % slow and flat, and nothing reports it.**
- This breaks AHI's own contract. `AHIA_MixFreq` is documented as "The
  actual mixing rate may or may not be exactly what you asked for"
  (`audioctrl.c` ~line 342), and `AHIC_MixFreq_Query` exists to return the
  actual rate.

**Precedents in the same tree:**
- `Drivers/Toccata/toccata.c` ~line 249 does exactly the right thing:
  `AudioCtrl->ahiac_MixFreq = T_FindFrequency(AudioCtrl->ahiac_MixFreq);`.
- `Drivers/ac97/ac97-main.c` ~line 147 forces `ahiac_MixFreq = 48000`, which
  is honest, if blunt.

**One trap to check in the core (`Device/audioctrl.c`):**
- `AHI_AllocAudioA` derives `ahiac_MinBuffSamples`/`ahiac_MaxBuffSamples` and
  the anti-click length from the *requested* `ahiac_MixFreq` (around lines
  177–290) **before** it calls `AHIsub_AllocAudio` (around line 524).
- If the driver then changes the rate, those derived values were computed
  from the request. `BuffSamples` (around line 77) appears to be computed
  later, in `AHIsub_Update`/buffer setup.
- **Establish exactly which values go stale** when a driver rewrites the
  rate, and whether Toccata's write-back already exercises this path
  harmlessly. If any stale value matters (e.g. a `MaxBuffSamples` too small
  for the true rate), the fix includes recomputing it after
  `AHIsub_AllocAudio` returns.

---

## 1. Read first

1. **The AHI sources,** in the ABIv11 tree (the Dell's):
   `/home/miller/Work/projects/Vulkan4Aros/src/abi/v11/AROS/workbench/devs/AHI/`:
   - `Device/audioctrl.c` (`AHI_AllocAudioA` and its autodoc,
     `AHI_ControlAudioA`, `AHIC_MixFreq_Query`);
   - `Device/modeinfo.c` (`AHI_GetAudioAttrsA` and its autodoc);
   - `Drivers/HDAudio/main.c` and `misc.c` (the frequency table,
     `set_frequency_info`, stream format programming);
   - `Drivers/Toccata/toccata.c` (the precedent);
   - `Drivers/ac97/ac97-main.c`;
   - `Drivers/Common/library.c` (how the PCI drivers share `AllocAudio`);
   - `Docs/ahidev.texinfo` (the driver-author documentation: what a driver
     may do to `ahiac_MixFreq`).
2. **The upstream tree for the PR:** `/home/miller/Work/projects/Vulkan4Aros/src/abi/v1/AROS`
   (remote `origin` = `aros-development-team/AROS`; fork remote as used for
   the getrusage PR, `Ashahell/AROS`). Confirm the same code is there on
   upstream master; the PR is based on **upstream master**, not on our
   vendored v11 snapshot.
3. **Our records:**
   - `audio_io/probe_rate.c`;
   - `audio_io/audio_ahi_live.c` (the `AHIC_MixFreq_Query` use);
   - the wiki article and evidence named in §0.1;
   - `llm-wiki/raw/articles/2026-10-03-riqemu1-ac97-resamples-48k-to-44k1-so-playback-is-8-1-slow.md`;
   - `docs/AhiV7.md`;
   - `docs/superpowers/plans/2026-10-06-levi-perf-opencode-prompt.md` §3
     (repo rules).
4. **The Vulkan4AROS skill `aros-development`:**
   - ABIv11 for the Dell (r12 convention, v11 SDK and toolchain), ABIv1 for
     QEMU and upstream;
   - how AROS libraries and devices are built with mmake;
   - the "probe build contract" wiki article
     (`2026-10-03-the-ahi-probe-build-contract.md`);
   - the AHI loader alignment article for riqemu1.
5. **The precedent PR flow:** our AROS getrusage PR
   aros-development-team/AROS#1490 (branch on the fork, PR text in
   `Vulkan4Aros/docs/dev/dispatch/drafts/`) and its review history.

---

## 2. Hard rules

- **Stealth rule:** never name ReIncarnation on any public tracker, PR or
  commit message that goes upstream. Upstream text describes the AROS bug,
  the measurement and the fix, nothing else.
- **Nothing outward-facing without the owner's word in chat:** no push to
  the fork, no PR, no issue comment. H4 prepares; the owner decides.
- **Dell:**
  - only the owner reboots it;
  - check `status` first, and never quit an RIAPP you did not start;
  - test binaries go to `RAM:`;
  - prefer `status` over `--ui-windows`;
  - one `--ui-capture` per job at most;
  - read logs with `--get`.
- **Replacing the Dell's `DEVS:AHI/hdaudio.audio` is a system change.** Do
  it **only** with the owner's explicit "yes" for that run, and keep a
  verified backup copy. Restore the original and verify by checksum before
  you finish, unless the owner says to keep the patched driver.
  - **AHI must not be in use** when the driver is swapped: no RIAPP or other
    AHI client running.
  - **A loaded driver stays resident** until expunged, so a swap may need an
    `AVAIL FLUSH` (or equivalent), or a reboot, which only the owner does.
    Find out which, before you ask.
- **QEMU guests:** use one only if no other session holds it. riqemu1 must
  come back at 1280x1024 if anything restarts it (verify with a screendump).
- **Repo rules:**
  - commit only your own files; tag `[ahi-rate]`; trailer
    `Co-Authored-By: OpenCode <noreply@opencode.ai>`;
  - `bash scripts/ri_audit.sh` must read `AUDIT 0/0 PASS` before each commit
    in this repo (isolated worktree if the tree has foreign WIP);
  - **do not push** this repo; the advisor reviews first.
- **Vulkan4AROS repo** (if the patch is carried there as a diff, like
  `src/abi-patches/v1/aros/0042-…`): commit only your own paths. That tree
  carries other sessions' work in progress, so never stage it.
- **Clean-room:** AROS-side work only. No ReBirth or ASM content is
  involved.

---

## 3. Phases

### H0: correct our own records (first, before anything else)

1. **The wiki article** `2026-10-03-dell-ahi-hands-back-44100-for-every-rate.md`:
   - add a `Status: Outdated (2026-10-07)` block at the top explaining the
     `AHIDB_Frequency` index-0 semantics, with the autodoc and driver line
     citations;
   - say what the correct query is, and what it returns on the Dell
     (`mix=48000` in the live backend logs; quote one log line with its date);
   - correct the index line in `llm-wiki/index.md` the same way;
   - append a log entry to `llm-wiki/log.md`.
2. **`audio_io/probe_rate.c`:**
   - make `got=` come from `AHIC_MixFreq_Query`;
   - keep the `AHIDB_Frequency` list dump, but label it as the codec's list;
   - add a line per request: `req=… mixq=… listidx0=…`, so the difference is
     visible.

   It is a probe, not shipping code, but it must not mislead the next
   reader. Follow the probe build contract.
3. **`docs/AhiV7.md`:**
   - **G3:** remove the 44100 claim. The real G3 is "latency not reported",
     plus the HDAudio off-list case (§0.2).
   - **G7:** keep the QEMU AC97 case, stated precisely (guest 48000, host
     44100, conversion outside AHI). Add the HDAudio off-list case as the
     AHI-side example, marked [CHECKED: source] until H2 measures it.
   - **§3.4:** drop the 44100 bullet if it is there.
   - **§15.2/§15.5:** reword the "48000 request on hardware that runs 44100"
     corpus item as "an off-list rate request".
   - **Q11:** rewrite it as "Which drivers fail to write back the selected
     rate? (H3)".
   - **Appendix E:** correct the row, and add a row saying what was wrong
     and why.
4. **Commit** as `ahi-rate H0: the "44100 for every rate" finding was AHIDB_Frequency index 0, not the rate; records corrected [ahi-rate]`.

### H1: reproduce the real bug

**The probe (`RATEPROBE`).** Extend `probe_rate.c`, or add a sibling probe
following the same build contract. For each requested `AHIA_MixFreq` in
`{ 44100, 48000, 50000, 60000, 96000, 100000, 32000, 22050 }`:

1. `AHI_AllocAudioA` on the HDAudio mode;
2. read `AHIC_MixFreq_Query` (call this `M`);
3. **measure the true hardware rate `H`.** Load one 16-bit sound of N frames
   (N = 4 × M, a few seconds) with sample frequency = `M`, play it looping on
   one channel, and timestamp every `AHIA_SoundFunc` callback with
   `ReadEClock()`. One loop takes N/H seconds of wall time, because AHI mixes
   N frames at M and the hardware consumes them at H.
   - Compute `H = N / (mean loop duration)` over at least 8 loops, and print
     the spread.
   - **Positive control:** at a listed rate (48000), H must equal M within
     0.1 %. If it does not, the method is wrong; fix it before reading any
     other row.
4. Print `req=… M=… H=… ratio=H/M`.

**Expectation, to confirm or refute:**
- for listed rates, `M = H = req`;
- for off-list rates above 44100, `M = req` but `H` = the nearest listed rate,
  so the ratio is ≠ 1;
- for rates below 44100, `M = H = 44100`, because the clamp is already
  honest.

**Run it** on the Dell (ABIv11 build), while no other AHI client runs. If a
free ABIv1 QEMU guest with HDA emulation exists, run it there too; otherwise
note that it was skipped.

**Evidence:** `docs/evidence/audio/ahi-rate/2026-10-07-h1-reproduce.md`,
with the verbatim probe output.

**Commit** as `ahi-rate H1: off-list MixFreq on HDAudio — mixer runs the request, hardware runs the nearest rate (probe + evidence) [ahi-rate]`.

### H2: the HDAudio fix, proven on the Dell

1. **The patch** (in the upstream-based tree):
   - after the nearest-rate selection in `_AHIsub_AllocAudio`, set
     `AudioCtrl->ahiac_MixFreq = card->frequencies[card->selected_freq_index].frequency;`;
   - keep the existing clamp, which becomes redundant but harmless; remove it
     only if the result reads better;
   - recheck the `AHISF_CANRECORD` loop, which now always matches. Decide
     whether record capability should stay conditional on the request being
     exact, and justify the choice in the PR text.
2. **The core** (`Device/audioctrl.c`): if §0.2's trap is real (values
   derived from the requested rate before the driver call), recompute them
   after `AHIsub_AllocAudio` when the rate changed. Keep this a separate,
   minimal hunk with its own justification. If it is not real, say why.
3. **Build** `hdaudio.audio` for ABIv11 (for the Dell) and ABIv1 (for
   upstream CI and the QEMU check).
   - Use the AROS build system: find the mmake target for the AHI HDAudio
     driver in each tree.
   - Verify that the ABI of each binary matches its tree, per the skill's
     r12 rules.
4. **Dell proof** (owner "yes" required; see §2):
   - back up the original driver and verify the backup;
   - install the patched driver and rerun `RATEPROBE`;
   - **Expected:** for every request, `M = H` within 0.1 %. Off-list
     requests now read the nearest listed rate.
   - Play a song in RIAPP at its normal 48000 to confirm nothing regressed:
     the `mix=` log line plus the owner's ear;
   - then restore the original driver (unless the owner says keep), and
     verify by checksum.
5. **Evidence:** `docs/evidence/audio/ahi-rate/2026-10-07-h2-fix.md`.
6. **Commit** the patch as a diff where the project keeps AROS patches (e.g.
   `Vulkan4Aros/src/abi-patches/v1/aros/00NN-ahi-hdaudio-report-selected-rate.diff`),
   following the getrusage precedent, plus the evidence here:
   `ahi-rate H2: HDAudio writes back the selected rate; Dell probe M==H for all requests [ahi-rate]`.

### H3: survey the other drivers

**For every AHI driver in the tree,** answer in a table, with `file:line`:
- does `AllocAudio` accept any `ahiac_MixFreq`, or round, or clamp?
- does it write back what it chose?
- does it program the hardware from something other than `ahiac_MixFreq`?

The drivers: `ac97`, `VIA-AC97`, `CMI8738`, `SB128`, `EMU10kx`, `Envy24`,
`Envy24HT`, `HDAudio`, `Alsa`, `OSS`, `PulseAudio`, `WASAPI`, `Paula`,
`Toccata`, `Aura`, `SoundBlasterAWE`, `Void`, `Filesave`, and any others
present. Many PCI drivers share `Drivers/Common/library.c`; trace the actual
call path.

**Classify each** as:
- **honest:** writes back, or accepts any rate;
- **honest-by-force:** fixed rate, written back;
- **dishonest:** selects without write-back. The same bug as HDAudio.
- **unknown:** couldn't tell, with the reason.

**Fix every dishonest driver** in the same minimal style, one hunk each.
Build each for ABIv1 at least, so the PR compiles.

**Do not test on hardware we do not have.** State which fixes are
compile-proven only.

**Evidence:** `docs/evidence/audio/ahi-rate/2026-10-07-h3-survey.md`.

### H4: prepare the upstream PR (no push, no PR without the owner)

1. **A branch on the upstream-based tree:** `ahi-report-selected-rate`, on
   current upstream master, with one commit per driver, plus the core hunk
   if needed.
2. **PR text draft:**
   `Vulkan4Aros/docs/dev/dispatch/drafts/ahi-selected-rate-pr.md`. It covers:
   - the contract (quote the `AHIA_MixFreq` and `AHIC_MixFreq_Query`
     autodocs);
   - the HDAudio defect, with the line numbers;
   - the Dell measurement before and after, as a table of req/M/H. Describe
     the hardware generically ("Sandy Bridge laptop, Intel HDA"), with no
     project name;
   - the survey table;
   - which fixes are hardware-tested and which compile-only;
   - the Toccata precedent.
3. **A short note** that `AHIDB_Frequency` without `AHIDB_FrequencyArg` is
   index 0, not the current rate, is worth adding to the autodoc as a
   one-line clarification. Draft it as an **optional** separate commit; the
   owner and reviewers decide.
4. **Stop.** Report the branch and the draft path. Push and PR happen only
   on the owner's word.

### H5: the record

- **The llm-wiki:** one new article covering the misread, the real bug, the
  fix and the survey. Status blocks on the outdated items, and index and
  log updates.
- **The todo** (`docs/2026-09-24-improvement-todo.md`): an "AHI honest rate"
  item, ticked as far as reached, with the PR status.
- **Commit** as `ahi-rate H5: wiki + todo [ahi-rate]`.

---

## 4. Traps

- **`AHIDB_Frequency` is not the current rate** (§0.1). Any probe line that
  calls something "got" must come from `AHIC_MixFreq_Query` or from a
  measurement.
- **The SoundFunc timing method needs its positive control** (a listed rate
  gives H = M). Without it, a method error looks like a driver bug.
- **`AHI_BestAudioID` refusing a rate is not the same as the card lacking
  it.** The 2026-10-03 probe saw it refuse 44100 while 44100 was list[0].
  Don't build conclusions on `BestAudioID`; allocate on a known-good mode ID.
- **Run probes in the foreground** with output redirected to a `RAM:` file
  and `--get` it. `Run` returns before the child prints.
- **A second AHI client next to a live RIAPP** risks contention AHI cannot
  arbitrate. Probe only with no RIAPP running.
- **ABI mismatch crashes on the first LVO call.** Check r12 moves per the
  skill before any Dell run.
- **A driver swap may not take effect without an expunge** (the old driver
  stays resident). Prove which binary is loaded, e.g. by a version string or
  a debug line, before trusting any "after" number.

---

## 5. Out of scope

- QEMU's AC97 44100 host-side rate. That is a QEMU `-audiodev` frequency
  default, outside AHI; record it, don't fix it here.
- AHI latency reporting (patch 2), the task-context `PlayerFunc` option
  (patch 3) and the conformance tool (patch 4).
- Any change to RIAPP's audio backend, beyond nothing: it already uses the
  right query.

---

## 6. Evidence layout

- **This repo:**
  - `docs/evidence/audio/ahi-rate/2026-10-07-h{1,2,3}-*.md`;
  - `audio_io/probe_rate.c` (corrected);
  - the wiki updates.
- **Vulkan4AROS:**
  - the patch diff(s);
  - the PR draft;
  - the upstream-based branch.

Every number is verbatim tool output, or derived with its components shown.

---

## 7. Success gates

| Gate | PASS condition | Evidence |
|---|---|---|
| GH0 | Wiki article, index, log and `docs/AhiV7.md` corrected with citations; `probe_rate.c` reports `AHIC_MixFreq_Query` as "got"; audit 0/0 | commit, diffs |
| GH1 | Positive control H = M at 48000 within 0.1 %; the off-list rows show M ≠ H as predicted (or the prediction refuted, with the data) | H1 evidence |
| GH2 | Patched HDAudio: M = H within 0.1 % for every request on the Dell; owner's "yes" for the swap quoted; original restored and checksummed (or the owner's "keep" quoted); RIAPP regression check | H2 evidence |
| GH3 | Survey table for every driver in the tree, with `file:line`; every dishonest driver fixed; compile-only fixes labelled | H3 evidence |
| GH4 | An upstream-based branch, one commit per driver, building for ABIv1; PR draft with no project name; nothing pushed | branch log, draft path |
| GH5 | Wiki article + status blocks + index/log; todo item | commit |
| G-all | Audit 0/0 at each commit in this repo; own files only in both repos; nothing pushed; Dell `RAM:` cleaned; the Dell's `DEVS:AHI` as found (or as the owner said) | `git status`, `list RAM:`, checksum line |

---

## 8. Reporting and handoff

After each phase, report in at most 10 lines: the gates passed with a key
value each (e.g. "req 50000: M=50000 H=48003 → after fix M=48000 H=47998"),
the commit hash, and anything unproven.

If you stop mid-way:

```
HANDOFF ahi-rate
  last green phase : H?
  last commit      : <hash(es), both repos>
  next step        : <one line>
  open questions   : <list>
  lanes touched    : <Dell? QEMU?> and state left in (DEVS:AHI driver checksum!)
```

## 9. Owner decisions (list them; do not decide)

1. Whether to swap the Dell's `hdaudio.audio` for the H2 test, and whether to
   keep the patched driver afterwards.
2. Whether and when to push the branch and open the upstream PR.
3. Whether to include the optional `AHIDB_Frequency` autodoc clarification.
