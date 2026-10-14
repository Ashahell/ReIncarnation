#!/usr/bin/env python3
"""Mutation harness for the mixer-mute automatable decision (t230, 2026-10-15).

`t230` is a PIN, not a fix: the decision (keep the mixer On/Off
Song-automatable, reversing the parity article's p. 56 line) was already
implemented by P-MIX1, so the test was green the moment it was written and was
**never RED**. That makes it exactly the kind of test that looks like evidence
and proves nothing on its own.

So its teeth are established here instead, by breaking each thing it claims to
hold and requiring the gate to notice:

  m1   `automatable` back to 0 on the mute rows. This is the literal drift the
       p. 56 line would cause, and the one a future reader "restoring manual
       fidelity" would reintroduce while thinking they were being careful.
  m2   the mute keys dropped from `RI_AUTO_ALLOW`. That silently removes the
       RECORDING half while `automatable` still says 1 -- a control that looks
       half-automatable, which is a subtler defect than either state.
  m3   the tripwire made to see a mixer-automating song that is not there. This
       does not mutate production code; it proves the census is not vacuous,
       i.e. that it would actually fire when a song grows a mixer key. A guard
       that cannot fail is not a guard.

Scored ONLY on the gate's return code, after `assert_baseline()`.
"""
import subprocess, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "scripts", "ri_build_host.sh")
TEST = "t230_mute_automatable"

REG = ("gui", "ctlreg.c")
AUT = ("engine", "seq", "autolane.c")
TST = ("tests", "unit", "t230_mute_automatable.c")

MUTANTS = {
    "m1-automatable-reverted-to-zero":
        [("re", REG, r'R\((MIX_\w+), 0, SWITCH, "", "On/Off", 0, 1, 1, RI_MIDI_CC_NONE, 1, MUTE,',
           r'R(\1, 0, SWITCH, "", "On/Off", 0, 1, 1, RI_MIDI_CC_NONE, 0, MUTE,')],
    "m2-mute-keys-dropped-from-allow":
        [("re", AUT, r"0x0B15u, 0x0B16u,", "0x0B15u,",)],
    "m3-tripwire-made-to-fire-spuriously":
        [("re", TST, r'if \(strstr\(buf, "0x0B"\) \|\| strstr\(buf, "0x0b"\)\)',
           'if (strstr(buf, "tempo=") /* every scaffold has this */)')],
}

MARKERS = {
    "m1-automatable-reverted-to-zero": ["RI_BIND_MUTE", "RI_AUTO_MIX_MUTE"],
    # markers must name tokens in the file actually mutated (autolane.c, not .h)
    "m2-mute-keys-dropped-from-allow": ["RI_AUTO_ALLOW", "0x0B50u"],
    "m3-tripwire-made-to-fire-spuriously": ["TRIPWIRE", "with_mix"],
}


def run_gate():
    b = subprocess.run(["bash", BUILD, "all"], cwd=ROOT, capture_output=True, text=True)
    if b.returncode != 0:
        return -99, "BUILD FAILED: " + (b.stdout or b.stderr)[-200:]
    t = subprocess.run(["bash", BUILD, "test", TEST], cwd=ROOT,
                       capture_output=True, text=True)
    return t.returncode, (t.stdout or t.stderr)


def assert_baseline():
    rc, out = run_gate()
    if rc != 0 or "PASS" not in out:
        print("BASELINE FAILED (rc=%d) -- every mutant would score as a kill "
              "for the wrong reason. Fix the gate first.\n%s" % (rc, out[-400:]))
        sys.exit(2)
    print("baseline: gate green on unmutated code\n")


def restore(backups):
    for rel, data in backups.items():
        p = os.path.join(ROOT, *rel)
        with open(p, "wb") as fh:
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
        for _, rel, _a, _b in ops:
            with open(os.path.join(ROOT, *rel), encoding="utf-8") as fh:
                body = fh.read()
            for tkn in MARKERS[name]:
                if tkn not in body and real:
                    print("%-38s NOT A REAL MUTANT (lost %r)" % (name, tkn))
                    real = False
                if tkn not in body:
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
            print("%-38s rc=%-4d SURVIVED  <-- the pin is too weak" % name)
            survived += 1
        restore(backups)
finally:
    restore(backups)

total = len(MUTANTS)
print("\nmutation score: %d/%d killed (%d skipped or malformed -- each "
      "counts against the score, never for it)" % (killed, total, survived))
sys.exit(1 if survived or killed < total else 0)
