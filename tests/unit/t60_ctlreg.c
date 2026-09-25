/* t60_ctlreg — ReBirth 2.0.1 control registry (§12.10 G1).
 * Oracle: Appendix C "Standard MIDI Mapping Tables" (p. 195-198) typed
 * independently below — every controller number must resolve to exactly
 * the section / group / legend the manual names, and no other control may
 * carry a controller. Plus: unique ids, sane ranges, Song-automation
 * exclusions (p. 72-73), selector sizes, and that every engine binding
 * points at an id block the engine actually dispatches.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "engine/dsp/rb808.h"
#include "engine/dsp/rb909.h"
#include "engine/fx/route.h"

struct CCRow { uint8_t cc; uint8_t section; const char *group; const char *legend; };

static const struct CCRow APPX_C[] = {
    { 7, RI_SEC_MASTER, "", "Level" },
    { 11, RI_SEC_MIX_SYNTH1, "", "Level" }, { 12, RI_SEC_MIX_SYNTH1, "", "Pan" }, { 13, RI_SEC_MIX_SYNTH1, "", "Delay" },
    { 14, RI_SEC_MIX_SYNTH2, "", "Level" }, { 15, RI_SEC_MIX_SYNTH2, "", "Pan" }, { 16, RI_SEC_MIX_SYNTH2, "", "Delay" },
    { 17, RI_SEC_MIX_808, "", "Level" }, { 18, RI_SEC_MIX_808, "", "Pan" }, { 19, RI_SEC_MIX_808, "", "Delay" },
    { 20, RI_SEC_MIX_909, "", "Level" }, { 21, RI_SEC_MIX_909, "", "Pan" }, { 22, RI_SEC_MIX_909, "", "Delay" },
    { 23, RI_SEC_SYNTH1, "", "Waveform" }, { 24, RI_SEC_SYNTH1, "", "Tune" }, { 25, RI_SEC_SYNTH1, "", "Cutoff" },
    { 26, RI_SEC_SYNTH1, "", "Reso" }, { 27, RI_SEC_SYNTH1, "", "Env Mod" }, { 28, RI_SEC_SYNTH1, "", "Decay" },
    { 29, RI_SEC_SYNTH1, "", "Accent" },
    { 30, RI_SEC_SYNTH2, "", "Waveform" }, { 31, RI_SEC_SYNTH2, "", "Tune" }, { 32, RI_SEC_SYNTH2, "", "Cutoff" },
    { 33, RI_SEC_SYNTH2, "", "Reso" }, { 34, RI_SEC_SYNTH2, "", "Env Mod" }, { 35, RI_SEC_SYNTH2, "", "Decay" },
    { 36, RI_SEC_SYNTH2, "", "Accent" },
    { 37, RI_SEC_808, "AC", "Level" }, { 38, RI_SEC_808, "BD", "Level" }, { 39, RI_SEC_808, "BD", "Tone" },
    { 40, RI_SEC_808, "BD", "Decay" }, { 41, RI_SEC_808, "SD", "Level" }, { 42, RI_SEC_808, "SD", "Tone" },
    { 43, RI_SEC_808, "SD", "Snappy" }, { 44, RI_SEC_808, "LT", "Level" }, { 45, RI_SEC_808, "LT", "Tune" },
    { 46, RI_SEC_808, "LT", "Switch" }, { 47, RI_SEC_808, "MT", "Level" }, { 48, RI_SEC_808, "MT", "Tune" },
    { 49, RI_SEC_808, "MT", "Switch" }, { 50, RI_SEC_808, "HT", "Level" }, { 51, RI_SEC_808, "HT", "Tune" },
    { 52, RI_SEC_808, "HT", "Switch" }, { 53, RI_SEC_808, "RS", "Level" }, { 54, RI_SEC_808, "RS", "Switch" },
    { 55, RI_SEC_808, "CP", "Level" }, { 56, RI_SEC_808, "CP", "Switch" }, { 57, RI_SEC_808, "CB", "Level" },
    { 58, RI_SEC_808, "CY", "Level" }, { 59, RI_SEC_808, "CY", "Tone" }, { 60, RI_SEC_808, "CY", "Decay" },
    { 61, RI_SEC_808, "OH", "Level" }, { 62, RI_SEC_808, "OH", "Decay" }, { 63, RI_SEC_808, "CH", "Level" },
    { 64, RI_SEC_808, "", "Instrument Selection" },
    { 65, RI_SEC_909, "AC", "Level" }, { 66, RI_SEC_909, "BD", "Level" }, { 67, RI_SEC_909, "BD", "Tune" },
    { 68, RI_SEC_909, "BD", "Attack" }, { 69, RI_SEC_909, "BD", "Decay" }, { 70, RI_SEC_909, "SD", "Level" },
    { 71, RI_SEC_909, "SD", "Tune" }, { 72, RI_SEC_909, "SD", "Tone" }, { 73, RI_SEC_909, "SD", "Snappy" },
    { 74, RI_SEC_909, "LT", "Level" }, { 75, RI_SEC_909, "LT", "Tune" }, { 76, RI_SEC_909, "LT", "Decay" },
    { 77, RI_SEC_909, "MT", "Level" }, { 78, RI_SEC_909, "MT", "Tune" }, { 79, RI_SEC_909, "MT", "Decay" },
    { 80, RI_SEC_909, "HT", "Level" }, { 81, RI_SEC_909, "HT", "Tune" }, { 82, RI_SEC_909, "HT", "Decay" },
    { 83, RI_SEC_909, "HH", "Level" }, { 84, RI_SEC_909, "RS", "Level" }, { 85, RI_SEC_909, "CP", "Level" },
    { 86, RI_SEC_909, "CH", "Decay" }, { 87, RI_SEC_909, "OH", "Decay" }, { 88, RI_SEC_909, "CC", "Level" },
    { 89, RI_SEC_909, "CC", "Tune" }, { 90, RI_SEC_909, "RC", "Level" }, { 91, RI_SEC_909, "RC", "Tune" },
    { 92, RI_SEC_909, "", "Flam" }, { 93, RI_SEC_909, "", "Instrument Selection" },
    { 94, RI_SEC_PCF, "", "Pattern" }, { 95, RI_SEC_PCF, "", "Mode" }, { 96, RI_SEC_PCF, "", "Freq" },
    { 97, RI_SEC_PCF, "", "Q" }, { 98, RI_SEC_PCF, "", "Amt" }, { 99, RI_SEC_PCF, "", "Decay" },
    { 100, RI_SEC_DELAY, "", "Steps" }, { 101, RI_SEC_DELAY, "", "Triplet" }, { 102, RI_SEC_DELAY, "", "Pan" },
    { 103, RI_SEC_DELAY, "", "F.Back" },
    { 104, RI_SEC_DIST, "", "Shape" }, { 105, RI_SEC_DIST, "", "Amount" },
    { 106, RI_SEC_COMP, "", "Ratio" }, { 107, RI_SEC_COMP, "", "Threshold" },
    { 108, RI_SEC_TRANSPORT, "", "Shuffle" },
};
#define NAPPX (sizeof(APPX_C) / sizeof(APPX_C[0]))

static int is_value_kind(uint8_t k) {
    return k == RI_CK_KNOB || k == RI_CK_FADER || k == RI_CK_SWITCH || k == RI_CK_SELECTOR;
}

static const struct RICtlDef *find_leg(uint8_t sec, const char *grp, const char *leg) {
    uint32_t i;
    for (i = 0; i < ri_ctlreg_count(); i++) {
        const struct RICtlDef *d = ri_ctlreg_at(i);
        if (d->section == sec && !strcmp(d->group, grp) && !strcmp(d->legend, leg))
            return d;
    }
    return 0;
}

int main(void) {
    uint32_t i, j, n = ri_ctlreg_count(), withcc = 0, s, totb = 0;
    RI_ASSERT(n > 200, "registry too small: %u (ReBirth has >200 panel controls)", n);

    /* ids unique + reg_id encodes section; ranges sane */
    for (i = 0; i < n; i++) {
        const struct RICtlDef *a = ri_ctlreg_at(i);
        RI_ASSERT(a->section < RI_SEC_COUNT, "row %u bad section", i);
        RI_ASSERT((a->reg_id >> 8) == a->section, "row %u reg_id/section mismatch", i);
        RI_ASSERT(a->min_v <= a->def_v && a->def_v <= a->max_v, "%s/%s default out of range",
            a->group, a->legend);
        RI_ASSERT(a->legend && a->legend[0], "row %u empty legend", i);
        RI_ASSERT(ri_ctlreg_find(a->reg_id) == a, "row %u not found by id", i);
        for (j = i + 1; j < n; j++)
            RI_ASSERT(ri_ctlreg_at(j)->reg_id != a->reg_id, "duplicate reg_id %04x", a->reg_id);
        if (a->midi_cc != RI_MIDI_CC_NONE) {
            withcc++;
            RI_ASSERT(is_value_kind(a->kind), "%s/%s has a controller but is not a value control",
                a->group, a->legend);
        }
        if (!is_value_kind(a->kind))
            RI_ASSERT(!a->automatable, "%s/%s non-control marked automatable", a->group, a->legend);
    }

    /* Appendix C: exact set, exact targets */
    RI_ASSERT(withcc == NAPPX, "controllers in registry %u, Appendix C has %u", withcc, (unsigned)NAPPX);
    for (i = 0; i < NAPPX; i++) {
        const struct RICtlDef *d = ri_ctlreg_by_cc(APPX_C[i].cc);
        RI_ASSERT(d != 0, "CC %u missing", APPX_C[i].cc);
        if (!d)
            continue;
        RI_ASSERT(d->section == APPX_C[i].section && !strcmp(d->group, APPX_C[i].group) &&
            !strcmp(d->legend, APPX_C[i].legend), "CC %u -> %s %s/%s, manual says %s/%s",
            APPX_C[i].cc, ri_ctlreg_section_name(d->section), d->group, d->legend,
            APPX_C[i].group, APPX_C[i].legend);
    }

    /* Song automation exclusions (p. 72-73): Tempo, Mute x4, Master Level, Shuffle */
    RI_ASSERT(!find_leg(RI_SEC_TRANSPORT, "", "Tempo")->automatable, "Tempo automatable");
    RI_ASSERT(!find_leg(RI_SEC_TRANSPORT, "", "Shuffle")->automatable, "Shuffle automatable");
    RI_ASSERT(!find_leg(RI_SEC_MASTER, "", "Level")->automatable, "Master Level automatable");
    for (s = RI_SEC_MIX_SYNTH1; s <= RI_SEC_MIX_909; s++) {
        RI_ASSERT(!find_leg((uint8_t)s, "", "On/Off")->automatable, "mute automatable (sec %u)", s);
        RI_ASSERT(find_leg((uint8_t)s, "", "Level")->automatable, "mixer level must be automatable");
    }
    RI_ASSERT(find_leg(RI_SEC_SYNTH1, "", "Cutoff")->automatable, "Cutoff must be automatable");

    /* ranges from the manual */
    RI_ASSERT(find_leg(RI_SEC_TRANSPORT, "", "Tempo")->min_v == 20 &&
        find_leg(RI_SEC_TRANSPORT, "", "Tempo")->max_v == 500, "tempo range (p. 145)");
    RI_ASSERT(find_leg(RI_SEC_808, "", "Instrument Selection")->max_v == 11 &&
        find_leg(RI_SEC_909, "", "Instrument Selection")->max_v == 11, "selector 12 positions (p. 203)");
    RI_ASSERT(find_leg(RI_SEC_PCF, "", "Pattern")->max_v == 53, "PCF patterns 0..53");
    RI_ASSERT(find_leg(RI_SEC_DELAY, "", "Steps")->min_v == 1 &&
        find_leg(RI_SEC_DELAY, "", "Steps")->max_v == 32, "delay steps 1..32 (p. 70)");
    RI_ASSERT(find_leg(RI_SEC_SYNTH1, "", "Tune")->max_v - find_leg(RI_SEC_SYNTH1, "", "Tune")->min_v == 24,
        "tune spans 2 octaves in semitones (p. 155)");
    RI_ASSERT(find_leg(RI_SEC_909, "Steps", "Step 1")->max_v == 3, "909 step off/low/high/flam");
    RI_ASSERT(find_leg(RI_SEC_808, "Steps", "Step 16")->max_v == 1, "808 step on/off");
    RI_ASSERT(find_leg(RI_SEC_SYNTH1, "Pitch", "C+") != 0 && find_leg(RI_SEC_SYNTH1, "Pitch", "C") != 0,
        "13 pitch keys low C..high C");

    /* TB-303 knob order: Waveform, Tune, Cutoff, Reso, Env Mod, Decay, Accent */
    {
        static const char *const ord[7] = { "Waveform", "Tune", "Cutoff", "Reso", "Env Mod", "Decay", "Accent" };
        for (i = 0; i < 7; i++) {
            const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_SYNTH1 << 8) | i));
            RI_ASSERT(d && !strcmp(d->legend, ord[i]), "synth control %u is not %s", i, ord[i]);
        }
    }

    /* bindings land in dispatched blocks with valid voices */
    for (i = 0; i < n; i++) {
        const struct RICtlDef *d = ri_ctlreg_at(i);
        switch (d->bind) {
        case RI_BIND_303:
            RI_ASSERT(d->engine_id >= 0x0300u && d->engine_id <= 0x0317u, "303 id %04x", d->engine_id);
            break;
        case RI_BIND_808V: case RI_BIND_808ALL:
            RI_ASSERT(d->engine_id >= 0x0400u && d->engine_id <= 0x0405u, "808 id %04x", d->engine_id);
            RI_ASSERT(d->voice < RI_808_NSOUNDS, "808 voice %u", d->voice);
            break;
        case RI_BIND_909V:
            RI_ASSERT(d->engine_id >= 0x0900u && d->engine_id <= 0x0902u, "909 id %04x (flamres is a placeholder)", d->engine_id);
            RI_ASSERT(d->voice < RI_909_NVOICES, "909 voice %u", d->voice);
            break;
        case RI_BIND_FX:
            RI_ASSERT(d->engine_id >= 0x0A00u && d->engine_id <= 0x0A0Fu, "fx id %04x", d->engine_id);
            break;
        case RI_BIND_PAN: case RI_BIND_SEND:
            RI_ASSERT(d->voice < RI_ROUTE_NSECTIONS, "route section %u", d->voice);
            break;
        case RI_BIND_INSERT:
            RI_ASSERT(d->engine_id < RI_ROUTE_NUNITS, "insert unit %u", d->engine_id);
            RI_ASSERT(d->voice < RI_ROUTE_NSECTIONS || (d->voice == RI_ROUTE_MASTER && d->engine_id == RI_ROUTE_COMP),
                "insert owner %u", d->voice);
            break;
        case RI_BIND_NONE: case RI_BIND_909HAT: case RI_BIND_TEMPO:
            break;
        default:
            RI_ASSERT(0, "unknown bind %u", d->bind);
        }
    }
    /* the 909 engine voice table is not the panel order: spot-check the bridge */
    RI_ASSERT(find_leg(RI_SEC_909, "CC", "Tune")->voice == RB909_CR, "909 CC -> RB909_CR");
    RI_ASSERT(find_leg(RI_SEC_909, "CH", "Decay")->voice == RB909_CH, "909 CH decay voice");
    RI_ASSERT(find_leg(RI_SEC_808, "SD", "Tone")->engine_id == RI_CTL_808_TUNE, "808 SD Tone = body pitch");

    /* coverage record */
    for (s = 0; s < RI_SEC_COUNT; s++) {
        uint32_t b = 0, c = ri_ctlreg_section_count(s, &b);
        totb += b;
        printf("  %-16s %3u controls, %3u engine-bound\n", ri_ctlreg_section_name(s), c, b);
    }
    printf("  total %u controls, %u engine-bound, %u Appendix C controllers\n", n, totb, (unsigned)NAPPX);
    RI_RESULT("ctlreg");
}
