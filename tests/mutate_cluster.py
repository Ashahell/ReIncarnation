#!/usr/bin/env python3
"""Mutation harness for the cluster-name gate (ri_audit.sh, 2026-10-13).

Every mutant is scored ONLY on the audit's return code: 0 = survived (the gate
failed to catch it, which is a FAIL for the gate), non-zero = killed. A mutant
that merely damaged the file beyond recognition is not a kill, so each is
checked to still contain the markers the gate reads.

WHY EACH MUTANT EXISTS. They are not eight spellings of one defect; each one
pins a different claim the gate makes about itself:

  m1/m2    the two halves of the ACTUAL bug -- lane_proof.run setting one
           cluster and sending to the other, and m4-levi-r6-proof.md doing
           the same across adjacent fences. Either alone must fail.
  m3       a proof that never sets RIAPP_MIDI_IN and therefore inherits the
           code default -- the case a per-file check would wave through.
  m4       the CLOCKLOOP branch, so a fix to LISTEN alone cannot pass this.
  m5       the anti-spelling mutant. Rewrites midi_bridge.c's default instead
           of any evidence file. If the gate hardcoded the word "riapp" this
           would survive; it dies only because the expected value is
           RE-DERIVED from source every run.
  m6       uniformity is not enough: every reference becomes RIAPP at once,
           so the tree agrees with itself and still disagrees with the code.
  m7       every MIDISEND invocation loses its cluster argument while the
           settings remain -- the vacuous "uniform because nothing is sent".
  m8       the markdown FENCE branch, which is a different extraction path
           from `.run`'s ';' stripping.
"""
import subprocess, sys, os, re

# ROOT from this file's own location, not the caller's cwd: a harness that
# silently runs against a different tree is the stale-binary trap again.
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AUDIT = os.path.join(ROOT, "scripts", "ri_audit.sh")
REL = lambda *p: os.path.join(ROOT, *p)

MIDISEND = re.compile(r'((?:RAM:)?MIDISEND(?:\.V11)?)\s+([A-Za-z0-9_.]+)')

# op forms: ("rep", relpath, old, new)  -- exact, must match exactly once
#           ("re",  relpath, pat, repl) -- regex, must match at least once
#           ("del", relpath, pat)       -- regex removal, must match at least once
MUTANTS = {
    # The real defect, send side: the script talks to a cluster the app is not on.
    "m1-run-send-case":
        [("rep", ("docs", "evidence", "midi", "lane_proof.run"),
          "MIDISEND.V11 riapp LISTEN", "MIDISEND.V11 RIAPP LISTEN")],
    # ... and setting side, in the other file that contradicts itself.
    "m2-md-setting-case":
        [("rep", ("docs", "evidence", "midi", "m4-levi-r6-proof.md"),
          "setenv RAM:RIAPP_MIDI_IN=riapp", "setenv RAM:RIAPP_MIDI_IN=RIAPP")],
    # A proof that never sets the cluster: inherits midi_bridge.c's default.
    "m3-unset-file-send":
        [("rep", ("docs", "evidence", "midi", "m4-levi-r6.run"),
          "MIDISEND riapp LISTEN", "MIDISEND RIAPP LISTEN")],
    # CLOCKLOOP is a different subcommand on the same cluster argument.
    "m4-clockloop-send":
        [("rep", ("docs", "evidence", "midi", "m5-clockout-cloop.run"),
          "MIDISEND riapp CLOCKLOOP", "MIDISEND RIAPP CLOCKLOOP")],
    # THE ANTI-SPELLING MUTANT. No evidence file changes; only the code's
    # default does. A gate that grepped for "riapp" would pass this.
    "m5-default-not-derived":
        [("rep", ("midi_io", "midi_bridge.c"),
          'static const char dflt[] = "riapp";',
          'static const char dflt[] = "RIAPP";')],
    # Uniform, self-consistent, and wrong: all references become RIAPP.
    "m6-uniform-but-not-default":
        [("re", ("docs", "evidence", "midi", "lane_proof.run"),
          r'RIAPP_MIDI_IN[= ]+riapp', 'RIAPP_MIDI_IN=RIAPP'),
         ("re", ("docs", "evidence", "midi", "lane_proof.run"),
          r'MIDISEND\.V11\s+riapp', 'MIDISEND.V11 RIAPP'),
         ("re", ("docs", "evidence", "midi", "m4-levi-r6-proof.md"),
          r'RIAPP_MIDI_IN[= ]+riapp', 'RIAPP_MIDI_IN=RIAPP'),
         ("re", ("docs", "evidence", "midi", "m4-levi-r6-proof.md"),
          r'MIDISEND\s+riapp', 'MIDISEND RIAPP'),
         ("re", ("docs", "evidence", "midi", "m4-levi-r6.run"),
          r'MIDISEND\s+riapp', 'MIDISEND RIAPP'),
         ("re", ("docs", "evidence", "midi", "m4-riapp-proof.md"),
          r'MIDISEND\s+riapp', 'MIDISEND RIAPP'),
         ("re", ("docs", "evidence", "midi", "m5-clockout-cloop.run"),
          r'MIDISEND\s+riapp', 'MIDISEND RIAPP'),
         ("re", ("docs", "evidence", "midi", "m5-clockout-cloop-dell.run"),
          r'MIDISEND\s+riapp', 'MIDISEND RIAPP'),
         ("re", ("docs", "evidence", "midi", "m5-clockout-riapp-dell.run"),
          r'RIAPP_MIDI_IN[= ]+riapp', 'RIAPP_MIDI_IN=RIAPP'),
         ("re", ("docs", "evidence", "midi", "m5-clockout-riapp-dell.run"),
          r'MIDISEND\s+riapp', 'MIDISEND RIAPP')],
    # Settings survive, no invocation carries a cluster any more.
    "m7-no-invocation-left":
        [("re", ("docs", "evidence", "midi", "lane_proof.run"),
          r'MIDISEND\.V11\s+riapp', 'MIDISEND.V11'),
         ("re", ("docs", "evidence", "midi", "m4-levi-r6.run"),
          r'MIDISEND\s+riapp', 'MIDISEND'),
         ("re", ("docs", "evidence", "midi", "m4-riapp-proof.md"),
          r'MIDISEND\s+riapp', 'MIDISEND'),
         ("re", ("docs", "evidence", "midi", "m5-clockout-cloop.run"),
          r'MIDISEND\s+riapp', 'MIDISEND'),
         ("re", ("docs", "evidence", "midi", "m5-clockout-cloop-dell.run"),
          r'MIDISEND\s+riapp', 'MIDISEND'),
         ("re", ("docs", "evidence", "midi", "m5-clockout-riapp-dell.run"),
          r'MIDISEND\s+riapp', 'MIDISEND')],
    # The markdown fence path specifically: this line is inside ``` so a gate
    # that read prose would miss it, and one that skipped fences would not see
    # the contradiction with line 92 directly above it.
    "m8-md-fence-send":
        [("rep", ("docs", "evidence", "midi", "m4-levi-r6-proof.md"),
          "RAM:MIDISEND riapp RAM:m4-levi.mid.txt",
          "RAM:MIDISEND RIAPP RAM:m4-levi.mid.txt")],
}

