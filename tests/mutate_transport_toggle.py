#!/usr/bin/env python3
"""Mutation harness for the transport toggle's press edge (t241, 2026-10-26).

The defect: RI_KT_STOPPLAY toggles on the PLAY LED, and input.device repeats a
held key's DOWN, so one unreleased space key-down strobed the Dell's transport
1 493 800 times in 21.3 h (docs/evidence/midi/transport-storm/). The fix
latches the toggle key on its press and clears the latch on that key's own
release.

This harness proves the gates see each part of that rule:

  t241  the repeat does not toggle; the release does not toggle and does
        re-arm; another key's release does not re-arm; tempo keys still
        auto-repeat (the guard is the toggle, not the transport class); a
        fresh panel starts unlatched;
  t71   the keyboard-level dispatch still stops and plays -- with a real
        press/release between the two, which is the pin t71 was missing
        (it used to press space twice with no release and call that a law);
  t91   the canvas funnel still eats a space press.

Scored ONLY on the gates' return codes, and every mutant is checked to still
contain the tokens the gates read (a marker may never require a token the
mutant deliberately removes). The harness prints which gate killed each
mutant, so "t241 is the pin and t71 is not" is a measured fact.
"""
import subprocess, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "scripts", "ri_build_host.sh")
TESTS = ("t241_transport_toggle_edge", "t71_panelui", "t91_canvas_events")

PANEL = ("gui", "panelui.c")
KEYMAP = ("gui", "keymap.c")

LATCH = (
    "        if (a.arg == RI_KT_STOPPLAY) {\n"
    "            if (p->tr_toggle == (uint8_t)code)\n"
    "                break;                  "
    "/* auto-repeat of a held key, not a press */\n"
    "            p->tr_toggle = (uint8_t)code;\n"
    "        }\n"
)
CLEAR = (
    "    if ((raw & RI_RAW_UPFLAG) != 0u) {\n"
    "        if (p->tr_toggle == (uint8_t)code)\n"
    "            p->tr_toggle = 0;\n"
    "    }\n"
)

MUTANTS = {
    # m1: the guard removed -- back to a toggle on every repeat (the defect).
    "m1-no-repeat-guard":
        [("rep", PANEL,
          "            if (p->tr_toggle == (uint8_t)code)\n"
          "                break;                  "
          "/* auto-repeat of a held key, not a press */",
          "            if (0 && p->tr_toggle == (uint8_t)code)\n"
          "                break;                  "
          "/* auto-repeat of a held key, not a press */")],
    # m2: the guard widened to the whole transport class -- holding + stops
    # ramping, which is the behaviour the narrow guard exists to keep.
    "m2-guard-the-class":
        [("rep", PANEL, "        if (a.arg == RI_KT_STOPPLAY) {",
          "        if (1) {")],
    # m3: the release never re-arms -- the key toggles once and is dead.
    "m3-no-clear-on-release":
        [("rep", PANEL, CLEAR, "")],
    # m4: ANY release re-arms -- a held space resumes strobing when another
    # key comes up.
    "m4-clear-on-any-release":
        [("rep", PANEL, "        if (p->tr_toggle == (uint8_t)code)\n"
          "            p->tr_toggle = 0;",
          "        if (p->tr_toggle != 0)\n"
          "            p->tr_toggle = 0;")],
    # m5: the release treated as the action -- the up toggles too, which is
    # the other half of "which edge acts".
    "m5-release-toggles":
        [("rep", KEYMAP, "    if (up) {   /* only a held delete-tap cares about release; never a menu chord's */",
          "    if (up && code != RI_RAW_SPACE) {   /* only a held delete-tap cares about release; never a menu chord's */")],
}

# A marker must be a token the SURVIVING code still has.
MARKERS = {
    "m1-no-repeat-guard": ["RI_KT_STOPPLAY", "tr_toggle", "RI_RAW_UPFLAG"],
    "m2-guard-the-class": ["RI_KT_STOPPLAY", "tr_toggle", "RI_KT_TEMPO_UP"],
    "m3-no-clear-on-release": ["tr_toggle", "RI_KT_STOPPLAY", "RI_KA_TRANSPORT"],
    "m4-clear-on-any-release": ["RI_RAW_UPFLAG", "tr_toggle", "RI_KT_STOPPLAY"],
    "m5-release-toggles": ["RI_RAW_SPACE", "RI_KT_STOPPLAY", "up"],
}


def run_gate():
    """Run EVERY gate and report which ones failed."""
    out, failed = [], []
    for t in TESTS:
        r = subprocess.run(["bash", BUILD, "test", t], cwd=ROOT,
                           capture_output=True, text=True)
        out.append("%s rc=%d %s" % (t, r.returncode, (r.stdout or "") + (r.stderr or "")))
        if r.returncode != 0:
            failed.append(t)
    return failed, "\n".join(out)


def assert_baseline():
    failed, out = run_gate()
    if failed:
        print("BASELINE FAILED -- fix the tests first.\n%s" % out[-2000:])
        sys.exit(2)
    print("baseline: all %d gates green on unmutated code\n" % len(TESTS))


def main():
    assert_baseline()
    killed = 0
    for name, edits in MUTANTS.items():
        backups = {}
        ok = True
        for kind, rel, old, new in edits:
            path = os.path.join(ROOT, *rel)
            with open(path, "r") as fh:
                text = fh.read()
            if text.count(old) != 1:
                print("  EDIT NOT UNIQUE (%s found %d times) -- not surgical"
                      % (old[:40], text.count(old)))
                ok = False
                break
            hit = text.replace(old, new)
            if any(tok not in hit for tok in MARKERS[name]):
                print("  MARKER LOST -- mutant removed a token the gate reads")
                ok = False
                break
            backups[path] = text
            with open(path, "w") as fh:
                fh.write(hit)
        if ok:
            failed, out = run_gate()
            survived = "SURVIVED" if not failed else "killed"
            print("%-32s %-8s %s" % (name, survived,
                                     " ".join(failed) if failed else "-"))
            if failed:
                killed += 1
        for path, text in backups.items():
            with open(path, "w") as fh:
                fh.write(text)
    print("\nscore: %d/%d mutants killed" % (killed, len(MUTANTS)))
    return 0 if killed == len(MUTANTS) else 1


if __name__ == "__main__":
    sys.exit(main())
