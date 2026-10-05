/* gui/ctlreg.c — ReBirth 2.0.1 panel control registry (§12.10 G1).
 * Inventory, legends, controller numbers and Song-automation flags are E1
 * (ReBirth RB-338 2.0.1 Owner's Manual: reference chapter p. 143-165,
 * Appendix C p. 195-198, Song automation exclusions p. 72-73). Knob order
 * follows the TB-303 panel (Waveform, Tune, Cutoff, Reso, Env Mod, Decay,
 * Accent). Defaults are E0 neutral playing positions (the manual gives
 * none); Levels 100, bipolar/shape knobs 64, sends 0.
 * Notes kept honest rather than silently resolved:
 * - Synth Tune spans +/-12 semitones (p. 155: "+-1 octaves in steps of
 *   semi-tones"): range 52..76 on the engine's value-64 semitone law.
 * - 808 SD "Tone" is the body pitch (p. 37) -> engine TUNE on the SD voice.
 * - Mixer On/Off IS the mute (p. 72, p. 157 figure) — not Song-automated.
 * - PCF Mode is LP/BP (p. 160); Appendix C's "LP/HP" is a manual typo.
 * - PCF Freq/Q/Amt/Decay are vertical sliders (p. 159 figure; p. 160 "the
 *   Amount slider"), not knobs.
 * - Instrument Selection has 12 positions: AC + 11 instruments (p. 203).
 * - Pattern-section Shuffle automation is not stated in the manual: kept
 *   non-automatable until ReBirth is checked (ledger: shuffle-scope.md).
 * Append-only: never reorder rows inside a section (reg_id is the index).
 */
#include "gui/ctlreg.h"
#include "engine/seq/autolane.h"
#include <string.h>
#include "engine/dsp/rb303.h"
#include "engine/dsp/rb808.h"
#include "engine/dsp/rb909.h"
#include "engine/dsp/levi.h"
#include "engine/fx/fx.h"
#include "engine/fx/route.h"

