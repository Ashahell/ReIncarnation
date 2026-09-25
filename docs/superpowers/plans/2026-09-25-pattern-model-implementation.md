# Pattern model + RBNG v1.1 — Implementation Plan

**Spec:** [`docs/superpowers/specs/2026-09-25-pattern-model-design.md`](../specs/2026-09-25-pattern-model-design.md) (revised 2026-09-25; **owner re-approval of the §7 revisions is a precondition — Task 0**).
**Tracker:** `docs/2026-09-24-improvement-todo.md` §12.7 (first sub-slice).
**Fidelity rule:** ReBirth RB-338 2.0.1 Owner's Manual (E1, page refs as in the spec) is the target; the TB-303 / TR-808 / TR-909 decide where ReBirth is silent; every unverified choice is an E0 ledger row under `docs/evidence/sequencer/`, never a silent constant.
**House rules (unchanged):** TDD — watch each new test fail first (RED) for the right reason, then GREEN. No allocation, IO, libm, time or global RNG in `engine/`. `-std=c99 -Wall -Wextra -Werror -pedantic -ftrapv`. Full `scripts/ri_audit.sh` 0/0 before every commit. Commit messages end with the gate ref `[§12.7a]` and the Co-Authored-By line. Existing goldens must not move unless a task says so explicitly and records why.

---

## 0. File map

| File | New/Changed | Purpose |
|------|-------------|---------|
| `engine/seq/pattern.h` | new | types, constants, lane tables, API |
| `engine/seq/pattern.c` | new | model setters/validation, key↔note, edit ops, 909 click-cycle helper |
| `engine/seq/pattern_emit.c` | new | `ri_pattern303_to_steps`, `ri_sched_emit_pattern` (303 + drum) |
| `engine/seq/sched.h` / `sched.c` | changed | `RI_STEP_UP/DOWN`, `RI_EVFLAG_OCTAVE` emission, `RI_VOICE_ALL`, carry-in/out, gate constant |
| `project/rbng.h` / `rbng.c` | changed | v1.1 `BANK` chunk, PATT→bank conversion, banks in `RISong` |
| `tools/render.c` | changed | `struct RISong` → `static` (stack budget) |
| `scripts/ri_build_host.sh` | changed | `MOD_sched` += `pattern.c pattern_emit.c` |
| `scripts/ri_audit.sh` | changed | Phase 7b (pattern model) gates |
| `tests/unit/t53_pattern_model.c` | new | Task 2–3 |
| `tests/unit/t54_pattern_edit.c` | new | Task 4 |
| `tests/unit/t55_pattern_emit.c` | new | Task 5–7 |
| `tests/unit/t56_rbng_bank.c` | new | Task 8 |
| `tests/golden/formats/bank-v11.rbng` + `.sha256` | new | v1.1 byte golden |
| `docs/evidence/sequencer/303-base-note.md` etc. | new | ledger rows (Task 1) |
| `docs/superpowers/specs/2026-09-25-pattern-model-design.md` | changed | §2 slide-direction amendment (Task 5), §4 bank header (Task 8) |

---

## Task 0 — Preconditions (no code)

1. Owner re-approves spec §7 (the revision list) in chat. Record the approval line + date in the spec status header.
2. Confirm the tree is clean for the touched files:
   `git status --short engine/seq project/rbng.* tools/render.c scripts/`.
3. Baseline: `bash scripts/ri_audit.sh` → 0/0; save the log to `/tmp/ri/audit-baseline-12.7a.log` for diffing counts later.
4. Re-read before coding: `engine/seq/sched.c` (gate-fraction block at the `frac_tick` computation, the §8 table, cap guards), `project/rbng.c::parse_image` (chunk loop, unknown-chunk preservation), `docs/evidence/sequencer/gate-length.md` (D-h: OFF at `step_start + step_ticks/2`).

**Fidelity finding to carry into Task 6 (verified in code, 2026-09-25):** the walker's step flag `RI_STEP_SLIDE` means **slide INTO this step** (the gate from the previous note is held and this note glides). ReBirth and the TB-303 mean **slide FROM this step to the next** (“the selected step will be tied to the next”, p. 154; the 303's slide switch lengthens the *current* gate and glides into the *next* pitch). The pattern model stores the ReBirth meaning; the converter shifts it by one step. This is not optional: without the shift every ReBirth pattern plays its slides one step late.

---

## Task 1 — Ledger rows for every open or E0 choice (docs only)

Create, each with Status / Rule / Source / How it closes (same layout as `gate-length.md`):

