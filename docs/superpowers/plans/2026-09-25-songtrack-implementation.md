# Song track Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the owner-approved song track (dense 999x4 pattern-selection grid, downbeat capture, change-only emission, measure edits, STRK codec) as pure, tested C.

**Architecture:** Two headers over one TU. `engine/seq/songtrack.h` holds the model + edits and depends on `transport.h` only; `engine/seq/songtrack_emit.h` holds the emitter and is the only one that names `sched.h`/`clock.h` types. `engine/seq/songtrack.c` implements both (no alloc/IO, no mutable static state). `project/rbng.{h,c}` gains the STRK chunk plus a `track` field on `RISong` and includes the MODEL header only, so the project layer never pulls scheduler headers. One new test file `t59_songtrack`; audit wiring beside t58, including an AROS compile-only line and the static-state grep.

**Tech Stack:** C99, host gcc; `x86_64-aros-gcc` compile-only check added to the audit's existing "AROS compile of new AROS-side code" loop (Task 5 — today NO engine/seq TU is cross-compiled by the audit, so this plan adds the gate instead of claiming one); `ri_assert.h` test helpers.

**Spec:** `docs/superpowers/specs/2026-09-25-songtrack-design.md` — the plan argues from the spec; executors read both. Every E1 quote used here is already located in that spec's section 0 (grepped from `/tmp/rb201-manual.pdf`); no external fact enters this plan.

**Already landed (do not re-implement):** `RI_SONG_BARS` + `ri_bar_quantize_next` (`e85f198`), and the `RI_SONG_BARS == RI_SEQ_MAX_BARS` identity correction (`11d72c9`). `engine/seq/transport.h` is the single source of both; this plan consumes them. The quantizer's own cases (downbeat keeps its bar, mid-measure moves to the next, clamp at 998) are pinned in `t58_transport` (`q exact 0/1`, `q mid 0`, `q late 0`); t59 only pins the composition with capture.

## Review 2026-09-25 (applied in this revision)

Blunt summary: the model, edit and codec designs are sound and the tests are unusually sharp (the phase pin, the overflow pin, the minor-forcing fix). What was wrong was mostly consistency — with the spec, with the plan's own laws, and with its own TDD rule. Every fix below is already written into the tasks. Nothing here changes the owner-approved behaviour.

| # | Severity | Finding | Fix in this revision |
|---|----------|---------|----------------------|
| R1 | HIGH | The emitter's carry mirrored the TRACK even when the cap dropped an event. A dropped change was therefore recorded as "sent" and never re-sent, so the section kept the wrong pattern until the next real change. "Deterministic loss" was permanent loss. | The carry mirrors what was EMITTED, per instance. A capped change is re-sent at the next measure (late, never lost). Test: cap 2 at bar 4, then the same bar again sends the remaining 2. |
| R2 | HIGH | The walker never normalized the loop: a raw loop `start 995 len 10` let the walk reach bar 999 and stop, while transport (which clamps) keeps looping. This broke the "normalize first, reuse transport laws" law. | Walker clamps a local copy with `ri_loop_clamp(&l, RI_SONG_BARS)` before any arithmetic. Test: an unclamped loop never reaches 999. |
| R3 | HIGH | Spec §6 requires a replay property and an emission sweep; the plan dropped both silently (it deferred the run VIEW, which is a different thing). Replay is the one property that proves emission and `selected()` cannot disagree. | Both added to Task 2 (replay over a deterministic 999-bar pattern; alternating sweep = 4 + 998 events). |
| R4 | MEDIUM | TDD rule broken by the plan's own steps: every task went scaffolding RED → full body, so the required BEHAVIORAL RED never happened. | Each task gets Step 3a (stub bodies that compile but return wrong or no-op results → record the first failing `RI_ASSERT` line), then 3b (real body). |
| R5 | MEDIUM | Layer guard in Task 1 was WEAKENED in Task 2 (sched/clock/RILoop un-banned) to fit the emitter. Growing `rbng.h → songtrack.h` would also drag `sched.h`/`clock.h` into the project layer. | Split `songtrack_emit.h`. The Task 1 guard is never narrowed; a second guard pins `songtrack_emit.h` to model + sched/clock + transport only. |
| R6 | MEDIUM | "Bulk writers mask (`& 31`)" contradicts the fail-closed law every other slot entry point obeys (capture refuses, the codec rejects). Masking turns corrupt 40 into a real, wrong pattern 8. No legitimate caller produces >31. | Bulk writers VALIDATE then write, all-or-nothing, and return `int` (0 ok / 2 refused, track untouched). The mask tests become refusal tests. |
| R7 | MEDIUM | Codec reject tests passed on ANY error (no message check), and the out-of-range byte was the FIRST body byte, which hid partial stores. The note claiming "never partially stored" was only true for byte 0. | `parse_strk` validates the whole body before storing. Tests assert the specific `err` text and put the bad byte LAST (bar 998, instance 3). |
| R8 | MEDIUM | Task 1 edited `scripts/ri_build_aros.sh` line 56 — the GUI's RISECT proof-app TU list, owned by the §12.10 session, which never calls songtrack. That is a coexistence clash, and it proves nothing for the audit (the audit builds the default target, not `sections`). | Dropped. Task 5 adds `engine/seq/songtrack.c` to the audit's AROS compile-only loop instead. |
| R9 | MEDIUM | "No mutable static state in `songtrack.c`" had no enforcement point, against the plan's own Proof discipline. | Task 5 audit grep (Phase 7b style) fails on any non-const file-scope static object in `songtrack.c`. |
| R10 | LOW | Spec/plan drift: the spec still says init-loop "CLEARS the rest of the loop" (contradicted by the E1 quote the plan uses); it still shows the `prev[4], force` signature and "(track, banks, …)" inputs; spec §6's "pattern-mode path never calls capture" has no code in this slice (there is no record path yet). | Task 0 Step 4 amends the spec. The capture-gating test is DEFERRED to the record-path slice, with a named ledger row. Post-r2 status (2026-09-26): the gating half is CLOSED by m68 record-gate (`ri_record_capture`, t59 scope); the run-view half stays deferred (GUI song-editor slice). |
| R11 | LOW | Quantizer cases in spec §6 (mid-bar 5 → 6, downbeat stays) are already pinned by t58 (`q exact`/`q mid`), so the plan should cite that rather than leave the gap unexplained. Nits: a comment said "4-bar paste" for a 2-bar clip; `clip.len` passed to `%u` without the cast used elsewhere. | Cited in Task 1; nits fixed. |

**Verified, not just argued:** the code in this revision was assembled verbatim from the plan's blocks into a scratch worktree at HEAD `879be9e` (Tasks 1–4, rbng edits as written), built under the repo `CFLAGS` with `-Werror`: `PASS songtrack`. Each of the eight mutants in Task 5 Step 4 was applied and FAILED as stated ((d) as a hard fault with rc 1 and no assert line). The run found one defect in this revision itself, since fixed: the emit header's comment named the codec header and tripped its own layer guard.

Accepted as-is, after checking: the local `seq = 0` per range call (same convention as `pattern_emit.c`; the sort key reaches `seq` only after sample/type/device/voice, and the track emits at most one PATTERN_CHANGE per (sample, device)); the separate `ppq` argument next to `map->ppq` (same convention as `ri_sched_emit_*`); the 999-bar cap per walker call (the carry crosses calls, so a longer looped render is just more calls); the relative `fopen("engine/seq/…")` in the guard (the audit `cd`s to `$ROOT`).

## Song laws (normative — implementation, tests, and integration must preserve them)

### Ownership
* `RISongTrack` owns exactly the 999x4 slot grid — pattern NUMBERS, never pattern data. Banks own the patterns.
* `transport.h` owns cursor/geometry/loop laws (`RI_SONG_BARS`, `ri_ppq_or_default`, `ri_loop_clamp`, staging). This slice REUSES them and redefines nothing: no local bar ceiling, no forked 999.
* One direction only: `songtrack.h` -> `transport.h`. `transport.h` never includes `songtrack.h`. No mutable static state in `songtrack.c`.
* The emitter owns nothing: it reads (track, loop, carry, range, map) and appends events. Cursor stays in transport; sounding changeover stays in the streaming slice.

### Cursor
* The cursor is ticks. Bar indices are 0-based `0..RI_SONG_BARS-1`; `RI_SONG_BARS` (999) is the END BOUNDARY and is never a valid start — the same law transport seeks obey.
* Samples come only from `ri_map_tick`. This slice never converts ticks<->samples itself.
* Capture, emission, and display never move the cursor.

### Geometry
* A song is ALWAYS 999 bars (E1 section 0). No length field, no short song, no tail pointer.
* Every public entry point normalizes BEFORE arithmetic: clamp lengths against `RI_SONG_BARS - start` FIRST, then add. `start + len` is never evaluated on an unbounded `len` (unsigned addition is modulo — it would wrap past the clamp and read/write out of bounds).
* Refusal vs repair, stated once: out-of-range SLOT VALUES are always refused (reads, single writes, bulk writes, codec — one law, no masking). Out-of-range BAR RANGES in bulk operations are repaired by clamping to the song (the operation stays total over positions); a range that clamps to zero is a no-op.

### Capture
* Downbeats only. Quantization (`ri_bar_quantize_next`, transport) is CALLER-side; the model writes exactly the bar it is told and is gate-blind (testable without a transport).
* One slot per call, per (bar, instance). Multi-section passes are N calls (E1 multi-pass, p. 76).
* Slot > 31, bar >= 999, instance >= 4, NULL: REFUSE (rc 2). Reads return slot 0 in those cases.

### Emission
* Recorded truth, not sounding truth: marks which slot is SELECTED at each measure line. The p. 20 pattern-end changeover belongs to the streaming slice.
* Change detection spans windows through a caller-owned carry (`known` mask + `prev[4]`), never a per-call local — otherwise a per-block render loop re-emits all four slots every block.
* The loop arrives raw and is normalized FIRST: the walker clamps a local copy with `ri_loop_clamp(&l, RI_SONG_BARS)` (transport law, reused), so a loop can never carry the walk to bar 999.
* Loop wrap preserves phase: `bar = start + ((bar - start) % len)`. Never `bar = start`.
* `loop->len_bars == 0` (after the clamp) behaves as loop OFF. Cap pressure drops later instances/bars first, deterministically. The carry mirrors what was EMITTED: a change the cap dropped stays pending and is re-sent at the next measure — late, never lost.

### Edits
* Init-song fills all 999 from the four live selections. Init-loop fills EVERY bar inside the loop (E1 p. 176: *"all measures inside the Loop are filled"*) and touches nothing outside.
* Cut removes (tail shifts left, freed end fills slot 0); copy is pure; paste inserts (tail shifts right, drops past the end); paste-replace overwrites. Nothing ever grows.
* Slot-value law: every writer refuses a slot > 31. Bulk writers validate ALL input first and then write, so a refusal leaves the track untouched (all-or-nothing, rc 2).
* Slot 0 is a NEUTRAL SELECTION, never silence. Silence is user data.

### Codec
* `STRK` body is exactly `RI_RBNG_STRK_BYTES` (derived, 3996), row-major, no pad. `!= 3996` rejects; slot > 31 rejects; minor-0 never carries it; missing STRK means slot 0.
* Writer: omit STRK when the track is all-zero; raise the minor to 1 when banks OR a non-empty track exist (a track-without-banks written as minor 0 would be unreadable by its own reader).

### Proof discipline
Every invariant has (1) one named enforcement point, (2) executable tests, (3) at least one boundary test, (4) at least one mutation test.

