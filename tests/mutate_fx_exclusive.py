#!/usr/bin/env python3
"""Mutation harness for the FX rack exclusivity ruling (t222_fx_exclusive,
2026-10-14).

Every mutant is scored ONLY on the gate's return code: 0 = survived (the test
failed to catch it, which is a FAIL for the test), non-zero = killed. A
mutant that merely damaged the file beyond recognition is not a kill, so each
is checked to still contain the markers the test reads.

Owner ruling: "as intuitive as possible. If the user turns on an FX panel that
conflicts with another, that other one should be turned off and the newly
activated FX panel should become active."

WHY SEVEN MUTANTS AND NOT SIX SPELLINGS. The two halves of the ruling fail in
different files and in different ways, and the dangerous ones are the ones
that still LOOK correct on screen:

  m1   the panel never displaces. Every lamp still reflects its own toggle,
       so the rack looks like four independent switches -- the pre-ruling
       behaviour, still shipping the "newly activated" module's own lamp.
  m2   the lamp is STORED rather than derived. This is the mutant the whole
       shared-rack design exists to kill: each canvas keeps a private val[0],
       so the displaced module's lamp stays lit. Every "is the switch wired"
       assertion passes; only the displacement assertions see it.
  m3   the ENGINE does not enforce exclusivity. The panel is right and the
       engine is wrong, so the screen shows one module live while two
       actually render -- the mirror image of m1.
  m4   engage does not route. The owner's original defect, verbatim: the
       switch lights, the panel is honest, and PCF/Comp stay silent with dark
       meters. Everything about exclusivity passes.
  m5   engage STEALS an insert somebody routed deliberately. Radio-exclusive
       routing means claiming a unit that is already owned silently takes it
       away from another strip -- a regression in the opposite direction,
       and invisible to any test that only checks "something owns it now".
  m6   OFF clears the whole rack. Makes OFF symmetric with ON, so switching
       a module off silently brings the displaced one back. Sounds
       reasonable, is not what was ruled, and is the kind of "helpful"
       behaviour that erases a user's choice.
  m7   an unbound canvas falls back to private state. The cheapest possible
       implementation, and the one that would let an unlaid-out canvas
       light itself and steal the selection.
"""
import subprocess, os, shutil, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REL = lambda *p: os.path.join(ROOT, *p)
BUILD = os.path.join(ROOT, "scripts", "ri_build_host.sh")
TEST = "t222_fx_exclusive"

SFX = ("gui", "sectfx.c")
ENG = ("engine", "engine.c")

MUTANTS = {
    # m1: the press never claims the rack.
    "m1-panel-never-displaces":
        [("rep", SFX,
          "            s->rack->live = (uint8_t)fx_slot(s);",
          "            s->val[idx] = 1;")],

    # m2: store the selection instead of deriving it (the lying lamp).
    "m2-value-stored-not-derived":
        [("re", SFX,
          r"    if \(idx == RI_SFX_ONOFF\)\n        return fx_on\(s\);\n    return idx < RI_SFX_NCTL \? s->val\[idx\] : 0;",
          "    return idx < RI_SFX_NCTL ? s->val[idx] : 0;")],

    # m3: engine-side exclusivity removed.
    "m3-engine-no-exclusivity":
        [("re", ENG,
          r"    for \(i = 0; i < RI_ENGINE_FX_COUNT; i\+\+\)\n        e->fx_on\[i\] = \(i == mod\) \? 1u : 0u;",
          "    for (i = 0; i < RI_ENGINE_FX_COUNT; i++)\n        if (i == mod) e->fx_on[i] = 1u;")],

    # m4: engage without routing -- the owner's reported defect returns.
    "m4-engage-does-not-route":
        [("re", ENG,
          r"    if \(unit >= 0 && ri_route_owner\(&e->route, \(uint32_t\)unit\) == RI_ROUTE_NONE\)\n        ri_route_assign\(&e->route, \(uint32_t\)unit, RI_FX_ENGAGE_OWNER\);",
          "    (void)unit;")],

    # m5: engage steals an insert that is already owned.
    "m5-engage-steals-owned-insert":
        [("re", ENG,
          r"if \(unit >= 0 && ri_route_owner\(&e->route, \(uint32_t\)unit\) == RI_ROUTE_NONE\)",
          "if (unit >= 0)")],

    # m6: OFF clears everything (symmetric with ON).
    "m6-off-clears-the-whole-rack":
        [("re", ENG,
          r"    if \(!value\) \{           /\* OFF clears itself and disturbs nobody \*/\n        e->fx_on\[mod\] = 0u;\n        return;\n    \}",
          "    if (!value) {\n        for (i = 0; i < RI_ENGINE_FX_COUNT; i++) e->fx_on[i] = 1u;\n        return;\n    }")],

    # m7: unbound canvas keeps private state.
    "m7-unbound-canvas-lights-itself":
        [("re", SFX,
          r"        if \(!s->rack\)\n            return 1;",
          "        if (!s->rack) { s->val[idx] = (int16_t)(s->val[idx] ? 0 : 1); return 1; }")],
}