| File | Content |
|------|---------|
| `docs/evidence/sequencer/303-base-note.md` | `RI_303_BASE_NOTE = 36` (C2) — **E0**. Reason: keeps every first-light / v1.0 note in range (first-light uses MIDI 45 = key 9, no octave), and the 303's three-octave span −12…+24 then covers MIDI 24–60 (C1–C4), the commonly cited TB-303 playable range (verify). Closes by exporting a key-0 step from ReBirth 2.0.1 and measuring f0 (E3). |
| `docs/evidence/sequencer/slide-direction.md` | Slide stored as “tie to next” (E1, p. 154); converter shifts to the walker's “slide into” semantics; last-row slide wraps to row 0 when cyclic. Slide into a Pause holds the gate through that step (existing §8 NOTE_CONTINUE row) — **E0 pending ReBirth check**. |
| `docs/evidence/sequencer/shuffle-scope.md` | OPEN: shuffle on/off per pattern vs per section (p. 21, 147 say “for the desired sections”). This slice stores **nothing** in `RIPattern`; section state owns the flag (§12.9). Closes by the A1/A2 switch test in ReBirth. |
| `docs/evidence/909/flam-level.md` | OPEN: flam hit level. E0: flam hits play at **low** level (a flam is a separate hit type entered with one click, p. 30; high requires the second click, which flam mode does not offer). |
| `docs/evidence/sequencer/transpose-encoding.md` | E0 rule (Task 4.6): absolute semitone `s = key + 12·oct`, `s' = s + n`, fold by ±12 into [−12, 24], encode canonically (oct 0 preferred for 0 ≤ s' ≤ 12). Closes by transposing a known pattern in ReBirth and reading the step editor. |
| `docs/evidence/sequencer/random-alter.md` | ReBirth distributions are unknowable → **not a parity target**; scope of each op is E1 (p. 53–54). E0 weights listed exactly as in Task 4.7/4.8. Alter = seeded Fisher–Yates permutation (E1: “randomly shuffling the data”). |
| `docs/evidence/sequencer/random-pattern-rhythm.md` | OPEN: whether Random/Alter **Pattern** apply to rhythm sections (menu “varies with the focus”, p. 52). This slice implements Random/Alter **Drum** (documented) only; whole-pattern drum random is refused (rc 2) until verified. |

Commit: `docs: pattern-model ledger rows (E0/OPEN) [§12.7a]`.

---

## Task 2 — Types, constants, lane tables, validation (`pattern.h`, `pattern.c`)

### 2.1 Header (write in full)

```c
/* pattern.h — per-instance pattern model (spec 2026-09-25 §1). */
#ifndef RI_PATTERN_H
#define RI_PATTERN_H
#include <stdint.h>

#define RI_PATTERN_STEPS           16u   /* storage rows, always 16 (p. 53-54) */
#define RI_PATTERN_BANKS            4u   /* A-D (p. 147) */
#define RI_PATTERN_PER_BANK         8u
#define RI_PATTERN_BANK_PATTERNS   32u
#define RI_PATTERN_KIND_303         0u
#define RI_PATTERN_KIND_DRUM        1u
#define RI_PATTERN_PAYLOAD_VERSION  1u

/* 303 (spec §1.1) */
#define RI_303_KEYS        13u   /* low C .. high C */
#define RI_303_BASE_NOTE   36u   /* E0: docs/evidence/sequencer/303-base-note.md */
#define RI_303_SEMI_MIN   (-12)  /* Down + key 0 */
#define RI_303_SEMI_MAX     24   /* Up + key 12 */

/* Drum (spec §1.2) */
#define RI_DRUM_CLASS_808  0u
#define RI_DRUM_CLASS_909  1u
#define RI_DRUM_LANES     16u
#define RI_DRUM_CLASSIC_LANES 11u
#define RI_DRUM_LANE_MASK_CLASSIC 0x07FFu
#define RI_DRUM_AC        0x01u  /* RIDrumRow.flags: global accent row */

/* Canonical lane order = ReBirth tap-record / panel order (p. 32) */
enum { RI_L808_BD, RI_L808_SD, RI_L808_LT, RI_L808_MT, RI_L808_HT, RI_L808_RS,
       RI_L808_CP, RI_L808_CB, RI_L808_CY, RI_L808_OH, RI_L808_CH };
enum { RI_L909_BD, RI_L909_SD, RI_L909_LT, RI_L909_MT, RI_L909_HT, RI_L909_RS,
       RI_L909_CP, RI_L909_CH, RI_L909_OH, RI_L909_CC, RI_L909_RC };

struct RI303Row  { uint8_t key; uint8_t flags; };           /* RI_STEP_* */
struct RIDrumRow { uint16_t on, high, flam; uint8_t flags; uint8_t pad; };

struct RIPattern {
    uint8_t kind, length, payload_ver, drum_class;
    union { struct RI303Row r303[RI_PATTERN_STEPS];
            struct RIDrumRow drum[RI_PATTERN_STEPS]; } row;
};

struct RIPatternBank {
    uint8_t instance, kind, drum_class, pad;
    struct RIPattern pat[RI_PATTERN_BANK_PATTERNS];
};

/* Lane -> engine voice id (hosting uses these; 808 lane == rb808 slot). */
extern const uint8_t RI_LANE_TO_RB808_SLOT[RI_DRUM_CLASSIC_LANES];
extern const uint8_t RI_LANE_TO_RB909_VOICE[RI_DRUM_CLASSIC_LANES];

void ri_pattern_init(struct RIPattern *p, uint8_t kind, uint8_t drum_class);
void ri_bank_init(struct RIPatternBank *b, uint8_t instance, uint8_t kind,
                  uint8_t drum_class);
int  ri_pattern_set_length(struct RIPattern *p, uint32_t len);  /* clamps 1..16, rc 0 */
int  ri_pattern_valid(const struct RIPattern *p);               /* 0 ok, 2 bad */
/* 303 step setters: rc 2 on wrong kind / step >= 16 / key > 12 */
int  ri_p303_set(struct RIPattern *p, uint32_t step, uint8_t key, uint8_t flags);
/* Drum: set one lane state; state = RI_HIT_OFF/LOW/HIGH/FLAM */
#define RI_HIT_OFF  0u
#define RI_HIT_LOW  1u
#define RI_HIT_HIGH 2u
#define RI_HIT_FLAM 3u
int  ri_pdrum_set(struct RIPattern *p, uint32_t step, uint32_t lane, uint32_t state);
uint32_t ri_pdrum_get(const struct RIPattern *p, uint32_t step, uint32_t lane);
int  ri_pdrum_set_ac(struct RIPattern *p, uint32_t step, int on);
/* ReBirth step-button click (p. 30): 909 off->low->high->off, flam mode
 * off<->flam; 808 off<->on. Returns the new state. */
uint32_t ri_pdrum_click(struct RIPattern *p, uint32_t step, uint32_t lane, int flam_mode);
#endif
```

### 2.2 Semantics to implement (pattern.c)

- `ri_pattern_init`: kind 303 → every row `{key 0, flags RI_STEP_REST}` (= ReBirth Clear state, p. 52); drum → all zero. `length = 16`, `payload_ver = 1`, `drum_class` stored (0 for 303).
- `ri_pattern_valid` returns 2 when: kind > 1; length 0 or > 16; payload_ver ≠ 1; 303 key > 12 or flags outside `REST|ACCENT|SLIDE|UP|DOWN`; drum: any bit outside `RI_DRUM_LANE_MASK_CLASSIC` in `on/high/flam` for Classic classes; `high & ~on` ≠ 0; `flam & ~on` ≠ 0; `high & flam` ≠ 0; class 808 with `high|flam` ≠ 0; `flags & ~RI_DRUM_AC` ≠ 0.
- `ri_pdrum_set` encodes: OFF clears the lane in on/high/flam; LOW sets on only; HIGH sets on+high; FLAM sets on+flam. 808 accepts only OFF/LOW (HIGH/FLAM → rc 2, state unchanged).
- `ri_pdrum_click` (909, p. 30–31): flam_mode 0: OFF→LOW→HIGH→OFF; flam_mode 1: OFF→FLAM, any other→OFF. 808: OFF↔LOW regardless of flam_mode.
- Lane tables: `RI_LANE_TO_RB808_SLOT = {0..10}` (identity — `rb808.h` slot order `BD SD LT MT HT RS CP CB CY OH CH` is the same canonical order; assert it in the test by comparing against `rb808_slot_of` of each default sound). `RI_LANE_TO_RB909_VOICE = { RB909_BD, RB909_SD, RB909_LT, RB909_MT, RB909_HT, RB909_RS, RB909_CP, RB909_CH, RB909_OH, RB909_CR, RB909_RD }` (engine ids differ from panel order — the table is the only bridge).

### 2.3 TDD

1. Write `tests/unit/t53_pattern_model.c` covering: init defaults (303 rows = key 0 + REST; drum zero; length 16); `set_length` clamps 0→1, 17→16, 255→16; `ri_pattern_valid` accepts init states and rejects each invalid encoding listed in 2.2 (one assertion per rule); 303 setter rejects key 13, step 16, wrong kind (and leaves the row unchanged — compare bytes); drum setter per state, 808 HIGH/FLAM refusal; click cycles for 909 (both modes) and 808 exactly as 2.2; lane tables (808 identity + default-sound cross-check; 909 table equals the RB909 ids listed); `sizeof(struct RIPattern) == 132` and `sizeof(struct RIPatternBank) == 4228` (4 + 32 × 132; pins the layout and the static budget); static assert that `RI_STEP_UP/DOWN` do not collide with `SLIDE|ACCENT|REST|FLAM`.
2. RED: `bash scripts/ri_build_host.sh sched` fails (missing pattern.c) → add the files to `MOD_sched`, stub the API returning 2 → run `bash scripts/ri_build_host.sh all && bash scripts/ri_build_host.sh test t53_pattern_model` → assertions fail.
3. GREEN: implement. Re-run t53 + full audit.
4. Commit `feat: pattern model types, validation, 909 click cycle, lane tables [§12.7a]`.

`RI_STEP_UP 0x10u` / `RI_STEP_DOWN 0x20u` are added to `sched.h` in this task (flags namespace belongs to the walker); `RI_RBNG_UP/DOWN` parallel constants go in with Task 8.

---

## Task 3 — 303 key ↔ note (pure helpers in `pattern.c`)

```c
/* Normalized octave of a 303 row: Up xor Down -> +1 / -1, both or none -> 0 (p. 154). */
int ri_p303_octave(uint8_t flags);
/* Absolute semitone relative to RI_303_BASE_NOTE: key + 12*oct, in [-12, 24]. */
int ri_p303_semi(const struct RI303Row *r);
uint8_t ri_p303_note(const struct RI303Row *r);            /* BASE + semi */
/* Fold any semitone into [-12, 24] by octaves; *folded = 1 if it moved. */
int ri_p303_fold(int semi, int *folded);
/* Canonical encoding of a semitone in [-12, 24] (transpose-encoding.md). */
void ri_p303_encode(int semi, uint8_t *key, uint8_t *octflags);
```

Encoding: `semi < 0` → Down, key = semi + 12; `0 ≤ semi ≤ 12` → no octave, key = semi; `semi > 12` → Up, key = semi − 12. `ri_p303_encode` never produces Up+Down.

TDD in t53 (append): all 13 keys × {none, Up, Down, Up+Down} → note table equals `36 + key + 12·oct` with Up+Down = none; fold of −13 → −1 (folded), 25 → 13 (folded), −12/24 unfolded; encode/decode round-trip for every semi in [−12, 24]; high C canonical form = key 12, no octave. Commit with Task 2 if done in one sitting, otherwise `feat: 303 key/octave helpers [§12.7a]`.

---

## Task 4 — Edit ops (`pattern.c`), ReBirth Edit menu p. 51–54

API (all return 0 ok, 2 refused with **no state change** — tests compare the full struct bytes before/after on every refusal):

```c
int ri_pattern_clear(struct RIPattern *p);
int ri_bank_copy(const struct RIPatternBank *src, uint32_t sslot,
                 struct RIPatternBank *dst, uint32_t dslot);        /* same kind+class */
int ri_bank_cut(struct RIPatternBank *b, uint32_t slot, struct RIPattern *clip);
int ri_bank_paste(struct RIPatternBank *b, uint32_t slot, const struct RIPattern *clip);
int ri_pattern_shift(struct RIPattern *p, int dir);                 /* -1 left, +1 right */
int ri_pdrum_shift_lane(struct RIPattern *p, uint32_t lane, int dir);
int ri_p303_transpose(struct RIPattern *p, int semis, uint32_t *nfolded); /* |semis| <= 12 */
#define RI_RND_PATTERN  0u
#define RI_RND_PITCHES  1u
#define RI_RND_ACCENTS  2u  /* "Accents etc.": Note/Pause, Accent, Slide, Octave */
int ri_p303_random(struct RIPattern *p, uint32_t what, uint32_t seed);
int ri_p303_alter(struct RIPattern *p, uint32_t what, uint32_t seed);
int ri_pdrum_random_lane(struct RIPattern *p, uint32_t lane, uint32_t seed);
int ri_pdrum_alter_lane(struct RIPattern *p, uint32_t lane, uint32_t seed);
```

4.1 **Clear** (p. 52): 303 → every row key 0, flags `REST` only; drum → on/high/flam/flags all 0. Length **unchanged**. Both kinds.

4.2 **Cut/Copy/Paste** (p. 51): slot bounds 0–31; kind and drum_class of source and destination bank must match (a 303 pattern cannot land in the 808) → rc 2 otherwise. Cut = copy to clip, then clear the slot (clear, not remove: banks always hold 32 patterns). Copy across instances of the same kind is allowed (303A → 303B).

4.3 **Shift Pattern Left/Right** (p. 53): rotate **all 16 rows** by one, ignoring `length` (E1: “does not take Pattern length into consideration”). Right: row 15 → row 0. Both kinds. For 303 rows the whole row (key + flags) moves, including the slide-to-next flag — so a slide on row 15 moved to row 0 now ties row 0 → row 1 (correct by construction of the stored meaning).

4.4 **Shift Drum Left/Right** (p. 53): same rotation applied to one lane's bits in on/high/flam; other lanes and the AC flags untouched. Lane ≥ 11 on Classic → rc 2. 303 kind → rc 2.

4.5 Rotations are pure bit/row moves — no RNG.

4.6 **Transpose** (p. 54, `transpose-encoding.md`): `|semis| > 12` → rc 2. For every row whose flags lack `REST`: `semi = ri_p303_semi(row) + semis`; fold; encode; keep ACCENT/SLIDE/REST bits, replace UP/DOWN with the encoded octave; count folds in `*nfolded`. Rows with REST keep their key unchanged (they are silent; ReBirth moves “notes”). Drum → rc 2.

4.7 **Random** (p. 53). One deterministic generator used everywhere:

```c
/* Numerical Recipes LCG; top 16 bits used. Never global. */
static uint32_t ri_lcg(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s >> 16; }
static uint32_t ri_pick(uint32_t *s, uint32_t n) { return ri_lcg(s) % n; }  /* n <= 65536 */
```

E0 weights (random-alter.md) — all over **16 rows**, ignoring length:
- `RI_RND_PITCHES`: key = pick(13) for every row; flags untouched.
- `RI_RND_ACCENTS`: per row, REST if pick(16) < 4 (i.e. 75 % notes); ACCENT if pick(16) < 4; SLIDE if pick(16) < 3; octave: r = pick(16): r < 2 → Down, r < 4 → Up, else none. Key untouched.
- `RI_RND_PATTERN`: pitches pass, then accents pass (same stream, that order).
- `ri_pdrum_random_lane`: per row r = pick(16); 909: r < 8 off, < 12 low, < 15 high, else flam; 808: r < 11 off else on. AC flags untouched. 303 kind → rc 2.
- Whole-pattern random for drum kinds → rc 2 (random-pattern-rhythm.md OPEN).

4.8 **Alter** (p. 54 — “randomly shuffling the data in an existing Pattern”): seeded Fisher–Yates over the 16 rows (`for i = 15..1: j = pick(i+1); swap`), applied to:
- `RI_RND_PATTERN`: whole rows (key + flags).
- `RI_RND_PITCHES`: the key column only.
- `RI_RND_ACCENTS`: the flags column only (REST/ACCENT/SLIDE/UP/DOWN as one tuple per row — E0: combinations are preserved, recorded in random-alter.md).
- `ri_pdrum_alter_lane`: the lane's per-row state (on/high/flam bits) permuted as a unit.
Property by construction: the multiset of rows/columns is preserved and an empty pattern stays empty (E1 requirement). Alter is permitted on drum kinds only via the lane variant (whole-pattern → rc 2, same OPEN row).

### TDD — `tests/unit/t54_pattern_edit.c`

- Clear: from a random-filled 303 pattern → all rows `{0, REST}`, length preserved (set 11 first); drum → zero, length preserved.
- Copy/Cut/Paste: round-trip equality; cross-kind/class refusal with byte-identical state; slot 32 refusal; cut leaves a cleared slot.
- Shift: length 5 pattern with data in row 10 → after Shift Right the row-10 data is in row 11 (proves length is ignored); 16 × Shift Right = identity; Left∘Right = identity; slide on row 15 lands on row 0.
- Shift Drum: only the chosen lane moves; other lanes and AC bits bit-identical; lane 11 refused on 909.
- Transpose: +12 on key 12 Up (semi 24) → folds to 24 (nfolded = 1); +1 on key 11 none → key 12 none; −1 on key 0 none → key 11 Down; REST rows unchanged; |13| refused; drum refused; result never has Up+Down.
- Random: same seed → identical struct bytes (run twice on fresh copies); different seed → differs; `RI_RND_PITCHES` leaves every flags byte identical; `RI_RND_ACCENTS` leaves every key identical; drum lane random touches only that lane and yields only valid states (run `ri_pattern_valid`); 808 lane random yields only off/on; whole-pattern drum random refused.
- Alter: empty 303 pattern (init) stays byte-identical; alter preserves the sorted multiset of rows (sort a copy of rows before/after and compare); pitches-only alter preserves the flags column exactly; drum lane alter preserves the lane's hit count per state.
- Determinism over platforms: hard-code the expected key sequence for seed 1 (`RI_RND_PITCHES`, 16 keys) as a literal array — computed once by the implementation, reviewed by hand against the LCG formula in the commit, then frozen (this pins the generator).

Commit `feat: pattern edit ops per ReBirth Edit menu (clear/cut/copy/paste/shift/transpose/random/alter) [§12.7a]`.

---

## Task 5 — Walker changes (`sched.h` / `sched.c`), goldens must not move

5.1 **Octave flag emission.** In `ri_sched_emit_timed`, when a step's flags contain exactly one of `RI_STEP_UP/DOWN` (after normalization), OR `RI_EVFLAG_OCTAVE` into that step's NOTE_ON / NOTE_CONTINUE flags. The note value in `RIStep` is already absolute (the converter applies the octave; the walker never adds 12). No existing step has bits 0x10/0x20 set → existing goldens unchanged (assert by running the full audit).

5.2 **Total-accent voice id.** `#define RI_VOICE_ALL 0xFFFFu` in `sched.h` (voice field of an `RI_EV_ACCENT` that applies to all voices of a drum section).

5.3 **Gate constant.** Replace the literal in the `frac_tick` computation with `RI_SCHED_GATE_NUM / RI_SCHED_GATE_DEN` (1/2, from gate-length.md, D-h) defined in `sched.h` with the ledger reference. Behaviour identical (integer math `step_ticks * NUM / DEN` must equal `step_ticks / 2u` for every ppq in the tests — assert for ppq 24..960 in t55).

5.4 **Cyclic carry** (slide-to-next across the loop seam, spec §2):

```c
struct RISchedCarry {
    uint8_t valid;     /* 1 = carry_in describes a held gate */
    uint8_t held_note; /* MIDI note whose gate is still high */
    uint8_t pad[2];
};
/* Extended entry: identical to ri_sched_emit_timed when both carries are NULL. */
uint32_t ri_sched_emit_timed_carry(const struct RITempoMap *map, uint64_t start_tick,
    uint32_t ppq, const struct RIStep *steps, uint32_t nsteps, uint16_t device,
    const struct RISchedOpts *opts, const struct RISchedCarry *carry_in,
    struct RISchedCarry *carry_out, struct RIEvent *out, uint32_t cap);
```

- `carry_in->valid`: start with `gate = 1`, `held = carry_in->held_note` (the previous iteration ended with its gate held by a slide into this iteration's step 0).
- `carry_out != NULL` and the last step leaves the gate high **because step 0 of the next iteration slides in** (the converter tells the walker via a new step flag `RI_STEP_TIE_OUT 0x40u` on the last step): suppress the final pattern-end NOTE_OFF and the last step's fractional OFF, set `carry_out = {1, held}`. Otherwise `carry_out->valid = 0` and behaviour is unchanged.
- `ri_sched_emit_timed` becomes a wrapper passing NULL carries (byte-identical output — t1_sched, t21_* and t38 unmodified).

TDD (in t55): every existing sched test still passes unmodified (audit); new cases: octave flag on Up/Down/none/both; gate-constant equivalence sweep; carry: iteration A ends with TIE_OUT → no final NOTE_OFF, carry valid; iteration B with carry_in and a note on step 0 → NOTE_ON carries SLIDE, no NOTE_OFF before it; NULL carries reproduce `ri_sched_emit_timed` event arrays byte-for-byte on the t1_sched fixtures.

Commit `feat: walker octave flag, total-accent voice id, gate constant, cyclic carry [§12.7a]`.

---

## Task 6 — 303 converter + `ri_sched_emit_pattern` (303 path) (`pattern_emit.c`)

```c
/* ReBirth rows -> walker steps (slide shifted from "tie to next" to "slide into"). */
uint32_t ri_pattern303_to_steps(const struct RIPattern *p, int cyclic,
                                struct RIStep out[RI_PATTERN_STEPS]);
uint32_t ri_sched_emit_pattern(const struct RIPattern *p, uint16_t device,
    const struct RITempoMap *map, uint64_t start_tick, uint32_t ppq,
    const struct RISchedOpts *opts, const struct RISchedCarry *carry_in,
    struct RISchedCarry *carry_out, struct RIEvent *out, uint32_t cap);
```

Conversion for `i` in `[0, length)`:
- `out[i].note = ri_p303_note(&row[i])` (REST rows: note = `RI_303_BASE_NOTE`, ignored by the walker).
- Flags: `REST` and `ACCENT` copied from row i (accent on a REST row is dropped — the walker must not emit an ACCENT for a silent step; spec §2); `UP/DOWN` normalized copy.
- **Slide shift:** walker `SLIDE` on step i ⇔ `row[i−1].flags & SLIDE` and row i−1 is a note (a slide flag on a Pause row does nothing — nothing is sounding to tie). For i = 0: when `cyclic` and `row[length−1]` is a note with SLIDE, step 0 does **not** get SLIDE (the seam is handled by carry), and step `length−1` gets `RI_STEP_TIE_OUT`.
- Slide into a Pause (row i−1 note + SLIDE, row i REST): walker step i = `REST | SLIDE` → NOTE_CONTINUE (gate held through the rest; E0 row slide-direction.md).
- Rows `[length, 16)` are never emitted (spec §2).

TDD (t55): hand-built ReBirth patterns with expected event lists:
1. Row 0 C + SLIDE, row 1 E → ONE NOTE_ON at step 0, NOTE_ON with SLIDE at step 1, no NOTE_OFF between them, fractional OFF only after step 1 (D-h).
2. Same pattern with the slide flag on row 1 instead → NOTE_OFF (fractional) after step 0, plain NOTE_ON step 1 — proves the direction.
3. Slide on a Pause row → no effect on anything.
4. Accent on a Pause row → no ACCENT event.
5. Up on row 3 key 0 → note 48 + OCTAVE flag; Down key 12 → 36 + OCTAVE; Up+Down → 36 no OCTAVE.
6. Length 5 with data in rows 5–15 → only steps 0–4 emit.
7. Cyclic seam: row 15 SLIDE + row 0 note, two iterations chained through carry → one continuous gate across the seam, glide into row 0's pitch.
8. The v1.0 first-light song converted to a bank (Task 8 helper) and emitted through `ri_sched_emit_pattern` produces the **same event array** as the current first-light path (`tests/golden/303/first-light.events`) — the golden gate that the new path is a faithful superset.

Commit `feat: 303 pattern emit with ReBirth slide direction + cyclic seam [§12.7a]`. Also amend spec §2 with the slide-direction paragraph (reference slide-direction.md) in the same commit.

---

## Task 7 — Drum emit path (`pattern_emit.c`)

Per step i in `[0, length)`, at the shuffled tick (same rule as the 303 walker: odd steps + `shuffle_ticks`; shuffle applies to rhythm sections too, p. 21):
1. If `flags & RI_DRUM_AC`: one `RI_EV_ACCENT`, voice `RI_VOICE_ALL`, value 1 (total accent; level comes from the section's Accent knob at the voice).
2. For each set lane L in `on` (ascending lane order → deterministic `seq`): `RI_EV_NOTE_ON`, voice = value = L, flags = `RI_EVFLAG_ACCENT` iff `high` bit L (909 per-hit high level).
3. For each set lane L in `flam`: `RI_EV_FLAM` at `sample + flam_samples` (global Flam-knob width from `opts->flam_ms`, P-05), voice = L, value = saturated sample delay, flags `RI_EVFLAG_FLAM2` — the same second-hit contract the 303 path already uses. Flam hits carry **no** `RI_EVFLAG_ACCENT` (flam-level.md E0: low).
4. No NOTE_OFF for drums (one-shot voices; decay is voice-side).
5. Empty rows emit nothing. Cap guard per emit site (existing policy: later steps drop first, deterministic). Final insertion sort by `ri_event_less`.

Explicitly **not** emitted: `accent == 2` style overloads. The engine hosting slice (out of scope) will map `NOTE_ON` + `ACCENT` flag → `rb909_trigger(accent=1)` and `RI_EV_FLAM` → `rb909_arm_flam(width)`; record this contract in `pattern.h` so hosting cannot reintroduce the overload.

TDD (t55):
- 808 pattern BD on 0/4/8/12 + AC on 4 → 4 NOTE_ON (voice 0) + 1 ACCENT (voice 0xFFFF) at step 4's sample; ordering ACCENT after NOTE_ON per the §8 type priority (NOTE_ON=3 < ACCENT=5).
- 909 HIGH on CH (lane 7) → NOTE_ON flags ACCENT; LOW → flags 0; FLAM on SD → NOTE_ON (no ACCENT) + FLAM at +1680 samples @48 kHz/35 ms.
- OH + CH same step → both NOTE_ONs present (voice rules apply later, p. 35).
- Shuffle 50 on a drum pattern delays odd steps by exactly the 303 walker's amount (compare with ri_sched_emit_timed on a note pattern of the same length).
- Length 7 → no events from rows 7–15; cap pressure (cap 5) → deterministic truncation identical on two runs.
- Kind/class mismatch (e.g., 808 pattern carrying `high` bits via a hand-corrupted struct) → `ri_sched_emit_pattern` returns 0 events (validate first, fail closed).

Commit `feat: drum pattern emit (per-lane hits, 909 levels, flam, total accent) [§12.7a]`.

---

## Task 8 — RBNG v1.1 `BANK` chunk (`project/rbng.[ch]`)

8.1 **Model in `RISong`:**
```c
#define RI_RBNG_MAX_BANKS 8u        /* bounded rack (D-l); Classic uses 4 */
struct RISong { ...existing fields...;
    uint8_t nbanks;
    struct RIPatternBank bank[RI_RBNG_MAX_BANKS]; };
```
`sizeof(struct RISong)` grows by 8 × 4228 B ≈ 34 KB → make every `struct RISong` instance static or caller-owned static: `tools/render.c:1099` (`struct RISong song;` → `static struct RISong song;`) and any test declaring it on the stack (grep `struct RISong ` in tests/). Gate in audit Phase 7b: no automatic-storage `struct RISong` outside `project/`.

8.2 **Byte layout (big-endian chunk framing as today; row fields little-endian as in the spec):**
```
'BANK' size
  u8 instance, u8 kind, u8 drum_class, u8 count(1..32)
  count x { u8 slot(0..31), u8 kind, u8 length(1..16), u8 payload_ver(=1),
            16 rows: 303 -> {u8 key, u8 flags}      (32 B)
                     drum -> {LE16 on, LE16 high, LE16 flam, u8 flags} (112 B) }
```
Spec §4 is amended to add the bank header `drum_class` byte (per-pattern kind byte kept for self-description; must equal the bank kind).

8.3 **Writer:** `VERS` minor = 1; one `BANK` per bank; only slots that differ from the cleared state are written (sparse), in ascending slot order; zero non-empty slots → still write the header with count 0? → **no**: count ≥ 1 is required, so an all-empty bank writes slot 0 cleared (keeps instance/kind/class on disk). Never write `PATT`.

8.4 **Reader** (inside `parse_image`, same defensive style):
- Reject (rc 2, nothing stored, err `"BANK @<off>: <reason>"`): instance ≥ `RI_RBNG_MAX_BANKS`; kind > 1; drum_class > 1 for drum, ≠ 0 for 303; count 0 or > 32; per-pattern kind ≠ bank kind; slot > 31; duplicate slot; payload_ver ≠ 1; chunk length ≠ exact sum; any pattern failing `ri_pattern_valid` after length clamp (length is clamped 1–16 per the spec, all other invalid encodings reject).
- Missing slots load as `ri_pattern_init` of the bank's kind/class; duplicate instance across two BANK chunks → reject.
- Minor 0 files: BANK present → reject (“BANK requires 1.1”).

8.5 **v1.0 `PATT` compatibility (reader only):** existing parse path byte-for-byte untouched (t1_formats pins). After parsing, `rbng_patt_to_bank(song, &warn)` builds bank instance 0 / kind 303 / slot 0: length = min(nsteps, 16); each v1.0 step: REST → `{0, REST}`; note → `semi = note − RI_303_BASE_NOTE`, fold (warning “PATT: step %u note %u folded by %d octave(s)”), encode; accent copied; **slide shifted back one step** (v1.0 slide = “slide into step i” → ReBirth slide on row i−1; v1.0 slide on step 0 is dropped with a warning — v1.0 files are one-shot, not cyclic); FLAM on a 303 step dropped with a warning (303 has no flam). Steps beyond 16 → warning “PATT: %u steps truncated to 16”. `ri_song_to_steps` stays untouched (v1.0 bridge).

8.6 **Unknown-chunk rules** unchanged (BANK is now known; older readers skip it as unknown optional — ID starts with 'B', uppercase, so v1.0 readers preserve it verbatim).

TDD — `tests/unit/t56_rbng_bank.c` + fuzz:
- Round-trip: 4 banks (303A, 303B, 808, 909) with mixed content incl. 909 HIGH/FLAM, AC, Up/Down rows, lengths 1/7/16 → write → read → `memcmp` equal banks.
- Sparse: a bank with only slot 17 set writes exactly one pattern record; read-back slot 17 equal, others cleared.
- Each reject rule in 8.4 via `rbng_test_inject_unknown`-style mutation helpers (add `rbng_test_patch_bytes(src,dst,off,bytes,n)` to the test-only helper set; never used by engine/).
- v1.0 compat: every existing v1.0 golden (`tests/golden/303/first-light.rbng`, the t1_formats corpus) reads unchanged (existing pins) **and** converts to a bank whose emit equals the v1.0 walker events (Task 6 case 8); out-of-range note (MIDI 70 → semi 34 → folds to 22) produces the warning; v1.0 slide on step 3 appears as SLIDE on row 2.
- Byte golden: `tests/golden/formats/bank-v11.rbng` written from a fixed fixture; sha256 sidecar; Phase 13 audit verifies it.
- Fuzz: `scripts/ri_fuzz.sh` corpus gains 2 BANK seeds; 10k mutations → no crash, no store on reject (existing harness contract).

Commit `feat: RBNG v1.1 BANK chunk + v1.0 PATT conversion (slide shift, octave fold) [§12.7a]`.

---

## Task 9 — Audit Phase 7b (pattern model) in `scripts/ri_audit.sh`

Add after the scheduler phase:
```
echo "== Phase 7b: pattern model (§12.7a) =="
for t in t53_pattern_model t54_pattern_edit t55_pattern_emit t56_rbng_bank; do
  bash "$ROOT/scripts/ri_build_host.sh" test $t >/dev/null || { echo "FAIL: $t"; exit 1; }
done
# no RNG/time/global state in the model
if grep -nE "\brand\(|srand|time\(|clock\(|static uint32_t [a-z_]*seed" engine/seq/pattern*.c; then echo "FAIL: nondeterminism in pattern model"; exit 1; fi
# the 909 accent==2 overload must not come back through the emitter
if grep -n "accent *== *2\|accent = 2" engine/seq/pattern_emit.c; then echo "FAIL: accent/flam overload"; exit 1; fi
# ledger rows exist for every E0/OPEN constant
for f in sequencer/303-base-note sequencer/slide-direction sequencer/shuffle-scope 909/flam-level sequencer/transpose-encoding sequencer/random-alter sequencer/random-pattern-rhythm; do
  test -f "docs/evidence/$f.md" || { echo "FAIL: missing ledger $f"; exit 1; }; done
grep -q "RI_303_BASE_NOTE" docs/evidence/sequencer/303-base-note.md || { echo "FAIL: base-note ledger unlinked"; exit 1; }
# RISong never on the stack outside project/
if grep -rnE "^\s+struct RISong [a-z_]+;" tools/ engine/ audio_io/ tests/ | grep -v static; then echo "FAIL: stack RISong"; exit 1; fi
```
Audit rule from the skill lessons: every grep gate is tested against a **known-bad** input once (temporarily add `accent == 2` to pattern_emit.c, confirm FAIL, revert) before the phase is trusted; record that in the commit message.

Commit together with Task 8 or separately `chore: audit Phase 7b pattern-model gates [§12.7a]`.

---

## Task 10 — Records, tracker, wiki

1. `docs/2026-09-24-improvement-todo.md` §12.7: tick “pattern banks/lengths” and “RBNG chunk plan” as done (m-number of the session), leave shuffle flags (OPEN, §12.9), streaming emission and song track open.
2. Spec status: “implemented in <commits>; open items §6 unchanged”.
3. llm-wiki (karpathy-llm-wiki skill, project conventions): new raw record `llm-wiki/raw/articles/2026-09-2x-pattern-model-rbng-v11.md` — what landed, the slide-direction finding (with the two contrasting event lists from Task 6 cases 1–2 quoted verbatim from the test), ledger rows, test counts, audit result; index entry + log entry.
4. Final `bash scripts/ri_audit.sh` 0/0; `git status` shows only intended files; commit `docs: pattern model record + tracker [§12.7a]`; push when the owner asks.

---

## Acceptance checklist (all must be true)

- [ ] Every step representable in the model is playable on a 303 (key 0–12 + Up/Down; no MIDI clamp anywhere).
- [ ] Slide direction matches ReBirth/TB-303 (Task 6 cases 1–2) and survives the loop seam (case 7).
- [ ] 808 cannot hold per-instrument level or flam; 909 holds off/low/high/flam per instrument; AC is a separate row on both.
- [ ] Shift/Alter act on 16 steps regardless of length; Shift/Random/Alter Drum act on one lane; Clear keeps length and sets Low C + Pause.
- [ ] Transpose ±12 with octave fold; never produces Up+Down.
- [ ] Emit uses flam bit → FLAM event (no accent overload); total accent → `RI_VOICE_ALL`.
- [ ] RBNG v1.1 round-trips; v1.0 files read unchanged and convert with warnings, not silently.
- [ ] All pre-existing goldens and tests unmodified and green; `ri_audit.sh` 0/0.
- [ ] Seven ledger rows present; every E0 constant cites one.

## Out of scope (next slices)

Engine hosting of 808/909 patterns (lane→voice tables are ready), streaming per-block emission for 4 sections with independent lengths (§5.2), section state (selected bank/pattern, on/off, shuffle flag once shuffle-scope.md closes), song track + transport (§12.9), GUI step editors and Pitch Mode (§12.10), ReBirth black-box measurements that close the OPEN rows (§8 of the review).
