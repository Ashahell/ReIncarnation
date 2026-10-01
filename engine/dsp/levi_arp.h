/* levi_arp.h — Levi arpeggiator stepper (v2 feature 3, owner order).
 * Own 8 modes (clean-room: arrangement/function from the manual, no
 * ASM content). Deterministic: Entropy wanders an LCG held in the
 * struct, seeded at arp start (song position); no RNG, no globals,
 * matching the levi.h voice-bank law. Pure C, host-tested.
 */
#ifndef RI_LEVI_ARP_H
#define RI_LEVI_ARP_H
#include <stdint.h>

#define RI_LEVI_ARP_UP 0u     /* low -> high, wrap */
#define RI_LEVI_ARP_DOWN 1u   /* high -> low, wrap */
#define RI_LEVI_ARP_UPDOWN 2u /* low -> high -> low (ends not repeated) */
#define RI_LEVI_ARP_CHORD 3u  /* all held notes together, re-struck */
#define RI_LEVI_ARP_OCTUP 4u  /* root, root+12 alternating */
#define RI_LEVI_ARP_OCTDOWN 5u        /* root, root-12 alternating */
#define RI_LEVI_ARP_RANDOM 6u /* seeded LCG pick from the chord */
#define RI_LEVI_ARP_ENTROPY 7u        /* seeded walk: neighbour drift + leaps */
#define RI_LEVI_ARP_PHRASE 8u /* chord root + rule-authored offsets (P8b) */
#define RI_LEVI_ARP_NMODES 9u

#define RI_LEVI_ARP_MAXNOTES 6u /* lane count == voice slots */

/* Phrase rest step (P8b): the step is silent, the position advances. */
#define RI_LEVI_ARP_REST 100

/* Rate knob 0..127 -> arp steps per quarter note (transport clock
 * subdivision, delay-tempo-clock precedent: knob full = 4/quarter). */
#define RI_LEVI_ARP_STEPSQ(rate) \
    (((uint32_t)(rate) >= 96u) ? 4u : ((uint32_t)(rate) >= 64u) ? 2u : \
    ((uint32_t)(rate) >= 32u) ? 1u : 0u)

struct RILeviArp {
    uint8_t on;    /* gate: 0 = passthrough (step returns 1, silent) */
    uint8_t mode;  /* RI_LEVI_ARP_* */
    uint8_t rate;  /* 0..127 UI value */
    uint8_t n;     /* held notes 0..6 */
    uint8_t notes[RI_LEVI_ARP_MAXNOTES]; /* low -> high, set at start */
    uint8_t pos;   /* stepper position */
    uint8_t dir;   /* updown/octave direction flag */
    uint32_t lcg;  /* entropy state (seeded at start, never global) */
    uint8_t phrase; /* phrase 0..127 (factory 0..63, user 64..127) */
    uint8_t phpad[3];
    const int8_t *uphr; /* user bank (64x16, NULL = unison) */
};

void ri_levi_arp_init(struct RILeviArp *a);
/* Latch the chord (copied low -> high) + seed; returns 0 ok, 2 bad
 * (NULL / empty / too many / bad mode). Starting re-arms pos/dir. */
int ri_levi_arp_start(struct RILeviArp *a, const uint8_t *notes, uint32_t n,
    uint32_t mode, uint32_t seed);
/* Rewind the pattern position without reseeding (length wrap). */
void ri_levi_arp_rewind(struct RILeviArp *a);
/* Factory phrase offset for phrase 0..63 step 0..15 (own rule, zero
 * bytes): major 2-octave walk, RI_LEVI_ARP_REST marks rests. Bad
 * index/step reads 0. Pure. */
int8_t ri_levi_arp_phrase(uint8_t idx, uint32_t step);
/* Next step: *note_out gets the MIDI note (chord re-strike for CHORD
 * mode); returns 0 note ready, 1 silent/off, 2 bad. */
int ri_levi_arp_step(struct RILeviArp *a, uint8_t *note_out);

#endif
