#!/usr/bin/env python3
"""Mutation harness for the displaced FX lamp (t223_fx_lamp_repaint,
2026-10-14).

Owner ear proof: "if Delay is turned on, and you then turn on Comp, comp makes
an audible difference, but the green square on Delay remains lit."

Scored ONLY on the gate's return code, and only after assert_baseline()
confirms the gate is genuinely green -- `ri_build_host.sh all` does not build
the unit test, so a gate that does not COMPILE returns non-zero and every
mutant would read as a kill for the wrong reason.

The bug is stale pixels, not a wrong model: the selection is shared and
derived, so the displaced lamp is already dark in the model. So the mutants
here all attack the *instruction* -- who gets told to repaint -- and the one
that matters most is m1, which is the original defect verbatim.

  m1   the rack never reports a displacement. This is the stub the test was
       written against, and reverting to it restores the green square.
  m2   re-claiming the LIVE slot reports itself as displaced. Harmless to a
       human, but it repaints the canvas the user just pressed and wastes the
       one repaint the displaced canvas actually needed.
  m3   releasing the rack reports the released slot as displaced -- OFF made
       symmetric with ON. Invisible on screen (that canvas is being cleared
       anyway) and wrong in the model, so only an assertion on the RETURN
       value can see it.
  m4   the press path bypasses the rack helper and assigns `live` directly.
       The struct field then never moves and the shell is told to repaint
       nobody, while every value/led assertion still passes: the bookkeeping
       is decorative.
  m5   the bounds check is dropped. A slot past the last canvas is accepted,
       and the shell indexes its canvas table with the result -- so this
       repaints an unrelated canvas. The refusals were added after
       implementation and not watched RED; this mutant is the evidence they
       have teeth.
  m6   the stale SECTION is computed from the live slot instead of the
       displaced one -- the shell is told to repaint whoever is now live, and
       the stale pixels survive. This was originally aimed at riapp.c and
       SURVIVED, because a shell that cannot link on the host cannot be
       tested; the decision was extracted into ri_sfx_rack_stale_section() so
       the same mistake is now a one-token mutation.
"""
import subprocess, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "scripts", "ri_build_host.sh")
TEST = "t223_fx_lamp_repaint"

SFX = ("gui", "sectfx.c")
APP = ("app", "riapp.c")

MUTANTS = {
    "m1-never-reports-displacement":
        [("re", SFX,
          r"    prev = r->live;\n    r->live = \(uint8_t\)slot;\n    return prev == slot \? RI_SFX_LIVE_NONE : prev; /\* re-claiming displaces nobody \*/",
          "    prev = r->live; (void)prev;\n    r->live = (uint8_t)slot;\n    return RI_SFX_LIVE_NONE;")],

    "m2-reclaim-reports-itself":
        [("re", SFX,
          r"return prev == slot \? RI_SFX_LIVE_NONE : prev;",
          "return prev;")],

    "m3-release-reports-the-released-slot":
        [("re", SFX,
          r"    if \(slot == RI_SFX_LIVE_NONE\) \{\n        r->live = RI_SFX_LIVE_NONE;\n        return RI_SFX_LIVE_NONE;\n    \}",
          "    if (slot == RI_SFX_LIVE_NONE) {\n        uint32_t was = r->live;\n        r->live = RI_SFX_LIVE_NONE;\n        return was;\n    }")],

    "m4-press-bypasses-the-helper":
        [("re", SFX,
          r"        s->rack->displaced = \(uint8_t\)ri_sfx_rack_take\(s->rack,\n            fx_on\(s\) \? RI_SFX_LIVE_NONE : fx_slot\(s\)\);",
          "        if (fx_on(s))\n            s->rack->live = RI_SFX_LIVE_NONE;\n        else\n            s->rack->live = (uint8_t)fx_slot(s);")],

    "m5-bounds-check-dropped":
        [("re", SFX,
          r"    if \(slot >= RI_SFX_RACK_SLOTS\)\n        return RI_SFX_LIVE_NONE;",
          "    if (slot > 255u)\n        return RI_SFX_LIVE_NONE;")],

    "m6-reports-the-live-canvas":
        [("re", SFX,
          r"    return \(uint32_t\)RI_SEC_PCF \+ d;",
          "    return (uint32_t)RI_SEC_PCF + r->live;  /* the NEW live one */")],
}

# A mutant's marker must never require the token that mutant removes.
MARKERS = {
    "m1-never-reports-displacement": ["ri_sfx_rack_take", "RI_SFX_RACK_SLOTS"],
    "m2-reclaim-reports-itself": ["prev = r->live", "RI_SFX_RACK_SLOTS"],
    "m3-release-reports-the-released-slot": ["RI_SFX_LIVE_NONE", "RI_SFX_RACK_SLOTS"],
    "m4-press-bypasses-the-helper": ["ri_sfx_rack_take", "fx_slot"],
    "m5-bounds-check-dropped": ["prev = r->live", "ri_sfx_rack_take"],
    "m6-reports-the-live-canvas": ["ri_sfx_rack_stale_section", "RI_SFX_RACK_SLOTS"],
}


def run_gate():
    b = subprocess.run(["bash", BUILD, "all"], cwd=ROOT,
                       capture_output=True, text=True)
    if b.returncode != 0:
        return -99, "BUILD FAILED: " + (b.stdout or b.stderr)[-200:]
    t = subprocess.run(["bash", BUILD, "test", TEST], cwd=ROOT,
                       capture_output=True, text=True)
    return t.returncode, (t.stdout or t.stderr)


def assert_baseline():
    """Refuse to score anything unless the gate is green on unmutated code."""
    rc, out = run_gate()
    if rc != 0 or "PASS" not in out:
        print("BASELINE FAILED (rc=%d) -- every mutant would score as a kill "
              "for the wrong reason. Fix the gate first.\n%s" % (rc, out[-400:]))
        sys.exit(2)
    print("baseline: gate green on unmutated code\n")


def restore(backups):
    for rel, data in backups.items():
        with open(os.path.join(ROOT, *rel), "wb") as fh:
            fh.write(data)


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
            src, n = re.subn(a, b.replace("\\", "\\\\"), src, count=1)
            if n < 1:
                print("%-38s SKIP (pattern did not match)" % name)
                ok = False
                survived += 1
                break
            with open(p, "w", encoding="utf-8") as fh:
                fh.write(src)
        if not ok:
            restore(backups)
            continue

        real = True
        for _, rel, _a, _b in ops:
            with open(os.path.join(ROOT, *rel), encoding="utf-8") as fh:
                body = fh.read()
            for tkn in MARKERS[name]:
                if tkn not in body:
                    print("%-38s NOT A REAL MUTANT (lost %r)" % (name, tkn))
                    real = False
                    survived += 1
        if not real:
            restore(backups)
            continue

        rc, out = run_gate()
        if rc == -99:
            print("%-38s BUILD FAILURE (rc=-99) -- NOT a kill" % name)
        elif rc != 0:
            print("%-38s rc=%-4d KILLED" % (name, rc))
            killed += 1
        else:
            print("%-38s rc=%-4d SURVIVED  <-- test is too weak" % (name, rc))
            survived += 1
        restore(backups)
finally:
    restore(backups)

total = len(MUTANTS)
print("\nmutation score: %d/%d killed (%d skipped or malformed -- each "
      "counts against the score, never for it)" % (killed, total, survived))
sys.exit(1 if survived or killed < total else 0)
