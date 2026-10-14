#!/usr/bin/env python3
"""Mutation harness for the lane-log analyser (t233_lane_evlog, 2026-10-15).

t233 was NEVER RED -- the analyser already existed when the test was written --
so it is a pin and this harness is what gives it teeth. Each mutant removes one
finding, or makes the tool fire on everything, and the gate must notice.

  m1   MIXER_NEVER_TOUCHED never fires. This is the "quietly stop reporting"
       mutant: the tool still runs, still exits 0, and looks healthy.
  m2   DELAY_SEND_NEVER_TOUCHED never fires. Removing the finding that would
       have saved two ear-proof cycles.
  m3   the tool fires on EVERYTHING -- the `not mixer` conditions dropped. This
       is the one that matters most: a finding tool that reports everything
       trains the reader to skim, and it passes every "does it find X?" assertion
       while being useless. Only the clean-session guard can catch it.
  m4   SWITCH_REPEATED_VALUE never fires.
  m5   the switch test forgets the `max != 1` filter, so every KNOB that the
       user dragged reports a repeated value -- the over-firing shape again, from
       a different line.
"""
import subprocess, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "scripts", "ri_build_host.sh")
TEST = "t233_lane_evlog"
TOOL = ("scripts", "lane_evlog.py")

MUTANTS = {
    "m1-mixer-never-reported":
        [("re", TOOL, r"    if len\(seq\) >= 50 and not mixer:", "    if False:")],
    "m2-delay-finding-removed":
        [("re", TOOL, r"    if len\(dly\) >= 10 and not mixer:", "    if False:")],
    "m3-fires-on-everything":
        [("re", TOOL, r"    if len\(seq\) >= 50 and not mixer:",
          "    if len(seq) >= 1:"),
         ("re", TOOL, r"    if len\(dly\) >= 10 and not mixer:", "    if len(dly) >= 1:")],
    "m4-switch-repeat-removed":
        [("re", TOOL, r"        if runs >= 1:", "        if False:")],
    "m5-knob-drags-reported-as-switches":
        [("re", TOOL, r"        if not d or d\['max'\] != 1:", "        if not d:")],
}

MARKERS = {
    "m1-mixer-never-reported": ["MIXER_NEVER_TOUCHED", "DELAY_SEND_NEVER_TOUCHED"],
    "m2-delay-finding-removed": ["MIXER_NEVER_TOUCHED", "MIXER_SECS"],
    "m3-fires-on-everything": ["MIXER_NEVER_TOUCHED", "DELAY_SEND_NEVER_TOUCHED"],
    "m4-switch-repeat-removed": ["SWITCH_REPEATED_VALUE", "MIXER_SECS"],
    # marker must be a token actually present in scripts/lane_evlog.py
    "m5-knob-drags-reported-as-switches": ["SWITCH_REPEATED_VALUE", "def analyse"],
}


def run_gate():
    t = subprocess.run(["bash", BUILD, "test", TEST], cwd=ROOT,
                       capture_output=True, text=True)
    return t.returncode, (t.stdout or t.stderr)


def assert_baseline():
    rc, out = run_gate()
    if rc != 0 or "PASS" not in out:
        print("BASELINE FAILED (rc=%d) -- fix the gate first.\n%s" % (rc, out[-400:]))
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
        for rel in {r for _, r, *_ in ops}:
            if rel not in backups:
                with open(os.path.join(ROOT, *rel), "rb") as fh:
                    backups[rel] = fh.read()
        ok = True
        for kind, rel, a, b in ops:
            p = os.path.join(ROOT, *rel)
            with open(p, encoding="utf-8") as fh:
                src = fh.read()
            src, n = re.subn(a, b, src, count=1)
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
        body = ""
        with open(os.path.join(ROOT, *TOOL), encoding="utf-8") as fh:
            body = fh.read()
        for tkn in MARKERS[name]:
            if tkn not in body:
                if real:
                    print("%-38s NOT A REAL MUTANT (lost %r)" % (name, tkn))
                    real = False
                survived += 1
        if not real:
            restore(backups)
            continue

        rc, out = run_gate()
        if rc != 0:
            print("%-38s rc=%-4d KILLED" % (name, rc))
            killed += 1
        else:
            print("%-38s rc=%-4d SURVIVED  <-- the test is too weak" % (name, rc))
            survived += 1
        restore(backups)
finally:
    restore(backups)

total = len(MUTANTS)
print("\nmutation score: %d/%d killed (%d skipped or malformed -- each "
      "counts against the score, never for it)" % (killed, total, survived))
sys.exit(1 if survived or killed < total else 0)
