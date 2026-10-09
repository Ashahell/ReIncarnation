/* midi_io/midi_levi.c — the Leviasynth channel's map and parser (M4b).
 * See midi_levi.h. E1 rows are cited by manual page (pp. 168-169).
 */
#include <string.h>

#include "midi_io/midi_levi.h"
#include "engine/dsp/levi.h"
#include "engine/seq/autolane.h"

/* Mod-envelope / LFO keys (RI_AUTO_BLK_LEVIMOD): ENV n field f is
 * (n << 5) | f, LFO n field f is 0xA0 | (n << 4) | f. Per-oscillator keys
 * (RI_AUTO_BLK_LEVIOP) are op << 5 | param. Both encodings are the
 * engine's (engine/engine.c), used here only to build keys. */
#define MEV(e, f) ((uint16_t)(RI_AUTO_BLK_LEVIMOD | (((uint32_t)(e) << 5) | ((uint32_t)(f) & 0x1Fu))))
#define LFOV(l, f) ((uint16_t)(RI_AUTO_BLK_LEVIMOD | 0xA0u | \
    (((uint32_t)(l) << 4) | ((uint32_t)(f) & 0xFu))))
#define OPV(o, p) ((uint16_t)(RI_AUTO_BLK_LEVIOP | \
    (((uint32_t)(o) << 5) | ((uint32_t)(p) & 31u))))

/* The manual's ENV n A/D/S/R are the modulation envelopes' times, which
 * are the oscillator-envelope time params in our model (RI_LEVI_OP_*). */
#define ENV_ATK(e) MEV((e), RI_LEVI_OP_ATTACK)
#define ENV_DEC(e) MEV((e), RI_LEVI_OP_DECAY)
#define ENV_SUS(e) MEV((e), RI_LEVI_OP_SUSTAIN)
#define ENV_REL(e) MEV((e), RI_LEVI_OP_RELEASE)

struct RIMidiLeviRow {
    uint8_t cc;
    uint8_t page;     /* E1 manual page (168/169) */
    uint16_t key;     /* 0 = unmapped */
    const char *why;  /* NULL when mapped */
};

/* E1 pp. 168-169, "MIDI CC Charts / Sorted by CC number". Only CCs whose
 * parameter we can address with one 7-bit value are mapped; everything
 * else carries its reason, so the gaps are on the record rather than
 * silently ignored. */
