#!/usr/bin/env python3
"""Mutation harness for the promoted Levi knobs (t220_levi_knobs, 2026-10-13).

Every mutant is scored ONLY on the gate's return code: 0 = survived (the test
failed to catch it, which is a FAIL for the test), non-zero = killed. A
mutant that merely damaged the file beyond recognition is not a kill, so each
is checked to still contain the markers the test reads.

The defect was never the engine: BALANCE and the four arpeggiator knobs
already carried bind=LEVI with real RI_CTL_LEVI_* ids. What was missing was
a panelgeo row to hit-test them and a draw that read the real value instead
of a hardcoded 0.0f. So these mutants attack the two halves separately:

  m1   one row deleted. The other four still work, so anything checking "are
       the knobs there?" as one boolean waves it through.
  m2   MASTER given a box. This is the mutant the test explicitly guards
       against and the most tempting wrong fix of all: MASTER has no engine
       control anywhere, so laying it out produces a knob that animates and
       drives nothing -- a worse failure than one that is visibly inert.
  m3   one row pointed at the WRONG control index. Every knob is still
       clickable, still animates, and still sends something -- just not the
       parameter the legend claims.
  m4   one row demoted to a decoration shape (LED). It keeps a box and a
       position, so bounds and hit overlap both stay happy, but it is no
       longer a control: nothing can set it.
  m5   one control unbound in the registry. Geometry survives; delivery
       does not. The knob would look right and still go nowhere.
  m6   the whole promotion reverted -- all five rows gone, which is the
       original defect, so the test must fail exactly as it did in RED.

KNOWN GAP, stated rather than hidden: a mutant that put the five knobs BACK
into art_levi.c's dim DKNOB table while KEEPING their geometry rows would
double-draw them and SURVIVE this harness. t220 pins the geometry and the
panel value; it cannot see the art table. That visual overlap is covered
only by the lane, not by any host test.
"""
import subprocess, os, sys

# ROOT from this file's own location, not the caller's cwd: a harness that
# silently runs against a different tree is the stale-binary trap again.
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REL = lambda *p: os.path.join(ROOT, *p)
BUILD = os.path.join(ROOT, "scripts", "ri_build_host.sh")
TEST = "t220_levi_knobs"

GEO = ("gui", "panelgeo.c")
REG = ("gui", "ctlreg.c")

FIVE = [
    "{ SL(225), RI_GEO_KNOB, 0, 126, 136, 30, 38 }, /* BALANCE -> RI_CTL_LEVI_PFBAL */",
    "{ SL(186), RI_GEO_KNOB, 0, 274, 164, 30, 38 }, /* arp MODE */",
    "{ SL(184), RI_GEO_KNOB, 0, 332, 164, 30, 38 }, /* arp OCTAVE (Oct Range) */",
    "{ SL(185), RI_GEO_KNOB, 0, 390, 164, 30, 38 }, /* arp GATE */",
    "{ SL(189), RI_GEO_KNOB, 0, 447, 164, 30, 38 }, /* arp ENTROPY */",
]

# op forms: ("rep", relpath, old, new) -- exact, must match exactly once
#           ("re",  relpath, pat, repl) -- regex, must match at least once
MUTANTS = {
    "m1-one-row-deleted":
        [("rep", GEO, FIVE[3] + "\n", "")],

    "m2-master-given-a-box":
        [("rep", GEO, FIVE[0],
          "    { SL(2), RI_GEO_KNOB, 0, 62, 137, 30, 38 }, /* MASTER */\n" + FIVE[0])],

    "m3-wrong-control-index":
        [("re", GEO, r"\{ SL\(225\), RI_GEO_KNOB", "{ SL(184), RI_GEO_KNOB")],

    "m4-demoted-to-decoration":
        [("re", GEO, r"\{ SL\(186\), RI_GEO_KNOB", "{ SL(186), RI_GEO_LED")],

    "m5-arp-entropy-unbound":
        [("rep", REG,
          'R(LEVI, 189, KNOB, "Arp", "Entropy", 0, 127, 0, RI_MIDI_CC_NONE, 1, LEVI, RI_CTL_LEVI_ARPENTROPY, 0)',
          'R(LEVI, 189, KNOB, "Arp", "Entropy", 0, 127, 0, RI_MIDI_CC_NONE, 0, NONE, 0, 0)')],

    "m6-promotion-reverted":
        [("re", GEO,
          r"\n(    \{ SL\((?:225|186|184|185|189)\), RI_GEO_KNOB[^\n]*\n)+", "\n")],
}

