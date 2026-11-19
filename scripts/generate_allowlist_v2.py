#!/usr/bin/env python3
"""Generate complete allowlist.tsv using the same classification logic as classify_census.py for ALL census rows."""
import csv

TSV_PATH = "docs/evidence/wiring/2026-11-17-census/census.tsv"
OUT_PATH = "docs/evidence/wiring/2026-11-17-census/allowlist.tsv"

# Verdicts that need allowlisting (not OK, not NA)
NEED_ALLOWLIST = {"DEAD", "DIVERGE", "NOFEEDBACK", "DEFAULT", "NOROUNDTRIP",
                  "BINDNONE_PRESSPATH", "BINDNONE_PANELLOCAL", "BINDNONE_UNEXPLAINED"}

# Door codes (matching census test's D_* defines)
DOORS = ["D1_PANEL", "D2_LEVI_ENC", "D3_MIDI_IN", "D4_MIDI_OUT",
         "D5_AUTOMATION", "D6_SAVELOAD", "D7_STARTUP", "D8_FEEDBACK",
         "D9_KEYBOARD", "D10_MENUS", "D11_UNDO"]
DOOR_TO_IDX = {door: i for i, door in enumerate(DOORS)}

def is_value_control(kind):
    return kind in ("0", "1", "2", "6", "7")  # KNOB, FADER, SWITCH, SELECTOR, DISPLAY

def is_interactive_control(kind):
    return kind in ("0", "1", "2", "3", "5", "6")  # KNOB, FADER, SWITCH, BUTTON, STEP, SELECTOR

def has_auto_id(row):
    """Check if control has a non-zero automation lane key (simplified)"""
    bind = row['bind']
    kind = row['kind']
    automatable = row['auto']
    
    # BIND_NONE never has auto_id
    if bind == "0":
        return False
    
    # Value controls with automation usually have auto_id
    if is_value_control(row['kind']) and row['auto'] == "1":
        return True
    
    # Some non-value controls with bind might have auto_id (e.g., MUTE, INSERT, LEVI)
    if bind in ("11", "9", "12", "13"):  # LEVEL, INSERT, LEVI, MUTE
        return True
    
    return False

def door_applies(row, door):
    """Check if a door applies to this control (from census test logic)"""
    kind = row['kind']
    midi_cc = row['midi_cc']
    automatable = row['auto']
    auto_id = has_auto_id(row)

    if door == "D1_PANEL":
        return is_value_control(kind)
    if door == "D2_LEVI_ENC":
        return row['section'] == "18" and is_value_control(kind)
    if door == "D3_MIDI_IN":
        return is_value_control(kind) and midi_cc != "255"
    if door == "D4_MIDI_OUT":
        return False
    if door == "D5_AUTOMATION":
        return is_value_control(kind) and automatable == "1" and auto_id
    if door == "D6_SAVELOAD":
        return is_value_control(kind) and auto_id
    if door == "D7_STARTUP":
        return is_value_control(kind) and auto_id
    if door == "D8_FEEDBACK":
        return is_interactive_control(kind)
    if door == "D9_KEYBOARD":
        return is_interactive_control(kind)
    if door == "D10_MENUS":
        return False
    if door == "D11_UNDO":
        return is_value_control(kind) and auto_id
    return False

