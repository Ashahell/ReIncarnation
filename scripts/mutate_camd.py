#!/usr/bin/env python3
"""Mutation harness for the camd link-lifetime gates.

Every mutant is scored ONLY on the audit's return code: 0 = survived (the gate
failed to catch it, which is a FAIL for the gate), non-zero = killed. A mutant
that merely fails to parse is not a kill, so each is checked to still contain
the function body the gate reads.
"""
import subprocess, sys, re

SRC = "platform/aros/midi_camd.c"
ORIG = open(SRC).read()

EARLY = ("    if (s_txcluster[0] && strcmp(dst, s_txcluster) != 0)\n"
         "        return 1;")
LATE = ("        if (rc == 0 && s_txlink)\n"
        "            PutMidi(s_txlink, pkt);")

# Each mutant is a list of (anchor, replacement) pairs applied in order.
MUTANTS = {
  # The defect being fixed: a send frees the link the sender task is inside.
  "m1-send-frees-link":
      [(EARLY,
        "    if (s_txcluster[0] && strcmp(dst, s_txcluster) != 0) {\n"
        "        DeleteMidi(s_tx);\n        s_tx = 0;\n        s_txlink = 0;\n"
        "        return 1;\n    }")],
  # Build the link before the turnstile: two tasks can both create one.
  "m2-create-outside-claim":
      [("    if (!tx_claim())\n        return 1;\n    {\n        int rc = 0;\n"
        "        if (!s_tx) {",
        "    if (!s_tx) {\n        s_tx = CreateMidi(MIDI_Name, (IPTR)\"RIPAL-TX\", TAG_END);\n"
        "        if (s_tx)\n            s_txlink = AddMidiLink(s_tx, MLTYPE_Sender,\n"
        "                MLINK_Location, (IPTR)dst, TAG_END);\n    }\n"
        "    if (!tx_claim())\n        return 1;\n    {\n        int rc = 0;\n"
        "        if (0) {")],
  # Drop the refusal entirely: the message is sent on the wrong cluster's link.
  "m3-no-refuse-on-mismatch":
      [(EARLY, "    if (0)\n        return 1;")],
  # Unbalance the turnstile: claim with no release on the failure path.
  "m4-claim-leak":
      [("        tx_release();\n        return rc;", "        return rc;")],
  # Keep the comparison but only AFTER the send: nothing can be refused.
  "m5-refuse-after-put":
      [(EARLY, ""),
       (LATE, LATE + "\n        if (s_txcluster[0] && strcmp(dst, s_txcluster) != 0)\n"
                 "            return 1;")],
  # Keep the comparison in place but make it a no-op that returns nothing.
  "m6-refuse-is-a-noop":
      [(EARLY,
        "    if (s_txcluster[0] && strcmp(dst, s_txcluster) != 0)\n"
        "        s_txcluster[0] = 0;")],
}

def audit():
    r = subprocess.run(["bash", "scripts/ri_audit.sh"],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return r.returncode, r.stdout.decode("utf-8", "replace")

killed = survived = 0
try:
    for name, steps in MUTANTS.items():
        text = ORIG
        ok = True
        for old, new in steps:
            if text.count(old) != 1:
                print("ERROR: anchor for %s matched %d times -- harness broken"
                      % (name, text.count(old)))
                sys.exit(2)
            text = text.replace(old, new, 1)
        open(SRC, "w").write(text)
        rc, out = audit()
        intact = "ri_pal_midi_send" in open(SRC).read()
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
    open(SRC, "w").write(ORIG)

print("\nmutation score: %d/%d killed" % (killed, killed + survived))
sys.exit(0 if survived == 0 else 1)
