/* sched.h — RIEvent contract + total-order comparator (Task 3, gate G3).
 * Spec §8 frozen struct, event types, payload mapping, and same-sample
 * ordering. Comparator only — no builder/walker yet (Task 4).
 * stdint.h only. In-memory scheduling struct, never serialized.
 */
#ifndef RI_SCHED_H
#define RI_SCHED_H
#include <stdint.h>

struct RIEvent {
    uint64_t sample;    /* master-clock sample position (absolute) */
    uint32_t type;      /* RI_EV_* below */
    uint16_t device;    /* 0..3 classic (303A, 303B, 808, 909), else RIDevice index */
    uint16_t voice;     /* per-device voice index */
    uint16_t value;     /* note/param/layer id (type-dependent) */
    uint16_t flags;     /* accent/slide/flam2/octave bits (type-dependent) */
    uint32_t seq;       /* insertion index: makes the sort key total.
                         * DEVIATION from the spec §8 struct (which lists no
                         * insertion field): §8 requires the full sort key
                         * (sample, type-priority, device, voice, insertion
                         * index) so two producers emitting the same
                         * (sample, type, device, voice) resolve
                         * deterministically (D0). The counter lives here,
                         * assigned at emit time; never serialized. */
};

/* Event types [LOCKED] — numeric order IS the §8 same-sample priority:
 * TRANSPORT → PATTERN_CHANGE → NOTE_OFF → NOTE_ON → NOTE_CONTINUE →
 * ACCENT → FLAM → AUTOMATION → PARAM → METER. */
#define RI_EV_TRANSPORT      0
#define RI_EV_PATTERN_CHANGE 1
#define RI_EV_NOTE_OFF       2
#define RI_EV_NOTE_ON        3
#define RI_EV_NOTE_CONTINUE  4   /* rest+slide: gate stays high, pitch slews */
#define RI_EV_ACCENT         5
#define RI_EV_FLAM           6   /* scheduler-emitted second hit, nominal +35 ms */
#define RI_EV_AUTOMATION     7
#define RI_EV_PARAM          8
#define RI_EV_METER          9

/* Total sort key: (sample, type-priority, device, voice, insertion index).
 * Returns nonzero iff a orders strictly before b. */
static inline int ri_event_less(const struct RIEvent *a, const struct RIEvent *b) {
    if (a->sample != b->sample)
        return a->sample < b->sample;
    if (a->type != b->type)
        return a->type < b->type;
    if (a->device != b->device)
        return a->device < b->device;
    if (a->voice != b->voice)
        return a->voice < b->voice;
    return a->seq < b->seq;
}

/* Minimal walker (Task 4, gate G4) + musical-timing upgrade (Task 7,
 * gate G7): sorted-array emit for ONE section only. Insertion order = song
 * order (seq = emit index). Full song arrangement is Task 13 — NOT here.
 *
 * Step flags (input): RI_STEP_SLIDE / RI_STEP_ACCENT / RI_STEP_REST /
 * RI_STEP_FLAM (Task 7: flam second-hit request on a NOTE step; ignored on
 * rest steps, which carry no pitch).
 * Event flag bits (§8 payload mapping; NOTE_ON/OFF/CONTINUE flags =
 * accent | slide | octave, plus Task-7 legato continuity):
 * RI_EVFLAG_SLIDE / RI_EVFLAG_ACCENT / RI_EVFLAG_OCTAVE / RI_EVFLAG_LEGATO.
 * FLAM events use a SEPARATE type-dependent meaning (spec §8: value =
 * delay in samples, flags = second-hit bit): RI_EVFLAG_FLAM2.
 * Bit meanings are type-dependent per the §8 payload table, so LEGATO
 * (NOTE_ON context) and FLAM2 (FLAM context) never collide. [E0, Task 7]
 *
 * Emits per the §8 gate/slide table: new note (no slide) -> NOTE_ON
 * (+ACCENT); new note + slide -> NOTE_ON with slide flag; rest + slide ->
 * NOTE_CONTINUE (value = held pitch); rest (no slide) -> NOTE_OFF; accent
 * flag on any of the above -> ACCENT (value = level 1). A NOTE_OFF for the
 * previous note is emitted at a step boundary only when the new step does
 * not continue the gate (new note without slide and without legato mode,
 * or rest without slide); the final note gets its NOTE_OFF at the pattern
 * end tick.
 *
 * Task-7 timing overlays (applied in the spec §7 order):
 * shuffle -> slide-legato resolution -> flam emission.
 * - shuffle (opts.shuffle_pct 0..100, clamped): 1-based even 16th steps
 *   (0-based odd indices) fire shuffle_pct*(ppq/4)/100 ticks late (rounded
 *   to whole ticks in the tick domain, then ri_map_tick). Slide durations
 *   stretch/shrink implicitly: NOTE_OFF/NOTE_ON simply land on the offset
 *   positions (shuffle-stretch-slides stay [HYPOTHESIS] per §8).
 * - legato (opts.legato != 0): a new note step ties instead of retriggering:
 *   the gate stays high (no NOTE_OFF) and the NOTE_ON carries SLIDE +
 *   LEGATO (env-continuity: envelopes do NOT reset voice-side). Rests still
 *   break/restore the gate per the §8 table.
 * - flam (RI_STEP_FLAM on a note step): after the step's own events, emit
 *   RI_EV_FLAM at note_sample + flam_samples where flam_samples =
 *   round(flam_ms * map.sr / 1000). value saturates at 65535 (spec §8
 *   bound); the sample offset never saturates. Default flam_ms = 35.0
 *   (P-05 nominal); negative clamps to 0.
 */
