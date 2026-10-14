#!/usr/bin/env python3
"""lane_evlog.py -- read a ReIncarnation lane event log and name what it cannot
explain, so the next "looks live but isn't" is found by grep instead of by ear.

WHY THIS EXISTS. In one session the same defect shape produced six faults:
a control that looked live and was not (the FX enables, the strip mute, the
displaced FX lamp, the insert rockers, the inverted On/Off, the G7 CC that moved
the panel and never reached the engine). EVERY ONE was found by the owner
pressing something and noticing a silence, then by me reading the log. Each
round cost an ear-proof cycle.

Two of those rounds were lost to the same mistake: a symptom assembled from
evidence that never actually overlapped. The owner moved Delay F.Back 142 times
in a session where the delay send sat at 0, and separately raised the send to 64
in a session that never touched a delay knob. Neither log shows a defect. Both
look like "the FX knobs do nothing". Two ear-proof cycles spent proving they
didn't.

THIS TOOL FINDS THAT CLASS OF MISTAKE IN THE LOG, IN SECONDS:

  MIXER_NEVER_TOUCHED      a busy session in which no mixer control was ever
                           moved -- so a mixer knob being at its default is
                           the explanation for anything that "does nothing"
  DELAY_SEND_NEVER_TOUCHED delay parameters were adjusted and the send that
                           feeds the line was never opened: there was nothing
                           for them to modulate
  SWITCH_REPEATED_VALUE    a 0/1 switch reported the same value twice in a row.
                           Either something else displaced it (radio routing)
                           or it does not toggle. The tool cannot tell which and
                           does not guess: it narrows, a human decides.

A finding is a QUESTION, never a verdict. Each says what it observed and what
that rules out.

USAGE
    lane_evlog.py <log> [<log> ...]          human-readable
    lane_evlog.py --json <log> ...           machine-readable

The control registry is READ FROM gui/ctlreg.c rather than duplicated here, so
one source of truth decides what a knob is. Run from the repo root.
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CTLREG = os.path.join(ROOT, "gui", "ctlreg.c")

# `R(SEC, IDX, KIND, "GROUP", "LEGEND", min, max, def, cc, auto, bind, ...)`
ROW = re.compile(
    r'R\((\w+),\s*(\d+),\s*(\w+),\s*"([^"]*)",\s*"([^"]*)",\s*(-?\d+),\s*(-?\d+),'
    r'\s*(-?\d+),\s*([^,]+),\s*(\d+),\s*(\w+)')
EV = re.compile(r'^ev\s+(\d+)\s+(\d+)\s+(\w+)\s*(.*)$')
CTL = re.compile(r'^([0-9a-fA-F]{4})=(-?\d+)\s*$')

MIXER_SECS = {4, 5, 6, 7, 8, 20}          # MIX_SYNTH1..MASTER, MIX_LEVI


def load_registry():
    """(sec << 8 | idx) -> dict. Registry is the single source of truth for
    what a control IS; duplicating that list here is how the two drift."""
    out = {}
    try:
        src = open(CTLREG, encoding='utf-8').read()
    except OSError:
        return out
    secname = {}
    for m in re.finditer(r'#define RI_SEC_(\w+)\s+(\d+)u', src):
        secname[m.group(1)] = int(m.group(2))
    for name, sec in secname.items():
        secname.setdefault(name, sec)
    hdr = open(os.path.join(ROOT, "gui", "ctlreg.h"), encoding='utf-8').read()
    for m in re.finditer(r'#define RI_SEC_(\w+)\s+(\d+)u', hdr):
        secname[m.group(1)] = int(m.group(2))
    for m in ROW.finditer(src):
        s, idx, kind, grp, leg, lo, hi, dv, _cc, auto, bind = m.groups()
        if s not in secname:
            continue
        out[(secname[s] << 8) | int(idx)] = {
            'sec': secname[s], 'idx': int(idx), 'kind': kind,
            'group': grp, 'legend': leg,
            'min': int(lo), 'max': int(hi), 'bind': bind,
            'auto': int(auto) == 1,
        }
    return out


def parse(path):
    """-> (build, [(ms, reg, val)] in order)."""
    build = None
    seq = []
    with open(path, encoding='utf-8', errors='replace') as fh:
        for line in fh:
            m = EV.match(line.strip())
            if not m:
                continue
            _n, ms, kind, rest = m.groups()
            if kind == 'RUN':
                b = re.search(r'build=(\w+)', rest)
                if b:
                    build = b.group(1)
            elif kind == 'CTL':
                c = CTL.match(rest.strip())
                if c:
                    seq.append((int(ms), int(c.group(1), 16), int(c.group(2))))
    return build, seq


def analyse(path, reg):
    build, seq = parse(path)
    f = []

    def add(code, detail, rules_out):
        f.append({'code': code, 'detail': detail, 'rules_out': rules_out})

    if not seq:
        add('NO_PANEL_PUSH',
            'no CTL events at all: the panel moved nothing this session',
            'anything reported as "the panel does nothing" was not tested by '
            'this session -- it never touched a control')
        return build, seq, f

    mixer = [x for x in seq if (x[1] >> 8) in MIXER_SECS]
    secs = sorted({x[1] >> 8 for x in seq})

    if len(seq) >= 50 and not mixer:
        add('MIXER_NEVER_TOUCHED',
            '%d panel events across section(s) %s, and no mixer control was '
            'ever moved' % (len(seq), ','.join('%02x' % s for s in secs)),
            'every mixer knob is still at its default, so a mixer knob '
            '"doing nothing" is not evidence of a fault -- it was never moved')

    # delay params adjusted while the send that feeds the line was never opened
    dly = [x for x in seq if (x[1] >> 8) == 10 and (x[1] & 0xFF) in (2, 3, 4, 5)]
    if len(dly) >= 10 and not mixer:
        names = sorted({'%s' % reg.get(x[1], {}).get('legend', '?') for x in dly})
        add('DELAY_SEND_NEVER_TOUCHED',
            '%d delay-parameter events (%s) and the send was never opened'
            % (len(dly), ', '.join(names)),
            'the delay line received no signal in this session, so no delay '
            'parameter could have made an audible difference. This is a '
            'SESSION artefact, not a delay fault.')

    # a 0/1 switch that reported the same value twice running
    byreg = {}
    for ms, r, v in seq:
        byreg.setdefault(r, []).append(v)
    for r, vals in sorted(byreg.items()):
        d = reg.get(r)
        if not d or d['max'] != 1:
            continue
        runs = sum(1 for i in range(1, len(vals)) if vals[i] == vals[i - 1] and vals[i])
        if runs >= 1:
            add('SWITCH_REPEATED_VALUE',
                '%04x %s/%s reported %d in a row (%s)'
                % (r, d['group'], d['legend'], runs,
                   ''.join(str(v) for v in vals[:12])),
                'either another control displaced it (radio routing) or it does '
                'not toggle. The log alone cannot tell these apart; check the '
                'section family before calling it a fault.')

    return build, seq, f


def main(argv):
    args = [a for a in argv[1:] if not a.startswith('-')]
    as_json = '--json' in argv[1:]
    if not args:
        print(__doc__.strip())
        return 2
    reg = load_registry()
    if not reg:
        print('could not read %s -- run from the repo root' % CTLREG, file=sys.stderr)
        return 2
    out = []
    for p in args:
        if not os.path.exists(p):
            print('no such log: %s' % p, file=sys.stderr)
            return 2
        build, seq, f = analyse(p, reg)
        out.append({'log': os.path.basename(p), 'build': build,
                    'events': len(seq), 'findings': f})
    if as_json:
        print(json.dumps(out, indent=2))
        return 0
    for r in out:
        print('%s  build=%s  %d panel events' % (r['log'], r['build'], r['events']))
        if not r['findings']:
            print('   (nothing unexplained)')
        for f in r['findings']:
            print('   [%s] %s' % (f['code'], f['detail']))
            print('       -> %s' % f['rules_out'])
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