## Global Constraints

- `-std=c99 -Wall -Wextra -Werror -pedantic -ftrapv` (repo `CFLAGS`).
- No allocation, IO, libm, time, or global RNG in `engine/` (audit Phase 0a/0b grep this literally: `malloc|calloc|realloc|free(|Forbid|Disable(` and `tanhf|sinf|cosf|expf|powf|fmodf|mul_add|ffast-math`). Wording in comments is load-bearing — avoid `free(`.
- TDD: watch each test fail first (RED) for a BEHAVIORAL reason (wrong value/branch), then GREEN. Missing-header / implicit-declaration failures are scaffolding only. Every task therefore has Step 3a: add the declarations with STUB bodies (return 0 / do nothing / return 2) that compile cleanly under `-Werror` (`(void)` every unused parameter), run the test, and record the first failing `RI_ASSERT` line in the commit body. Step 3b then replaces the stubs with the real bodies.
- `mkdir -p /tmp/ri/run` before any file-writing test (the audit wipes `/tmp/ri`).
- Full `scripts/ri_audit.sh` 0/0 before every commit.
- Commit messages end with `[§12.9b]` plus the `Co-Authored-By: OpenCode <noreply@opencode.ai>` line.
- Existing goldens must not move (this slice adds none).

## Review Focus

- Same-bar re-capture overwrites (never appends, never duplicates) — Task 1.
- Capture composed with the downbeat quantizer at bar 998 lands 998, never 999 — Task 1.
- Loop wrap PRESERVES PHASE: an 8-bar walk over a 2-bar loop emits 10 events, not 6 (a `bar = loop_start` wrap would emit 6 and still pass the old 4-bar test) — Task 2.
- Cross-window establishment fires once, not per call — Task 2.
- A capped change is re-sent at the next measure, never lost (carry mirrors emitted) — Task 2.
- A raw loop past the song end is clamped before the walk and never reaches 999 — Task 2.
- Replay of the emitted events reproduces `selected()` at all 999 bars — Task 2.
- Bulk writers refuse slot > 31 all-or-nothing; nothing masks — Task 3.
- STRK rejects carry their own `err` text; the bad byte is the LAST one — Task 4.
- Cut at the end fills the freed tail with slot 0 without reading past bar 998 — Task 3.
- Paste-insert overflow drops past the end and `start + len` never wraps — Task 3.
- A track with no banks round-trips (minor becomes 1) — Task 4.

---

### Task 0: Preconditions (no code)

**Files:** none (read-only).

- [ ] **Step 1: Confirm a clean tree for the touched paths**
```bash
git status --short engine/seq project/rbng.h project/rbng.c tests/unit/t59_songtrack.c scripts/
```
Expected: empty output.

- [ ] **Step 2: Baseline audit 0/0, save the log**
```bash
mkdir -p /tmp/ri/run && bash scripts/ri_audit.sh > /tmp/ri/audit-baseline-129b.log 2>&1; echo "AUDIT_RC=$?"
```
Expected: `AUDIT_RC=0`, tail `AUDIT 0/0 PASS`.

- [ ] **Step 3: Re-read the integration points**
```bash
sed -n '179,200p;337,346p' project/rbng.c   # write_bank + the VERS/SONG writer block
grep -n "parse_bank\|else if (memcmp(cid" project/rbng.c | head -20
```
Expected: (a) `write_bank` writes `chunk_head(f, "BANK", bl)` then the body then an even pad — STRK mirrors that shape with a fixed even length (3996: no pad); (b) the writer's minor is currently `s->nbanks > 0u ? 1u : 0u` — Task 4 must extend that expression, or a track-without-banks file is written minor 0 and its own reader rejects it; (c) `parse_bank` is a separate static function reached from the `else if (memcmp(cid, ...))` chain — `parse_strk` mirrors it exactly, including `ck_err(err, errcap, cid, off, "msg")`.

- [ ] **Step 4: Amend the spec so plan and spec agree (docs only, own commit)**

In `docs/superpowers/specs/2026-09-25-songtrack-design.md`:
- §laws Edits and §3: init-loop fills EVERY bar inside the loop and touches nothing outside (E1 p. 176 *"all measures inside the Loop are filled"*). Delete "CLEARS the rest of the loop" in both places: it contradicts the quote in §0.
- §laws Emission and §2: replace the `ri_track_emit_measure(t, bar, prev[4], force, ev, n)` form with the carry form used here (`RITrackCarry`, establishment = cold carry), drop "banks" from the emitter's inputs (it reads slot numbers only), add "loop normalized with `ri_loop_clamp` first" and "carry mirrors emitted events, capped changes are re-sent".
- §laws Edits: slot > 31 is refused by every writer, bulk writers are all-or-nothing (R6).
- §3 run view: mark it DEFERRED to the GUI song-editor slice (no caller in this slice). The replay property stays in §6 because it does not need the view.
- §6: the "pattern-mode path never calls capture" case moves to the record-path slice (row added to §5 Open items). No record path exists in this slice to test. Post-r2 status (2026-09-26): CLOSED by m68 record-gate.
```bash
git add docs/superpowers/specs/2026-09-25-songtrack-design.md
git commit -m "docs: songtrack spec aligned with the plan review (init-loop, carry, refusal) [§12.9b]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 1: Data model + capture (t59 part 1)

**Files:**
- Create: `engine/seq/songtrack.h`
- Create: `engine/seq/songtrack.c`
- Create: `tests/unit/t59_songtrack.c` (grows in later tasks; this task owns model + capture)

**Interfaces:**
- Consumes: `RI_SONG_BARS` and the inline `ri_bar_quantize_next` from `engine/seq/transport.h` (constants and inline geometry only). The Task 1 layer-guard test pins that nothing from transport or beyond appears in `songtrack.h` as a struct/symbol other than those.
- Produces (exact signatures, used by Tasks 2-4):
```c
#define RI_SONGTRACK_BARS      RI_SONG_BARS   /* single source; no forked 999 */
#define RI_SONGTRACK_INSTANCES 4u
#define RI_SONGTRACK_MAX_SLOT  31u
struct RISongTrack { uint8_t slot[RI_SONGTRACK_BARS][RI_SONGTRACK_INSTANCES]; };
void     ri_track_init(struct RISongTrack *t);
uint8_t  ri_track_selected(const struct RISongTrack *t, uint64_t bar, uint32_t instance);
int      ri_track_capture(struct RISongTrack *t, uint64_t bar, uint32_t instance, uint8_t slot);
int      ri_track_is_empty(const struct RISongTrack *t);
```
`ri_track_capture` returns 0 wrote / 2 refused (the fail-closed law, testable). `selected` returns the slot, 0 on refusal. `ri_track_is_empty` is the single enforcement point of the codec's "all-zero" omission rule (Task 4).

- [ ] **Step 1: Write the failing test**

```c
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/songtrack.h"

/* Song geometry comes from transport.h; the track must not fork it. */
typedef char ri_t59_bars_match[(RI_SONGTRACK_BARS == RI_SONG_BARS) ? 1 : -1];

int main(void) {
    struct RISongTrack t;
    uint64_t b;
    uint32_t i;
    (void)sizeof(ri_t59_bars_match);
    RI_ASSERT(RI_SONGTRACK_BARS == 999u, "bars %u", RI_SONGTRACK_BARS);
    ri_track_init(&t);
    RI_ASSERT(ri_track_is_empty(&t) == 1, "fresh empty");
    RI_ASSERT(ri_track_selected(&t, 0u, 0u) == 0u, "init slot");
    RI_ASSERT(ri_track_selected(&t, 998u, 3u) == 0u, "init tail");
    /* Fail-closed reads: bar 999/5000, instance 4/255, NULL track. */
    RI_ASSERT(ri_track_selected(&t, 0u, 4u) == 0u, "read inst 4");
    RI_ASSERT(ri_track_selected(&t, 0u, 255u) == 0u, "read inst 255");
    RI_ASSERT(ri_track_selected(0, 0u, 0u) == 0u, "read null");
    /* Boundary reads are pinned through a WRAPPER, so a botched range check
     * lands in `guard` (a wrong value and a wrong byte) instead of reading
     * undefined memory past a bare array and happening to see zero. */
    {
        struct { struct RISongTrack t; unsigned char guard[32]; } w;
        uint32_t g;
        memset(&w, 0x5A, sizeof w);
        ri_track_init(&w.t);
        RI_ASSERT(ri_track_selected(&w.t, 999u, 0u) == 0u, "read bar 999");
        RI_ASSERT(ri_track_selected(&w.t, 5000u, 0u) == 0u, "read bar 5000");
        for (g = 0u; g < sizeof w.guard; g++)
            RI_ASSERT(w.guard[g] == 0x5A, "read past the grid");
    }
    /* Capture writes exactly; neighbors untouched. */
    RI_ASSERT(ri_track_capture(&t, 5u, 1u, 7u) == 0, "capture rc");
    RI_ASSERT(ri_track_selected(&t, 5u, 1u) == 7u, "capture stored");
    RI_ASSERT(ri_track_is_empty(&t) == 0, "non-empty");
    RI_ASSERT(ri_track_selected(&t, 4u, 1u) == 0u, "neighbor low");
    RI_ASSERT(ri_track_selected(&t, 6u, 1u) == 0u, "neighbor high");
    RI_ASSERT(ri_track_selected(&t, 5u, 0u) == 0u, "neighbor inst");
    /* Same-bar re-capture overwrites. */
    RI_ASSERT(ri_track_capture(&t, 5u, 1u, 3u) == 0, "recapture rc");
    RI_ASSERT(ri_track_selected(&t, 5u, 1u) == 3u, "recapture wins");
    /* Refusals: nothing stored, neighbors untouched. */
    RI_ASSERT(ri_track_capture(&t, 999u, 0u, 9u) == 2, "cap bar 999");
    RI_ASSERT(ri_track_capture(&t, 0u, 4u, 9u) == 2, "cap inst 4");
    RI_ASSERT(ri_track_capture(&t, 0u, 0u, 32u) == 2, "cap slot 32");
    RI_ASSERT(ri_track_capture(0, 0u, 0u, 1u) == 2, "cap null");
    RI_ASSERT(ri_track_selected(&t, 998u, 0u) == 0u, "refusal leak");
    /* Highest legal values land. */
    RI_ASSERT(ri_track_capture(&t, 998u, 3u, 31u) == 0, "top rc");
    RI_ASSERT(ri_track_selected(&t, 998u, 3u) == 31u, "top stored");
    /* Composition with the transport quantizer: a mid-measure flip at
     * bar 998 quantizes to 998 (never 999) and stores there. */
    RI_ASSERT(ri_bar_quantize_next(998u * 384u + 200u, 96u) == 998u, "q 998");
    RI_ASSERT(ri_track_capture(&t, ri_bar_quantize_next(998u * 384u + 200u, 96u),
        0u, 9u) == 0, "q cap rc");
    RI_ASSERT(ri_track_selected(&t, 998u, 0u) == 9u, "q cap stored");
    /* Layer guard: songtrack.h sees transport.h geometry only. */
    {
        FILE *fh = fopen("engine/seq/songtrack.h", "r");
        char line[256];
        int bad = 0, has_transport = 0;
        RI_ASSERT(fh != 0, "open header");
        if (fh) {
            while (fgets(line, sizeof line, fh)) {
                if (strstr(line, "transport.h"))
                    has_transport = 1;
                /* BANNED set, permanent: the emitter's types live in
                 * songtrack_emit.h (Task 2), so this guard is never narrowed.
                 * Never delete it to make a build pass. */
                if (strstr(line, "RISeq") || strstr(line, "RITransport") ||
                    strstr(line, "RILoop") || strstr(line, "sched.h") ||
                    strstr(line, "clock.h") || strstr(line, "pattern.h"))
                    bad = 1;
            }
            fclose(fh);
        }
        RI_ASSERT(has_transport, "no transport include");
        RI_ASSERT(!bad, "layer leak");
    }
    /* Property loop: all-999 round-trip. */
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            RI_ASSERT(ri_track_capture(&t, b, i, (uint8_t)(b % 32u)) == 0, "prop cap");
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            RI_ASSERT(ri_track_selected(&t, b, i) == (uint8_t)(b % 32u),
                "roundtrip %llu/%u", (unsigned long long)b, i);
    RI_RESULT("songtrack");
}
```
Note: the track is ~4 KB — as a test-local struct that is fine on a host 8 MB stack. `fopen`/`fgets`/`strstr` need `stdio.h` + `string.h` (both included above).

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | head -3`
Expected: FAIL with `engine/seq/songtrack.h: No such file or directory` (scaffolding RED only).

