#!/usr/bin/env python3
"""Parse classify.md and generate complete allowlist.tsv with all by-design/needs-owner entries."""
import re

CLASSIFY_PATH = "docs/evidence/wiring/2026-11-17-census/classify.md"
TSV_PATH = "docs/evidence/wiring/2026-11-17-census/census.tsv"
OUT_PATH = "docs/evidence/wiring/2026-11-17-census/allowlist.tsv"

DOORS = ["D1_PANEL", "D2_LEVI_ENC", "D3_MIDI_IN", "D4_MIDI_OUT",
         "D5_AUTOMATION", "D6_SAVELOAD", "D7_STARTUP", "D8_FEEDBACK",
         "D9_KEYBOARD", "D10_MENUS", "D11_UNDO"]
DOOR_TO_IDX = {door: i for i, door in enumerate(DOORS)}

def parse_classify():
    """Parse classify.md for by-design and needs-owner entries."""
    entries = []
    
    with open(CLASSIFY_PATH, 'r') as f:
        content = f.read()
    
    # Parse the "Detailed Findings by Section" section
    # Format: - **REG_ID** GROUP/LEGEND (kind=K, bind=B, cc=CC, auto=A)
    #           - Door: DOOR | Verdict: VERDICT | **CLASSIFICATION**: REASON
    #           - Code: FILE:LINE
    
    # First, find all control headers and their door verdicts
    # Pattern: - **REG_ID** ... (kind=..., bind=..., cc=..., auto=...)
    control_pattern = re.compile(
        r'-\s+\*\*([0-9A-Fa-f]{4})\*\*\s+([^\(]+)\s+\(kind=(\d+),\s+bind=(\d+),\s+cc=(\d+),\s+auto=(\d+)\)'
    )
    
    # Pattern for door verdict lines
    door_pattern = re.compile(
        r'-\s+Door:\s+(\w+)\s+\|\s+Verdict:\s+(\w+)\s+\|\s+\*\*([\w-]+)\*\*:\s+(.+)'
    )
    
    lines = []
    with open(CLASSIFY_PATH, 'r') as f:
        lines = f.readlines()
    
    i = 0
    current_control = None
    while i < len(lines):
        line = lines[i]
        
        # Match control header
        m = control_pattern.match(line)
        if m:
            reg_id, group_legend, kind, bind, cc, auto = m.groups()
            if '/' in group_legend:
                parts = group_legend.split('/', 1)
                group = parts[0].strip()
                legend = parts[1].strip()
            else:
                group = ""
                legend = group_legend.strip()
            current_control = {
                'reg_id': reg_id,
                'group': group,
                'legend': legend,
                'kind': kind,
                'bind': bind,
                'midi_cc': cc,
                'automatable': auto
            }
        
        elif current_control and line.strip().startswith('- Door:'):
            m = re.match(r'-\s+Door:\s+(\w+)\s+\|\s+Verdict:\s+(\w+)\s+\|\s+\*\*([\w-]+)\*\*:\s+(.+)', line.strip())
            if m:
                door, verdict, classification, reason = m.groups()
                classification = classification.lower()
                reason = reason.strip()
                if classification in ('by-design', 'needs-owner'):
                    entries.append({
                        'reg_id': current_control['reg_id'],
                        'door': door,
                        'verdict': verdict,
                        'reason': reason,
                        'classification': classification
                    })
        
        i += 1
    
    # Also parse Needs-Owner List section
    # Format: - **REG_ID** (SECTION/GROUP/LEGEND) door DOOR: REASON
    needs_owner_pattern = re.compile(
        r'-\s+\*\*([0-9A-Fa-f]{4})\*\s+\([^)]+\)\s+door\s+(\w+):\s+(.+)'
    )
    in_needs_owner = False
    for line in lines:
        if '## Needs-Owner List' in line:
            in_needs_owner = True
            continue
        if in_needs_owner and line.startswith('## ') and 'Needs-Owner' not in line:
            in_needs_owner = False
        if in_needs_owner:
            m = needs_owner_pattern.match(line.strip())
            if m:
                reg_id, door, reason = m.groups()
                entries.append({
                    'reg_id': reg_id,
                    'door': door,
                    'verdict': 'BINDNONE_PANELLOCAL',  # needs-owner are all BINDNONE_PANELLOCAL for LEVI Master
                    'reason': reason,
                    'classification': 'needs-owner'
                })
    
    return entries

def main():
    entries = parse_classify()
    print(f"Found {len(entries)} by-design/needs-owner entries")
    
    # Load census TSV to verify
    import csv
    census = {}
    with open(TSV_PATH, 'r') as f:
        reader = csv.DictReader(f, delimiter='\t')
        for row in reader:
            census[row['reg_id']] = row
    
    # Verify and filter
    DOORS = ["D1_PANEL", "D2_LEVI_ENC", "D3_MIDI_IN", "D4_MIDI_OUT",
             "D5_AUTOMATION", "D6_SAVELOAD", "D7_STARTUP", "D8_FEEDBACK",
             "D9_KEYBOARD", "D10_MENUS", "D11_UNDO"]
    DOOR_TO_IDX = {door: i for i, door in enumerate(DOORS)}
    
    allowlist = []
    for entry in entries:
        if entry['door'] not in DOORS:
            print(f"WARNING: Unknown door {entry['door']} for {entry['reg_id']}")
            continue
        if entry['reg_id'] not in census:
            print(f"WARNING: {entry['reg_id']} not in census")
            continue
        census_verdict = census[entry['reg_id']].get(entry['door'], '')
        if census_verdict != entry['verdict']:
            print(f"MISMATCH: {entry['reg_id']} {entry['door']} classify={entry['verdict']} census={census_verdict}")
            continue
        allowlist.append(entry)
    
    # Write allowlist TSV
    DOORS = ["D1_PANEL", "D2_LEVI_ENC", "D3_MIDI_IN", "D4_MIDI_OUT",
             "D5_AUTOMATION", "D6_SAVELOAD", "D7_STARTUP", "D8_FEEDBACK",
             "D9_KEYBOARD", "D10_MENUS", "D11_UNDO"]
    DOOR_TO_IDX = {door: i for i, door in enumerate(DOORS)}
    
    with open(OUT_PATH, 'w') as f:
        f.write("# Wiring Census Allow-List (W3)\n")
        f.write("# Format: reg_id<TAB>door_idx<TAB>verdict<TAB>reason<TAB>date\n")
        f.write("# Auto-generated from W1 classify.md (by-design + needs-owner only)\n")
        f.write(f"# Total entries: {len(allowlist)}\n\n")
        
        for entry in allowlist:
            door_idx = DOOR_TO_IDX[entry['door']]
            f.write(f"{entry['reg_id']}\t{DOOR_TO_IDX[entry['door']]}\t{entry['verdict']}\t{entry['reason']}\t2026-11-18\n")
    
    print(f"Generated {OUT_PATH} with {len(allowlist)} entries")

if __name__ == "__main__":
    main()