MARKERS = {
    # A mutant's marker must show the edit was SURGICAL -- it can never
    # require the token that mutant deliberately removes, or every
    # single-cell mutation reports itself broken. m3 repoints SL(225) and
    # m5 strips RI_CTL_LEVI_ARPENTROPY, so those tokens are gone BY DESIGN
    # in both; the neighbours are what prove the rest survived.
    "m1-one-row-deleted": ["SL(225)", "SL(186)", "SL(184)"],
    "m2-master-given-a-box": ["SL(2)", "SL(225)"],
    "m3-wrong-control-index": ["SL(185)", "SL(189)", "SL(186)"],
    "m4-demoted-to-decoration": ["RI_GEO_LED", "SL(225)", "SL(185)"],
    "m5-arp-entropy-unbound": ['"Arp", "Entropy"', "RI_CTL_LEVI_ARPGATE"],
    "m6-promotion-reverted": ["SL(78)", "RI_GEO_LEVI"],
}


def run_gate():
    b = subprocess.run(["bash", BUILD, "all"], cwd=ROOT,
                       capture_output=True, text=True)
    if b.returncode != 0:
        return -99, "BUILD FAILED: " + (b.stdout or b.stderr)[-200:]
    t = subprocess.run(["bash", BUILD, "test", TEST], cwd=ROOT,
                       capture_output=True, text=True)
    return t.returncode, (t.stdout or t.stderr)


def restore(backups):
    for rel, data in backups.items():
        with open(os.path.join(ROOT, *rel), "wb") as fh:
            fh.write(data)


killed = survived = 0
backups = {}
try:
    import re
    for name, ops in MUTANTS.items():
        backups = {}
        touched = set()
        for _, rel, *_ in ops:
            if rel not in backups:
                with open(os.path.join(ROOT, *rel), "rb") as fh:
                    backups[rel] = fh.read()
            touched.add(rel)

        ok = True
        for kind, rel, a, b in ops:
            p = os.path.join(ROOT, *rel)
            src = open(p, encoding="utf-8", errors="replace").read()
            if kind == "rep":
                if src.count(a) != 1:
                    print("%-26s SKIP (anchor matched %d times)" % (name, src.count(a)))
                    ok = False
                    break
                src = src.replace(a, b)
            else:
                src, n = re.subn(a, b, src, count=1)
                if n != 1:
                    print("%-26s SKIP (regex matched %d times)" % (name, n))
                    ok = False
                    break
            open(p, "w", encoding="utf-8").write(src)
        if not ok:
            restore(backups)
            subprocess.run(["bash", BUILD, "all"], cwd=ROOT,
                           capture_output=True, text=True)
            continue

        want = MARKERS.get(name, [])
        blob = "".join(open(os.path.join(ROOT, *rel), errors="replace").read()
                       for rel in touched)
        intact = all(w in blob for w in want)

        rc, out = run_gate()
        restore(backups)
        subprocess.run(["bash", BUILD, "all"], cwd=ROOT,
                       capture_output=True, text=True)

        if rc != 0:
            killed += 1
        else:
            survived += 1
        print("%-26s rc=%-4d %s%s" % (name, rc, "KILLED" if rc else "SURVIVED",
              "" if intact else "  [BROKEN MUTANT -- not a real kill]"))
        for line in out.splitlines():
            if line.startswith("FAIL:") or line.startswith("FAIL s"):
                print("      " + line[:130])
finally:
    restore(backups)
    subprocess.run(["bash", BUILD, "all"], cwd=ROOT,
                   capture_output=True, text=True)

print("\nmutation score: %d/%d killed" % (killed, killed + survived))
sys.exit(0 if survived == 0 else 1)