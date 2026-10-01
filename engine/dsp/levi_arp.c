/* levi_arp.c — Levi arpeggiator stepper bodies (v2 feature 3).
 * Own 8 modes; deterministic Entropy (struct-held LCG, seeded at
 * start); no RNG, no globals. */
#include "engine/dsp/levi_arp.h"

void ri_levi_arp_init(struct RILeviArp *a) {
    uint32_t i;
    if (!a)
        return;
    a->on = 0u;
    a->mode = RI_LEVI_ARP_UP;
    a->rate = 64u;
    a->n = 0u;
    for (i = 0u; i < RI_LEVI_ARP_MAXNOTES; i++)
        a->notes[i] = 60u;
    a->pos = 0u;
    a->dir = 0u;
    a->lcg = 1u;
    a->phrase = 0u;
    a->phpad[0] = a->phpad[1] = a->phpad[2] = 0u;
    a->uphr = 0;
}

int ri_levi_arp_start(struct RILeviArp *a, const uint8_t *notes, uint32_t n,
    uint32_t mode, uint32_t seed) {
    uint32_t i, j;
    if (!a || !notes || n == 0u || n > RI_LEVI_ARP_MAXNOTES ||
        mode >= RI_LEVI_ARP_NMODES)
        return 2;
    for (i = 0u; i < n; i++)
        a->notes[i] = notes[i];
    /* Latch low -> high (insertion sort, n <= 6). */
    for (i = 1u; i < n; i++) {
        uint8_t v = a->notes[i];
        j = i;
        while (j > 0u && a->notes[j - 1u] > v) {
            a->notes[j] = a->notes[j - 1u];
            j--;
        }
        a->notes[j] = v;
    }
    a->n = (uint8_t)n;
    a->mode = (uint8_t)mode;
    a->pos = 0u;
    a->dir = 0u;
    a->lcg = seed ? seed : 0x9E3779B9u;
    return 0;
}

static uint32_t next_lcg(struct RILeviArp *a) {
    a->lcg = a->lcg * 1664525u + 1013904223u;
    return a->lcg >> 16;
}

void ri_levi_arp_rewind(struct RILeviArp *a) {
    if (!a)
        return;
    a->pos = 0u;
    a->dir = 0u;
}

/* Own factory phrase rule (fidelity P8b): 64 phrases = 8 families x 8
 * variants over a major 2-octave ladder; rests peppered by hash. */
int8_t ri_levi_arp_phrase(uint8_t idx, uint32_t step) {
    static const int8_t LADDER[15] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 22, 24 };
    uint32_t family, variant, deg;
    if (idx >= 64u || step >= 16u)
        return 0;
    family = idx / 8u;
    variant = idx % 8u;
    if ((step * 7u + variant * 3u + family) % 13u == 0u)
        return RI_LEVI_ARP_REST;
    deg = (step * (3u + family) + variant) % 15u;
    return LADDER[deg];
}

int ri_levi_arp_step(struct RILeviArp *a, uint8_t *note_out) {
    uint8_t m;
    if (!a || !note_out)
        return 2;
    if (!a->on || a->n == 0u)
        return 1;
    m = a->mode;
    if (m == RI_LEVI_ARP_UP) {
        *note_out = a->notes[a->pos % a->n];
        a->pos = (uint8_t)((a->pos + 1u) % a->n);
    } else if (m == RI_LEVI_ARP_DOWN) {
        *note_out = a->notes[a->n - 1u - (a->pos % a->n)];
        a->pos = (uint8_t)((a->pos + 1u) % a->n);
    } else if (m == RI_LEVI_ARP_UPDOWN) {
        *note_out = a->notes[a->pos % a->n];
        if (a->n < 2u) {
            a->pos = 0u;
        } else if (a->dir == 0u) {
            if (a->pos + 1u >= a->n) {
                a->dir = 1u;
                a->pos = (uint8_t)(a->n - 2u);
            } else {
                a->pos++;
            }
        } else {
            if (a->pos == 0u) {
                a->dir = 0u;
                a->pos = 1u;
            } else {
                a->pos--;
            }
        }
    } else if (m == RI_LEVI_ARP_CHORD) {
        *note_out = a->notes[0];
    } else if (m == RI_LEVI_ARP_OCTUP) {
        uint32_t up = a->notes[0] + 12u;
        *note_out = (a->pos & 1u) ? (uint8_t)(up > 127u ? 127u : up) : a->notes[0];
        a->pos++;
    } else if (m == RI_LEVI_ARP_OCTDOWN) {
        uint8_t root = a->notes[0];
        uint8_t dn = root >= 12u ? (uint8_t)(root - 12u) : 0u;
        *note_out = (a->pos & 1u) ? dn : root;
        a->pos++;
    } else if (m == RI_LEVI_ARP_RANDOM) {
        *note_out = a->notes[next_lcg(a) % a->n];
    } else if (m == RI_LEVI_ARP_ENTROPY) {
        uint32_t r = next_lcg(a);
        int drift = (int)(r % 3u) - 1;
        int idx = (int)(a->pos % (a->n ? a->n : 1u)) + drift;
        if (idx < 0)
            idx = 1 < (int)a->n ? 1 : 0;
        if (idx >= (int)a->n)
            idx = (int)a->n >= 2 ? (int)a->n - 2 : 0;
        a->pos = (uint8_t)idx;
        if ((r >> 8) % 8u == 0u) {
            uint32_t up = (uint32_t)a->notes[idx] + 12u;
            *note_out = (uint8_t)(up > 127u ? 127u : up);
        } else {
            *note_out = a->notes[idx];
        }
    } else if (m == RI_LEVI_ARP_PHRASE) {
        /* Chord root + rule-authored offset (own phrases, P8b). */
        uint8_t ph = a->phrase;
        int8_t off;
        uint32_t st = a->pos % 16u;
        int note;
        a->pos++;
        if (a->n == 0u)
            return 1;
        if (ph >= 128u)
            ph = 127u;
        if (ph < 64u) {
            off = ri_levi_arp_phrase(ph, st);
        } else if (a->uphr) {
            off = a->uphr[(ph - 64u) * 16u + st];
            if (off < -24)
                off = -24;
            if (off > 24 && off != RI_LEVI_ARP_REST)
                off = 24;
        } else {
            off = 0;
        }
        if (off == RI_LEVI_ARP_REST)
            return 1;
        note = (int)a->notes[0] + (int)off;
        if (note < 0)
            note = 0;
        if (note > 127)
            note = 127;
        *note_out = (uint8_t)note;
    } else {
        return 2;
    }
    return 0;
}