# A mutant's marker must never require the token that mutant removes -- that
# would score a real kill as "not a real mutant".
MARKERS = {
    "m1-panel-never-displaces": ["ri_sfx_rack_init", "RI_SFX_LIVE_NONE"],
    "m2-value-stored-not-derived": ["fx_slot", "ri_sfx_rack_init"],
    "m3-engine-no-exclusivity": ["ri_fx_engage", "RI_FX_ENGAGE_OWNER"],
    "m4-engage-does-not-route": ["ri_fx_engage", "ri_fx_unit_of"],
    "m5-engage-steals-owned-insert": ["RI_FX_ENGAGE_OWNER", "ri_route_owner"],
    "m6-off-clears-the-whole-rack": ["ri_fx_engage", "ri_route_assign"],
    "m7-unbound-canvas-lights-itself": ["fx_slot", "ri_sfx_rack_init"],
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
        for _, rel, *_ in ops:
            if rel not in backups:
                with open(os.path.join(ROOT, *rel), "rb") as fh:
                    backups[rel] = fh.read()

        ok = True
        for kind, rel, a, b in ops:
            p = os.path.join(ROOT, *rel)
            with open(p, encoding="utf-8") as fh:
                src = fh.read()
            if kind == "rep":
                if src.count(a) != 1:
                    print("%-34s SKIP (anchor matched %d times)" % (name, src.count(a)))
                    ok = False
                    survived += 1
                    break
                src = src.replace(a, b, 1)
            else:
                import re
                src, n = re.subn(a, b, src, count=1)
                if n < 1:
                    print("%-34s SKIP (pattern did not match)" % name)
                    ok = False
                    survived += 1
                    break
            with open(p, "w", encoding="utf-8") as fh:
                fh.write(src)
        if not ok:
            restore(backups)
            continue

        real = True
        for rel, toks in [(rel, MARKERS[name]) for _, rel, *_ in ops]:
            with open(os.path.join(ROOT, *rel), encoding="utf-8") as fh:
                body = fh.read()
            for tkn in toks:
                if tkn not in body:
                    print("%-34s NOT A REAL MUTANT (lost %r)" % (name, tkn))
                    real = False
        if not real:
            restore(backups)
            continue

        rc, out = run_gate()
        if rc == -99:
            print("%-34s BUILD FAILURE (rc=-99) -- NOT a kill" % name)
        elif rc != 0:
            print("%-34s rc=%-4d KILLED" % (name, rc))
            killed += 1
        else:
            print("%-34s rc=%-4d SURVIVED  <-- test is too weak" % (name, rc))
            survived += 1
        restore(backups)
finally:
    restore(backups)

total = len(MUTANTS)
print("\nmutation score: %d/%d killed (%d skipped or malformed -- each "
      "counts against the score, never for it)" % (killed, total, survived))
sys.exit(1 if survived or killed < total else 0)
