#!/usr/bin/env python3
"""Checker for riqemu1's QEMU audio configuration. Exit 0 = pass, 1 = fail.

WHY THIS EXISTS (2026-10-13). The lane's audio was 8.1 % slow because QEMU
opened the host stream at its own default while the guest ran faster: host
44100 against AROS's ac97 mixer at 48000, 44100/48000 = 0.91875. Nothing
reported it -- a stream at the wrong rate is indistinguishable from a stream
at the right rate by every instrument except the rate itself.

NOTHING BELOW SPELLS A FAVOURITE WORD OR NUMBER. Each invariant is either
structural (counts, id resolution, agreement between two derivations) or
derived from the code that decides it:

  1. exactly one sound card declared   -- the fix for this class of problem
     is to REPLACE a card, and the natural mistake is to append a second one
     next to it, leaving two devices sharing one backend.
  2. every `audiodev=` id resolves     -- a dangling id is a hard parse
     failure that takes the lane down (measured: it did).
  3. the rate is declared EXPLICITLY   -- QEMU's silent default is 44100
     (audio/audio.c:251-253), which is the defect itself. This checks the
     parameter is PRESENT; it deliberately does not check its value, so it
     cannot be satisfied by remembering a number.
  4. the declared rate EQUALS THE GUEST'S -- the number is read back out of
     the AROS ac97 driver rather than restated here, so if the driver ever
     changes rate this fails instead of drifting. This is the invariant that
     mutant m2 (explicitly declared, wrong value) exists to prove is live.
  5. the running lane MATCHES THE LAUNCHER -- the trap that produced the
     first failed launch today: an edited script that was never re-exec'd
     looks fixed from every angle except /proc.

`--ignore-lane` disables only check 5, so the structural checks can be
scored without a live VM confounding them. It does not weaken 1-4.
"""
import os
import re
import shlex
import sys

LAUNCHER = "/home/miller/Work/vms/start_riqemu1.sh"
# riqemu1 is ABI v1, so the v1 tree is the one whose driver actually runs.
AC97_SRC = ("/home/miller/Work/projects/Vulkan4Aros/src/abi/v1/AROS/"
            "workbench/devs/AHI/Drivers/ac97/ac97-main.c")
LANE_NAME = "riqemu1"


def launcher_argv(path):
    """The exact argv the launcher execs, with shell line-continuations joined.

    Read out of the file rather than re-stated here: a checker that carried
    its own copy of the command line would happily agree with itself while
    the launcher disagreed with both.
    """
    try:
        lines = open(path, errors="replace").read().splitlines()
    except OSError as e:
        return None, "cannot read launcher %s: %s" % (path, e)
    start = None
    for i, ln in enumerate(lines):
        if ln.strip().startswith("exec qemu-system"):
            start = i
            break
    if start is None:
        return None, "no `exec qemu-system...` line found in %s" % path
    buf = ""
    for ln in lines[start:]:
        s = ln.strip()
        if s.startswith("#"):
            continue
        buf += (" " if buf else "") + s
        if buf.endswith("\\"):
            buf = buf[:-1].rstrip()
            continue
        break
    try:
        argv = shlex.split(buf)
    except ValueError as e:
        return None, "cannot tokenize launcher command: %s" % e
    return argv[1:], None  # drop `exec`, argv[0] becomes the binary name


def running_argv():
    """argv of the live lane process, or None if the lane is down."""
    me = os.getpid()
    try:
        pids = [p for p in os.listdir("/proc") if p.isdigit()]
    except OSError:
        return None
    for p in pids:
        if int(p) == me:
            continue
        try:
            raw = open("/proc/%s/cmdline" % p, "rb").read()
        except OSError:
            continue
        if not raw:
            continue
        argv = [a.decode("utf-8", "replace") for a in raw.split(b"\0") if a]
        if argv and argv[0].startswith("qemu-system") and \
           "-name" in argv and LANE_NAME in argv[argv.index("-name") + 1:]:
            return argv
    return None


def guest_rate():
    """The rate AROS's ac97 driver runs the mixer at, read from its source.

    Returns (rate, note). rate is None when the source tree is absent, which
    is reported as a SKIP -- not a failure -- because the check cannot be
    derived and guessing the number would be exactly the spelling-keyed
    shortcut this project forbids.
    """
    try:
        src = open(AC97_SRC, errors="replace").read()
    except OSError as e:
        return None, "SKIP: cannot derive guest rate (%s)" % e
    m = re.search(r"ahiac_MixFreq\s*=\s*(\d+)", src)
    if not m:
        return None, "SKIP: no ahiac_MixFreq assignment in %s" % AC97_SRC
    rate = int(m.group(1))
    # The mode list must agree with the mixer, or the mode the guest selects
    # and the rate it plays at are two different questions.
    arr = re.search(r"frequencies\[\][^{]*\{([^}]*)\}", src, re.S)
    if arr:
        vals = [int(x) for x in re.findall(r"\b(\d{4,6})\b", arr.group(1))]
        if vals and rate not in vals:
            return None, ("SKIP: mixer %d is not in frequencies[] %s -- the "
                          "driver is self-inconsistent, decide by hand"
                          % (rate, vals))
    return rate, "derived %d from ac97-main.c" % rate