- [ ] **Step 3a: Behavioral RED** — create the header below with `songtrack.c` STUBS: `ri_track_init` empty, `ri_track_selected` returns 0, `ri_track_capture` returns 2, `ri_track_is_empty` returns 1 (all parameters `(void)`-cast). Wire the build (Step 4 edit), run the test. Expected: first failure `capture rc` (stub refuses a legal write). Record that line for the commit body.

- [ ] **Step 3b: Write minimal implementation**

```c
/* songtrack.h — dense pattern-selection track (spec 2026-09-25 §1).
 * Pure, no alloc, no IO, no mutable static state.
 * One direction only: songtrack -> transport (geometry constants). */
#ifndef RI_SONGTRACK_H
#define RI_SONGTRACK_H
#include <stdint.h>
#include "engine/seq/transport.h"

#define RI_SONGTRACK_BARS      RI_SONG_BARS /* single source; never a forked 999 */
#define RI_SONGTRACK_INSTANCES 4u           /* 303A 303B 808 909 (Classic) */
#define RI_SONGTRACK_MAX_SLOT  31u          /* 4 banks x 8 (p. 147) */

struct RISongTrack { uint8_t slot[RI_SONGTRACK_BARS][RI_SONGTRACK_INSTANCES]; };

void     ri_track_init(struct RISongTrack *t);
uint8_t  ri_track_selected(const struct RISongTrack *t, uint64_t bar, uint32_t instance);
int      ri_track_capture(struct RISongTrack *t, uint64_t bar, uint32_t instance, uint8_t slot);
int      ri_track_is_empty(const struct RISongTrack *t);
#endif
```
```c
/* songtrack.c — song track model (spec 2026-09-25 §1, laws §Capture).
 * Reads and single writes REFUSE out-of-range input (fail-closed);
 * nothing here wraps, clamps up, or allocates. */
#include "engine/seq/songtrack.h"

void ri_track_init(struct RISongTrack *t) {
    uint32_t b, i;
    if (!t)
        return;
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[b][i] = 0u;
}

uint8_t ri_track_selected(const struct RISongTrack *t, uint64_t bar, uint32_t instance) {
    if (!t)
        return 0u;
    if (bar >= (uint64_t)RI_SONGTRACK_BARS || instance >= RI_SONGTRACK_INSTANCES)
        return 0u; /* end boundary is never a valid start */
    return t->slot[bar][instance];
}

int ri_track_capture(struct RISongTrack *t, uint64_t bar, uint32_t instance, uint8_t slot) {
    if (!t)
        return 2;
    if (bar >= (uint64_t)RI_SONGTRACK_BARS || instance >= RI_SONGTRACK_INSTANCES)
        return 2;
    if (slot > RI_SONGTRACK_MAX_SLOT)
        return 2;
    t->slot[bar][instance] = slot; /* one slot per (bar, instance): overwrite */
    return 0;
}

int ri_track_is_empty(const struct RISongTrack *t) {
    uint32_t b, i;
    if (!t)
        return 1;
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            if (t->slot[b][i] != 0u)
                return 0;
    return 1;
}
```
(`RI_SONGTRACK_BARS == RI_SONG_BARS` is pinned by the file-scope typedef in the test, not by a comment. Because the constant is aliased, a transport-side edit propagates instead of forking.)

- [ ] **Step 4: Wire the build, then run test to verify it passes**