#define R(sec, idx, kind, grp, leg, mn, mx, df, cc, aut, bnd, eid, vc) \
    { (uint16_t)((RI_SEC_##sec << 8) | (idx)), RI_SEC_##sec, RI_CK_##kind, \
      grp, leg, mn, mx, df, (uint8_t)(cc), aut, RI_BIND_##bnd, 0, \
      (uint16_t)(eid), (uint16_t)(vc) }

static const struct RICtlDef RI_CTLREG[] = {
    R(SYNTH1, 0, SWITCH, "", "Waveform", 0, 1, 0, 23, 1, 303, RI_CTL_303A_WAVE, 0),
    R(SYNTH1, 1, KNOB, "", "Tune", 52, 76, 64, 24, 1, 303, RI_CTL_303A_TUNE, 0),
    R(SYNTH1, 2, KNOB, "", "Cutoff", 0, 127, 96, 25, 1, 303, RI_CTL_303A_CUTOFF, 0),
    R(SYNTH1, 3, KNOB, "", "Reso", 0, 127, 32, 26, 1, 303, RI_CTL_303A_RESO, 0),
    R(SYNTH1, 4, KNOB, "", "Env Mod", 0, 127, 64, 27, 1, 303, RI_CTL_303A_ENVMOD, 0),
    R(SYNTH1, 5, KNOB, "", "Decay", 0, 127, 64, 28, 1, 303, RI_CTL_303A_DECAY, 0),
    R(SYNTH1, 6, KNOB, "", "Accent", 0, 127, 64, 29, 1, 303, RI_CTL_303A_ACCENT, 0),
    R(SYNTH1, 7, BUTTON, "Pitch", "C", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 8, BUTTON, "Pitch", "C#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 9, BUTTON, "Pitch", "D", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 10, BUTTON, "Pitch", "D#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 11, BUTTON, "Pitch", "E", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 12, BUTTON, "Pitch", "F", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 13, BUTTON, "Pitch", "F#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 14, BUTTON, "Pitch", "G", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 15, BUTTON, "Pitch", "G#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 16, BUTTON, "Pitch", "A", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 17, BUTTON, "Pitch", "A#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 18, BUTTON, "Pitch", "B", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 19, BUTTON, "Pitch", "C+", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 20, SWITCH, "Step", "Down", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 21, SWITCH, "Step", "Up", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 22, SWITCH, "Step", "Accent", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 23, SWITCH, "Step", "Slide", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 24, SWITCH, "Step", "Note/Pause", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 25, BUTTON, "Step", "Back", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 26, BUTTON, "Step", "Step", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 27, SWITCH, "Step", "Pitch Mode", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 28, BUTTON, "Step", "Clear", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH1, 29, DISPLAY, "Step", "Step display", 1, 16, 1, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 0, SWITCH, "", "Waveform", 0, 1, 0, 30, 1, 303, RI_CTL_303B_WAVE, 0),
    R(SYNTH2, 1, KNOB, "", "Tune", 52, 76, 64, 31, 1, 303, RI_CTL_303B_TUNE, 0),
    R(SYNTH2, 2, KNOB, "", "Cutoff", 0, 127, 96, 32, 1, 303, RI_CTL_303B_CUTOFF, 0),
    R(SYNTH2, 3, KNOB, "", "Reso", 0, 127, 32, 33, 1, 303, RI_CTL_303B_RESO, 0),
    R(SYNTH2, 4, KNOB, "", "Env Mod", 0, 127, 64, 34, 1, 303, RI_CTL_303B_ENVMOD, 0),
    R(SYNTH2, 5, KNOB, "", "Decay", 0, 127, 64, 35, 1, 303, RI_CTL_303B_DECAY, 0),
    R(SYNTH2, 6, KNOB, "", "Accent", 0, 127, 64, 36, 1, 303, RI_CTL_303B_ACCENT, 0),
    R(SYNTH2, 7, BUTTON, "Pitch", "C", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 8, BUTTON, "Pitch", "C#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 9, BUTTON, "Pitch", "D", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 10, BUTTON, "Pitch", "D#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 11, BUTTON, "Pitch", "E", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 12, BUTTON, "Pitch", "F", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 13, BUTTON, "Pitch", "F#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 14, BUTTON, "Pitch", "G", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 15, BUTTON, "Pitch", "G#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 16, BUTTON, "Pitch", "A", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 17, BUTTON, "Pitch", "A#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 18, BUTTON, "Pitch", "B", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 19, BUTTON, "Pitch", "C+", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 20, SWITCH, "Step", "Down", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 21, SWITCH, "Step", "Up", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 22, SWITCH, "Step", "Accent", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 23, SWITCH, "Step", "Slide", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 24, SWITCH, "Step", "Note/Pause", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 25, BUTTON, "Step", "Back", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 26, BUTTON, "Step", "Step", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 27, SWITCH, "Step", "Pitch Mode", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 28, BUTTON, "Step", "Clear", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(SYNTH2, 29, DISPLAY, "Step", "Step display", 1, 16, 1, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 0, KNOB, "AC", "Level", 0, 127, 64, 37, 1, 808ALL, RI_CTL_808_ACCENT, 0),
    R(808, 1, KNOB, "BD", "Level", 0, 127, 100, 38, 1, 808V, RI_CTL_808_LEVEL, RB808_BD),
    R(808, 2, KNOB, "BD", "Tone", 0, 127, 64, 39, 1, 808V, RI_CTL_808_TONE, RB808_BD),
    R(808, 3, KNOB, "BD", "Decay", 0, 127, 64, 40, 1, 808V, RI_CTL_808_DECAY, RB808_BD),
    R(808, 4, KNOB, "SD", "Level", 0, 127, 100, 41, 1, 808V, RI_CTL_808_LEVEL, RB808_SD),
    R(808, 5, KNOB, "SD", "Tone", 0, 127, 64, 42, 1, 808V, RI_CTL_808_TUNE, RB808_SD),
    R(808, 6, KNOB, "SD", "Snappy", 0, 127, 64, 43, 1, 808V, RI_CTL_808_SNAPPY, RB808_SD),
    R(808, 7, KNOB, "LT", "Level", 0, 127, 100, 44, 1, 808V, RI_CTL_808_LEVEL, RB808_LT),
    R(808, 8, KNOB, "LT", "Tune", 0, 127, 64, 45, 1, 808V, RI_CTL_808_TUNE, RB808_LT),
    R(808, 9, SWITCH, "LT", "Switch", 0, 1, 0, 46, 1, NONE, 0, 0),
    R(808, 10, KNOB, "MT", "Level", 0, 127, 100, 47, 1, 808V, RI_CTL_808_LEVEL, RB808_MT),
    R(808, 11, KNOB, "MT", "Tune", 0, 127, 64, 48, 1, 808V, RI_CTL_808_TUNE, RB808_MT),
    R(808, 12, SWITCH, "MT", "Switch", 0, 1, 0, 49, 1, NONE, 0, 0),
    R(808, 13, KNOB, "HT", "Level", 0, 127, 100, 50, 1, 808V, RI_CTL_808_LEVEL, RB808_HT),
    R(808, 14, KNOB, "HT", "Tune", 0, 127, 64, 51, 1, 808V, RI_CTL_808_TUNE, RB808_HT),
    R(808, 15, SWITCH, "HT", "Switch", 0, 1, 0, 52, 1, NONE, 0, 0),
    R(808, 16, KNOB, "RS", "Level", 0, 127, 100, 53, 1, 808V, RI_CTL_808_LEVEL, RB808_RS),
    R(808, 17, SWITCH, "RS", "Switch", 0, 1, 0, 54, 1, NONE, 0, 0),
    R(808, 18, KNOB, "CP", "Level", 0, 127, 100, 55, 1, 808V, RI_CTL_808_LEVEL, RB808_CP),
    R(808, 19, SWITCH, "CP", "Switch", 0, 1, 0, 56, 1, NONE, 0, 0),
    R(808, 20, KNOB, "CB", "Level", 0, 127, 100, 57, 1, 808V, RI_CTL_808_LEVEL, RB808_CB),
    R(808, 21, KNOB, "CY", "Level", 0, 127, 100, 58, 1, 808V, RI_CTL_808_LEVEL, RB808_CY),
    R(808, 22, KNOB, "CY", "Tone", 0, 127, 64, 59, 1, 808V, RI_CTL_808_TONE, RB808_CY),
    R(808, 23, KNOB, "CY", "Decay", 0, 127, 64, 60, 1, 808V, RI_CTL_808_DECAY, RB808_CY),
    R(808, 24, KNOB, "OH", "Level", 0, 127, 100, 61, 1, 808V, RI_CTL_808_LEVEL, RB808_OH),
    R(808, 25, KNOB, "OH", "Decay", 0, 127, 64, 62, 1, 808V, RI_CTL_808_DECAY, RB808_OH),
    R(808, 26, KNOB, "CH", "Level", 0, 127, 100, 63, 1, 808V, RI_CTL_808_LEVEL, RB808_CH),
    R(808, 27, SELECTOR, "", "Instrument Selection", 0, 11, 1, 64, 1, NONE, 0, 0),
    R(808, 28, STEP, "Steps", "Step 1", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 29, STEP, "Steps", "Step 2", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 30, STEP, "Steps", "Step 3", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 31, STEP, "Steps", "Step 4", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 32, STEP, "Steps", "Step 5", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 33, STEP, "Steps", "Step 6", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 34, STEP, "Steps", "Step 7", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 35, STEP, "Steps", "Step 8", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 36, STEP, "Steps", "Step 9", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 37, STEP, "Steps", "Step 10", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 38, STEP, "Steps", "Step 11", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 39, STEP, "Steps", "Step 12", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 40, STEP, "Steps", "Step 13", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 41, STEP, "Steps", "Step 14", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 42, STEP, "Steps", "Step 15", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(808, 43, STEP, "Steps", "Step 16", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 0, KNOB, "AC", "Level", 0, 127, 64, 65, 1, NONE, 0, 0),
    R(909, 1, KNOB, "BD", "Level", 0, 127, 100, 66, 1, 909V, RI_CTL_909_LEVEL, RB909_BD),
    R(909, 2, KNOB, "BD", "Tune", 0, 127, 64, 67, 1, 909V, RI_CTL_909_TUNE, RB909_BD),
    R(909, 3, KNOB, "BD", "Attack", 0, 127, 64, 68, 1, NONE, 0, 0),
    R(909, 4, KNOB, "BD", "Decay", 0, 127, 64, 69, 1, 909V, RI_CTL_909_DECAY, RB909_BD),
    R(909, 5, KNOB, "SD", "Level", 0, 127, 100, 70, 1, 909V, RI_CTL_909_LEVEL, RB909_SD),
    R(909, 6, KNOB, "SD", "Tune", 0, 127, 64, 71, 1, 909V, RI_CTL_909_TUNE, RB909_SD),
    R(909, 7, KNOB, "SD", "Tone", 0, 127, 64, 72, 1, NONE, 0, 0),
    R(909, 8, KNOB, "SD", "Snappy", 0, 127, 64, 73, 1, NONE, 0, 0),
    R(909, 9, KNOB, "LT", "Level", 0, 127, 100, 74, 1, 909V, RI_CTL_909_LEVEL, RB909_LT),
    R(909, 10, KNOB, "LT", "Tune", 0, 127, 64, 75, 1, 909V, RI_CTL_909_TUNE, RB909_LT),
    R(909, 11, KNOB, "LT", "Decay", 0, 127, 64, 76, 1, 909V, RI_CTL_909_DECAY, RB909_LT),
    R(909, 12, KNOB, "MT", "Level", 0, 127, 100, 77, 1, 909V, RI_CTL_909_LEVEL, RB909_MT),
    R(909, 13, KNOB, "MT", "Tune", 0, 127, 64, 78, 1, 909V, RI_CTL_909_TUNE, RB909_MT),
    R(909, 14, KNOB, "MT", "Decay", 0, 127, 64, 79, 1, 909V, RI_CTL_909_DECAY, RB909_MT),
    R(909, 15, KNOB, "HT", "Level", 0, 127, 100, 80, 1, 909V, RI_CTL_909_LEVEL, RB909_HT),
    R(909, 16, KNOB, "HT", "Tune", 0, 127, 64, 81, 1, 909V, RI_CTL_909_TUNE, RB909_HT),
    R(909, 17, KNOB, "HT", "Decay", 0, 127, 64, 82, 1, 909V, RI_CTL_909_DECAY, RB909_HT),
    R(909, 18, KNOB, "HH", "Level", 0, 127, 100, 83, 1, 909HAT, 0, 0),
    R(909, 19, KNOB, "RS", "Level", 0, 127, 100, 84, 1, 909V, RI_CTL_909_LEVEL, RB909_RS),
    R(909, 20, KNOB, "CP", "Level", 0, 127, 100, 85, 1, 909V, RI_CTL_909_LEVEL, RB909_CP),
    R(909, 21, KNOB, "CH", "Decay", 0, 127, 64, 86, 1, 909V, RI_CTL_909_DECAY, RB909_CH),
    R(909, 22, KNOB, "OH", "Decay", 0, 127, 64, 87, 1, 909V, RI_CTL_909_DECAY, RB909_OH),
    R(909, 23, KNOB, "CC", "Level", 0, 127, 100, 88, 1, 909V, RI_CTL_909_LEVEL, RB909_CR),
    R(909, 24, KNOB, "CC", "Tune", 0, 127, 64, 89, 1, 909V, RI_CTL_909_TUNE, RB909_CR),
    R(909, 25, KNOB, "RC", "Level", 0, 127, 100, 90, 1, 909V, RI_CTL_909_LEVEL, RB909_RD),
    R(909, 26, KNOB, "RC", "Tune", 0, 127, 64, 91, 1, 909V, RI_CTL_909_TUNE, RB909_RD),
    R(909, 27, KNOB, "", "Flam", 0, 127, 64, 92, 1, NONE, 0, 0),
    R(909, 28, SELECTOR, "", "Instrument Selection", 0, 11, 1, 93, 1, NONE, 0, 0),
    R(909, 29, SWITCH, "", "Flam button", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 30, STEP, "Steps", "Step 1", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 31, STEP, "Steps", "Step 2", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 32, STEP, "Steps", "Step 3", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 33, STEP, "Steps", "Step 4", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 34, STEP, "Steps", "Step 5", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 35, STEP, "Steps", "Step 6", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 36, STEP, "Steps", "Step 7", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 37, STEP, "Steps", "Step 8", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 38, STEP, "Steps", "Step 9", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 39, STEP, "Steps", "Step 10", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 40, STEP, "Steps", "Step 11", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 41, STEP, "Steps", "Step 12", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 42, STEP, "Steps", "Step 13", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 43, STEP, "Steps", "Step 14", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 44, STEP, "Steps", "Step 15", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(909, 45, STEP, "Steps", "Step 16", 0, 3, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(MIX_SYNTH1, 0, SWITCH, "", "On/Off", 0, 1, 1, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(MIX_SYNTH1, 1, METER, "", "Meter", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(MIX_SYNTH1, 2, FADER, "", "Level", 0, 127, 100, 11, 1, LEVEL, 0, 0),
    R(MIX_SYNTH1, 3, KNOB, "", "Pan", 0, 127, 64, 12, 1, PAN, 0, 0),
    R(MIX_SYNTH1, 4, KNOB, "", "Delay", 0, 127, 0, 13, 1, SEND, 0, 0),
    R(MIX_SYNTH1, 5, SWITCH, "", "Dist", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_DIST, 0),
    R(MIX_SYNTH1, 6, SWITCH, "", "PCF", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_PCF, 0),
    R(MIX_SYNTH1, 7, SWITCH, "", "Comp", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_COMP, 0),
    R(MIX_SYNTH2, 0, SWITCH, "", "On/Off", 0, 1, 1, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(MIX_SYNTH2, 1, METER, "", "Meter", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(MIX_SYNTH2, 2, FADER, "", "Level", 0, 127, 100, 14, 1, LEVEL, 0, 1),
    R(MIX_SYNTH2, 3, KNOB, "", "Pan", 0, 127, 64, 15, 1, PAN, 0, 1),
    R(MIX_SYNTH2, 4, KNOB, "", "Delay", 0, 127, 0, 16, 1, SEND, 0, 1),
    R(MIX_SYNTH2, 5, SWITCH, "", "Dist", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_DIST, 1),
    R(MIX_SYNTH2, 6, SWITCH, "", "PCF", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_PCF, 1),
    R(MIX_SYNTH2, 7, SWITCH, "", "Comp", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_COMP, 1),
    R(MIX_808, 0, SWITCH, "", "On/Off", 0, 1, 1, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(MIX_808, 1, METER, "", "Meter", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(MIX_808, 2, FADER, "", "Level", 0, 127, 100, 17, 1, LEVEL, 0, 2),
    R(MIX_808, 3, KNOB, "", "Pan", 0, 127, 64, 18, 1, PAN, 0, 2),
    R(MIX_808, 4, KNOB, "", "Delay", 0, 127, 0, 19, 1, SEND, 0, 2),
    R(MIX_808, 5, SWITCH, "", "Dist", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_DIST, 2),
    R(MIX_808, 6, SWITCH, "", "PCF", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_PCF, 2),
    R(MIX_808, 7, SWITCH, "", "Comp", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_COMP, 2),
    R(MIX_909, 0, SWITCH, "", "On/Off", 0, 1, 1, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(MIX_909, 1, METER, "", "Meter", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(MIX_909, 2, FADER, "", "Level", 0, 127, 100, 20, 1, LEVEL, 0, 3),
    R(MIX_909, 3, KNOB, "", "Pan", 0, 127, 64, 21, 1, PAN, 0, 3),
    R(MIX_909, 4, KNOB, "", "Delay", 0, 127, 0, 22, 1, SEND, 0, 3),
    R(MIX_909, 5, SWITCH, "", "Dist", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_DIST, 3),
    R(MIX_909, 6, SWITCH, "", "PCF", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_PCF, 3),
    R(MIX_909, 7, SWITCH, "", "Comp", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_COMP, 3),
    R(MIX_LEVI, 0, SWITCH, "", "On/Off", 0, 1, 1, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(MIX_LEVI, 1, METER, "", "Meter", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(MIX_LEVI, 2, FADER, "", "Level", 0, 127, 100, 109, 1, LEVEL, 0, 4),
    R(MIX_LEVI, 3, KNOB, "", "Pan", 0, 127, 64, 110, 1, PAN, 0, 4),
    R(MIX_LEVI, 4, KNOB, "", "Delay", 0, 127, 0, 111, 1, SEND, 0, 4),
    R(MIX_LEVI, 5, SWITCH, "", "Dist", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_DIST, 4),
    R(MIX_LEVI, 6, SWITCH, "", "PCF", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_PCF, 4),
    R(MIX_LEVI, 7, SWITCH, "", "Comp", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_COMP, 4),
    R(MASTER, 0, FADER, "", "Level", 0, 127, 100, 7, 1, LEVEL, 0, RI_ROUTE_MASTER),
    R(MASTER, 1, METER, "", "Meter L", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(MASTER, 2, METER, "", "Meter R", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(MASTER, 3, SWITCH, "", "Comp", 0, 1, 0, RI_MIDI_CC_NONE, 1, INSERT, RI_ROUTE_COMP, RI_ROUTE_MASTER),
    R(PCF, 0, SWITCH, "", "On/Off", 0, 1, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PCF, 1, METER, "", "Meter", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(PCF, 2, SELECTOR, "", "Pattern", 0, 53, 0, 94, 1, FX, RI_FXID_PCF_PATTERN, 0),
    R(PCF, 3, SWITCH, "", "Mode", 0, 1, 0, 95, 1, FX, RI_FXID_PCF_MODE, 0),
    R(PCF, 4, FADER, "", "Freq", 0, 127, 64, 96, 1, FX, RI_FXID_PCF_BASE, 0),
    R(PCF, 5, FADER, "", "Q", 0, 127, 64, 97, 1, FX, RI_FXID_PCF_Q, 0),
    R(PCF, 6, FADER, "", "Amt", 0, 127, 0, 98, 1, FX, RI_FXID_PCF_AMT, 0),
    R(PCF, 7, FADER, "", "Decay", 0, 127, 64, 99, 1, FX, RI_FXID_PCF_DECAY, 0),
    R(DELAY, 0, SWITCH, "", "On/Off", 0, 1, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(DELAY, 1, METER, "", "Meter", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(DELAY, 2, SELECTOR, "", "Steps", 1, 32, 3, 100, 1, FX, RI_FXID_DELAY_STEPS, 0),
    R(DELAY, 3, SWITCH, "", "Triplet", 0, 1, 0, 101, 1, FX, RI_FXID_DELAY_TRIPLET, 0),
    R(DELAY, 4, KNOB, "", "Pan", 0, 127, 64, 102, 1, FX, RI_FXID_DELAY_RETPAN, 0),
    R(DELAY, 5, KNOB, "", "F.Back", 0, 127, 48, 103, 1, FX, RI_FXID_DELAY_FB, 0),
    R(DIST, 0, SWITCH, "", "On/Off", 0, 1, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(DIST, 1, METER, "", "Meter", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(DIST, 2, KNOB, "", "Amount", 0, 127, 64, 105, 1, FX, RI_FXID_DIST_DRIVE, 0),
    R(DIST, 3, KNOB, "", "Shape", 0, 127, 64, 104, 1, FX, RI_FXID_DIST_SHAPE, 0),
    R(COMP, 0, SWITCH, "", "On/Off", 0, 1, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(COMP, 1, METER, "", "Meter", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(COMP, 2, KNOB, "", "Ratio", 0, 127, 64, 106, 1, FX, RI_FXID_COMP_RATIO, 0),
    R(COMP, 3, KNOB, "", "Threshold", 0, 127, 64, 107, 1, FX, RI_FXID_COMP_THRESH, 0),
    R(COMP, 4, METER, "", "Level Reduction", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(TRANSPORT, 0, SWITCH, "", "Song/Pattern", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(TRANSPORT, 1, DISPLAY, "", "Tempo", 20, 500, 120, RI_MIDI_CC_NONE, 0, TEMPO, 0, 0),
    R(TRANSPORT, 2, KNOB, "", "Shuffle", 0, 127, 0, 108, 0, NONE, 0, 0),
    R(TRANSPORT, 3, DISPLAY, "", "Bar", 1, 999, 1, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(TRANSPORT, 4, BUTTON, "", "Play", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(TRANSPORT, 5, BUTTON, "", "Stop", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(TRANSPORT, 6, BUTTON, "", "Rewind", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(TRANSPORT, 7, BUTTON, "", "Fast Forward", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(TRANSPORT, 8, BUTTON, "", "Record", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(TRANSPORT, 9, SWITCH, "", "Loop", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(TRANSPORT, 10, DISPLAY, "", "Loop Start", 1, 999, 1, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(TRANSPORT, 11, DISPLAY, "", "Loop Length", 1, 999, 4, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(TRANSPORT, 12, LED, "", "MIDI In", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(TRANSPORT, 13, LED, "", "Sync", 0, 2, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(TRANSPORT, 14, BUTTON, "", "Tap", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(PAT_SYNTH1, 0, SWITCH, "", "Section Off", 0, 1, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_SYNTH1, 1, SELECTOR, "", "Bank", 0, 3, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_SYNTH1, 2, SELECTOR, "", "Pattern", 0, 7, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_SYNTH1, 3, DISPLAY, "", "Length", 1, 16, 16, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(PAT_SYNTH1, 4, SWITCH, "", "Shuffle", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(PAT_SYNTH2, 0, SWITCH, "", "Section Off", 0, 1, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_SYNTH2, 1, SELECTOR, "", "Bank", 0, 3, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_SYNTH2, 2, SELECTOR, "", "Pattern", 0, 7, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_SYNTH2, 3, DISPLAY, "", "Length", 1, 16, 16, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(PAT_SYNTH2, 4, SWITCH, "", "Shuffle", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(PAT_808, 0, SWITCH, "", "Section Off", 0, 1, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_808, 1, SELECTOR, "", "Bank", 0, 3, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_808, 2, SELECTOR, "", "Pattern", 0, 7, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_808, 3, DISPLAY, "", "Length", 1, 16, 16, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(PAT_808, 4, SWITCH, "", "Shuffle", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(PAT_909, 0, SWITCH, "", "Section Off", 0, 1, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_909, 1, SELECTOR, "", "Bank", 0, 3, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_909, 2, SELECTOR, "", "Pattern", 0, 7, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_909, 3, DISPLAY, "", "Length", 1, 16, 16, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(PAT_909, 4, SWITCH, "", "Shuffle", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 0, KNOB, "", "Cutoff", 0, 127, 96, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_CUTOFF, 0),
    R(LEVI, 1, KNOB, "", "Reso", 0, 127, 32, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RESO, 0),
    R(LEVI, 2, SWITCH, "", "Mode", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MODE, 0),
    R(LEVI, 3, KNOB, "", "Ratio", 0, 127, 32, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RATIO, 0),
    R(LEVI, 37, SELECTOR, "Algo", "Algorithm", 0, 63, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ALGO, 0),
    R(LEVI, 38, SELECTOR, "Algo", "Target", 0, 63, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ALGOB, 0),
    R(LEVI, 39, KNOB, "Algo", "Morph", 0, 100, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MORPH, 0),
    R(LEVI, 40, SELECTOR, "Algo", "Op", 0, 7, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 41, SELECTOR, "Algo", "Op Mode", 0, 6, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_OPMODE, 0),
    R(LEVI, 42, SELECTOR, "Filter", "Type", 0, 3, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_FTYPE, 0),
    R(LEVI, 43, KNOB, "Filter", "Drive", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DRIVE, 0),
    R(LEVI, 44, KNOB, "Analog", "Cutoff", 0, 127, 96, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_CUTOFF2, 0),
    R(LEVI, 45, KNOB, "Analog", "Reso", 0, 127, 32, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RESO2, 0),
    R(LEVI, 46, KNOB, "Env", "Attack", 0, 127, 27, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ATTACK, 0),
    R(LEVI, 47, KNOB, "Env", "Decay", 0, 127, 95, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DECAY, 0),
    R(LEVI, 48, KNOB, "Env", "Sustain", 0, 127, 102, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SUSTAIN, 0),
    R(LEVI, 49, KNOB, "Env", "Release", 0, 127, 84, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RELEASE, 0),
    R(LEVI, 50, SWITCH, "Env", "Loop", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_LOOP, 0),
    R(LEVI, 51, DISPLAY, "Algo", "Algo Num", 0, 64, 1, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 52, SWITCH, "Arp", "On", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPON, 0),
    R(LEVI, 53, KNOB, "Arp", "Rate", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPRATE, 0),
    R(LEVI, 54, SWITCH, "Seq", "On", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQON, 0),
    R(LEVI, 55, KNOB, "Seq", "Len", 1, 16, 16, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQLEN, 0),
    R(LEVI, 56, SWITCH, "Matrix", "R1", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ROUTE0, 0),
    R(LEVI, 57, SWITCH, "Matrix", "R2", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ROUTE1, 0),
    R(LEVI, 58, SWITCH, "Matrix", "R3", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ROUTE2, 0),
    R(LEVI, 59, SWITCH, "Matrix", "R4", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ROUTE3, 0),
    R(LEVI, 60, SWITCH, "Matrix", "R5", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ROUTE4, 0),
    R(LEVI, 61, SWITCH, "Matrix", "R6", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ROUTE5, 0),
    R(LEVI, 62, SWITCH, "Matrix", "R7", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ROUTE6, 0),
    R(LEVI, 63, SWITCH, "Matrix", "R8", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ROUTE7, 0),
    R(LEVI, 64, SWITCH, "Fx", "Pre", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_PREBYPASS, 0),
    R(LEVI, 65, SWITCH, "Fx", "Dly", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DBYPASS, 0),
    R(LEVI, 66, SWITCH, "Fx", "Rev", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RBYPASS, 0),
    R(LEVI, 67, SWITCH, "Fx", "Post", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_POSTBYPASS, 0),
    /* Hardware page UI (fidelity plan P1, 2026-09-30): module select, the
     * 8 MASTER CONTROL encoders (they send their page target) and the
     * display. UI-only rows: never automated themselves. */
    R(LEVI, 68, SELECTOR, "Module", "Module", 0, 35, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 69, KNOB, "Master", "Control 1", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 70, KNOB, "Master", "Control 2", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 71, KNOB, "Master", "Control 3", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 72, KNOB, "Master", "Control 4", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 73, KNOB, "Master", "Control 5", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 74, KNOB, "Master", "Control 6", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 75, KNOB, "Master", "Control 7", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 76, KNOB, "Master", "Control 8", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 77, DISPLAY, "Master", "Display", 0, 34, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    /* Osc Env Level & Bias knobs (fidelity P2, manual p. 54): 64 = none. */
    R(LEVI, 78, KNOB, "Bias", "Env Level", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_BIAS_ENVL, 0),
    R(LEVI, 79, KNOB, "Bias", "Attack", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_BIAS_ATK, 0),
    R(LEVI, 80, KNOB, "Bias", "Decay", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_BIAS_DEC, 0),
    R(LEVI, 81, KNOB, "Bias", "Release", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_BIAS_REL, 0),
    R(LEVI, 82, BUTTON, "Master", "Page Up", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 83, BUTTON, "Master", "Page Down", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    /* Algorithm modes, morph list, solo/mute (fidelity P3, pp. 58-61). */
    R(LEVI, 84, SELECTOR, "Algo", "Algo Mode", 0, 2, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_AMODE, 0),
    R(LEVI, 85, KNOB, "Algo", "Slot 1", 0, 63, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SLOT0 + 0u, 0),
    R(LEVI, 86, KNOB, "Algo", "Slot 2", 0, 65, 65, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SLOT0 + 1u, 0),
    R(LEVI, 87, KNOB, "Algo", "Slot 3", 0, 65, 65, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SLOT0 + 2u, 0),
    R(LEVI, 88, KNOB, "Algo", "Slot 4", 0, 65, 65, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SLOT0 + 3u, 0),
    R(LEVI, 89, KNOB, "Algo", "Slot 5", 0, 65, 65, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SLOT0 + 4u, 0),
    R(LEVI, 90, KNOB, "Algo", "Slot 6", 0, 65, 65, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SLOT0 + 5u, 0),
    R(LEVI, 91, KNOB, "Algo", "Slot 7", 0, 65, 65, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SLOT0 + 6u, 0),
    R(LEVI, 92, KNOB, "Algo", "Slot 8", 0, 65, 65, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SLOT0 + 7u, 0),
    R(LEVI, 93, KNOB, "Algo", "Morph Pos", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MPOS, 0),
    R(LEVI, 94, KNOB, "Algo", "Solo", 0, 8, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SOLO, 0),
    R(LEVI, 95, KNOB, "Algo", "Mute 1-7", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MUTELO, 0),
    R(LEVI, 96, SWITCH, "Algo", "Mute 8", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MUTEHI, 0),
    /* Filters + VCA (fidelity P4, pp. 62-70). Amounts/keytrack 64 = 0,
     * levels 64 = unity; digital keytrack 0 %, analog 100 %. */
    R(LEVI, 97, SELECTOR, "Filter", "D Type", 0, 17, 14, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DTYPE, 0),
    R(LEVI, 98, KNOB, "Filter", "Morph/Drive", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DMORPH, 0),
    R(LEVI, 99, SWITCH, "Filter", "Drive Post", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DPOST, 0),
    R(LEVI, 100, SELECTOR, "Filter", "Vowel Order", 0, 7, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VORDER, 0),
    R(LEVI, 101, KNOB, "Filter", "D Keytrack", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DKEYTRK, 0),
    R(LEVI, 102, KNOB, "Filter", "LFO1 Amt", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DLFO1, 0),
    R(LEVI, 103, KNOB, "Filter", "D Level", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DLEVEL, 0),
    R(LEVI, 104, KNOB, "Analog", "A Keytrack", 0, 127, 96, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_AKEYTRK, 0),
    R(LEVI, 105, KNOB, "Analog", "LFO2 Amt", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ALFO2, 0),
    R(LEVI, 106, KNOB, "VCA", "OSCs Level", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_OSCLVL, 0),
    R(LEVI, 107, KNOB, "VCA", "VCA Level", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VCALVL, 0),
    R(LEVI, 108, KNOB, "VCA", "Patch Level", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_PATCHLVL, 0),
    R(LEVI, 109, KNOB, "VCA", "LFO3 Amt", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VLFO3, 0),
    /* Pre-wired envelope amounts + VCA initial level (fidelity P5). */
    R(LEVI, 110, KNOB, "Filter", "ENV1 Amt", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DENV1, 0),
    R(LEVI, 111, KNOB, "Analog", "ENV2 Amt", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_AENV2, 0),
    R(LEVI, 112, KNOB, "VCA", "Init Level", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VINIT, 0),
    /* Macro knobs + buttons (fidelity P5b, pp. 120-123). */
    R(LEVI, 113, KNOB, "Macro", "Macro 1", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MKNOB0 + 0u, 0),
    R(LEVI, 114, KNOB, "Macro", "Macro 2", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MKNOB0 + 1u, 0),
    R(LEVI, 115, KNOB, "Macro", "Macro 3", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MKNOB0 + 2u, 0),
    R(LEVI, 116, KNOB, "Macro", "Macro 4", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MKNOB0 + 3u, 0),
    R(LEVI, 117, KNOB, "Macro", "Macro 5", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MKNOB0 + 4u, 0),
    R(LEVI, 118, KNOB, "Macro", "Macro 6", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MKNOB0 + 5u, 0),
    R(LEVI, 119, KNOB, "Macro", "Macro 7", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MKNOB0 + 6u, 0),
    R(LEVI, 120, KNOB, "Macro", "Macro 8", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MKNOB0 + 7u, 0),
    R(LEVI, 121, SWITCH, "Macro", "Button 1", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MBTN0 + 0u, 0),
    R(LEVI, 122, SWITCH, "Macro", "Button 2", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MBTN0 + 1u, 0),
    R(LEVI, 123, SWITCH, "Macro", "Button 3", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MBTN0 + 2u, 0),
    R(LEVI, 124, SWITCH, "Macro", "Button 4", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MBTN0 + 3u, 0),
    R(LEVI, 125, SWITCH, "Macro", "Button 5", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MBTN0 + 4u, 0),
    R(LEVI, 126, SWITCH, "Macro", "Button 6", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MBTN0 + 5u, 0),
    R(LEVI, 127, SWITCH, "Macro", "Button 7", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MBTN0 + 6u, 0),
    R(LEVI, 128, SWITCH, "Macro", "Button 8", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_MBTN0 + 7u, 0),
    /* Voice allocator (fidelity P6a, manual pp. 87-96). */
    R(LEVI, 129, SELECTOR, "Voice", "Polyphony", 0, 8, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_POLYMODE, 0),
    R(LEVI, 130, KNOB, "Voice", "Density", 0, 127, 127, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_UDENSITY, 0),
    R(LEVI, 131, KNOB, "Voice", "Poly Limit", 0, 127, 127, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ULIMIT, 0),
    /* Voice params (fidelity P6b, manual pp. 87-96). */
    R(LEVI, 132, KNOB, "Voice", "Detune", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VDETUNE, 0),
    R(LEVI, 133, KNOB, "Voice", "Analog Feel", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VAFEEL, 0),
    R(LEVI, 134, KNOB, "Voice", "Random Phase", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VRNDPH, 0),
    R(LEVI, 135, KNOB, "Voice", "Pan", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VPAN, 0),
    R(LEVI, 136, KNOB, "Voice", "Width", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VWIDTH, 0),
    R(LEVI, 137, SELECTOR, "Voice", "Pan Mode", 0, 2, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VPANMODE, 0),
    R(LEVI, 138, KNOB, "Voice", "Bend Range", 0, 127, 11, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VBENDRNG, 0),
    R(LEVI, 139, KNOB, "Voice", "Vibrato Rate", 0, 127, 94, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VVIBRATE, 0),
    R(LEVI, 140, KNOB, "Voice", "Vibrato Amt", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VVIBAMT, 0),
    R(LEVI, 141, KNOB, "Voice", "Vibrato Delay", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VVIBDLY, 0),
    R(LEVI, 142, SELECTOR, "Voice", "Glide", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VGLIDE, 0),
    R(LEVI, 143, KNOB, "Voice", "Glide Time", 0, 127, 57, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VGLTIME, 0),
    R(LEVI, 144, KNOB, "Voice", "Glide Curve", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VGLCURVE, 0),
    /* Stereo + scales (fidelity P6c, manual pp. 87-96). */
    R(LEVI, 145, KNOB, "Voice", "Vintage", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VINTAGE, 0),
    R(LEVI, 146, SELECTOR, "Voice", "Scale", 0, 15, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VSCALE, 0),
    R(LEVI, 147, SELECTOR, "Voice", "Microtune", 0, 7, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VMICRO, 0),
    R(LEVI, 148, SWITCH, "Voice", "Key Lock", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VKEYLOCK, 0),
    R(LEVI, 149, KNOB, "Voice", "Spread", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VSPREAD, 0),
    R(LEVI, 150, KNOB, "Voice", "OscPan 1", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VOSCPAN1 + 0u, 0),
    R(LEVI, 151, KNOB, "Voice", "OscPan 2", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VOSCPAN1 + 1u, 0),
    R(LEVI, 152, KNOB, "Voice", "OscPan 3", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VOSCPAN1 + 2u, 0),
    R(LEVI, 153, KNOB, "Voice", "OscPan 4", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VOSCPAN1 + 3u, 0),
    R(LEVI, 154, KNOB, "Voice", "OscPan 5", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VOSCPAN1 + 4u, 0),
    R(LEVI, 155, KNOB, "Voice", "OscPan 6", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VOSCPAN1 + 5u, 0),
    R(LEVI, 156, KNOB, "Voice", "OscPan 7", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VOSCPAN1 + 6u, 0),
    R(LEVI, 157, KNOB, "Voice", "OscPan 8", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VOSCPAN1 + 7u, 0),
    /* Delay (fidelity P7a, manual pp. 83-86). */
    R(LEVI, 158, SELECTOR, "Delay", "Type", 0, 3, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DLYTYPE, 0),
    R(LEVI, 159, KNOB, "Delay", "Time", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DLYTIME, 0),
    R(LEVI, 160, KNOB, "Delay", "Feedback", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DLYFB, 0),
    R(LEVI, 161, KNOB, "Delay", "Wet Tone", 0, 127, 127, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DLYWTONE, 0),
    R(LEVI, 162, KNOB, "Delay", "FB Tone", 0, 127, 127, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DLYFBTONE, 0),
    R(LEVI, 163, KNOB, "Delay", "Dry/Wet", 0, 127, 32, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DLYDRYWET, 0),
    R(LEVI, 164, SWITCH, "Delay", "BPM Sync", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DLYBPM, 0),
    /* Reverb (fidelity P7b, manual pp. 83-86). */
    R(LEVI, 165, SELECTOR, "Reverb", "Type", 0, 3, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RTYPE, 0),
    R(LEVI, 166, KNOB, "Reverb", "Pre-Dly", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RPREDLY, 0),
    R(LEVI, 167, KNOB, "Reverb", "Time", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RTIME, 0),
    R(LEVI, 168, KNOB, "Reverb", "Tone", 0, 127, 127, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RTONE, 0),
    R(LEVI, 169, KNOB, "Reverb", "Hi Damp", 0, 127, 127, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RHIDAMP, 0),
    R(LEVI, 170, KNOB, "Reverb", "Lo Damp", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RLODAMP, 0),
    R(LEVI, 171, KNOB, "Reverb", "Dry/Wet", 0, 127, 32, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RDRYWET, 0),
    R(LEVI, 172, SWITCH, "Reverb", "Freeze", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RFREEZE, 0),
    /* Mod FX pre/post (fidelity P7c, manual pp. 83-86). */
    R(LEVI, 173, SELECTOR, "PreFx", "Type", 0, 8, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_PTYPE, 0),
    R(LEVI, 174, SELECTOR, "PreFx", "Preset", 0, 3, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_PPRESET, 0),
    R(LEVI, 175, KNOB, "PreFx", "Param 1", 0, 127, 32, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_PP1, 0),
    R(LEVI, 176, KNOB, "PreFx", "Param 2", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_PP2, 0),
    R(LEVI, 177, KNOB, "PreFx", "Dry/Wet", 0, 127, 32, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_PDRYWET, 0),
    R(LEVI, 178, SELECTOR, "PostFx", "Type", 0, 8, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_OTYPE, 0),
    R(LEVI, 179, SELECTOR, "PostFx", "Preset", 0, 3, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_OPRESET, 0),
    R(LEVI, 180, KNOB, "PostFx", "Param 1", 0, 127, 32, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_OP1, 0),
    R(LEVI, 181, KNOB, "PostFx", "Param 2", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_OP2, 0),
    R(LEVI, 182, KNOB, "PostFx", "Dry/Wet", 0, 127, 32, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ODRYWET, 0),
    /* Device arp (fidelity P8b, manual pp. 99-104). */
    R(LEVI, 183, SELECTOR, "Arp", "Oct Mode", 0, 2, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPOCTMODE, 0),
    R(LEVI, 184, KNOB, "Arp", "Oct Range", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPOCTRANGE, 0),
    R(LEVI, 185, KNOB, "Arp", "Gate", 0, 127, 127, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPGATE, 0),
    R(LEVI, 186, SELECTOR, "Arp", "Mode", 0, 8, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPMODE, 0),
    R(LEVI, 187, KNOB, "Arp", "Length", 0, 127, 127, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPLEN, 0),
    R(LEVI, 188, SELECTOR, "Arp", "Phrase", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPPHRASE, 0),
    R(LEVI, 189, KNOB, "Arp", "Entropy", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPENTROPY, 0),
    R(LEVI, 190, KNOB, "Arp", "Swing", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPSWING, 0),
    R(LEVI, 191, KNOB, "Arp", "Ratchet", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPRATCHET, 0),
    R(LEVI, 192, KNOB, "Arp", "Chance", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPCHANCE, 0),
    R(LEVI, 193, SWITCH, "Arp", "Latch", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPLATCH, 0),
    R(LEVI, 194, SWITCH, "Arp", "ClockLock", 0, 1, 1, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPCLOCK, 0),
    R(LEVI, 195, KNOB, "Arp", "Step Off", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPSTEPPOFF, 0),
    /* Device sequencer (fidelity P8c, manual pp. 105-119). */
    R(LEVI, 196, SELECTOR, "Seq", "Rate", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQRATE, 0),
    R(LEVI, 197, SELECTOR, "Seq", "Mode", 0, 2, 1, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQMODE, 0),
    R(LEVI, 198, KNOB, "Seq", "Swing", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQSWING, 0),
    R(LEVI, 199, KNOB, "Seq", "Gate", 0, 127, 127, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQGATE, 0),
    R(LEVI, 200, KNOB, "Seq", "Prob", 0, 127, 127, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQPROB, 0),
    R(LEVI, 201, KNOB, "Seq", "Drift", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQDRIFT, 0),
    R(LEVI, 202, KNOB, "Seq", "Transpose", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQTRANSP, 0),
    R(LEVI, 203, KNOB, "Seq", "Trk Len", 0, 127, 127, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQTRKLEN, 0),
    R(LEVI, 204, SWITCH, "Seq", "Rec", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQREC, 0),
    R(LEVI, 205, KNOB, "Seq", "Step", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQSTEP, 0),
    R(LEVI, 206, SWITCH, "Seq", "Clear", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQCLEAR, 0),
    R(LEVI, 207, KNOB, "Seq", "St Trig", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQSTRIG, 0),
    R(LEVI, 208, KNOB, "Seq", "St Prob", 0, 127, 127, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQSPROB, 0),
    R(LEVI, 209, KNOB, "Seq", "St Drift", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQSDRIFT, 0),
    R(LEVI, 210, KNOB, "Seq", "St Entr", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_SEQSENTR, 0),
    /* Ribbon (fidelity P8d, manual pp. 97-98). */
    R(LEVI, 211, SELECTOR, "Ribbon", "Mode", 0, 3, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RBNMODE, 0),
    R(LEVI, 212, KNOB, "Ribbon", "Pos", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RBNPOS, 0),
    R(LEVI, 213, SWITCH, "Ribbon", "Touch", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_RBNTOUCH, 0),
    /* Performance amounts (fidelity P9b): the VEL>ENV / POLYAT slots P5
     * drew dim on the two filter pages and the VCA page. */
    R(LEVI, 215, KNOB, "Filter", "D Vel", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DVEL, 0),
    R(LEVI, 216, KNOB, "Filter", "D Polyat", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_DPAT, 0),
    R(LEVI, 217, KNOB, "Analog", "A Vel", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_AVEL, 0),
    R(LEVI, 218, KNOB, "Analog", "A Polyat", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_APAT, 0),
    R(LEVI, 219, KNOB, "VCA", "V Vel", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VVEL, 0),
    R(LEVI, 220, KNOB, "VCA", "V Polyat", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_VPAT, 0),
    /* Keyboard zones (fidelity P9c): the PERFORMANCE page. Device-wide,
     * so the section-wide 0x0E loop applies them (the setter is
     * idempotent per voice). The split key has no row yet: it arrives
     * with the zone patches (P10). */
    R(LEVI, 221, KNOB, "Performance", "Octave", 0, 4, 2, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_PFOCT, 0),
    R(LEVI, 222, SELECTOR, "Performance", "Mode", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_PFMODE, 0),
    R(LEVI, 223, SELECTOR, "Performance", "Select", 0, 2, 2, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_PFSEL, 0),
    R(LEVI, 224, SELECTOR, "Performance", "Split", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_PFSPLIT, 0),
    R(LEVI, 225, KNOB, "Performance", "Balance", 0, 127, 64, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_PFBAL, 0),
    /* Performance buttons (fidelity P9d): the glide hold and chord mode. */
    R(LEVI, 226, SWITCH, "Performance", "Glide", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_GLIDE, 0),
    R(LEVI, 227, SWITCH, "Performance", "Chord", 0, 1, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_CHORD, 0),
    /* LFO step editor gate (fidelity P8e): panel-only page switch, the
     * editor slots themselves carry RI_LEVI_LSKEYs. */
    R(LEVI, 214, SWITCH, "Lfo", "Step", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 4, SELECTOR, "", "Lane", 0, 5, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 5, BUTTON, "", "Step", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 6, BUTTON, "", "Back", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 7, DISPLAY, "", "Step display", 1, 16, 1, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 8, STEP, "Steps", "Step 1", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 9, STEP, "Steps", "Step 2", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 10, STEP, "Steps", "Step 3", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 11, STEP, "Steps", "Step 4", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 12, STEP, "Steps", "Step 5", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 13, STEP, "Steps", "Step 6", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 14, STEP, "Steps", "Step 7", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 15, STEP, "Steps", "Step 8", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 16, STEP, "Steps", "Step 9", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 17, STEP, "Steps", "Step 10", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 18, STEP, "Steps", "Step 11", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 19, STEP, "Steps", "Step 12", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 20, STEP, "Steps", "Step 13", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 21, STEP, "Steps", "Step 14", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 22, STEP, "Steps", "Step 15", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 23, STEP, "Steps", "Step 16", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 24, BUTTON, "Pitch", "C", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 25, BUTTON, "Pitch", "C#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 26, BUTTON, "Pitch", "D", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 27, BUTTON, "Pitch", "D#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 28, BUTTON, "Pitch", "E", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 29, BUTTON, "Pitch", "F", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 30, BUTTON, "Pitch", "F#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 31, BUTTON, "Pitch", "G", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 32, BUTTON, "Pitch", "G#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 33, BUTTON, "Pitch", "A", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 34, BUTTON, "Pitch", "A#", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 35, BUTTON, "Pitch", "B", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(LEVI, 36, BUTTON, "Pitch", "C+", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(PAT_LEVI, 0, SWITCH, "", "Section Off", 0, 1, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_LEVI, 1, SELECTOR, "", "Bank", 0, 3, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_LEVI, 2, SELECTOR, "", "Pattern", 0, 7, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0),
    R(PAT_LEVI, 3, DISPLAY, "", "Length", 1, 16, 16, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
    R(PAT_LEVI, 4, SWITCH, "", "Shuffle", 0, 1, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0),
};

#define RI_CTLREG_N (uint32_t)(sizeof(RI_CTLREG) / sizeof(RI_CTLREG[0]))

static const char *const RI_SEC_NAMES[RI_SEC_COUNT] = {
    "Synth 1", "Synth 2", "808", "909",
    "Mixer Synth 1", "Mixer Synth 2", "Mixer 808", "Mixer 909",
    "Master", "PCF", "Delay", "Dist", "Comp", "Transport",
    "Pattern Synth 1", "Pattern Synth 2", "Pattern 808", "Pattern 909",
    "Levi", "Pattern Levi", "Mixer Levi"
};

uint32_t ri_ctlreg_count(void) {
    return RI_CTLREG_N;
}

const struct RICtlDef *ri_ctlreg_at(uint32_t i) {
    return i < RI_CTLREG_N ? &RI_CTLREG[i] : 0;
}

/* Per-section index range into RI_CTLREG, and why the lookup is a bounded scan
 * rather than a binary search (2026-10-05).
 *
 * ri_ctlreg_find used to scan all RI_CTLREG_N (485) entries, so a repaint paid a
 * cost proportional to where a control sat in the table. Measured: the transport's
 * 31 items resolve at a mean depth of 224, i.e. 6949 comparisons per repaint, and
 * timed inside one process that scan is 45 % of the section's disjoint-clip build
 * (ratio 0.4448-0.4527 over three runs).
 *
 * THE TABLE IS GROUPED BY SECTION, and every section's entries are ADJACENT --
 * t170 verifies exactly that, for all 21 sections, against the table itself. So
 * the scan can be bounded to the section's own run: the transport's is 15 entries
 * rather than 485, and a lookup of an id in that section now costs at most 15
 * comparisons instead of a mean of 224.
 *
 * WHY NOT A BINARY SEARCH, since a bounded scan is already most of the win. Two
 * attempts were installed and both failed, and the reason is worth writing down:
 * **the ids do not ascend in reg_id, at either level.** Across the whole table they
 * invert at section boundaries, and INSIDE a section they invert too -- LEVI's run
 * is ordered by sub-panel, so table[445] is reg_id 4835 and table[446] is 4822. So
 * a binary search over the table, or over a section's range, returns NULL for ids
 * that exist. A bounded linear scan needs no ordering assumption at all, which is
 * why it is what ships.
 *
 * The bounds are literals, with no run-time state and nothing to initialise, and
 * they are VERIFIED AGAINST THE TABLE by t170_ctlreg_index along with exhaustive
 * equivalence to a full linear scan over all 65536 possible reg_ids. That matters:
 * this session twice believed a measurement that reported a property the code did
 * not have -- a geometry-shape enum written from memory, and a probe that compared
 * a reg_id against an index -- and both looked like clean answers. */
static const uint16_t RI_CTLREG_SEC_LO[RI_SEC_COUNT] = {0, 30, 60, 104, 150, 158, 166, 174, 190, 194, 202, 208, 212, 217, 232, 237, 242, 247, 252, 480, 182};
static const uint16_t RI_CTLREG_SEC_HI[RI_SEC_COUNT] = {29, 59, 103, 149, 157, 165, 173, 181, 193, 201, 207, 211, 216, 231, 236, 241, 246, 251, 479, 484, 189};

const struct RICtlDef *ri_ctlreg_find(uint16_t reg_id) {
    uint32_t sec = (uint32_t)(reg_id >> 8), i, lo, hi;
    if (sec >= RI_SEC_COUNT)
        return 0;
    lo = RI_CTLREG_SEC_LO[sec];
    hi = RI_CTLREG_SEC_HI[sec];
    for (i = lo; i <= hi; i++)
        if (RI_CTLREG[i].reg_id == reg_id)
            return &RI_CTLREG[i];
    return 0;
}

const struct RICtlDef *ri_ctlreg_by_cc(uint8_t cc) {
    uint32_t i;
    if (cc == RI_MIDI_CC_NONE)
        return 0;
    for (i = 0; i < RI_CTLREG_N; i++)
        if (RI_CTLREG[i].midi_cc == cc)
            return &RI_CTLREG[i];
    return 0;
}

const char *ri_ctlreg_section_name(uint32_t section) {
    return section < RI_SEC_COUNT ? RI_SEC_NAMES[section] : "?";
}

uint32_t ri_ctlreg_section_count(uint32_t section, uint32_t *bound) {
    uint32_t i, n = 0, b = 0;
    for (i = 0; i < RI_CTLREG_N; i++) {
        if (RI_CTLREG[i].section != section)
            continue;
        n++;
        if (RI_CTLREG[i].bind != RI_BIND_NONE)
            b++;
    }
    if (bound)
        *bound = b;
    return n;
}

/* Instrument names, Owner's Manual p. 148 (808) and p. 151 (909), in
 * Instrument Selection order. Shared 808 slots name both sounds (p. 149). */
struct RICtlName { const char *abbr, *name; };
static const struct RICtlName RI_NAMES_808[12] = {
    { "AC", "Accent" }, { "BD", "Bass Drum" }, { "SD", "Snare Drum" },
    { "LT", "Low Tom / Low Conga" }, { "MT", "Middle Tom / Middle Conga" },
    { "HT", "High Tom / High Conga" }, { "RS", "Rim Shot / Claves" },
    { "CP", "Hand Claps / Maracas" }, { "CB", "Cowbell" }, { "CY", "Cymbal" },
    { "OH", "Open Hi-hat" }, { "CH", "Closed Hi-hat" }
};
static const struct RICtlName RI_NAMES_909[13] = {
    { "AC", "Accent" }, { "BD", "Bass Drum" }, { "SD", "Snare Drum" },
    { "LT", "Low Tom" }, { "MT", "Middle Tom" }, { "HT", "High Tom" },
    { "RS", "Rim Shot" }, { "CP", "Hand Claps" }, { "CH", "Closed Hi-hat" },
    { "OH", "Open Hi-hat" }, { "CC", "Crash Cymbal" }, { "RC", "Ride Cymbal" },
    { "HH", "Hi-hats (CH + OH)" } /* shared Level knob (p. 151), not selectable */
};

static uint32_t help_put(char *buf, uint32_t cap, uint32_t at, const char *t) {
    while (*t && at + 1u < cap)
        buf[at++] = *t++;
    buf[at] = 0;
    return at;
}

uint32_t ri_ctlreg_help(uint16_t reg_id, int opt, char *buf, uint32_t cap) {
    const struct RICtlDef *d = ri_ctlreg_find(reg_id);
    const struct RICtlName *t;
    uint32_t n, k, at;
    if (!buf || !cap)
        return 0;
    buf[0] = 0;
    if (!d || (d->section != RI_SEC_808 && d->section != RI_SEC_909))
        return 0;
    t = d->section == RI_SEC_808 ? RI_NAMES_808 : RI_NAMES_909;
    n = d->section == RI_SEC_808 ? 12u : 13u;
    if (d->kind == RI_CK_SELECTOR && !d->group[0]) { /* Instrument Selection */
        if (opt < 0 || opt > 11)
            return 0;
        at = help_put(buf, cap, 0, t[opt].name);
        at = help_put(buf, cap, at, " (");
        at = help_put(buf, cap, at, t[opt].abbr);
        return help_put(buf, cap, at, ")");
    }
    for (k = 0; k < n; k++)
        if (!strcmp(d->group, t[k].abbr)) {
            at = help_put(buf, cap, 0, t[k].name);
            at = help_put(buf, cap, at, ": ");
            return help_put(buf, cap, at, d->legend);
        }
    return 0;
}

/* Skin-file tokens (format 1). Append-only like the table above: a token
 * once shipped never changes meaning. */
static const char *const RI_SEC_TOKENS[RI_SEC_COUNT] = {
    "303", 0, "808", "909", "mix-303a", "mix-303b", "mix-808", "mix-909", "master",
    "pcf", "delay", "dist", "comp", "transport", "pat-303a", "pat-303b", "pat-808", "pat-909",
    "levi", "pat-levi", "mix-levi"
};
static const char *const RI_CK_TOKENS[9] = {
    "knob", "fader", "switch", "button", "led", "step", "selector", "display", "meter"
};

const char *ri_ctlreg_section_token(uint32_t section) {
    return section < RI_SEC_COUNT ? RI_SEC_TOKENS[section] : 0;
}

int ri_ctlreg_section_by_token(const char *tok) {
    uint32_t i;
    if (!tok)
        return -1;
    for (i = 0; i < RI_SEC_COUNT; i++)
        if (RI_SEC_TOKENS[i] && !strcmp(RI_SEC_TOKENS[i], tok))
            return (int)i;
    return -1;
}

const char *ri_ctlreg_kind_token(uint32_t kind) {
    return kind < 9u ? RI_CK_TOKENS[kind] : 0;
}

int ri_ctlreg_kind_by_token(const char *tok) {
    uint32_t i;
    if (!tok)
        return -1;
    for (i = 0; i < 9u; i++)
        if (!strcmp(RI_CK_TOKENS[i], tok))
            return (int)i;
    return -1;
}

/* Lane key per bind (engine/seq/autolane.h blocks). */
/* Route section -> automation strip: identity except the Levi/master
 * cross (route 4 = Levi strip 5, route 5 = master strip 4). */
static uint16_t auto_strip_of(uint16_t voice) {
    if (voice == 4u)
        return RI_AUTO_STRIP_LEVI;
    if (voice == (uint16_t)RI_ROUTE_MASTER)
        return RI_AUTO_STRIP_MASTER;
    return voice;
}
uint16_t ri_ctlreg_auto_id(const struct RICtlDef *d) {
    if (!d)
        return 0u;
    switch (d->bind) {
    case RI_BIND_303:
    case RI_BIND_FX:
    case RI_BIND_LEVI:
        return d->engine_id;
    case RI_BIND_808V:
        return RI_AUTO_ID_808(d->engine_id, d->voice);
    case RI_BIND_808ALL:
        return RI_AUTO_ID_808(d->engine_id, 0u);
    case RI_BIND_909V:
        return RI_AUTO_ID_909(d->engine_id, d->voice);
    case RI_BIND_909HAT:
        return RI_AUTO_ID_909(RI_CTL_909_LEVEL, RI_AUTO_909_HATPAIR);
    case RI_BIND_LEVEL:
        return RI_AUTO_ID_MIX(auto_strip_of(d->voice), RI_AUTO_MIX_LEVEL);
    case RI_BIND_PAN:
        return RI_AUTO_ID_MIX(auto_strip_of(d->voice), RI_AUTO_MIX_PAN);
    case RI_BIND_SEND:
        return RI_AUTO_ID_MIX(auto_strip_of(d->voice), RI_AUTO_MIX_SEND);
    case RI_BIND_INSERT:
        return RI_AUTO_ID_MIX(d->voice == (uint16_t)RI_ROUTE_MASTER ? RI_AUTO_STRIP_MASTER
            : auto_strip_of(d->voice), RI_AUTO_MIX_DIST + d->engine_id);
    default:
        return 0u;
    }
}
