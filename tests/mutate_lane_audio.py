#!/usr/bin/env python3
"""Mutation harness for tests/lane_audio.py (riqemu1 audio config, 2026-10-13).

Scored ONLY on the checker's return code: 0 = survived (the checker failed to
catch it, which is a FAIL for the checker), non-zero = killed. As with
mutate_camd.py, each mutant must still leave behind what the checker reads --
a file gutted beyond recognition is a broken mutant, not a kill.

Each mutant isolates exactly ONE invariant, so a kill can be attributed to
the property it targets rather than to whatever fires first:

  m1  rate not declared at all      -> invariant 3 (explicit declaration)
  m2  rate declared but WRONG       -> invariant 4 (derived from the guest).
      THIS IS THE ANTI-SPELLING MUTANT. m2 satisfies every presence check --
      `out.frequency=` is right there in the line -- and it is exactly what
      happens if someone "fixes" the defect by remembering 44100, the number
      QEMU defaults to anyway. A checker that grepped for the parameter, or
      for a favourite number, survives this. Only reading the rate back out
      of the AROS ac97 driver kills it.
  m3  backend id renamed out from
      under the card                -> invariant 2 (id resolution), with
      frequency still present so invariants 3 and 4 cannot be what fires
  m4  a SECOND card appended beside
      the existing one              -> invariant 1 (exactly one card). This
      is the literal mistake Deadwood's replace-the-card suggestion would
      have produced had it been applied alongside AC97 instead of to it.
  m5  a non-audio argument changed
      while the lane keeps running  -> invariant 5 (lane not stale). Run
      WITHOUT --ignore-lane, unlike m1-m4, because detecting this is the
      whole point; the launcher stays structurally valid throughout, so
      invariant 5 is the only one that can fire.

m1-m4 run with --ignore-lane so a live process cannot mask their cause.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHECKER = os.path.join(ROOT, "tests", "lane_audio.py")
LAUNCHER = "/home/miller/Work/vms/start_riqemu1.sh"

if not os.path.isfile(LAUNCHER):
    print("ERROR: launcher %s not found" % LAUNCHER)
    sys.exit(2)
ORIG = open(LAUNCHER, errors="replace").read()

# ops are (old, new) applied in order; every `old` must match exactly once,
# so a harness that drifts away from the file fails loudly instead of
# "killing" mutants that never got applied.
MUTANTS = {
    "m1-rate-not-declared": {
        "flags": ["--ignore-lane"],
        "ops": [(",out.frequency=48000", "")],
        "marker": "exec qemu-system",
    },
    "m2-rate-declared-but-wrong": {
        "flags": ["--ignore-lane"],
        # Leading comma: the bare text also appears in the launcher's own
        # comment, and mutating prose would "kill" m2 without touching the
        # argument. The harness's uniqueness check caught this first time.
        "ops": [(",out.frequency=48000", ",out.frequency=44100")],
        "marker": "out.frequency=44100",
    },
    "m3-backend-id-dangling": {
        "flags": ["--ignore-lane"],
        # rename ONLY the declaration (`pa,id=pa0`); the card's
        # `audiodev=pa0` reference is left pointing at the old id.
        "ops": [("pa,id=pa0", "pa,id=pb0")],
        "marker": "out.frequency=",
    },
    "m4-second-card-appended": {
        "flags": ["--ignore-lane"],
        "ops": [(" -device usb-tablet",
                 " -device intel-hda,audiodev=pa0 -device usb-tablet")],
        "marker": "-device AC97,audiodev=pa0",
    },
    "m5-lane-stale": {
        "flags": [],  # lane check ENABLED: this is what it exists for
        "ops": [("-m 2048", "-m 3072")],
        "marker": "-device AC97,audiodev=pa0",
    },
}


def restore():
    open(LAUNCHER, "w").write(ORIG)


def run(flags):
    r = subprocess.run([sys.executable, CHECKER] + flags,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return r.returncode, r.stdout.decode("utf-8", "replace")


try:
    # The unmutated tree must pass, or every mutant below is measuring noise.
    for flags in (["--ignore-lane"], []):
        rc, out = run(flags)
        if rc != 0:
            print("ERROR: baseline does not pass %s -- harness is invalid"
                  % (flags or ["<full>"]))
            for line in out.splitlines():
                if line.startswith("FAIL:"):
                    print("      " + line[:130])
            sys.exit(2)
    print("baseline: PASS (both modes)\n")

    killed = survived = 0
    for name, spec in MUTANTS.items():
        restore()
        for old, new in spec["ops"]:
            # Count against the file as it currently stands, not against the
            # pristine snapshot: multi-op mutants build on each other.
            text = open(LAUNCHER, errors="replace").read()
            if text.count(old) != 1:
                print("ERROR: %s anchor matched %d times -- harness broken"
                      % (name, text.count(old)))
                sys.exit(2)
            open(LAUNCHER, "w").write(text.replace(old, new, 1))

        rc, out = run(spec["flags"])

        # Intactness: the mutant must still be a launcher the checker can
        # actually read, or its kill proves nothing about the property.
        text = open(LAUNCHER, errors="replace").read()
        intact = spec["marker"] in text and "exec qemu-system" in text

        # A mutant counts as killed ONLY if it failed AND remained readable.
        # The two conditions are combined deliberately: crediting a kill for
        # a file the checker merely could not parse is how a mutation
        # harness starts lying.
        if rc != 0 and intact:
            killed += 1
            status = "KILLED"
        elif rc != 0:
            survived += 1
            status = "KILLED-BUT-BROKEN MUTANT -- not a real kill"
        else:
            survived += 1
            status = "SURVIVED"
        print("%-30s rc=%-3d %s" % (name, rc, status))
        for line in out.splitlines():
            if line.startswith("FAIL:"):
                print("      " + line[:130])

    print("\nmutation score: %d/%d killed" % (killed, killed + survived))
    sys.exit(0 if survived == 0 else 1)
finally:
    restore()