Edit `scripts/ri_build_host.sh` line 11: append ` engine/seq/songtrack.c` to `MOD_sched`.
Do NOT touch `scripts/ri_build_aros.sh`: its `sections` target is the §12.10 GUI proof app (another session's line), never calls songtrack, and is not what the audit builds. The AROS compile gate for this TU is added to the audit in Task 5.
Run: `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | tail -2`
Expected: `PASS songtrack`

- [ ] **Step 5: Commit**

```bash
git add engine/seq/songtrack.h engine/seq/songtrack.c tests/unit/t59_songtrack.c scripts/ri_build_host.sh
git commit -m "feat: song track model + downbeat capture [§12.9b]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 2: Emission — measure fn + range walker (t59 part 2)

**Files:**
- Create: `engine/seq/songtrack_emit.h` (the only header that names `sched.h`/`clock.h` types)
- Modify: `engine/seq/songtrack.c`, `tests/unit/t59_songtrack.c`

**Interfaces:**
- Consumes: `ri_seq_tick_of_bar` + `ri_map_tick` (transport/clock), `RILoop` (transport), `RIEvent` layout (sched), the `RI_SCHED_MAX_EVENTS` cap convention.
- Produces (exact signatures, consumed by the streaming slice later):
```c
/* Cross-window change cache: the caller owns it (RISchedCarry precedent).
 * known bit i = prev[i] holds what was last EMITTED for instance i. */
struct RITrackCarry { uint8_t known; uint8_t prev[RI_SONGTRACK_INSTANCES]; };

uint32_t ri_track_emit_measure(const struct RISongTrack *t, uint64_t bar,
    struct RITrackCarry *carry, const struct RITempoMap *map, uint32_t ppq,
    struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq);
uint32_t ri_track_emit_range(const struct RISongTrack *t, uint64_t bar_first,
    uint64_t bar_count, const struct RILoop *loop, const struct RITempoMap *map,
    uint32_t ppq, struct RITrackCarry *carry, struct RIEvent *out, uint32_t cap);
int ri_song_ended(uint64_t bar_now);
```
`ri_track_emit_measure` takes the append state (`out/n/cap/seq`) so the walker and any future caller share one append path; it updates `carry` itself (one owner, one enforcement point). `loop == NULL` means OFF. A cold carry (`known == 0`) establishes all four instances; a change dropped by the cap leaves its `known` bit/`prev` untouched, so it is re-sent at the next measure.

- [ ] **Step 1a: Extend the test's includes** (top of `tests/unit/t59_songtrack.c`)
```c
#include "engine/seq/songtrack_emit.h"   /* emitter: RIEvent/RITempoMap users */
```
And add a second layer guard next to the Task 1 guard (the Task 1 guard itself is NOT touched):
```c
    /* Layer guard 2: the emitter header may add sched/clock, nothing else. */
    {
        FILE *fh = fopen("engine/seq/songtrack_emit.h", "r");
        char line[256];
        int bad = 0, has_model = 0;
        RI_ASSERT(fh != 0, "open emit header");
        if (fh) {
            while (fgets(line, sizeof line, fh)) {
                if (strstr(line, "songtrack.h\""))
                    has_model = 1;
                if (strstr(line, "RISeq") || strstr(line, "RITransport") ||
                    strstr(line, "pattern.h") || strstr(line, "rbng.h"))
                    bad = 1;
            }
            fclose(fh);
        }
        RI_ASSERT(has_model, "emit header must build on the model");
        RI_ASSERT(!bad, "emit layer leak");
    }
```

- [ ] **Step 1b: Write the failing test** (append before `RI_RESULT("songtrack");`)

```c
    /* ---- emission ---- */
    {
        /* 140 BPM fixture, same shape as t55/pattern_emit tests. */
        static struct RISegment seg = { 0ULL, 428571428ULL };
        struct RITempoMap map;
        struct RISongTrack tr;
        struct RITrackCarry carry;
        struct RIEvent ev[64];
        uint32_t n, seq, k;
        map.segs = &seg; map.n = 1u; map.ppq = 96u; map.sr = 48000u;

        /* Establishment: a fresh carry emits all four slots at the bar. */
        ri_track_init(&tr);
        ri_track_capture(&tr, 3u, 0u, 5u);
        memset(&carry, 0, sizeof carry);
        n = 0u; seq = 0u;
        RI_ASSERT(ri_track_emit_measure(&tr, 3u, &carry, &map, 96u, ev, &n, 64u, &seq) == 4u,
            "establish added %u", n);
        RI_ASSERT(n == 4u && seq == 4u, "establish count");
        for (k = 0u; k < 4u; k++) {
            RI_ASSERT(ev[k].type == RI_EV_PATTERN_CHANGE, "type %u", ev[k].type);
            RI_ASSERT(ev[k].device == (uint16_t)k && ev[k].voice == 0u, "dev %u", k);
            RI_ASSERT(ev[k].sample == ri_map_tick(&map, ri_seq_tick_of_bar(96u, 3u)),
                "sample %u", k);
            RI_ASSERT(ev[k].seq == k, "seq %u", k);
        }
        RI_ASSERT(ev[0].value == 5u && ev[1].value == 0u, "values");
        RI_ASSERT(carry.known == 0x0Fu && carry.prev[0] == 5u, "carry seeded");

        /* Same bar again: no change, no events. */
        n = 0u;
        RI_ASSERT(ri_track_emit_measure(&tr, 3u, &carry, &map, 96u, ev, &n, 64u, &seq) == 0u,
            "steady added");

        /* Change to slot 0 fires (slot 0 is first-class), and back again. */
        ri_track_capture(&tr, 3u, 0u, 0u);
        n = 0u;
        RI_ASSERT(ri_track_emit_measure(&tr, 3u, &carry, &map, 96u, ev, &n, 64u, &seq) == 1u,
            "to-zero added");
        RI_ASSERT(ev[0].device == 0u && ev[0].value == 0u, "to-zero ev");
        ri_track_capture(&tr, 3u, 1u, 7u);
        n = 0u;
        RI_ASSERT(ri_track_emit_measure(&tr, 3u, &carry, &map, 96u, ev, &n, 64u, &seq) == 1u,
            "one-lane added");
        RI_ASSERT(ev[0].device == 1u && ev[0].value == 7u, "one-lane ev");

        /* End boundary is never a valid start, even with force pending. */
        memset(&carry, 0, sizeof carry);
        n = 0u;
        RI_ASSERT(ri_track_emit_measure(&tr, 999u, &carry, &map, 96u, ev, &n, 64u, &seq) == 0u,
            "measure bar 999");
        RI_ASSERT(carry.known == 0u, "999 left carry cold");

        /* Cap pressure: 4 pending, cap 2 -> instances 0,1 go out; 2,3 stay
         * PENDING (carry mirrors what was emitted) and are re-sent at the
         * next measure call — late, never lost. */
        memset(&carry, 0, sizeof carry);
        n = 0u;
        RI_ASSERT(ri_track_emit_measure(&tr, 4u, &carry, &map, 96u, ev, &n, 2u, &seq) == 2u,
            "cap added");
        RI_ASSERT(n == 2u && carry.known == 0x03u, "cap state %u", (unsigned)carry.known);
        n = 0u;
        RI_ASSERT(ri_track_emit_measure(&tr, 4u, &carry, &map, 96u, ev, &n, 64u, &seq) == 2u,
            "capped changes re-sent");
        RI_ASSERT(ev[0].device == 2u && ev[1].device == 3u && carry.known == 0x0Fu, "re-sent lanes");

        /* NULL safety. */
        n = 0u;
        RI_ASSERT(ri_track_emit_measure(0, 0u, &carry, &map, 96u, ev, &n, 64u, &seq) == 0u,
            "measure null track");
        RI_ASSERT(ri_track_emit_measure(&tr, 0u, 0, &map, 96u, ev, &n, 64u, &seq) == 0u,
            "measure null carry");
        RI_ASSERT(ri_track_emit_measure(&tr, 0u, &carry, 0, 96u, ev, &n, 64u, &seq) == 0u,
            "measure null map");
        RI_ASSERT(ri_track_emit_measure(&tr, 0u, &carry, &map, 96u, 0, &n, 64u, &seq) == 0u,
            "measure null out");
        RI_ASSERT(ri_track_emit_measure(&tr, 0u, &carry, &map, 96u, ev, 0, 64u, &seq) == 0u,
            "measure null n");
        RI_ASSERT(ri_track_emit_measure(&tr, 0u, &carry, &map, 96u, ev, &n, 64u, 0) == 0u,
            "measure null seq");
        RI_ASSERT(ri_song_ended(998u) == 0 && ri_song_ended(999u) == 1,
            "ended predicate");
    }
    /* ---- range walker: establishment spans windows, wrap keeps phase ---- */
    {
        static struct RISegment seg = { 0ULL, 428571428ULL };
        struct RITempoMap map;
        struct RISongTrack tr;
        struct RITrackCarry carry;
        struct RILoop lp;
        struct RIEvent ev[64];
        uint32_t n, k;
        map.segs = &seg; map.n = 1u; map.ppq = 96u; map.sr = 48000u;
        ri_track_init(&tr);
        ri_track_capture(&tr, 5u, 0u, 6u);

        /* Loop OFF: bars 3,4,5,6. Establishment at 3 (4 events), nothing at
         * 4, the rise at 5 (+1), and the fall back to 0 at 6 (+1) — change is
         * measured against the PREVIOUS bar, so returning to zeros is a change. */
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 3u, 4u, 0, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 6u, "offline range %u", n);
        RI_ASSERT(ev[0].sample == ri_map_tick(&map, ri_seq_tick_of_bar(96u, 3u)),
            "offline first sample");
        RI_ASSERT(ev[4].sample == ri_map_tick(&map, ri_seq_tick_of_bar(96u, 5u)),
            "offline change sample");

        /* The NEXT window does not re-establish (carry crosses blocks). */
        n = ri_track_emit_range(&tr, 3u, 2u, 0, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 0u, "second window re-established %u", n);

        /* Loop ON [4,6): 4 bars from 3 -> 3(est 4), 4(0), 5(+1), wrap->4(+1) = 6. */
        lp.on = 1u; lp.start_bar = 4u; lp.len_bars = 2u;
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 3u, 4u, &lp, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 6u, "wrap4 count %u", n);
        RI_ASSERT(ev[n - 1u].sample == ri_map_tick(&map, ri_seq_tick_of_bar(96u, 4u)),
            "wrap lands on the loop start here");

        /* PHASE PIN — 8 bars: 3,4,5,4,5,4,5,4 -> 4+0+1+1+1+1+1+1 = 10.
         * A `bar = loop_start` wrap would collapse 4,5,4,5,4,5 onto 4 and
         * emit 6: this count is what makes the wrap formula testable. */
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 3u, 8u, &lp, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 10u, "phase count %u (6 = wrap lost phase)", n);
        /* The walk alternates 4,5,4,5... so a 5-bar-bar sample appears twice
         * in the tail (proves the phase, not just the count). */
        RI_ASSERT(ev[n - 1u].sample == ri_map_tick(&map, ri_seq_tick_of_bar(96u, 4u)),
            "phase tail bar 4");
        RI_ASSERT(ev[n - 2u].sample == ri_map_tick(&map, ri_seq_tick_of_bar(96u, 5u)),
            "phase tail bar 5");

        /* Starting past the loop end folds into the loop (both bars legal). */
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 900u, 2u, &lp, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 5u, "fold count %u", n);
        RI_ASSERT(ev[0].sample == ri_map_tick(&map, ri_seq_tick_of_bar(96u, 4u)),
            "fold lands in loop");

        /* Zero-length loop with on set behaves as OFF (no modulo by 0), so
         * the walk is the same 3,4,5,6 sequence: 6 events. */
        lp.on = 1u; lp.start_bar = 4u; lp.len_bars = 0u;
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 3u, 4u, &lp, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 6u, "zero-len loop %u", n);

        /* Loop ON never reaches bar 999 (E1: end-of-song cannot fire). */
        lp.on = 1u; lp.start_bar = 4u; lp.len_bars = 2u;
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 990u, 20u, &lp, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n > 0u, "looped walk emitted");
        for (k = 0u; k < n; k++)
            RI_ASSERT(ev[k].sample < ri_map_tick(&map, ri_seq_tick_of_bar(96u, 999u)),
                "loop reached the end boundary");

        /* A RAW loop past the song end is normalized first (ri_loop_clamp):
         * [995, 1005) -> [995, 999), so the walk wraps instead of stopping at
         * 999. Track: 995/inst1 = 3, so every pass over 995,996 adds 2.
         * 20 bars from 990: est 4 + four passes x 2 = 12. Unclamped: 6. */
        ri_track_capture(&tr, 995u, 1u, 3u);
        lp.on = 1u; lp.start_bar = 995u; lp.len_bars = 10u;
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 990u, 20u, &lp, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 12u, "raw loop count %u (6 = loop not clamped)", n);
        for (k = 0u; k < n; k++)
            RI_ASSERT(ev[k].sample < ri_map_tick(&map, ri_seq_tick_of_bar(96u, 999u)),
                "raw loop reached the end boundary");
        ri_track_capture(&tr, 995u, 1u, 0u);

        /* No loop: the walk truncates before the end boundary. */
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 997u, 5u, 0, &map, 96u, &carry, ev, 64u);
        RI_ASSERT(n == 4u, "truncate count %u", n);

        /* Edges. */
        RI_ASSERT(ri_track_emit_range(0, 0u, 4u, 0, &map, 96u, &carry, ev, 64u) == 0u, "range null t");
        RI_ASSERT(ri_track_emit_range(&tr, 0u, 0u, 0, &map, 96u, &carry, ev, 64u) == 0u, "range count 0");
        RI_ASSERT(ri_track_emit_range(&tr, 0u, 4u, 0, &map, 96u, &carry, ev, 0u) == 0u, "range cap 0");
        RI_ASSERT(ri_track_emit_range(&tr, 0u, 4u, 0, 0, 96u, &carry, ev, 64u) == 0u, "range null map");
        RI_ASSERT(ri_track_emit_range(&tr, 0u, 4u, 0, &map, 96u, 0, ev, 64u) == 0u, "range null carry");
        RI_ASSERT(ri_track_emit_range(&tr, 0u, 4u, 0, &map, 96u, &carry, 0, 64u) == 0u, "range null out");
        memset(&carry, 0, sizeof carry);
        RI_ASSERT(ri_track_emit_range(&tr, 999u, 4u, 0, &map, 96u, &carry, ev, 64u) == 0u,
            "range past end no loop");
    }
    /* ---- replay + sweep properties (spec §6) ---- */
    {
        static struct RISegment seg = { 0ULL, 428571428ULL };
        static struct RIEvent big[4096];
        struct RITempoMap map;
        struct RISongTrack tr;
        struct RITrackCarry carry;
        uint8_t cur[4];
        uint32_t n, e, i;
        uint64_t b;
        map.segs = &seg; map.n = 1u; map.ppq = 96u; map.sr = 48000u;
        /* Change-dense, deterministic: instance i changes every i+1 bars and
         * every change is a real one (+7 mod 32 never repeats a neighbour). */
        ri_track_init(&tr);
        for (b = 0u; b < RI_SONGTRACK_BARS; b++)
            for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
                (void)ri_track_capture(&tr, b, i, (uint8_t)(((b / (i + 1u)) * 7u + i) % 32u));
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 0u, RI_SONGTRACK_BARS, 0, &map, 96u, &carry, big, 4096u);
        /* Replay: applying the events in order to a 4-slot cursor reproduces
         * selected() at EVERY bar — the dense grid is the only truth. */
        memset(cur, 0xFF, sizeof cur); /* unset lanes can never match */
        e = 0u;
        for (b = 0u; b < RI_SONGTRACK_BARS; b++) {
            uint64_t smp = ri_map_tick(&map, ri_seq_tick_of_bar(96u, b));
            while (e < n && big[e].sample == smp) {
                RI_ASSERT(big[e].type == RI_EV_PATTERN_CHANGE && big[e].device < 4u, "replay ev %u", e);
                if (big[e].device < 4u)
                    cur[big[e].device] = (uint8_t)big[e].value;
                e++;
            }
            for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
                RI_ASSERT(cur[i] == ri_track_selected(&tr, b, i),
                    "replay bar %llu inst %u", (unsigned long long)b, i);
        }
        RI_ASSERT(e == n, "replay consumed %u of %u", e, n);
        /* Sweep: instance 0 alternates every bar, others steady -> exactly
         * the range-start establishment plus one change per bar after it. */
        ri_track_init(&tr);
        for (b = 0u; b < RI_SONGTRACK_BARS; b++)
            (void)ri_track_capture(&tr, b, 0u, (uint8_t)(b % 2u));
        memset(&carry, 0, sizeof carry);
        n = ri_track_emit_range(&tr, 0u, RI_SONGTRACK_BARS, 0, &map, 96u, &carry, big, 4096u);
        RI_ASSERT(n == 4u + 998u, "sweep %u", n);
    }
```
Map note: expected SAMPLES always come from `ri_map_tick(&map, ri_seq_tick_of_bar(96u, bar))` — never a hard-coded `bar * 384`. Ticks are authoritative for position, the map is authoritative for samples.

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | grep -E "implicit|error" | head -3`
Expected: FAIL — `engine/seq/songtrack_emit.h: No such file or directory` (scaffolding RED).

- [ ] **Step 3a: Behavioral RED** — create `songtrack_emit.h` below and add STUB bodies (`ri_track_emit_measure`/`ri_track_emit_range` return 0, `ri_song_ended` returns 0). Run: expected first failure `establish added 0`. Record the line.

- [ ] **Step 3b: Write minimal implementation**

