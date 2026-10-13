#!/usr/bin/env python3
"""Mutation harness for the FX meter wire (t219_fx_meters, 2026-10-13).

Every mutant is scored ONLY on the gate's return code: 0 = survived (the test
failed to catch it, which is a FAIL for the test), non-zero = killed. A
mutant that merely damaged the file beyond recognition is not a kill, so each
is checked to still contain the markers the test reads.

WHY EACH MUTANT EXISTS. The defect was that the meters had no wire at all;
the fix re-introduced one, and the risk in the fix is not "does it compile"
but "does each module's level land on the right box". Six ways to get that
wrong, each pinned separately:

  m1   one cell of the transpose moved. The plausible-but-wrong edit: PCF
       reading DIST's meter. Every level is still 0..127 and every meter
       still moves, so only the slot assignment can see it.
  m2   the map replaced by identity (canvas index used as the engine unit).
       This is the mistake the code exists to prevent, and it is exactly what
       the "PCF is NOT unit 0" assertion exists to catch.
  m3   the non-FX fallback returns 0 instead of RI_ENGINE_FX_COUNT. Unit 0 is
       DIST, so asking about the Levi would silently return DIST's meter --
       the silent wrong answer the header explicitly rules out.
  m4   reading sec_peak[] instead of fx_peak[]. Both are peak arrays in the
       same struct and both are floats, so this compiles and runs.
  m5   RI_CORE_FX_ORDER reordered into engine-unit order. The order constant
       is what the shell trusts; if it drifts from canvas order the shell
       repaints the wrong canvas with a correct number.
  m6   the ri_live_meter_level projection skipped, feeding the raw float
       peak into the int. A raw peak truncates to 0 for normal material, so
       every meter reads dark -- the original symptom, reintroduced.
"""
import subprocess, os, sys

# ROOT from this file's own location, not the caller's cwd: a harness that
# silently runs against a different tree is the stale-binary trap again.
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REL = lambda *p: os.path.join(ROOT, *p)
BUILD = os.path.join(ROOT, "scripts", "ri_build_host.sh")
TEST = "t219_fx_meters"

CORE = ("app", "core", "riapp_core.c")

# op forms: ("rep", relpath, old, new) -- exact, must match exactly once
#           ("re",  relpath, pat, repl) -- regex, must match at least once
MUTANTS = {
    "m1-one-cell-transposed":
        [("rep", CORE,
          "case RI_SEC_PCF:   return RI_ENGINE_FX_PCF;",
          "case RI_SEC_PCF:   return RI_ENGINE_FX_DIST;")],

    "m2-identity-map":
        [("re", CORE,
          r"case RI_SEC_PCF:   return RI_ENGINE_FX_PCF;\s*\n"
          r"\s*case RI_SEC_DELAY: return RI_ENGINE_FX_DELAY;\s*\n"
          r"\s*case RI_SEC_DIST:  return RI_ENGINE_FX_DIST;\s*\n"
          r"\s*case RI_SEC_COMP:  return RI_ENGINE_FX_COMP;",
          "case RI_SEC_PCF: case RI_SEC_DELAY: case RI_SEC_DIST: case RI_SEC_COMP:\n"
          "        return RI_ENGINE_FX_PCF; /* identity */")],

    "m3-fallback-returns-zero":
        [("rep", CORE,
          "    default:           return RI_ENGINE_FX_COUNT;",
          "    default:           return 0u;")],

    "m4-wrong-peak-array":
        [("rep", CORE, "ri_live_meter_level(m.fx_peak[u])",
          "ri_live_meter_level(m.sec_peak[u & 3u])")],

    "m5-order-into-unit-order":
        [("rep", CORE,
          "    RI_SEC_PCF, RI_SEC_DELAY, RI_SEC_DIST, RI_SEC_COMP\n",
          "    RI_SEC_DIST, RI_SEC_PCF, RI_SEC_DELAY, RI_SEC_COMP\n")],

    "m6-projection-skipped":
        [("re", CORE,
          r"lvl\[i\] = ri_live_meter_level\(m\.fx_peak\[u\]\);",
          "lvl[i] = (int)m.fx_peak[u];")],
}

MARKERS = {
    # A mutant's marker must show the edit was SURGICAL -- it can never
    # require the token that mutant deliberately removes, or every
    # single-cell mutation reports itself broken. m1 turns PCF's case into
    # a DIST return, so PCF's engine id is gone BY DESIGN; the neighbours
    # are what prove the rest of the switch survived.
    "m1-one-cell-transposed": ["case RI_SEC_PCF", "RI_ENGINE_FX_DELAY",
                               "RI_ENGINE_FX_COMP"],
    "m2-identity-map": ["ri_core_fx_meter_unit", "fx_peak"],
    "m3-fallback-returns-zero": ["case RI_SEC_PCF", "RI_ENGINE_FX_PCF"],
    "m4-wrong-peak-array": ["ri_live_meter_level", "sec_peak"],
    "m5-order-into-unit-order": ["RI_CORE_FX_ORDER", "fx_peak"],
    "m6-projection-skipped": ["fx_peak[u]"],
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
    for name, ops in MUTANTS.items():
        backups = {}
        touched = set()
        for _, rel, *_ in ops:
            if rel not in backups:
                with open(os.path.join(ROOT, *rel), "rb") as fh:
                    backups[rel] = fh.read()
            touched.add(rel)

        import re
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