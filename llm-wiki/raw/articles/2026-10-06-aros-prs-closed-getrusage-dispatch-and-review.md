# AROS PRs #1313/#1314 closed upstream; getrusage() dispatched to opencode and reviewed (2026-10-06)

- Source: ReIncarnation session (advisor lane), 2026-10-06. Upstream facts come from `gh` against `aros-development-team/AROS`. Opencode's work is in the Vulkan4AROS repo (vendored tree `src/abi/v1/AROS`).
- Collected: 2026-10-06
- Published: 2026-10-06
- Related: [2026-10-05-review-latency-instrumentation-and-advice-log.md](2026-10-05-review-latency-instrumentation-and-advice-log.md); Vulkan4AROS wiki `2026-10-01-posixc-pr1313-1314-rework.md` and `2026-10-06-posixc-getrusage-rebase-and-proof.md`

## The two PR reworks were already done

- **Both merged by Kalamatee on 2026-10-01:**
  - #1313 "posixc: add the BSD struct rusage fields", upstream `47d7890efb`;
  - #1314 "posixc: forward strto*_l to strto* as an interim fix", upstream `dd49869e85`.
- **Each review point is met upstream:**
  - `getrusage` is back to `NOTIMPL`; the ENOSYS comment is dropped; the note says the library does not implement it.
  - The 14 BSD fields are unguarded.
  - The `strto*_l` files carry the FIXME and a once-only `bug()` notice (via `__sync_lock_test_and_set`).
  - The SPIRV-Tools claim was corrected; `SPIRV_TIMER_ENABLED` is Linux/Android-only.
- **Who did it:** a Vulkan4AROS opencode session on 2026-10-01 (`docs/dev/dispatch/AROS_posixc_pr1313_1314_rework.md`).
- **Our todo:** `docs/2026-09-24-improvement-todo.md` now has both items ticked. The optional `getrusage()` follow-up stays open.
- **Process note:** the advisor first reported the todo as already updated before the edit was made. It was corrected in the next reply and then made. Make the edit, then report it.

## getrusage() dispatch (owner request)

- Prompt file: `Vulkan4Aros/docs/dev/dispatch/AROS_posixc_getrusage_upstream.md`.
- **Starting point:** the local branch `posixc-getrusage` (`6936b31052`) sat on our pre-merge #1313 branch `1a768d872e`, so it had to be rebased onto current upstream master (`684b78f85c` at the time).
- **New upstream precedent:** `703c89a9b9` "posixc: support CLOCK_THREAD_CPUTIME_ID" (Bo Ståle Kopperud). It reads the same counter via a per-call `OpenResource("task.resource")` plus `QueryTaskTags(FindTask(NULL), TaskTag_CPUTime, ...)`, does not normalise, and returns EINVAL when the resource is missing. The prompt required consistency with it, or a justified difference.
- **Success definition:** gates G1–G20, each with a PASS condition and evidence. Among them: exactly 4 files; LVO getrusage 484 / setrlimit 486; DELTA/WALL in [0.85, 1.05]; 10,000 monotonic samples with `tv_usec < 10^6`; the error cases; campaign 29,121 equal to host 25077/0/4044; juggler exact; audit unchanged; no private names; nothing pushed.

## Opencode's result (reviewed)

- **Branch:** `posixc-getrusage-v2` = `684b78f85c` + `484d7136b4`, 4 files, +150/−6, not pushed.
- **Design kept:**
  - `ru_utime` comes from `TaskTag_CPUTime` via the genmodule-opened `TaskResBase` global; this is the same declaration pattern as upstream's `__threadhook.c`. It is normalised.
  - `ru_stime` = 0, and the BSD fields = 0 (documented as untracked).
  - `RUSAGE_CHILDREN` and any other `who` → −1/EINVAL; NULL → −1/EFAULT.
  - Missing resource → ENOSYS, which is unreachable because the library's Init already fails without the resource.
  - Per-task scope documented in BUGS.
- **Evidence:**
  - DELTA 2990258 µs / WALL 3000175 µs = 0.9967;
  - THREAD_DELTA_US=36 (per-task scope confirmed);
  - campaign diff 0/0/0;
  - juggler mean 0.00027, px>0.10 103;
  - LVO table diff and series apply proofs in `docs/evidence/2026-10-06-posixc-getrusage/`.
- **Re-checked here:** Vulkan4AROS `scripts/aros_audit.sh` gives 0 errors / 0 warnings.
- **PR draft corrected** (`docs/dev/dispatch/drafts/getrusage-pr.md`):
  - It claimed 0 µs agreement with `CLOCK_THREAD_CPUTIME_ID` "measured". The guest library predates that case, so the probe compared against a replica making the identical call, which proves nothing. Replaced with "same tag, same task, same value" and no measurement claim.
  - `RUSAGE_CHILDREN` → EINVAL is a deliberate deviation, because POSIX lists `RUSAGE_CHILDREN` as a valid `who`. The draft now says so and offers zeros if the reviewer prefers.
- **Owner decisions still open:**
  - Push the branch and open the PR. Both prerequisites are merged, so the "after #1313 merges" send order is moot.
  - Commit the Vulkan4AROS getrusage files. That tree also holds opencode's unrelated uncommitted NVK work (`0049`–`0051` patches, `NVK_vmm_refcount_root_fix.md`), so only getrusage paths may be staged.
- **Findings carried forward:**
  - `QueryTaskTagList` rounds `(tv_nsec + 500) / 1000`, so `tv_micro` can be 1000000.
  - `clock_gettime(CLOCK_THREAD_CPUTIME_ID)` does not normalise and can return `tv_nsec == 1000000000`.