New `engine/seq/songtrack_emit.h` — the emitter's header. `songtrack.h` is NOT edited, so the Task 1 layer guard stays exactly as written. One direction is preserved (emit → model → transport):
```c
/* songtrack_emit.h — song track change emission (spec 2026-09-25 §2).
 * Separate from songtrack.h so the model (and the RBNG codec header,
 * which includes the model) never pulls scheduler/clock headers.
 * The t59 guard greps this file: never name the codec header here. */
#ifndef RI_SONGTRACK_EMIT_H
#define RI_SONGTRACK_EMIT_H
#include "engine/seq/songtrack.h"
#include "engine/seq/sched.h"  /* RIEvent, RI_EV_* */
#include "engine/seq/clock.h"  /* RITempoMap */

/* Cross-window change cache: the caller owns it, one per play session.
 * known bit i = prev[i] holds what was last EMITTED for instance i. */
struct RITrackCarry { uint8_t known; uint8_t prev[RI_SONGTRACK_INSTANCES]; };

uint32_t ri_track_emit_measure(const struct RISongTrack *t, uint64_t bar,
    struct RITrackCarry *carry, const struct RITempoMap *map, uint32_t ppq,
    struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq);
uint32_t ri_track_emit_range(const struct RISongTrack *t, uint64_t bar_first,
    uint64_t bar_count, const struct RILoop *loop, const struct RITempoMap *map,
    uint32_t ppq, struct RITrackCarry *carry, struct RIEvent *out, uint32_t cap);
int ri_song_ended(uint64_t bar_now);
#endif
```
`songtrack.c` additions (add `#include "engine/seq/songtrack_emit.h"` at the top; it pulls the model header):
```c
uint32_t ri_track_emit_measure(const struct RISongTrack *t, uint64_t bar,
    struct RITrackCarry *carry, const struct RITempoMap *map, uint32_t ppq,
    struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq) {
    uint32_t i, added = 0u;
    uint64_t sample;
    if (!t || !carry || !map || !out || !n || !seq)
        return 0u;
    if (bar >= (uint64_t)RI_SONGTRACK_BARS)
        return 0u; /* end boundary is never a valid start; carry left cold */
    sample = ri_map_tick(map, ri_seq_tick_of_bar(ppq, bar));
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++) {
        uint8_t cur = ri_track_selected(t, bar, i);
        uint8_t bit = (uint8_t)(1u << i);
        if ((carry->known & bit) && cur == carry->prev[i])
            continue; /* recorded truth: no change, nothing to say */
        if (*n >= cap)
            continue; /* cap: later instances drop first; stays PENDING */
        out[*n].sample = sample;
        out[*n].type = RI_EV_PATTERN_CHANGE;
        out[*n].device = (uint16_t)i;
        out[*n].voice = 0u;
        out[*n].value = (uint16_t)cur; /* slot NUMBER, never pattern data */
        out[*n].flags = 0u;
        out[*n].seq = (*seq)++;
        (*n)++;
        added++;
        /* The carry mirrors what was EMITTED: a change the cap dropped keeps
         * its old prev/known bit and is re-sent at the next measure. */
        carry->prev[i] = cur;
        carry->known = (uint8_t)(carry->known | bit);
    }
    return added;
}

uint32_t ri_track_emit_range(const struct RISongTrack *t, uint64_t bar_first,
    uint64_t bar_count, const struct RILoop *loop, const struct RITempoMap *map,
    uint32_t ppq, struct RITrackCarry *carry, struct RIEvent *out, uint32_t cap) {
    uint32_t n = 0u, seq = 0u;
    uint64_t ls = 0u, llen = 0u, k;
    struct RILoop l;
    int looping = 0;
    if (!t || !carry || !map || !out || cap == 0u || bar_count == 0u)
        return 0u;
    /* Normalize FIRST (transport law, reused): a raw loop past the song end
     * is clamped exactly as transport clamps it; zero length -> OFF. */
    if (loop) {
        l = *loop;
        ri_loop_clamp(&l, (uint64_t)RI_SONGTRACK_BARS);
        looping = (l.on && l.len_bars > 0u) ? 1 : 0;
    }
    if (looping) {
        ls = (uint64_t)l.start_bar;
        llen = (uint64_t)l.len_bars;
        if (bar_first >= ls + llen)
            bar_first = ls + ((bar_first - ls) % llen); /* fold in, keep phase */
    } else if (bar_first >= (uint64_t)RI_SONGTRACK_BARS) {
        return 0u; /* past the end with no loop: nothing to emit */
    }
    /* No call needs more bars than the song holds. This bound also makes
     * bar_first + k overflow-proof (both < 2000 after the fold). */
    if (bar_count > (uint64_t)RI_SONGTRACK_BARS)
        bar_count = (uint64_t)RI_SONGTRACK_BARS;
    for (k = 0u; k < bar_count; k++) {
        uint64_t bar = bar_first + k;
        if (looping && bar >= ls + llen)
            bar = ls + ((bar - ls) % llen); /* wrap PRESERVES PHASE */
        if (bar >= (uint64_t)RI_SONGTRACK_BARS)
            break; /* truncated before the end boundary */
        ri_track_emit_measure(t, bar, carry, map, ppq, out, &n, cap, &seq);
    }
    return n;
}

int ri_song_ended(uint64_t bar_now) {
    return bar_now >= (uint64_t)RI_SONGTRACK_BARS ? 1 : 0;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | tail -2`
Expected: `PASS songtrack`

- [ ] **Step 5: Commit**

```bash
git add engine/seq/songtrack_emit.h engine/seq/songtrack.c tests/unit/t59_songtrack.c
git commit -m "feat: songtrack change emission at measure lines [§12.9b]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 3: Track edits (t59 part 3)

**Files:**
- Modify: `engine/seq/songtrack.h`, `engine/seq/songtrack.c`, `tests/unit/t59_songtrack.c`

**Deferred (not in this task):** a run-length view of the track. Its only consumer is the GUI song editor, which is explicitly OUT of this slice, so it would be speculative code with no caller. It is recorded as a note for the GUI slice, not built here.

**Interfaces (exact signatures):**
```c
struct RITrackClip { uint16_t len; uint8_t slot[RI_SONGTRACK_BARS][RI_SONGTRACK_INSTANCES]; };
/* Writers that take slot values: 0 ok (incl. a range that clamps to
 * nothing), 2 refused (NULL, slot > 31, clip len > 999) — track untouched. */
int  ri_track_init_song(struct RISongTrack *t, const uint8_t slots[RI_SONGTRACK_INSTANCES]);
int  ri_track_init_loop(struct RISongTrack *t, const uint8_t slots[RI_SONGTRACK_INSTANCES],
                        uint64_t start_bar, uint64_t len_bars);
void ri_track_copy(const struct RISongTrack *t, uint64_t start, uint64_t len,
                   struct RITrackClip *clip);
void ri_track_cut(struct RISongTrack *t, uint64_t start, uint64_t len,
                  struct RITrackClip *clip);