static const struct RIMidiLeviRow k_rows[] = {
    /* Global / performance */
    { 1u, 168u, 0u, "mod wheel: a performance signal, not a parameter (CC 1, p. 168)" },
    { 5u, 168u, RI_CTL_LEVI_VGLTIME, 0 },
    { 7u, 168u, 0u, "master volume: no Leviasynth output level; the ReBirth master fader must not move from this channel (CC 7, p. 168)" },
    { 8u, 168u, RI_CTL_LEVI_PFBAL, 0 },
    { 9u, 168u, 0u, "polyphony: we have a polyphony mode, not a settable voice count (CC 9, p. 168)" },
    { 10u, 168u, RI_CTL_LEVI_UDENSITY, 0 },
    { 11u, 168u, 0u, "expression pedal: the engine has no expression input (CC 11, p. 168)" },
    /* VCA / voice */
    { 2u, 168u, RI_CTL_LEVI_VCALVL, 0 },
    { 95u, 169u, RI_CTL_LEVI_VDETUNE, 0 },
    /* Pre-FX, Delay, Reverb, Post-FX */
    { 12u, 168u, RI_CTL_LEVI_PP1, 0 },
    { 13u, 168u, RI_CTL_LEVI_PP2, 0 },
    { 14u, 169u, RI_CTL_LEVI_DLYFB, 0 },
    { 15u, 169u, RI_CTL_LEVI_DLYTIME, 0 },
    { 62u, 168u, RI_CTL_LEVI_DLYFBTONE, 0 },
    { 63u, 168u, RI_CTL_LEVI_DLYWTONE, 0 },
    { 66u, 168u, RI_CTL_LEVI_RTIME, 0 },
    { 67u, 168u, RI_CTL_LEVI_RTONE, 0 },
    { 68u, 168u, RI_CTL_LEVI_OP1, 0 },
    { 69u, 168u, RI_CTL_LEVI_OP2, 0 },
    { 91u, 169u, RI_CTL_LEVI_RDRYWET, 0 },
    { 92u, 169u, RI_CTL_LEVI_DLYDRYWET, 0 },
    { 93u, 169u, RI_CTL_LEVI_PDRYWET, 0 },
    { 94u, 169u, RI_CTL_LEVI_ODRYWET, 0 },
    /* Digital filter */
    { 50u, 168u, RI_CTL_LEVI_DMORPH, 0 },
    { 51u, 168u, RI_CTL_LEVI_DKEYTRK, 0 },
    { 52u, 168u, RI_CTL_LEVI_DLFO1, 0 },
    { 53u, 168u, RI_CTL_LEVI_DVEL, 0 },
    { 54u, 168u, RI_CTL_LEVI_DENV1, 0 },
    { 55u, 168u, RI_CTL_LEVI_CUTOFF, 0 },
    { 56u, 168u, RI_CTL_LEVI_RESO, 0 },
    /* Analog filter */
    { 57u, 168u, RI_CTL_LEVI_DRIVE, 0 },
    { 58u, 168u, RI_CTL_LEVI_AKEYTRK, 0 },
    { 59u, 168u, RI_CTL_LEVI_ALFO2, 0 },
    { 60u, 168u, RI_CTL_LEVI_AVEL, 0 },
    { 61u, 168u, RI_CTL_LEVI_AENV2, 0 },
    { 71u, 168u, RI_CTL_LEVI_RESO2, 0 },
    { 74u, 168u, RI_CTL_LEVI_CUTOFF2, 0 },
    /* Oscillators: level and feedback are one CC each; pitch is not */
    { 24u, 168u, OPV(0u, RI_LEVI_OP_INIT), 0 },
    { 25u, 168u, OPV(1u, RI_LEVI_OP_INIT), 0 },
    { 26u, 168u, OPV(2u, RI_LEVI_OP_INIT), 0 },
    { 27u, 168u, OPV(3u, RI_LEVI_OP_INIT), 0 },
    { 28u, 168u, OPV(4u, RI_LEVI_OP_INIT), 0 },
    { 29u, 168u, OPV(5u, RI_LEVI_OP_INIT), 0 },
    { 30u, 168u, OPV(6u, RI_LEVI_OP_INIT), 0 },
    { 31u, 168u, OPV(7u, RI_LEVI_OP_INIT), 0 },
    /* E1 p. 168: pitch is CC 33-41 (38 reserved), feedback CC 42-49. */
    { 33u, 168u, 0u, "OSC 1 pitch needs mode + coarse + fine; one CC cannot address them (CC 33, p. 168)" },
    { 34u, 168u, 0u, "OSC 2 pitch: mode + coarse + fine (CC 34, p. 168)" },
    { 35u, 168u, 0u, "OSC 3 pitch: mode + coarse + fine (CC 35, p. 168)" },
    { 36u, 168u, 0u, "OSC 4 pitch: mode + coarse + fine (CC 36, p. 168)" },
    { 37u, 168u, 0u, "OSC 5 pitch: mode + coarse + fine (CC 37, p. 168)" },
    { 39u, 168u, 0u, "OSC 6 pitch: mode + coarse + fine (CC 39, p. 168)" },
    { 40u, 168u, 0u, "OSC 7 pitch: mode + coarse + fine (CC 40, p. 168)" },
    { 41u, 168u, 0u, "OSC 8 pitch: mode + coarse + fine (CC 41, p. 168)" },
    { 42u, 168u, OPV(0u, RI_LEVI_OP_FEEDBACK), 0 },
    { 43u, 168u, OPV(1u, RI_LEVI_OP_FEEDBACK), 0 },
    { 44u, 168u, OPV(2u, RI_LEVI_OP_FEEDBACK), 0 },
    { 45u, 168u, OPV(3u, RI_LEVI_OP_FEEDBACK), 0 },
    { 46u, 168u, OPV(4u, RI_LEVI_OP_FEEDBACK), 0 },
    { 47u, 168u, OPV(5u, RI_LEVI_OP_FEEDBACK), 0 },
    { 48u, 168u, OPV(6u, RI_LEVI_OP_FEEDBACK), 0 },
    { 49u, 168u, OPV(7u, RI_LEVI_OP_FEEDBACK), 0 },
    /* Modulation envelopes 1-5 (CC 81-97) */
    { 81u, 168u, ENV_ATK(0u), 0 },
    { 82u, 168u, ENV_DEC(0u), 0 },
    { 83u, 168u, ENV_SUS(0u), 0 },
    { 84u, 169u, ENV_REL(0u), 0 },
    { 85u, 169u, ENV_ATK(1u), 0 },
    { 86u, 169u, ENV_DEC(1u), 0 },
    { 87u, 169u, ENV_SUS(1u), 0 },
    { 88u, 169u, ENV_REL(1u), 0 },
    { 89u, 169u, ENV_ATK(2u), 0 },
    { 90u, 169u, ENV_DEC(2u), 0 },
    { 96u, 169u, ENV_SUS(2u), 0 },
    { 97u, 169u, ENV_REL(2u), 0 },
    { 102u, 169u, ENV_ATK(3u), 0 },
    { 103u, 169u, ENV_DEC(3u), 0 },
    { 104u, 169u, ENV_SUS(3u), 0 },
    { 105u, 169u, ENV_REL(3u), 0 },
    { 106u, 169u, ENV_ATK(4u), 0 },
    { 107u, 169u, ENV_DEC(4u), 0 },
    { 108u, 169u, ENV_SUS(4u), 0 },
    { 109u, 169u, ENV_REL(4u), 0 },
    /* LFOs: rate and waveform are one CC each; level is a matrix slot */
    { 70u, 168u, 0u, "LFO 1 level: our LFO depth is a matrix-slot amount, which a CC alone cannot address (CC 70, p. 168)" },
    { 72u, 168u, LFOV(0u, RI_LEVI_LP_RATE), 0 },
    { 73u, 168u, LFOV(1u, RI_LEVI_LP_RATE), 0 },
    { 76u, 168u, LFOV(2u, RI_LEVI_LP_RATE), 0 },
    { 78u, 168u, LFOV(3u, RI_LEVI_LP_RATE), 0 },
    { 80u, 168u, LFOV(4u, RI_LEVI_LP_RATE), 0 },
    { 110u, 169u, LFOV(0u, RI_LEVI_LP_WAVE), 0 },
    { 111u, 169u, LFOV(1u, RI_LEVI_LP_WAVE), 0 },
    { 112u, 169u, LFOV(2u, RI_LEVI_LP_WAVE), 0 },
    { 113u, 169u, LFOV(3u, RI_LEVI_LP_WAVE), 0 },
    { 114u, 169u, LFOV(4u, RI_LEVI_LP_WAVE), 0 },
    /* Reserved and unsupported, kept on the record */
    { 0u, 168u, 0u, "bank select MSB: no patch bank system (CC 0, p. 168)" },
    { 32u, 168u, 0u, "bank select LSB: no patch bank system (CC 32, p. 168)" },
    { 16u, 168u, 0u, "macro 1 value: we have no user macros (CC 16, p. 168)" },
    { 23u, 168u, 0u, "macro 8 value: we have no user macros (CC 23, p. 168)" },
    { 4u, 168u, 0u, "LFO 2 level: matrix-slot amount, not addressable by one CC (CC 4, p. 168)" },
};

