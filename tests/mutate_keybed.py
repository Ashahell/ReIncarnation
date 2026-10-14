#!/usr/bin/env python3
"""Mutation harness for the keybed edge layer (t221_keybed, 2026-10-13).

Scored ONLY on the gate's return code: 0 = survived (a FAIL for the test),
non-zero = killed. A mutant that merely broke the file is not a kill, so each
is checked to still contain the markers the test reads.

A mutant's marker must show the edit was SURGICAL: it can never require a
token that mutant deliberately removes.

The keybed needs both edges. Each mutant removes one half or corrupts the
guard, because "latches on press" alone is exactly the half that looks right
and hangs a voice:

  m1  the latch call deleted -- no note-on is ever visible
  m2  release does not clear key_id -- the voice hangs forever, which is the
      failure this field exists to prevent
  m3  the range guard dropped, so ANY momentary button latches. The test's
      knob case is the one that sees this: a drag must not claim a held key.
  m4  ri_cev_init leaves key_id uninitialised, so the shell would read
      whatever was on the stack before the first press
  m5  the key range off by one -- the top key latches, or the first does not
"""
import subprocess, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REL = lambda *p: os.path.join(ROOT, *p)
BUILD = os.path.join(ROOT, "scripts", "ri_build_host.sh")
TEST = "t221_keybed"
SRC = ("app", "core", "canvas_events.c")

LATCH = "(levi_key_latch(st, ui->section, id), ri_sui_press(ui, idx))"
RELEASE = "st->key_id = RI_CEV_NODRAG;\n        st->drag_id = RI_CEV_NODRAG;"
INIT = "st->key_id = RI_CEV_NODRAG;\n}"

MUTANTS = {
    # (0, ...) rather than a bare call: deleting the latch outright leaves
    # levi_key_latch unused and -Werror turns that into a BUILD FAILURE,
    # which would be scored as a kill while testing nothing.
    "m1-latch-call-deleted":   [("rep", SRC, LATCH, "(levi_key_latch(st, 0u, 0u), ri_sui_press(ui, idx))")],
    "m2-release-keeps-key":    [("rep", SRC, RELEASE, "st->drag_id = RI_CEV_NODRAG;")],
    # (void) casts keep section/idx used, for the same -Werror reason.
    "m3-range-guard-dropped":  [("re", SRC,
        r"if \(section == RI_SEC_LEVI && idx >= RI_SLEVI_KEY0 &&\s*\n\s*idx < RI_SLEVI_KEY0 \+ RI_SLEVI_KEYS\)",
        "(void)section; (void)idx;\n    if (1)")],
    "m4-init-leaves-unset":    [("rep", SRC, INIT, "st->rep_ticks = 0u;\n}")],
    "m5-key-range-off-by-one": [("re", SRC,
        r"idx < RI_SLEVI_KEY0 \+ RI_SLEVI_KEYS\)", "idx < RI_SLEVI_KEY0 + RI_SLEVI_KEYS - 1u)")],
}

MARKERS = {
    "m1-latch-call-deleted": ["levi_key_latch", "ri_sui_press"],
    "m2-release-keeps-key": ["RI_CEV_NODRAG", "st->drag_id"],
    "m3-range-guard-dropped": ["levi_key_latch", "RI_CEV_NODRAG"],
    "m4-init-leaves-unset": ["ri_cev_init", "st->rep_ticks"],
    "m5-key-range-off-by-one": ["levi_key_latch", "RI_SLEVI_KEYS"],
}


def run_gate():
    b = subprocess.run(["bash", BUILD, "all"], cwd=ROOT, capture_output=True, text=True)
    if b.returncode != 0:
        return -99, "BUILD FAILED: " + (b.stdout or b.stderr)[-200:]
    t = subprocess.run(["bash", BUILD, "test", TEST], cwd=ROOT, capture_output=True, text=True)
    return t.returncode, (t.stdout or t.stderr)


def restore(bk):
    for rel, data in bk.items():
        with open(os.path.join(ROOT, *rel), "wb") as fh:
            fh.write(data)


killed = survived = 0
backups = {}
try:
    for name, ops in MUTANTS.items():
        backups = {}
        for _, rel, *_ in ops:
            if rel not in backups:
                with open(os.path.join(ROOT, *rel), "rb") as fh:
                    backups[rel] = fh.read()
        ok = True
        for kind, rel, a, b in ops:
            p = os.path.join(ROOT, *rel)
            src = open(p, encoding="utf-8", errors="replace").read()
            if kind == "rep":
                if src.count(a) != 1:
                    print("%-24s SKIP (anchor %d)" % (name, src.count(a))); ok = False; break
                src = src.replace(a, b)
            else:
                src, n = re.subn(a, b, src, count=1)
                if n != 1:
                    print("%-24s SKIP (regex %d)" % (name, n)); ok = False; break
            open(p, "w", encoding="utf-8").write(src)
        if not ok:
            restore(backups)
            subprocess.run(["bash", BUILD, "all"], cwd=ROOT, capture_output=True, text=True)
            continue
        blob = "".join(open(os.path.join(ROOT, *rel), errors="replace").read()
                       for _, rel, *_ in ops)
        intact = all(w in blob for w in MARKERS.get(name, []))
        rc, out = run_gate()
        restore(backups)
        subprocess.run(["bash", BUILD, "all"], cwd=ROOT, capture_output=True, text=True)
        if rc != 0:
            killed += 1
        else:
            survived += 1
        print("%-24s rc=%-4d %s%s" % (name, rc, "KILLED" if rc else "SURVIVED",
              "" if intact else "  [BROKEN MUTANT -- not a real kill]"))
        for line in out.splitlines():
            if line.startswith("FAIL:") or line.startswith("FAIL s"):
                print("      " + line[:120])
finally:
    restore(backups)
    subprocess.run(["bash", BUILD, "all"], cwd=ROOT, capture_output=True, text=True)

print("\nmutation score: %d/%d killed" % (killed, killed + survived))
sys.exit(0 if survived == 0 else 1)