int  ri_track_paste(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip);
int  ri_track_paste_replace(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip);
```
`RITrackClip` is caller-owned static (~4 KB) — no alloc anywhere, including the caller. `len` is `uint16_t` because 999 fits; bar indices travel as `uint64_t` (cursor domain). Every op clamps `len` against `RI_SONGTRACK_BARS - start` BEFORE adding anything. Writers taking slot values validate every value they would write BEFORE the first write (all-or-nothing); copy/cut take no slot input and stay `void`.

- [ ] **Step 1: Write the failing test** (append before `RI_RESULT("songtrack");`)

```c
    /* ---- edits ---- */
    {
        struct RISongTrack tr;
        struct RITrackClip clip;
        uint8_t s4[4];
        uint8_t bad4[4];
        uint64_t b;
        uint32_t i;

        /* init-song: every bar, every instance. */
        s4[0] = 3u; s4[1] = 1u; s4[2] = 0u; s4[3] = 2u;
        ri_track_init(&tr);
        ri_track_capture(&tr, 500u, 0u, 9u);
        ri_track_init_song(&tr, s4);
        for (b = 0u; b < RI_SONGTRACK_BARS; b++)
            for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
                RI_ASSERT(ri_track_selected(&tr, b, i) == s4[i],
                    "init song %llu/%u", (unsigned long long)b, i);
        /* Slot law (one law, no masking): {32,40,1,2} is refused whole and
         * the track keeps the previous init-song content. */
        bad4[0] = 32u; bad4[1] = 40u; bad4[2] = 1u; bad4[3] = 2u;
        RI_ASSERT(ri_track_init_song(&tr, bad4) == 2, "init song refuses slot 32/40");
        RI_ASSERT(ri_track_selected(&tr, 0u, 0u) == 3u && ri_track_selected(&tr, 998u, 2u) == 0u,
            "refused init song left the track untouched");
        RI_ASSERT(ri_track_init_loop(&tr, bad4, 0u, 4u) == 2 && ri_track_selected(&tr, 1u, 1u) == 1u,
            "init loop refuses bad slots, all-or-nothing");

        /* init-loop: EVERY bar inside the loop (E1 p. 176), nothing outside. */
        ri_track_init(&tr);
        ri_track_capture(&tr, 12u, 0u, 9u);   /* prior content inside */
        ri_track_capture(&tr, 20u, 0u, 9u);   /* content outside */
        ri_track_init_loop(&tr, s4, 10u, 4u);
        for (b = 10u; b < 14u; b++)
            for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
                RI_ASSERT(ri_track_selected(&tr, b, i) == s4[i],
                    "init loop %llu/%u", (unsigned long long)b, i);
        RI_ASSERT(ri_track_selected(&tr, 14u, 0u) == 0u, "init loop above");
        RI_ASSERT(ri_track_selected(&tr, 20u, 0u) == 9u, "init loop outside kept");
        /* Loop range clamps at the song end; zero len and past-end are no-ops. */
        ri_track_init(&tr);
        ri_track_init_loop(&tr, s4, 997u, 10u);
        RI_ASSERT(ri_track_selected(&tr, 997u, 0u) == 3u &&
            ri_track_selected(&tr, 998u, 3u) == 2u, "init loop clamped");
        ri_track_init(&tr);
        RI_ASSERT(ri_track_init_loop(&tr, s4, 10u, 0u) == 0 &&
            ri_track_init_loop(&tr, s4, 999u, 4u) == 0, "empty ranges are no-ops, not refusals");
        RI_ASSERT(ri_track_is_empty(&tr) == 1, "init loop no-ops");

        /* copy: pure, clamped, and overflow-proof on a huge len. */
        ri_track_init(&tr);
        for (b = 0u; b < RI_SONGTRACK_BARS; b++)
            for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
                (void)ri_track_capture(&tr, b, i, (uint8_t)(b % 8u));
        ri_track_copy(&tr, 5u, 4u, &clip);
        RI_ASSERT(clip.len == 4u, "copy len %u", (unsigned)clip.len);
        RI_ASSERT(clip.slot[0][0] == 5u && clip.slot[3][0] == 0u, "copy rows");
        RI_ASSERT(ri_track_selected(&tr, 5u, 0u) == 5u, "copy is pure");
        ri_track_copy(&tr, 998u, 5u, &clip);
        RI_ASSERT(clip.len == 1u, "copy clamps %u", (unsigned)clip.len);
        /* Overflow pin, non-zero start: 500 + (2^64-1) WRAPS to 499, so a bare
         * `start + len > BARS` check waves the huge len through and the fill
         * loop runs off the grid. The clamp is evaluated before any addition. */
        ri_track_copy(&tr, 500u, ~(uint64_t)0, &clip);
        RI_ASSERT(clip.len == 499u, "copy huge len %u", (unsigned)clip.len);
        ri_track_copy(&tr, 0u, ~(uint64_t)0, &clip);
        RI_ASSERT(clip.len == 999u, "copy huge len at 0 %u", (unsigned)clip.len);

        /* cut: clip holds the range, the tail closes the gap, the freed end
         * fills with slot 0 without reading past bar 998. */
        ri_track_cut(&tr, 2u, 3u, &clip);
        RI_ASSERT(clip.len == 3u && clip.slot[0][0] == 2u && clip.slot[2][0] == 4u, "cut clip");
        RI_ASSERT(ri_track_selected(&tr, 2u, 0u) == 5u, "gap closed");
        RI_ASSERT(ri_track_selected(&tr, 5u, 0u) == 0u, "shift alias");
        RI_ASSERT(ri_track_selected(&tr, 995u, 0u) == 6u, "shift tail from 998");
        RI_ASSERT(ri_track_selected(&tr, 996u, 0u) == 0u &&
            ri_track_selected(&tr, 998u, 0u) == 0u, "freed end zeroed");
        /* Cut aimed past the end clamps to the last bar available. */
        ri_track_cut(&tr, 997u, 5u, &clip);
        RI_ASSERT(clip.len == 2u, "cut end len %u", (unsigned)clip.len);
        RI_ASSERT(ri_track_selected(&tr, 997u, 0u) == 0u &&
            ri_track_selected(&tr, 998u, 0u) == 0u, "cut end zeroed");
        /* No-op cuts still clear the clip (stale clipboard is a bug source). */
        ri_track_cut(&tr, 999u, 3u, &clip);
        RI_ASSERT(clip.len == 0u, "cut past end");
        ri_track_cut(&tr, 0u, 0u, &clip);
        RI_ASSERT(clip.len == 0u, "cut zero len");

        /* paste inserts (tail shifts right, drops past the end). */
        ri_track_init(&tr);
        for (b = 0u; b < 6u; b++)
            (void)ri_track_capture(&tr, b, 0u, (uint8_t)(b + 1u));
        ri_track_copy(&tr, 0u, 2u, &clip);
        RI_ASSERT(clip.len == 2u && clip.slot[0][0] == 1u && clip.slot[1][0] == 2u,
            "paste pre");
        ri_track_paste(&tr, 3u, &clip);
        RI_ASSERT(ri_track_selected(&tr, 2u, 0u) == 3u, "insert kept bar 2");
        RI_ASSERT(ri_track_selected(&tr, 3u, 0u) == 1u && ri_track_selected(&tr, 4u, 0u) == 2u,
            "inserted clip");
        RI_ASSERT(ri_track_selected(&tr, 5u, 0u) == 4u && ri_track_selected(&tr, 6u, 0u) == 5u,
            "tail shifted");
        RI_ASSERT(ri_track_selected(&tr, 8u, 0u) == 0u, "tail end clear");
        /* Overflow: the 2-bar clip pasted at bar 998 keeps only bar 998. */
        RI_ASSERT(ri_track_paste(&tr, 998u, &clip) == 0, "overflow paste rc");
        RI_ASSERT(ri_track_selected(&tr, 998u, 0u) == 1u, "paste dropped overflow");
        /* Hand-built clip with an out-of-range slot in its LAST row is refused
         * whole: nothing shifts, nothing is written (validate before write). */
        clip.len = 2u;
        memset(clip.slot, 0, sizeof clip.slot);
        clip.slot[1][3] = 40u;
        RI_ASSERT(ri_track_paste(&tr, 0u, &clip) == 2, "paste refuses clip slot 40");
        RI_ASSERT(ri_track_selected(&tr, 0u, 0u) == 1u && ri_track_selected(&tr, 3u, 0u) == 1u,
            "refused paste shifted nothing");
        RI_ASSERT(ri_track_paste_replace(&tr, 0u, &clip) == 2 && ri_track_selected(&tr, 0u, 0u) == 1u,
            "paste-replace refuses too");
        clip.len = 1000u;
        clip.slot[1][3] = 0u;
        RI_ASSERT(ri_track_paste(&tr, 0u, &clip) == 2, "clip len 1000 is malformed");

        /* paste-replace overwrites in place, leaving the tail alone. */
        ri_track_init(&tr);
        for (b = 0u; b < 8u; b++)
            (void)ri_track_capture(&tr, b, 0u, (uint8_t)(b + 1u));
        ri_track_copy(&tr, 0u, 2u, &clip);
        ri_track_paste_replace(&tr, 3u, &clip);
        RI_ASSERT(ri_track_selected(&tr, 3u, 0u) == 1u && ri_track_selected(&tr, 4u, 0u) == 2u,
            "replace wrote");
        RI_ASSERT(ri_track_selected(&tr, 2u, 0u) == 3u && ri_track_selected(&tr, 5u, 0u) == 6u,
            "replace left neighbors");

        /* NULL / degenerate edges. */
        RI_ASSERT(ri_track_init_song(0, s4) == 2 && ri_track_init_song(&tr, 0) == 2, "init song null");
        RI_ASSERT(ri_track_init_loop(0, s4, 0u, 1u) == 2 && ri_track_init_loop(&tr, 0, 0u, 1u) == 2,
            "init loop null");
        ri_track_copy(&tr, 0u, 1u, 0);
        ri_track_copy(0, 0u, 1u, &clip);
        ri_track_cut(&tr, 0u, 1u, 0);
        ri_track_cut(0, 0u, 1u, &clip);
        RI_ASSERT(ri_track_paste(&tr, 0u, 0) == 2 && ri_track_paste(0, 0u, &clip) == 2, "paste null");
        RI_ASSERT(ri_track_paste_replace(&tr, 0u, 0) == 2 && ri_track_paste_replace(0, 0u, &clip) == 2,
            "replace null");
        clip.len = 0u;
        RI_ASSERT(ri_track_paste(&tr, 0u, &clip) == 0 && ri_track_paste_replace(&tr, 0u, &clip) == 0,
            "empty clip is a no-op");
    }
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | grep -E "implicit|error" | head -3`
Expected: FAIL with implicit declaration of `ri_track_init_song` (scaffolding RED).

- [ ] **Step 3a: Behavioral RED** — add the declarations below with STUB bodies (int writers return 0 and write nothing; copy/cut set `clip->len = 0` when `clip` is non-NULL). Run: expected first failure `init song 0/0`. Record the line.

- [ ] **Step 3b: Write minimal implementation** (append to `songtrack.h` + `songtrack.c`)

`songtrack.h` additions:
```c
struct RITrackClip { uint16_t len; uint8_t slot[RI_SONGTRACK_BARS][RI_SONGTRACK_INSTANCES]; };
/* Writers that take slot values: 0 ok (incl. a range that clamps to
 * nothing), 2 refused (NULL, slot > 31, clip len > 999) — track untouched. */
int  ri_track_init_song(struct RISongTrack *t, const uint8_t slots[RI_SONGTRACK_INSTANCES]);
int  ri_track_init_loop(struct RISongTrack *t, const uint8_t slots[RI_SONGTRACK_INSTANCES],
                        uint64_t start_bar, uint64_t len_bars);
void ri_track_copy(const struct RISongTrack *t, uint64_t start, uint64_t len,
                   struct RITrackClip *clip);
void ri_track_cut(struct RISongTrack *t, uint64_t start, uint64_t len,
                  struct RITrackClip *clip);
int  ri_track_paste(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip);
int  ri_track_paste_replace(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip);
```
`songtrack.c` additions — note the single normalization helper and the clamp-before-add rule:
```c
/* Slot law, one enforcement point for every bulk writer: validate ALL
 * values before the first write (capture refuses the same way). */
static int track_row_ok(const uint8_t row[RI_SONGTRACK_INSTANCES]) {
    uint32_t i;
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
        if (row[i] > RI_SONGTRACK_MAX_SLOT)
            return 0;
    return 1;
}

/* Clip rows [0, len) that a paste would write: all valid, or refuse. */
static int track_clip_ok(const struct RITrackClip *clip, uint64_t len) {
    uint64_t b;
    for (b = 0u; b < len; b++)
        if (!track_row_ok(clip->slot[b]))
            return 0;
    return 1;
}

/* Clamp [start, start+len) into the song. Never evaluates start+len on an
 * unbounded len (unsigned addition is modulo and would wrap the check). */
static uint64_t track_clamp_len(uint64_t start, uint64_t len) {
    if (start >= (uint64_t)RI_SONGTRACK_BARS)
        return 0u;
    if (len > (uint64_t)RI_SONGTRACK_BARS - start)
        return (uint64_t)RI_SONGTRACK_BARS - start;
    return len;
}