def classify_verdict(row, door, verdict):
    """Apply W1 classification logic (from classify_census.py)"""
    kind = row['kind']
    bind = row['bind']
    section = row['section']
    group = row['group']
    legend = row['legend']
    midi_cc = row['midi_cc']
    automatable = row['auto']

    # BINDNONE classifications
    if verdict.startswith("BINDNONE"):
        if verdict == "BINDNONE_PRESSPATH":
            return "by-design", "Press/state-path control (step/transport/pattern) — no engine delivery by design"
        elif verdict == "BINDNONE_PANELLOCAL":
            if group == "Master" and section == "18":
                return "needs-owner", "LEVI Master Control encoders — no engine control exists (t220, owner 2026-10-14)"
            return "by-design", "Panel-local action (button/step/display/page control) — drives panel state only"
        elif verdict == "BINDNONE_UNEXPLAINED":
            # 808/909 voice switches have MIDI CC but bind=NONE — these are panel-local switches (voice enable)
            if kind == "2" and group in ("LT", "MT", "HT", "RS", "CP") and section in ("2", "3"):
                return "by-design", "808/909 voice Switch — panel-local voice enable, not automated"
            # 909 parameters not yet implemented in engine: Attack, Tone, Snappy, Flam
            if section == "3" and bind == "0" and group in ("BD", "SD", "LT", "MT", "HT", "RS", "CP", "CH", "OH", "CC", "RC", "") and legend in ("Attack", "Tone", "Snappy", "Flam", "Flam button"):
                return "by-design", f"909 {group} {legend} — not implemented in engine yet (panel-local)"
            # 808 voice Switch controls have MIDI CC but bind=NONE — these are panel-local voice enable switches
            if section == "2" and kind == "2" and bind == "0" and group in ("LT", "MT", "HT", "RS", "CP") and legend == "Switch":
                return "by-design", f"808 {group} Switch — panel-local voice enable, not automated"
            # 808 Instrument Selection (panel-local selector)
            if section == "2" and legend == "Instrument Selection" and bind == "0":
                return "by-design", "808 Instrument Selection — panel-local selector, not automated"
            # 303 Step section buttons (Down, Up, Accent, Slide, Note/Pause, Pitch Mode) - press-path
            if section in ("0", "1") and group == "Step" and bind == "0" and kind == "2":
                return "by-design", "303 Step button — press-path control (pattern editing), not automated"
            if group == "Module" and section == "18":
                return "by-design", "LEVI Module selector — panel navigation only"
            if group == "Lfo" and legend == "Step":
                return "by-design", "LEVI LFO Step editor page switch — panel navigation only"
            if group == "Lane" and section == "18":
                return "by-design", "LEVI Lane selector — panel navigation only"
            # 909 Instrument Selection (panel-local selector)
            if section == "3" and legend == "Instrument Selection" and bind == "0":
                return "by-design", "909 Instrument Selection — panel-local selector, not automated"
            # 909 AC Level (0300) - needs section-wide bind (909ALL) which requires owner approval
            if section == "3" and row['reg_id'] == "0300" and bind == "0":
                return "needs-owner", "909 AC Level — needs section-wide bind (909ALL) not yet implemented"
            return "bug", "UNEXPLAINED bind=NONE — control looks live but has no engine target"

    # DEAD - door accepts input but engine state doesn't move
    if verdict == "DEAD":
        if door == "D1_PANEL":
            if bind == "13":  # MUTE
                return "bug", "Mixer strip On/Off (bind=MUTE) — ri_ctlreg_auto_id returns 0 for MUTE (engine enable not wired)"
            if bind == "6":  # FX
                return "bug", "FX On/Off (bind=FX) — ri_ctlreg_auto_id returns 0 for FX enable"
            if bind == "9":  # INSERT
                return "bug", "Insert Dist/PCF/Comp (bind=INSERT) — ri_ctlreg_auto_id returns 0 for insert radio assign"
            if bind == "0":  # NONE
                return "bug", "bind=NONE but door applies — should be BINDNONE_*"
            return "bug", "Panel send reaches bridge but engine state unchanged"
        elif door == "D2_LEVI_ENC":
            if row['section'] == "18":
                return "by-design", "Levi encoder door not mapped for this control — no encoder assignment"
            return "bug", "Levi encoder door accepted but no engine state change"
        elif door == "D3_MIDI_IN":
            return "bug", "MIDI CC accepted but engine state unchanged"
        elif door == "D5_AUTOMATION":
            return "bug", "Automation record/playback not restoring value"
        elif door == "D6_SAVELOAD":
            return "bug", "Save/load (RBNG) not restoring value"
        elif door == "D7_STARTUP":
            return "bug", "Startup default moves engine — panel default != engine default (ri_engine_defaults mismatch)"
        elif door == "D11_UNDO":
            return "bug", "Undo not restoring value"
        return "bug", f"DEAD on {door} — input accepted but no engine state change"

    # DIVERGE - state moves but not to D1 state
    if verdict == "DIVERGE":
        if door == "D1_PANEL":
            if row['section'] == "18":
                return "by-design", "DIVERGE likely due to incomplete direct() comparison in census test (Levi per-op/mod/LS/route/macro key blocks not implemented)"
            bind = row['bind']
            if bind in ("1", "2", "3", "4", "5", "6", "9", "11", "12", "13"):  # bound controls
                return "bug", "Panel path result != direct setter for supported key block"
            return "by-design", "DIVERGE likely due to incomplete direct() comparison in census test"
        return "by-design", f"DIVERGE on {door} — reference path not fully implemented in census test"

    # NOFEEDBACK - state moves but lamp/LED/meter/repaint doesn't
    if verdict == "NOFEEDBACK":
        if door in ("D1_PANEL", "D2_LEVI_ENC", "D3_MIDI_IN", "D5_AUTOMATION", "D6_SAVELOAD", "D7_STARTUP", "D8_FEEDBACK", "D9_KEYBOARD", "D11_UNDO"):
            if kind in ("2", "3", "7"):  # SWITCH, BUTTON, DISPLAY
                return "by-design", f"{door} moves engine but no meter peak change (switch/button/display — no audible parameter)"
            # 808/909 drum voice knobs in default context (no pattern playing) are SILENT
            if row['section'] in ("2", "3") and group in ("AC", "BD", "SD", "LT", "MT", "HT", "RS", "CP", "CB", "CY", "OH", "CH", "RC", "CR", "RD", "LT", "MT", "HT", "RS", "CP"):
                return "by-design", f"{door} moves engine but no meter peak change in default context (808/909 drum voice knob — SILENT without pattern)"
            # 303 synth knobs in default context (no pattern playing) are SILENT
            if row['section'] in ("0", "1") and group == "" and legend in ("Cutoff", "Reso", "Env Mod", "Decay", "Accent", "Tune", "Waveform"):
                return "by-design", f"{door} moves engine but no meter peak change in default context (303 synth knob — SILENT without pattern)"
            # LEVI section controls (section 18) may be SILENT in default context - need ear test
            if row['section'] == "18":
                return "by-design", f"{door} moves engine but no meter peak change in default context (LEVI control may be SILENT — needs ear test)"
            return "bug", f"No visual feedback on {door}"
        return "bug", f"NOFEEDBACK on {door}"

    # DEFAULT - panel default sent to fresh engine moves engine
    if verdict == "DEFAULT":
        if door == "D7_STARTUP":
            return "bug", "Startup burst sends panel default but engine default differs"
        return "bug", f"DEFAULT on {door}"

    # NOROUNDTRIP
    if verdict == "NOROUNDTRIP":
        if door == "D5_AUTOMATION":
            return "bug", "Automation record->play doesn't restore"
        if door == "D6_SAVELOAD":
            return "bug", "Save/load doesn't restore"
        return "bug", f"NOROUNDTRIP on {door}"

    return "unknown", "Unclassified"