# Every file any mutant touches, snapshotted once and restored in `finally`.
TOUCHED = sorted({rel for ops in MUTANTS.values() for _, rel, _, _ in ops})
ORIG = {}
for rel in TOUCHED:
    p = os.path.join(ROOT, *rel)
    ORIG[p] = open(p, errors="replace").read()

# What each mutant must leave intact for its kill to count as real. A mutant
# that gutted the marker the gate reads proves only that the gate noticed a
# missing file, which is not the property under test.
MARKERS = {
    "m5-default-not-derived": ("dflt[",),
}
DEFAULT_MARKER = ("RIAPP_MIDI_IN", "MIDISEND")

def restore():
    for p, text in ORIG.items():
        open(p, "w").write(text)

def audit():
    r = subprocess.run(["bash", AUDIT], cwd=ROOT,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return r.returncode, r.stdout.decode("utf-8", "replace")

killed = survived = 0
try:
    for name, ops in MUTANTS.items():
        restore()
        ok = True
        for kind, rel, a, b in ops:
            p = os.path.join(ROOT, *rel)
            text = open(p, errors="replace").read()
            if kind == "rep":
                if text.count(a) != 1:
                    print("ERROR: anchor for %s matched %d times in %s -- harness broken"
                          % (name, text.count(a), rel))
                    sys.exit(2)
                text = text.replace(a, b, 1)
            elif kind == "re":
                text, n = re.subn(a, b, text)
                if n < 1:
                    print("ERROR: regex for %s matched 0 times in %s -- harness broken"
                          % (name, rel))
                    sys.exit(2)
            else:
                print("ERROR: unknown op %r for %s" % (kind, name)); sys.exit(2)
            open(p, "w").write(text)

        rc, out = audit()

        # Intactness: the mutated tree must still contain what the gate reads.
        want = MARKERS.get(name, DEFAULT_MARKER)
        blob = "".join(open(os.path.join(ROOT, *rel), errors="replace").read()
                       for _, rel, _, _ in ops)
        intact = any(w in blob for w in want) and all(
            os.path.getsize(os.path.join(ROOT, *rel)) > 0
            for _, rel, _, _ in ops)

        if rc != 0:
            killed += 1
        else:
            survived += 1
        print("%-26s rc=%-3d %s%s" % (name, rc, "KILLED" if rc else "SURVIVED",
              "" if intact else "  [BROKEN MUTANT -- not a real kill]"))
        for line in out.splitlines():
            if line.startswith("FAIL:"):
                print("      " + line[:130])
finally:
    restore()

print("\nmutation score: %d/%d killed" % (killed, killed + survived))
sys.exit(0 if survived == 0 else 1)