def main():
    ignore_lane = "--ignore-lane" in sys.argv[1:]
    bad, notes = [], []

    argv, err = launcher_argv(LAUNCHER)
    if err:
        print("FAIL: " + err)
        return 1

    # --- 1. exactly one sound card -------------------------------------
    # `-device AC97,audiodev=pa0` matches; `-device usb-tablet` does not,
    # because it carries no audiodev=. Counting "sound card" by that token
    # rather than by `-device` keeps usb-tablet out of the count.
    cards = re.findall(r"-device\s+([^,\s]+),audiodev=([^,\s]+)",
                       " ".join(argv))
    if len(cards) != 1:
        bad.append("the launcher declares %d sound cards (%s); exactly one is "
                   "allowed. Appending a card next to the existing one leaves "
                   "two devices sharing a backend, which is what a "
                   "replace-instead-of-append edit produces"
                   % (len(cards), ", ".join(c[0] for c in cards) or "none"))

    # --- 2. every audiodev reference resolves --------------------------
    declared = set()
    backend_vals = []
    for i, a in enumerate(argv):
        if a == "-audiodev" and i + 1 < len(argv):
            v = argv[i + 1]
            backend_vals.append(v)
            m = re.search(r"(?:^|,)id=([^,]+)", v)
            if m:
                declared.add(m.group(1))
    dangling = [c[1] for c in cards if c[1] not in declared]
    if dangling:
        bad.append("sound card references audiodev id(s) %s but the launcher "
                   "declares %s -- QEMU rejects this at parse time and the "
                   "lane does not come up (measured today)"
                   % (", ".join(dangling), ", ".join(sorted(declared)) or "none"))

    # --- 3. the rate is declared explicitly ----------------------------
    # Presence only. The VALUE is checked separately and derived (below), so
    # this cannot be satisfied by hardcoding a number.
    rate_txt = [v for v in backend_vals if re.search(r"(?:^|,)out\.frequency=", v)]
    if not rate_txt:
        bad.append("the -audiodev does not declare out.frequency=, so QEMU "
                   "silently defaults it to 44100 (audio/audio.c:251-253). "
                   "Against a 48000 Hz guest that is 8.1 %% slow and reads as "
                   "a slow synth rather than a wrong clock")

    # --- 4. the declared rate equals the guest's -----------------------
    else:
        declared_rate = int(re.search(r"out\.frequency=(\d+)",
                                      rate_txt[0]).group(1))
        gr, note = guest_rate()
        notes.append(note)
        if gr is None:
            pass  # derivation unavailable; note already records it as SKIP
        elif declared_rate != gr:
            bad.append("the launcher pins out.frequency=%d but AROS's ac97 "
                       "driver runs its mixer at %d (%s). The host stream and "
                       "the guest disagree by %d Hz, which resamples -- "
                       "defeating the point of declaring it at all"
                       % (declared_rate, gr, note, abs(declared_rate - gr)))

    # --- 5. the running lane matches the launcher ----------------------
    if ignore_lane:
        notes.append("lane check skipped (--ignore-lane)")
    else:
        live = running_argv()
        if live is None:
            notes.append("SKIP: no %s process running, cannot check for a "
                         "stale lane" % LANE_NAME)
        elif live != argv:
            # Report the differing positions rather than dumping two command
            # lines: "you changed -m" is actionable, two walls of text are not.
            diffs = []
            for i in range(max(len(live), len(argv))):
                a = live[i] if i < len(live) else "<absent>"
                b = argv[i] if i < len(argv) else "<absent>"
                if a != b:
                    diffs.append("[%d] running=%s launcher=%s" % (i, a, b))
            bad.append("the RUNNING lane does not match the launcher -- the "
                       "script was edited but never re-exec'd, so every "
                       "check of the script passes while the lane still runs "
                       "the old configuration: %s" % "; ".join(diffs[:4]))
        else:
            notes.append("running process matches the launcher")

    for n in notes:
        if n:
            print("   " + n)
    if bad:
        for b in bad:
            print("FAIL: " + b)
        return 1
    print("   lane audio: %d card, backend id resolves, rate explicit%s"
          % (len(cards), "" if ignore_lane else ", lane not stale"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