#define RI_STEP_SLIDE 0x01u
#define RI_STEP_ACCENT 0x02u
#define RI_STEP_REST 0x04u
#define RI_STEP_FLAM 0x08u
#define RI_STEP_UP 0x10u
#define RI_STEP_DOWN 0x20u
/* Walker-internal: last step ties its gate into the next iteration
 * (cyclic seam, §12.7a). Never stored in RIPattern rows (validity
 * rejects it); set by the pattern converter on RIStep scratch only. */
#define RI_STEP_TIE_OUT 0x40u

#define RI_EVFLAG_SLIDE 0x01u
#define RI_EVFLAG_ACCENT 0x02u
#define RI_EVFLAG_OCTAVE 0x04u
#define RI_EVFLAG_LEGATO 0x08u /* env-continuity (NOTE_ON context only) */
#define RI_EVFLAG_FLAM2 0x10u  /* second-hit marker (RI_EV_FLAM context only) */

/* P-05 nominal default, Appendix A. Measured row: docs/evidence/sequencer/flam-default.md. */
#define RI_FLAM_MS_DEFAULT 35.0

struct RISchedOpts {
    uint8_t shuffle_pct; /* 0..100, clamped; 0 = straight */
    uint8_t legato;      /* nonzero = tie new notes (no retrigger) */
    double flam_ms;      /* per-second-hit delay; negative clamps to 0 */
};

#define RI_SCHED_MAX_EVENTS 256u

/* Gate-length rule (D-h, E0 fraction 1/2):
 * docs/evidence/sequencer/gate-length.md. */
#define RI_SCHED_GATE_NUM 1u
#define RI_SCHED_GATE_DEN 2u

/* Total-accent voice id: an RI_EV_ACCENT with voice == RI_VOICE_ALL
 * applies to all voices of a drum section (909 AC row). */
#define RI_VOICE_ALL 0xFFFFu

struct RISchedCarry {
    uint8_t valid;     /* 1 = carry_in describes a held gate */
    uint8_t held_note; /* MIDI note whose gate is still high */
    uint8_t pad[2];
};

struct RIStep {
    uint8_t note;  /* MIDI note (ignored when RI_STEP_REST set) */
    uint8_t flags; /* RI_STEP_* */
};

struct RITempoMap; /* engine/seq/clock.h (include it for the definition) */
uint32_t ri_sched_emit_sorted(const struct RITempoMap *map, uint64_t start_tick,
    uint32_t ppq, const struct RIStep *steps, uint32_t nsteps,
    uint16_t device, struct RIEvent *out, uint32_t cap);
/* Task-7 timed emit: shuffle/legato/flam overlays per the comment above.
 * opts may be NULL (= straight, no legato, default flam). One section only.
 * Under cap pressure each emit site is guarded, so later steps (and flams,
 * emitted per step after the step's own events) drop first — deterministic. */
uint32_t ri_sched_emit_timed(const struct RITempoMap *map, uint64_t start_tick,
    uint32_t ppq, const struct RIStep *steps, uint32_t nsteps,
    uint16_t device, const struct RISchedOpts *opts,
    struct RIEvent *out, uint32_t cap);
/* Extended entry: identical to ri_sched_emit_timed when both carries are
 * NULL. carry_in->valid starts the run with the gate held (cyclic seam);
 * carry_out reports the held gate when the last step carries TIE_OUT. */
uint32_t ri_sched_emit_timed_carry(const struct RITempoMap *map, uint64_t start_tick,
    uint32_t ppq, const struct RIStep *steps, uint32_t nsteps,
    uint16_t device, const struct RISchedOpts *opts, const struct RISchedCarry *carry_in,
    struct RISchedCarry *carry_out, struct RIEvent *out, uint32_t cap);
#endif
