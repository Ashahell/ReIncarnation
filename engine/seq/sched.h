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

/* Minimal walker (Task 4, gate G4): sorted-array emit for ONE section only.
 * Insertion order = song order (seq = emit index). Full shuffle/legato/flam
 * walker is Task 7 — NOT here.
 *
 * Step flags (input): RI_STEP_SLIDE / RI_STEP_ACCENT / RI_STEP_REST.
 * Event flag bits (§8 payload mapping; NOTE_ON/OFF/CONTINUE flags =
 * accent | slide | octave): RI_EVFLAG_SLIDE / RI_EVFLAG_ACCENT /
 * RI_EVFLAG_OCTAVE. [E0 decisions, Task 4]
 *
 * Emits per the §8 gate/slide table: new note (no slide) -> NOTE_ON
 * (+ACCENT); new note + slide -> NOTE_ON with slide flag; rest + slide ->
 * NOTE_CONTINUE (value = held pitch); rest (no slide) -> NOTE_OFF; accent
 * flag on any of the above -> ACCENT (value = level 1). A NOTE_OFF for the
 * previous note is emitted at a step boundary only when the new step does
 * not continue the gate (new note without slide, or rest without slide);
 * the final note gets its NOTE_OFF at the pattern end tick.
 */
#define RI_STEP_SLIDE 0x01u
#define RI_STEP_ACCENT 0x02u
#define RI_STEP_REST 0x04u

#define RI_EVFLAG_SLIDE 0x01u
#define RI_EVFLAG_ACCENT 0x02u
#define RI_EVFLAG_OCTAVE 0x04u

#define RI_SCHED_MAX_EVENTS 256u

struct RIStep {
    uint8_t note;  /* MIDI note (ignored when RI_STEP_REST set) */
    uint8_t flags; /* RI_STEP_* */
};

struct RITempoMap; /* engine/seq/clock.h (include it for the definition) */
uint32_t ri_sched_emit_sorted(const struct RITempoMap *map, uint64_t start_tick,
    uint32_t ppq, const struct RIStep *steps, uint32_t nsteps,
    uint16_t device, struct RIEvent *out, uint32_t cap);
#endif