#define NROWS ((uint32_t)(sizeof k_rows / sizeof k_rows[0]))

static const struct RIMidiLeviRow *find(uint8_t cc) {
    uint32_t i;
    for (i = 0u; i < NROWS; i++)
        if (k_rows[i].cc == cc)
            return &k_rows[i];
    return 0;
}

uint16_t midi_levi_cc_key(uint8_t cc) {
    const struct RIMidiLeviRow *r = find(cc);
    return r ? r->key : 0u;
}

uint32_t midi_levi_cc_page(uint8_t cc) {
    const struct RIMidiLeviRow *r = find(cc);
    return r ? (uint32_t)r->page : 0u;
}

const char *midi_levi_cc_why(uint8_t cc) {
    const struct RIMidiLeviRow *r = find(cc);
    return (r && r->why) ? r->why : "";
}

int midi_levi_message(const struct RIMidiChan *t, uint8_t status, uint8_t d1,
    uint8_t d2, struct RIMidiLeviAction *out) {
    uint8_t ch;
    uint16_t key;
    if (!out)
        return 0;
    memset(out, 0, sizeof *out);
    out->kind = RI_LEVI_ACT_NONE;
    if (status >= 0xF0u)               /* system/realtime belongs to M3, not here */
        return 0;
    ch = (uint8_t)((status & 0x0Fu) + 1u);
    if (midi_chan_role(t, ch) != RI_MCHAN_LEVI)
        return 0;
    d1 &= 0x7Fu;
    d2 &= 0x7Fu;
    switch (status & 0xF0u) {
    case 0x90u:                       /* note on (velocity 0 = off) */
        out->kind = RI_LEVI_ACT_NOTE;
        out->key = RI_CTL_LEVI_NOTE;
        out->note = d1;
        out->val = d2;
        out->on = (uint8_t)(d2 ? 1u : 0u);
        out->hi = (uint8_t)(d1 | (out->on ? 0x80u : 0u));
        return 1;
    case 0x80u:                       /* note off */
        out->kind = RI_LEVI_ACT_NOTE;
        out->key = RI_CTL_LEVI_NOTE;
        out->note = d1;
        out->val = 0u;
        out->on = 0u;
        out->hi = (uint8_t)(d1 & 0x7Fu);
        return 1;
    case 0xB0u: {                     /* control change */
        if (d1 == 1u) {               /* mod wheel (E1 p. 168) */
            out->kind = RI_LEVI_ACT_PERF;
            out->key = RI_CTL_LEVI_WHEEL;
            out->perf = RI_LEVI_PERF_WHEEL;
            out->val = d2;
            return 1;
        }
        if (d1 == 65u) {              /* glide toggle (E1 p. 168) */
            out->kind = RI_LEVI_ACT_PERF;
            out->perf = RI_LEVI_PERF_GLIDE;
            out->on = (uint8_t)(d2 >= 64u ? 1u : 0u);
            out->val = d2;
            return 1;
        }
        if (d1 == 64u || d1 == 123u)  /* sustain pedal, all notes off (E1 p. 168) */
            return 0;                 /* no engine support: listed, not faked */
        key = midi_levi_cc_key(d1);
        if (!key)
            return 0;
        out->kind = RI_LEVI_ACT_PARAM;
        out->key = key;
        out->val = d2;
        return 1;
    }
    case 0xD0u:                       /* channel aftertouch */
        out->kind = RI_LEVI_ACT_PERF;
        out->key = RI_CTL_LEVI_PRESS;
        out->perf = RI_LEVI_PERF_PRESS;
        out->val = d1;
        out->on = (uint8_t)(d1 ? 1u : 0u);
        return 1;
    case 0xA0u:                       /* poly aftertouch */
        out->kind = RI_LEVI_ACT_PERF;
        out->key = RI_CTL_LEVI_PAT;
        out->perf = RI_LEVI_PERF_POLYAT;
        out->note = d1;
        out->val = d2;
        out->hi = (uint8_t)(d1 & 0x7Fu);
        out->on = (uint8_t)(d2 ? 1u : 0u);
        return 1;
    case 0xE0u:                       /* pitch bend, 14 bit */
        out->kind = RI_LEVI_ACT_PERF;
        out->key = RI_CTL_LEVI_BEND;
        out->perf = RI_LEVI_PERF_BEND;
        out->val = (uint16_t)(d1 & 0x7Fu);
        out->hi = (uint8_t)(d2 & 0x7Fu);
        return 1;
    default:
        return 0;
    }
}