static void track_clip_fill(const struct RISongTrack *t, uint64_t start,
    uint64_t len, struct RITrackClip *clip) {
    uint64_t k;
    uint32_t i;
    clip->len = (uint16_t)len;
    for (k = 0u; k < len; k++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            clip->slot[k][i] = t->slot[start + k][i];
}

int ri_track_init_song(struct RISongTrack *t, const uint8_t slots[RI_SONGTRACK_INSTANCES]) {
    uint32_t b, i;
    if (!t || !slots || !track_row_ok(slots))
        return 2;
    /* Slot half of the E1 p. 75 promise: pattern-mode selections travel
     * into the song (the automation slice owns the knob half). */
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[b][i] = slots[i];
    return 0;
}

int ri_track_init_loop(struct RISongTrack *t, const uint8_t slots[RI_SONGTRACK_INSTANCES],
                       uint64_t start_bar, uint64_t len_bars) {
    uint64_t b, end, len;
    uint32_t i;
    if (!t || !slots || !track_row_ok(slots))
        return 2;
    len = track_clamp_len(start_bar, len_bars);
    if (len == 0u)
        return 0; /* range repair: an empty range is a no-op, not a refusal */
    end = start_bar + len; /* bounded: both <= 999 here */
    /* E1 p. 176: ALL measures inside the loop take the pattern-mode
     * selections; the writes replace whatever was there. */
    for (b = start_bar; b < end; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[b][i] = slots[i];
    return 0;
}

void ri_track_copy(const struct RISongTrack *t, uint64_t start, uint64_t len,
                   struct RITrackClip *clip) {
    uint64_t n;
    if (!t || !clip)
        return;
    clip->len = 0u; /* never leave a stale clipboard behind a refused op */
    n = track_clamp_len(start, len);
    if (n == 0u)
        return;
    track_clip_fill(t, start, n, clip);
}

void ri_track_cut(struct RISongTrack *t, uint64_t start, uint64_t len,
                  struct RITrackClip *clip) {
    uint64_t b, n;
    uint32_t i;
    if (!t || !clip)
        return;
    clip->len = 0u;
    n = track_clamp_len(start, len);
    if (n == 0u)
        return;
    track_clip_fill(t, start, n, clip);
    for (b = start; b + n < (uint64_t)RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[b][i] = t->slot[b + n][i]; /* close the gap */
    for (b = (uint64_t)RI_SONGTRACK_BARS - n; b < (uint64_t)RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[b][i] = 0u; /* freed end: slot 0, never silence */
}

int ri_track_paste(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip) {
    uint64_t len, b;
    uint32_t i;
    int64_t s;
    if (!t || !clip || clip->len > RI_SONGTRACK_BARS)
        return 2; /* a clip longer than the song is malformed caller data */
    len = track_clamp_len(at, clip->len); /* drops overflow, never grows */
    if (len == 0u)
        return 0;
    if (!track_clip_ok(clip, len))
        return 2; /* validated before the shift: all-or-nothing */
    for (s = (int64_t)((uint64_t)RI_SONGTRACK_BARS - len) - 1; s >= (int64_t)at; s--)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[(uint64_t)s + len][i] = t->slot[(uint64_t)s][i];
    for (b = 0u; b < len; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[at + b][i] = clip->slot[b][i];
    return 0;
}

int ri_track_paste_replace(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip) {
    uint64_t len, b;
    uint32_t i;
    if (!t || !clip || clip->len > RI_SONGTRACK_BARS)
        return 2;
    len = track_clamp_len(at, clip->len);
    if (len == 0u)
        return 0;
    if (!track_clip_ok(clip, len))
        return 2;
    for (b = 0u; b < len; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[at + b][i] = clip->slot[b][i];
    return 0;
}
```
Notes the executor must preserve:
- The paste shift walks DOWN from `999 - len - 1` so a bar is never overwritten before it is read; with `len == 999` and `at == 0` the shift is correctly empty (start index `-1`).
- `(int64_t)at` is safe here: `at < 999` is guaranteed by `track_clamp_len` returning 0 otherwise.
- `b + n < 999` in the cut walk cannot overflow (`n <= 999`, `b <= 999`).

- [ ] **Step 4: Run test to verify it passes**

Run: `bash scripts/ri_build_host.sh sched && bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | tail -2`
Expected: `PASS songtrack`

- [ ] **Step 5: Commit**

```bash
git add engine/seq/songtrack.h engine/seq/songtrack.c tests/unit/t59_songtrack.c
git commit -m "feat: songtrack measure edits + init from pattern mode [§12.9b]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 4: STRK codec + song integration

**Files:**
- Modify: `project/rbng.h`, `project/rbng.c`, `tests/unit/t59_songtrack.c`

**Interfaces:**
- `project/rbng.h` gains `#include "engine/seq/songtrack.h"`, a derived `RI_RBNG_STRK_BYTES`, and a `track` field on `RISong`.
- `rbng_write_song` / `rbng_read_song` signatures are UNCHANGED (the audit's `check_sig` pins them).

- [ ] **Step 1a: Extend the test's includes** (top of `tests/unit/t59_songtrack.c`, beside the existing include)

```c
#include "project/rbng.h"   /* STRK read/write + rbng_test_* helpers */
```

- [ ] **Step 1b: Write the failing test** (append before `RI_RESULT("songtrack");`)

```c
    /* ---- STRK codec: round-trip, three rejects, legacy shape, and the
     * track-without-banks minor fix. ---- */
    {
        const char *FA = "/tmp/ri/run/t59-strk.rbng";
        const char *FM = "/tmp/ri/run/t59-mut.rbng";
        const char *FL = "/tmp/ri/run/t59-legacy.rbng";
        struct RISong s, r;
        char err[256];
        unsigned char buf[16384];
        size_t nn;
        uint32_t k, soff = 0u;
        int found = 0, has_strk;
        FILE *f;

        /* song_valid requires nsteps != 0: every writer test sets it. */
        rbng_song_init(&s);
        s.nsteps = 4u;
        for (k = 0u; k < 4u; k++) {
            s.steps[k].note = (uint8_t)(45u + k);
            s.steps[k].flags = 0u;
        }
        RI_ASSERT(s.nbanks == 0u, "fresh banks");
        RI_ASSERT(ri_track_capture(&s.track, 10u, 0u, 9u) == 0, "codec cap a");
        RI_ASSERT(ri_track_capture(&s.track, 998u, 3u, 31u) == 0, "codec cap b");
        RI_ASSERT(rbng_write_song(FA, &s, err, sizeof err) == 0, "strk write %s", err);

        f = fopen(FA, "rb");
        RI_ASSERT(f != 0, "strk open");
        nn = f ? fread(buf, 1u, sizeof buf, f) : 0u;
        if (f)
            fclose(f);
        RI_ASSERT(nn > 32u, "strk file size %u", (unsigned)nn);
        /* A track without banks still forces the file minor to 1: writing it
         * as minor 0 would make the file unreadable by its own reader. */
        RI_ASSERT(buf[12] == 'V' && buf[13] == 'E' && buf[14] == 'R' && buf[15] == 'S',
            "VERS id");
        RI_ASSERT(buf[20] == 0u && buf[21] == 1u, "major");
        RI_ASSERT(buf[22] == 0u && buf[23] == 1u, "minor forced to 1");
        for (k = 0u; k + 8u <= (uint32_t)nn; k++) {
            if (memcmp(buf + k, "STRK", 4) == 0) {
                unsigned sz = (unsigned)(((unsigned)buf[k + 6] << 8) | (unsigned)buf[k + 7]);
                RI_ASSERT(buf[k + 4] == 0u && buf[k + 5] == 0u && sz == 3996u,
                    "STRK size %u", sz);
                soff = k;
                found = 1;
                break;
            }
        }
        RI_ASSERT(found, "no STRK chunk");
        /* Body is row-major (bar, then instance): offset 8 + bar*4 + inst. */
        RI_ASSERT(buf[soff + 8u + (10u * 4u) + 0u] == 9u, "strk body a");
        RI_ASSERT(buf[soff + 8u + (998u * 4u) + 3u] == 31u, "strk body b");

        /* Round-trip. */
        memset(&r, 0xA5, sizeof r);
        RI_ASSERT(rbng_read_song(FA, &r, err, sizeof err) == 0, "strk read %s", err);
        RI_ASSERT(memcmp(&s.track, &r.track, sizeof s.track) == 0, "track roundtrip");
        RI_ASSERT(r.nsteps == 4u && r.nbanks == 0u, "song fields kept");

        /* Reject (a): body size - 1 (exact-length law). */
        {
            unsigned char one[4];
            one[0] = 0u; one[1] = 0u; one[2] = 0x0Fu; one[3] = 0x9Bu;
            RI_ASSERT(rbng_test_patch_bytes(FA, FM, soff + 4u, one, 4u) == 0, "patch size");
            memset(&r, 0xA5, sizeof r);
            RI_ASSERT(rbng_read_song(FM, &r, err, sizeof err) != 0, "badlen accepted");
            RI_ASSERT(strstr(err, "STRK length mismatch") != 0, "badlen reason: %s", err);
            RI_ASSERT(ri_track_is_empty(&r.track) == 1, "badlen stored a track");
        }
        /* Reject (b): the LAST body byte (bar 998, instance 3) out of range.
         * Last, not first: a parser that stores while validating would have
         * written 3995 bytes (incl. bar 10 = 9) before rejecting. */
        {
            unsigned char v = 32u;
            RI_ASSERT(rbng_test_patch_bytes(FA, FM, soff + 8u + RI_RBNG_STRK_BYTES - 1u, &v, 1u) == 0,
                "patch slot");
            memset(&r, 0xA5, sizeof r);
            RI_ASSERT(rbng_read_song(FM, &r, err, sizeof err) != 0, "slot32 accepted");
            RI_ASSERT(strstr(err, "STRK slot out of range") != 0, "slot32 reason: %s", err);
            RI_ASSERT(ri_track_is_empty(&r.track) == 1, "slot32 partially stored a track");
        }
        /* Reject (c): STRK at file minor 0 (BANK precedent). */
        RI_ASSERT(rbng_test_set_vers(FA, FM, 1u, 0u, 0u) == 0, "setvers");
        memset(&r, 0xA5, sizeof r);
        RI_ASSERT(rbng_read_song(FM, &r, err, sizeof err) != 0, "minor0+STRK accepted");
        RI_ASSERT(strstr(err, "STRK requires 1.1") != 0, "minor0 reason: %s", err);

        /* Legacy shape: an empty track is omitted and the file stays minor 0. */
        rbng_song_init(&s);
        s.nsteps = 4u;
        for (k = 0u; k < 4u; k++) {
            s.steps[k].note = (uint8_t)(45u + k);
            s.steps[k].flags = 0u;
        }
        RI_ASSERT(ri_track_is_empty(&s.track) == 1, "fresh track empty");
        RI_ASSERT(rbng_write_song(FL, &s, err, sizeof err) == 0, "legacy write %s", err);
        f = fopen(FL, "rb");
        RI_ASSERT(f != 0, "legacy open");
        nn = f ? fread(buf, 1u, sizeof buf, f) : 0u;
        if (f)
            fclose(f);
        RI_ASSERT(buf[22] == 0u && buf[23] == 0u, "legacy minor 0");
        has_strk = 0;
        for (k = 0u; k + 4u <= (uint32_t)nn; k++)
            if (memcmp(buf + k, "STRK", 4) == 0)
                has_strk = 1;
        RI_ASSERT(!has_strk, "legacy file carries STRK");
        memset(&r, 0xA5, sizeof r);
        RI_ASSERT(rbng_read_song(FL, &r, err, sizeof err) == 0, "legacy read %s", err);
        RI_ASSERT(ri_track_is_empty(&r.track) == 1, "legacy track not empty");
        RI_ASSERT(ri_track_selected(&r.track, 500u, 0u) == 0u, "legacy default slot 0");
    }
```
Note: the reject pins are stated honestly — `rbng_read_song` runs `rbng_song_init` before parsing, so a reject leaves an EMPTY track; with the bad byte LAST, `ri_track_is_empty` proves the body was validated before any store (not that a previous object was preserved). Each reject also asserts its own `err` text, so a reject for the wrong reason (e.g. a broken chunk walk) cannot pass the case. Not covered by design: "duplicate STRK" and "STRK before VERS" — the writer can produce neither; they stay as parser guards, mirrored from BANK, and are listed as such in the record.

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | grep -E "implicit|error|undefined" | head -3`
Expected: FAIL — no `s.track` member (the field does not exist yet).

- [ ] **Step 3a: Behavioral RED** — apply only the three `rbng.h` edits below (include, `RI_RBNG_STRK_BYTES`, `track` field); leave `rbng.c` untouched. Run: expected first failure `minor forced to 1`. Record the line.

- [ ] **Step 3b: Write minimal implementation**

`project/rbng.h` — three edits:
```c
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"   /* RI_SONGTRACK_*: the song owns a track */
```
```c
#define RI_RBNG_MAX_BANKS 8u /* bounded rack (D-l); Classic uses 4 */
/* Derived, never a forked 999: the STRK body is the whole track grid. */
#define RI_RBNG_STRK_BYTES \
    ((uint32_t)RI_SONGTRACK_BARS * (uint32_t)RI_SONGTRACK_INSTANCES)
```
```c
    uint8_t nbanks;
    struct RIPatternBank bank[RI_RBNG_MAX_BANKS];
    /* v1.1 song track: pattern-selection grid; all-zero when absent. */
    struct RISongTrack track;
```
and one layout comment line in the header block:
```c
 *   'STRK' (v1.1): RI_SONGTRACK_BARS x RI_SONGTRACK_INSTANCES = 3996 slot
 *           bytes (u8 each), row-major (bar, then instance); even, no pad.
 *           Written only when the track is not all-zero; minor 0 never
 *           carries it; a missing STRK means slot 0 everywhere.
```
`project/rbng.c` — writer (four edits):
```c
    uint32_t total, k;
    uint32_t cprg_n, patt_n, auto_n, modr_n;
```
becomes
```c
    uint32_t total, k, tb, ti;
    uint32_t cprg_n, patt_n, auto_n, modr_n;
```
Then the size total (place right after the PATT/BANK block):
```c
    if (!ri_track_is_empty(&s->track))
        total += 8u + RI_RBNG_STRK_BYTES; /* 3996 is even: never padded */
```
Then the minor (the value currently reads `s->nbanks > 0u ? 1u : 0u` and MUST be widened, or a track without banks is written minor 0 and its own reader rejects it):
```c
    /* Legacy-shaped songs (no banks, no track) stay minor 0 and remain
     * byte-identical v1.0 files; banks OR a track make it 1.1. */
    wr16be(f, (s->nbanks > 0u || !ri_track_is_empty(&s->track)) ? 1u : 0u);
```
Then the chunk (place after the PATT/BANK block, before `chunk_head(f, "AUTO", auto_n);`):
```c
    if (!ri_track_is_empty(&s->track)) {
        chunk_head(f, "STRK", RI_RBNG_STRK_BYTES);
        for (tb = 0u; tb < (uint32_t)RI_SONGTRACK_BARS; tb++)
            for (ti = 0u; ti < RI_SONGTRACK_INSTANCES; ti++)
                fputc((int)s->track.slot[tb][ti], f);
    }
```
`project/rbng.c` — reader (two edits). New static function beside `parse_bank`:
```c
/* v1.1 STRK chunk body: the whole track grid, row-major (bar, instance).
 * saw_vers/file_minor come from the chunk loop (STRK needs VERS first,
 * the BANK precedent). Returns 0 ok, 1 reject (err filled). */
static int parse_strk(const unsigned char *cid, uint32_t off,
    const unsigned char *img, uint32_t doff, uint32_t size,
    struct RISong *s, int saw_vers, uint16_t file_minor,
    char *err, uint32_t errcap) {
    uint32_t k;
    if (!saw_vers) {
        ck_err(err, errcap, cid, off, "STRK before VERS");
        return 1;
    }
    if (file_minor == 0u) {
        ck_err(err, errcap, cid, off, "STRK requires 1.1");
        return 1;
    }
    if (size != RI_RBNG_STRK_BYTES) {
        ck_err(err, errcap, cid, off, "STRK length mismatch");
        return 1;
    }
    /* Validate the whole body BEFORE storing anything (all-or-nothing). */
    for (k = 0u; k < RI_RBNG_STRK_BYTES; k++) {
        if (img[doff + k] > RI_SONGTRACK_MAX_SLOT) {
            ck_err(err, errcap, cid, off, "STRK slot out of range");
            return 1;
        }
    }
    for (k = 0u; k < RI_RBNG_STRK_BYTES; k++)
        s->track.slot[k / RI_SONGTRACK_INSTANCES][k % RI_SONGTRACK_INSTANCES] = img[doff + k];
    return 0;
}
```
and the dispatch: add `saw_strk` to the flag declaration, then the branch next to the BANK one:
```c
    int saw_vers = 0, saw_song = 0, saw_patt = 0, saw_auto = 0, saw_modr = 0,
        saw_cprg = 0, saw_strk = 0;
```
```c
        } else if (memcmp(cid, "STRK", 4) == 0) {
            if (saw_strk) {
                ck_err(err, errcap, cid, off, "duplicate STRK");
                return 1;
            }
            saw_strk = 1;
            if (parse_strk(cid, off, img, doff, size, s, saw_vers, file_minor,
                    err, errcap) != 0)
                return 1;
```
`rbng_song_init` needs no edit: its `memset(s, 0, sizeof *s)` already clears the track (the all-zero default the codec law requires).

- [ ] **Step 4: Run test to verify it passes**

Run: `bash scripts/ri_build_host.sh all && bash scripts/ri_build_host.sh test t59_songtrack 2>&1 | tail -2`
Expected: `PASS songtrack` (chain with `&&`: a failed `all` must never feed a stale link — the m64 lesson).

- [ ] **Step 5: Commit**

```bash
git add project/rbng.h project/rbng.c tests/unit/t59_songtrack.c
git commit -m "feat: STRK codec + song track integration [§12.9b]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
```

---

### Task 5: Audit wiring + docs + records

**Files:**
- Modify: `scripts/ri_audit.sh`, `docs/autodoc/seq.doc`, `docs/2026-09-24-improvement-todo.md`, `llm-wiki/raw/articles/2026-09-25-songtrack.md` (new), `llm-wiki/log.md`, `llm-wiki/index.md`

- [ ] **Step 1: Wire t59 into the audit** — in the Phase 6b seq block, directly after the existing t58 line (`scripts/ri_audit.sh:152`)

```bash
bash "$ROOT/scripts/ri_build_host.sh" test t59_songtrack >/dev/null || { echo "FAIL: t59_songtrack"; exit 1; }
# law: no mutable static state in the song track (spec §Ownership) — one enforcement point
if grep -nE "^static [^()]*[;=]" engine/seq/songtrack.c | grep -v ":static const"; then echo "FAIL: mutable static state in songtrack.c"; exit 1; fi
```
And in the "AROS compile of new AROS-side code (compile-only, no link)" loop (`scripts/ri_audit.sh`, the `for tu in project/arexx_aros.c …` line), append ` engine/seq/songtrack.c`. This is the only AROS gate for the new TU. State it that way in the record: compile-only on the AROS toolchain, no link, no run.

Do not edit `scripts/ri_audit.sh` while an audit is running (the m24 lesson: a live run self-invalidates its baseline).

- [ ] **Step 2: Document the new seq surface in the autodoc** (`docs/autodoc/seq.doc`, which the audit existence-gates; `check_sig` covers the DSP/FX/mixer headers but no seq signatures, so state the signatures here as documentation — do not claim a gate that does not exist)

```
Song track (spec 2026-09-25 §1): songtrack.h.
  struct RISongTrack { uint8_t slot[RI_SONGTRACK_BARS][RI_SONGTRACK_INSTANCES]; }
  struct RITrackClip { uint16_t len; uint8_t slot[RI_SONGTRACK_BARS][RI_SONGTRACK_INSTANCES]; }
  void ri_track_init(struct RISongTrack *t)
  uint8_t ri_track_selected(const struct RISongTrack *t, uint64_t bar, uint32_t instance)
  int ri_track_capture(struct RISongTrack *t, uint64_t bar, uint32_t instance, uint8_t slot)
  int ri_track_is_empty(const struct RISongTrack *t)
  int ri_track_init_song(struct RISongTrack *t, const uint8_t slots[4])
  int ri_track_init_loop(struct RISongTrack *t, const uint8_t slots[4], uint64_t start_bar, uint64_t len_bars)
  void ri_track_copy(const struct RISongTrack *t, uint64_t start, uint64_t len, struct RITrackClip *clip)
  void ri_track_cut(struct RISongTrack *t, uint64_t start, uint64_t len, struct RITrackClip *clip)
  int ri_track_paste(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip)
  int ri_track_paste_replace(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip)
  (int writers: 0 ok / no-op, 2 refused — NULL, slot > 31, clip len > 999; track untouched)
Song track emission (spec 2026-09-25 §2): songtrack_emit.h (model + sched/clock).
  struct RITrackCarry { uint8_t known; uint8_t prev[4]; }   (known bit i = prev[i] was emitted)
  uint32_t ri_track_emit_measure(const struct RISongTrack *t, uint64_t bar, struct RITrackCarry *carry, const struct RITempoMap *map, uint32_t ppq, struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq)
  uint32_t ri_track_emit_range(const struct RISongTrack *t, uint64_t bar_first, uint64_t bar_count, const struct RILoop *loop, const struct RITempoMap *map, uint32_t ppq, struct RITrackCarry *carry, struct RIEvent *out, uint32_t cap)
  int ri_song_ended(uint64_t bar_now)
Song geometry reuses transport.h (RI_SONG_BARS, ri_bar_quantize_next, ri_loop_clamp); RBNG STRK carries the grid.
```

- [ ] **Step 3: Run the FULL gate**

Run: `bash scripts/ri_audit.sh > /tmp/ri/audit-129b.log 2>&1; echo "AUDIT_RC=$?"`
Expected: `AUDIT_RC=0`, tail `AUDIT 0/0 PASS`. Any FAIL names the culprit — fix under TDD, never by weakening a pin.

- [ ] **Step 4: Prove the new gates are load-bearing** (eight mutants; each: mutate, `bash scripts/ri_build_host.sh test t59_songtrack`, confirm FAIL, revert, confirm PASS; record the failing line in the wiki article)

- (a) `size != RI_RBNG_STRK_BYTES` -> `size > RI_RBNG_STRK_BYTES` in `parse_strk`: must FAIL the `badlen accepted` case.
- (b) replace the wrap formula with `bar = ls;` in `ri_track_emit_range`: must FAIL `phase count %u (6 = wrap lost phase)`.
- (c) `bar >= (uint64_t)RI_SONGTRACK_BARS` -> `bar > ...` in `ri_track_selected`: must FAIL the bar-999 read case.
- (d) revert `track_clamp_len` to a bare `start + len > RI_SONGTRACK_BARS` check: must FAIL as a HARD FAULT on `copy huge len` — the wrap admits a 2^64-1 fill and the loop runs off the grid, so the run dies instead of printing an assert. That is a failure the audit catches by exit code; record it as such rather than as a FAIL line.
- (e) carry mirrors the track (set `prev[i] = cur; known |= bit` for every instance, emitted or not): must FAIL `cap state 15` (all four marked known after only two were sent).
- (f) delete the `ri_loop_clamp` call in `ri_track_emit_range` (use `*loop` raw): must FAIL `raw loop count 6 (6 = loop not clamped)`.
- (g) merge `parse_strk`'s validate and store loops back into one: must FAIL `slot32 partially stored a track`.
- (h) replace `track_row_ok` with `return 1;`: must FAIL `init song refuses slot 32/40`.

- [ ] **Step 5: Records + tracker, commit, push**

Tick the todo file (song track done; streaming changeover done m67; record-path capture gating done m68; automation lanes stay open). Write the raw article covering:
- what landed;
- the E1 timing split (selection vs sounding);
- the loop-phase wrap rule, and why the old 4-bar test could not see it;
- the minor-forcing fix;
- the self-healing carry;
- refusal instead of masking;
- the deferred run view and the deferred capture-gating test;
- deviations;
- test counts;
- the audit result;
- the eight mutation lines. Update `llm-wiki/log.md` + `llm-wiki/index.md`.
```bash
git add scripts/ri_audit.sh docs/autodoc/seq.doc docs/2026-09-24-improvement-todo.md llm-wiki/raw/articles/2026-09-25-songtrack.md llm-wiki/log.md llm-wiki/index.md
git commit -m "docs: songtrack record + tracker + audit wiring [§12.9b]

Co-Authored-By: OpenCode <noreply@opencode.ai>"
git push origin main
git status --short  # must be empty
```

---

## Acceptance checklist

- [ ] Model + capture green: fail-closed reads/writes, overwrite, top-of-range (998/31), property round-trip over all 999 bars, layer guard, quantizer composition.
- [ ] Emission green: per-instance diff, establishment exactly once per carry, slot-0 changes fire, cap pressure deterministic AND self-healing (re-sent next measure), end boundary refused, phase-preserving wrap (8-bar/2-bar loop = 10 events), fold-in start, zero-length loop as OFF, raw loop clamped (12 events, never 999), truncation, `ri_song_ended`, replay property over 999 bars, sweep = 1002 events, second layer guard.
- [ ] Edits green: init-song (all bars + refusal all-or-nothing), init-loop (every loop bar, outside untouched, clamped, empty-range no-ops, refusal), copy purity + huge-len clamp, cut gap-close + zeroed tail + end clamp, paste insert + overflow drop + bad-clip refusal (nothing shifted) + len-1000 refusal, paste-replace in place + refusal, NULL/zero edges with rc.
- [ ] STRK green: round-trip byte-identical track, minor forced to 1 for track-without-banks, the three rejects each with its own `err` text, last-byte reject stores nothing, legacy shape omits STRK and stays minor 0.
- [ ] Spec amended (Task 0 Step 4); `docs/autodoc/seq.doc` carries both headers' signatures; static-state grep + AROS compile-only line in the audit; eight mutation proofs recorded; `ri_audit.sh` 0/0; tree clean.
