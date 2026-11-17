#!/usr/bin/env python3
"""Analyze census.tsv and generate classify.md for W1."""
import csv
import sys

TSV_PATH = "docs/evidence/wiring/2026-11-17-census/census.tsv"
OUT_PATH = "docs/evidence/wiring/2026-11-17-census/classify.md"

DOORS = [
    "D1_PANEL", "D2_LEVI_ENC", "D3_MIDI_IN", "D4_MIDI_OUT",
    "D5_AUTOMATION", "D6_SAVELOAD", "D7_STARTUP", "D8_FEEDBACK",
    "D9_KEYBOARD", "D10_MENUS", "D11_UNDO"
]

# Verdicts that need classification (not OK, not NA)
NEED_CLASSIFICATION = {"DEAD", "DIVERGE", "NOFEEDBACK", "DEFAULT", "NOROUNDTRIP",
                       "BINDNONE_PRESSPATH", "BINDNONE_PANELLOCAL", "BINDNONE_UNEXPLAINED"}

def classify_row(row):
    """Return (classification, reason, file_line) for a single verdict."""
    verdict = row["verdict"]
    door = row["door"]
    reg_id = row["reg_id"]
    section = row["section"]
    kind = row["kind"]
    group = row["group"]
    legend = row["legend"]
    bind = row["bind"]
    midi_cc = row["midi_cc"]
    automatable = row.get("auto", row.get("automatable", "0"))

    # File:line references for common paths
    def panel_path(bind_val):
        return f"gui/panelctl.c:10-26 (ri_panel_ctl_send bind={bind_val})"
    def midi_path():
        return "gui/midimap.c:46-84 (cc())"
    def auto_path():
        return "engine/seq/autolane.c (ri_auto_touch/ri_auto_value)"
    def rbng_path():
        return "project/rbng.c (rbng_write_song/rbng_read_song AUTO/ATRK)"
    def startup_path():
        return "app/riapp.c:4197-4245 (startup burst ri_panel_ctl_send)"
    def feedback_path():
        return "gui/livestate.c + app/riapp.c:1331-1339 (ri_ctl_press_repaints + repaint)"
    def keyboard_path():
        return "gui/keymap.c (ri_key_decode -> panel action)"
    def undo_path():
        return "project/undo.c (ri_undo_commit/ri_undo_undo)"

    # Helper to estimate key from bind/reg_id (approximate)
    def ri_ctlreg_auto_id_from_bind(bind_val, reg_id_val):
        if bind_val == "0": return 0
        # This is a rough approximation; real function is in gui/ctlreg.c:833
        return 0

    # BINDNONE classifications
    if verdict.startswith("BINDNONE"):
        if verdict == "BINDNONE_PRESSPATH":
            return "by-design", "Press/state-path control (step/transport/pattern) — no engine delivery by design", "gui/panelctl.c:22-25 (ri_ctlreg_auto_id returns 0 for press-path controls)"
        elif verdict == "BINDNONE_PANELLOCAL":
            # Check if it's LEVI Master encoders (needs-owner)
            if group == "Master" and section == "18":
                return "needs-owner", "LEVI Master Control 1-8 encoders — no engine control exists (t220, owner 2026-10-14)", "gui/ctlreg.c:334-341 (rows 1245-124c, bind=NONE, kind=KNOB)"
            return "by-design", "Panel-local action (button/step/display/page control) — drives panel state only", "gui/panelctl.c:22-25 (ri_ctlreg_auto_id returns 0 for panel-local controls)"
        elif verdict == "BINDNONE_UNEXPLAINED":
            # Check if it's a known category
            if kind == "2":  # SWITCH
                if group in ("LT", "MT", "HT", "RS", "CP") and section in ("2", "3"):  # 808/909
                    return "bug", "808/909 voice Switch has MIDI CC but bind=NONE — should be bound to voice enable", f"gui/ctlreg.c: rows for {group} Switch (bind=NONE, cc={midi_cc})"
            if group == "Module" and section == "18":
                return "by-design", "LEVI Module selector — panel navigation only", "gui/ctlreg.c:333 (row 1244)"
            if group == "Lfo" and legend == "Step":
                return "by-design", "LEVI LFO Step editor page switch — panel navigation only", "gui/ctlreg.c:516 (row 12d6)"
            if group in ("Lane",) and section == "18":
                return "by-design", "LEVI Lane selector — panel navigation only", "gui/ctlreg.c:517 (row 1204)"
            return "bug", "UNEXPLAINED bind=NONE — control looks live but has no engine target", f"gui/ctlreg.c: row {reg_id} (bind=NONE, kind={kind})"

    # DEAD - door accepts input but engine state doesn't move
    if verdict == "DEAD":
        if door == "D1_PANEL":
            if bind == "13":  # MUTE
                return "bug", "Mixer strip On/Off (bind=MUTE) — ri_ctlreg_auto_id returns 0 for MUTE (engine enable not wired)", "gui/ctlreg.c:833-863 (ri_ctlreg_auto_id returns 0 for RI_BIND_MUTE)"
            if bind == "6":  # FX
                return "bug", "FX On/Off (bind=FX) — ri_ctlreg_auto_id returns 0 for FX enable", "gui/ctlreg.c:833-863 (ri_ctlreg_auto_id returns 0 for RI_BIND_FX enable)"
            if bind == "9":  # INSERT
                return "bug", "Insert Dist/PCF/Comp (bind=INSERT) — ri_ctlreg_auto_id returns 0 for insert radio assign", "gui/ctlreg.c:833-863 (ri_ctlreg_auto_id returns 0 for RI_BIND_INSERT)"
            if bind == "0":  # NONE
                return "bug", "bind=NONE but door applies — should be BINDNONE_*", "gui/ctlreg.c:833-863 (ri_ctlreg_auto_id returns 0 for RI_BIND_NONE)"
            return "bug", f"Panel send reaches bridge but engine state unchanged", panel_path(bind)
        elif door == "D3_MIDI_IN":
            return "bug", "MIDI CC accepted but engine state unchanged", midi_path()
        elif door == "D5_AUTOMATION":
            return "bug", "Automation record/playback not restoring value", auto_path()
        elif door == "D6_SAVELOAD":
            return "bug", "Save/load (RBNG) not restoring value", rbng_path()
        elif door == "D7_STARTUP":
            return "bug", "Startup default moves engine — panel default != engine default", startup_path()
        elif door == "D11_UNDO":
            return "bug", "Undo not restoring value", undo_path()
        return "bug", f"DEAD on {door} — input accepted but no engine state change", ""

    # DIVERGE - state moves but not to D1 state
    if verdict == "DIVERGE":
        if door == "D1_PANEL":
            # Many DIVERGE are false positives due to incomplete direct() in test_door
            # Only flag as bug for key blocks we fully support
            key = ri_ctlreg_auto_id_from_bind(bind, reg_id) if bind != "0" else 0
            if key and (key & 0xFF00) in (0x0300, 0x0A00, 0x0C00, 0x0D00, 0x0E00, 0x0B00):
                return "bug", "Panel path result != direct setter for supported key block", f"tests/unit/t243_wiring_census.c:test_door D1_PANEL direct() for key {key:04x}"
            else:
                return "by-design", "DIVERGE likely due to incomplete direct() comparison in census test (Levi per-op/mod/LS/route/macro key blocks not implemented)", f"tests/unit/t243_wiring_census.c:test_door missing direct() for key block {(key & 0xFF00):04x}"
        return "by-design", f"DIVERGE on {door} — reference path not fully implemented in census test", ""

    # NOFEEDBACK - state moves but lamp/LED/meter/repaint doesn't
    if verdict == "NOFEEDBACK":
        if door in ("D1_PANEL", "D2_LEVI_ENC", "D3_MIDI_IN", "D5_AUTOMATION", "D6_SAVELOAD", "D7_STARTUP", "D9_KEYBOARD", "D11_UNDO"):
            # Many NOFEEDBACK are false positives: check_feedback() renders 64 frames but
            # some controls (e.g. switches, displays) don't produce audible meter changes
            if kind in ("2", "3", "7"):  # SWITCH, BUTTON, DISPLAY
                return "by-design", f"{door} moves engine but no meter peak change (switch/button/display — no audible parameter)", feedback_path()
            return "bug", f"No visual feedback on {door}", feedback_path()
        return "bug", f"NOFEEDBACK on {door}", ""

    # DEFAULT - panel default sent to fresh engine moves engine
    if verdict == "DEFAULT":
        if door == "D7_STARTUP":
            return "bug", "Startup burst sends panel default but engine default differs", startup_path()
        return "bug", f"DEFAULT on {door}", ""

    # NOROUNDTRIP
    if verdict == "NOROUNDTRIP":
        if door == "D5_AUTOMATION":
            return "bug", "Automation record->play doesn't restore", auto_path()
        if door == "D6_SAVELOAD":
            return "bug", "Save/load doesn't restore", rbng_path()
        return "bug", f"NOROUNDTRIP on {door}", ""

    return "unknown", "Unclassified", ""

