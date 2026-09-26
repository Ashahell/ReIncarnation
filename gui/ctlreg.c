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
    R(MASTER, 0, FADER, "", "Level", 0, 127, 100, 7, 0, NONE, 0, 0),
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
};

#define RI_CTLREG_N (uint32_t)(sizeof(RI_CTLREG) / sizeof(RI_CTLREG[0]))

static const char *const RI_SEC_NAMES[RI_SEC_COUNT] = {
    "Synth 1", "Synth 2", "808", "909",
    "Mixer Synth 1", "Mixer Synth 2", "Mixer 808", "Mixer 909",
    "Master", "PCF", "Delay", "Dist", "Comp", "Transport",
    "Pattern Synth 1", "Pattern Synth 2", "Pattern 808", "Pattern 909"
};

uint32_t ri_ctlreg_count(void) {
    return RI_CTLREG_N;
}

const struct RICtlDef *ri_ctlreg_at(uint32_t i) {
    return i < RI_CTLREG_N ? &RI_CTLREG[i] : 0;
}

const struct RICtlDef *ri_ctlreg_find(uint16_t reg_id) {
    uint32_t i;
    for (i = 0; i < RI_CTLREG_N; i++)
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
    "pcf", "delay", "dist", "comp", "transport", "pat-303a", "pat-303b", "pat-808", "pat-909"
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
uint16_t ri_ctlreg_auto_id(const struct RICtlDef *d) {
    if (!d)
        return 0u;
    switch (d->bind) {
    case RI_BIND_303:
    case RI_BIND_FX:
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
        return RI_AUTO_ID_MIX(d->voice, RI_AUTO_MIX_LEVEL);
    case RI_BIND_PAN:
        return RI_AUTO_ID_MIX(d->voice, RI_AUTO_MIX_PAN);
    case RI_BIND_SEND:
        return RI_AUTO_ID_MIX(d->voice, RI_AUTO_MIX_SEND);
    case RI_BIND_INSERT:
        return RI_AUTO_ID_MIX(d->voice == (uint16_t)RI_ROUTE_MASTER ? RI_AUTO_STRIP_MASTER : d->voice,
                              RI_AUTO_MIX_DIST + d->engine_id);
    default:
        return 0u;
    }
}