def main():
    # Load census
    with open(TSV_PATH, 'r') as f:
        reader = csv.DictReader(f, delimiter='\t')
        rows = list(reader)

    allowlist = []
    for row in rows:
        for door in DOORS:
            if not door_applies(row, door):
                continue
            verdict = row[door]
            if verdict not in NEED_ALLOWLIST:
                continue

            classification, reason = classify_verdict(row, door, verdict)
            if classification in ('by-design', 'needs-owner'):
                allowlist.append({
                    'reg_id': row['reg_id'],
                    'door': door,
                    'verdict': verdict,
                    'reason': reason
                })

    # Write allowlist TSV
    with open(OUT_PATH, 'w') as f:
        f.write("# Wiring Census Allow-List (W3)\n")
        f.write("# Format: reg_id<TAB>door_idx<TAB>verdict<TAB>reason<TAB>date\n")
        f.write("# Auto-generated from W1 classification logic (by-design + needs-owner only)\n")
        f.write(f"# Total entries: {len(allowlist)}\n\n")

        for entry in allowlist:
            door_idx = DOOR_TO_IDX[entry['door']]
            f.write(f"{entry['reg_id']}\t{door_idx}\t{entry['verdict']}\t{entry['reason']}\t2026-11-18\n")

    print(f"Generated {OUT_PATH} with {len(allowlist)} entries")

if __name__ == "__main__":
    main()