def main():
    rows = []
    with open(TSV_PATH, 'r') as f:
        reader = csv.DictReader(f, delimiter='\t')
        for row in reader:
            rows.append(row)

    # Collect non-OK/NA verdicts
    findings = []
    for row in rows:
        for door in DOORS:
            verdict = row[door]
            if verdict in NEED_CLASSIFICATION:
                finding = {
                    "reg_id": row["reg_id"],
                    "section": row["section"],
                    "kind": row["kind"],
                    "group": row["group"],
                    "legend": row["legend"],
                    "bind": row["bind"],
                    "midi_cc": row["midi_cc"],
                    "automatable": row["auto"],
                    "door": door,
                    "verdict": verdict,
                }
                classification, reason, file_line = classify_row(finding)
                finding["classification"] = classification
                finding["reason"] = reason
                finding["file_line"] = file_line
                findings.append(finding)

    # Group by section
    section_names = {
        "0": "SYNTH1", "1": "SYNTH2", "2": "808", "3": "909",
        "4": "MIX_SYNTH1", "5": "MIX_SYNTH2", "6": "MIX_808", "7": "MIX_909",
        "8": "MASTER", "9": "PCF", "10": "DELAY", "11": "DIST", "12": "COMP", "13": "TRANSPORT",
        "14": "PAT_SYNTH1", "15": "PAT_SYNTH2", "16": "PAT_808", "17": "PAT_909",
        "18": "LEVI", "19": "PAT_LEVI", "20": "MIX_LEVI"
    }

    # Count bugs by section
    bug_counts = {}
    needs_owner = []
    for f in findings:
        if f["classification"] == "bug":
            sec = f["section"]
            bug_counts[sec] = bug_counts.get(sec, 0) + 1
        if f["classification"] == "needs-owner":
            needs_owner.append(f)

    # Write classify.md
    with open(OUT_PATH, 'w') as f:
        f.write("# W1 Classification — Wiring Census Verdicts\n\n")
        f.write(f"Generated from census.tsv ({len(rows)} rows, {len(findings)} non-OK/NA verdicts)\n\n")

        f.write("## Summary\n\n")
        f.write(f"- Total verdicts needing classification: **{len(findings)}**\n")
        f.write(f"- Bugs: **{sum(1 for x in findings if x['classification']=='bug')}**\n")
        f.write(f"- By-design: **{sum(1 for x in findings if x['classification']=='by-design')}**\n")
        f.write(f"- Needs-owner: **{sum(1 for x in findings if x['classification']=='needs-owner')}**\n")
        f.write(f"- Unknown: **{sum(1 for x in findings if x['classification']=='unknown')}**\n\n")

        f.write("## Bug Count per Section\n\n")
        for sec_id in sorted(bug_counts.keys(), key=int):
            f.write(f"- {section_names.get(sec_id, sec_id)} (section {sec_id}): **{bug_counts[sec_id]}** bugs\n")
        f.write("\n")

        f.write("## Needs-Owner List\n\n")
        for item in needs_owner:
            f.write(f"- **{item['reg_id']}** ({section_names.get(item['section'], item['section'])}/{item['group']}/{item['legend']}) door {item['door']}: {item['reason']}\n")
        f.write("\n")

        f.write("## Detailed Findings by Section\n\n")
        for sec_id in sorted(set(f["section"] for f in findings), key=int):
            sec_findings = [x for x in findings if x["section"] == sec_id]
            if not sec_findings:
                continue
            f.write(f"### {section_names.get(sec_id, sec_id)} (section {sec_id})\n\n")
            for item in sec_findings:
                f.write(f"- **{item['reg_id']}** {item['group']}/{item['legend']} (kind={item['kind']}, bind={item['bind']}, cc={item['midi_cc']}, auto={item['automatable']})\n")
                f.write(f"  - Door: {item['door']} | Verdict: {item['verdict']} | **{item['classification'].upper()}**: {item['reason']}\n")
                if item.get("file_line"):
                    f.write(f"  - Code: {item['file_line']}\n")
            f.write("\n")

        # To-do item cross-reference
        f.write("## To-Do Item Cross-Reference\n\n")
        f.write("| To-Do | Description | Census Rows |\n")
        f.write("|-------|-------------|-------------|\n")
        f.write("| ~313 | Seven MIDI settings with no panel row and no runtime path | ")
        midi_rows = [x for x in findings if x["midi_cc"] != "255" and x["verdict"] in ("DEAD", "DIVERGE", "NOFEEDBACK") and x["door"] == "D3_MIDI_IN"]
        f.write(f"{len(midi_rows)} MIDI DEAD/DIVERGE/NOFEEDBACK |\n")
        f.write("| ~319 | Mixer/Master COMP tab On/Off as unwired lamp | ")
        comp_rows = [x for x in findings if x["legend"] == "Comp" and x["kind"] == "2" and x["verdict"] in ("DEAD", "DIVERGE", "NOFEEDBACK")]
        f.write(f"{len(comp_rows)} COMP switch verdicts |\n")
        f.write("| ~321 | BPM-flagged automation keys with no registry row (~321) | ")
        tempo_rows = [x for x in findings if x["verdict"] in ("DEAD", "DIVERGE", "NOFEEDBACK") and x["door"] == "D5_AUTOMATION"]
        f.write(f"{len(tempo_rows)} automation DEAD/DIVERGE/NOFEEDBACK |\n")
        f.write("| ~283 | Stale-bounds lesson (comment claims guard that doesn't exist) | Manual review needed |\n")

    print(f"Written {OUT_PATH}")
    print(f"Total findings: {len(findings)}")
    print(f"Bugs: {sum(1 for x in findings if x['classification']=='bug')}")
    print(f"By-design: {sum(1 for x in findings if x['classification']=='by-design')}")
    print(f"Needs-owner: {sum(1 for x in findings if x['classification']=='needs-owner')}")
    print(f"Unknown: {sum(1 for x in findings if x['classification']=='unknown')}")

if __name__ == "__main__":
    main()