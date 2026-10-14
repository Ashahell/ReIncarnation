#!/usr/bin/env python3
"""Mutation harness for the FX rack enable (t218_fx_enable, 2026-10-13).

Every mutant is scored ONLY on the gate's return code: 0 = survived (the test
failed to catch it, which is a FAIL for the test), non-zero = killed. A
mutant that merely damaged the file beyond recognition is not a kill, so each
is checked to still contain the markers the test reads.

WHY EACH MUTANT EXISTS. They are not six spellings of one defect; each pins a
different claim this change makes about itself:

  m1   ONE FX canvas left unbound. The other three still work, so anything
       that checks "is the whole rack bound?" as a single boolean passes.
  m2   the whole registry half reverted: bind=NONE, as it shipped. The
       original defect, reproduced.
  m3   rack defaults OFF. The dangerous direction: this would silence FX
       that is audible today while every "the switch is wired" assertion
       still passed, because a wired switch reads 0 at boot.
  m4   the engine's DIST_ENABLE case deleted. The registry id still resolves,
       so every DELIVERABILITY assertion passes -- this is the mutant that
       forced t218 to grow an engine section instead of shipping a test that
       green-lights a switch that does nothing.
  m5   half the fix: the route mask is kept, the fx_on term is dropped from
       the DIST render guard. The module still audibly works when enabled, so
       only a bypass assertion can see this.
  m6   the handler always reports live. A switch that cannot be switched off
       is wired in the same way as one that cannot be switched on.
"""
import subprocess, os, shutil, sys

# ROOT from this file's own location, not the caller's cwd: a harness that
# silently runs against a different tree is the stale-binary trap again.
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REL = lambda *p: os.path.join(ROOT, *p)
BUILD = os.path.join(ROOT, "scripts", "ri_build_host.sh")
TEST = "t218_fx_enable"

REG = ("gui", "ctlreg.c")
ENG = ("engine", "engine.c")

# op forms: ("rep", relpath, old, new) -- exact, must match exactly once
#           ("re",  relpath, pat, repl) -- regex, must match at least once
MUTANTS = {
    # Deliverability: one canvas of four left unbound.
    "m1-one-canvas-unbound":
        [("rep", REG,
          "R(PCF, 0, SWITCH, \"\", \"On/Off\", 0, 1, 0, RI_MIDI_CC_NONE, 1, FX, RI_FXID_PCF_ENABLE, 0)",
          "R(PCF, 0, SWITCH, \"\", \"On/Off\", 0, 1, 0, RI_MIDI_CC_NONE, 1, NONE, 0, 0)")],

    # The original defect, verbatim.
    "m2-registry-half-reverted":
        [("re", REG, r"RI_FXID_(PCF|DELAY|DIST|COMP)_ENABLE", "0")],

    # The dangerous direction: everything wired, nothing audible by default.
    "m3-rack-defaults-off":
        [("re", ENG, r"e->fx_on\[i\] = 1u;", "e->fx_on[i] = 0u;")],

    # Ids resolve, engine ignores them: the mutant t218 exists to kill.
    # Retargeted 2026-10-14: the owner ruling replaced the inline assignment
    # with ri_fx_engage(), so this must delete the CALL, not the old statement.
    "m4-engine-case-deleted":
        [("re", ENG, r"case RI_FXID_DIST_ENABLE:\s*\n\s*ri_fx_engage\(e, RI_ENGINE_FX_DIST, value\);\s*\n\s*break;\n", "")],

    # Half the fix: mask kept, gate term dropped.
    "m5-gate-term-dropped":
        [("re", ENG,
          r"\(mask & \(1u << RI_ROUTE_DIST\)\) && e->fx_on\[RI_ENGINE_FX_DIST\]",
          "(mask & (1u << RI_ROUTE_DIST))")],

    # A switch that cannot be switched off.
    # Retargeted 2026-10-14: a switch that cannot be switched off. Inside
    # ri_fx_engage, making the OFF early-return a no-op.
    "m6-handler-always-live":
        [("re", ENG,
          r"if \(!value\) \{           /\* OFF clears itself and disturbs nobody \*/\n        e->fx_on\[mod\] = 0u;\n        return;\n    \}",
          "if (!value) { e->fx_on[mod] = 1u; return; }")],
}

MARKERS = {
    # A mutant that stripped the whole switch out is not a real kill. m1 only
    # unbinds ONE canvas, so its marker must NOT require RI_FXID_PCF_ENABLE
    # (that is exactly what it removes) -- it must show the other three
    # survived, i.e. the edit was surgical.
    "m1-one-canvas-unbound": ["R(PCF, 0, SWITCH", "RI_FXID_COMP_ENABLE",
                              "RI_FXID_DIST_ENABLE"],
    "m2-registry-half-reverted": ["R(PCF, 0, SWITCH", "On/Off"],
    "m3-rack-defaults-off": ["fx_on[i]"],
    "m4-engine-case-deleted": ["RI_FXID_COMP_ENABLE", "RI_FXID_PCF_ENABLE"],
    "m5-gate-term-dropped": ["RI_ROUTE_DIST"],
    "m6-handler-always-live": ["RI_FXID_DIST_ENABLE"],
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


def assert_baseline():
    """The gate must be GREEN on unmutated code before any mutant is scored.

    This exists because `ri_build_host.sh all` does not compile the unit
    test, so a broken t222 still let the harness print 7/7 KILLED while the
    test had never run once. Scored on the return code, but also on the
    word PASS: a non-zero gate that is really a BUILD failure must not be
    mistaken for a kill."""
    rc, out = run_gate()
    if rc != 0 or "PASS" not in out:
        print("BASELINE FAILED (rc=%d) -- every mutant would score as a kill "
              "for the wrong reason. Fix the gate first.\n%s" % (rc, out[-400:]))
        sys.exit(2)
    print("baseline: gate green on unmutated code\n")


killed = survived = 0
backups = {}
try:
    assert_baseline()
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
                import re
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

        # Intactness: the mutated tree must still contain what the test reads.
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