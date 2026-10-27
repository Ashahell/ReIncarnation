#!/usr/bin/env python3
"""Mutation harness for the transport rewind (t242, 2026-10-26).

P-TR2: after the song ended, the transport could not be rewound, so every Play
was stopped again a few buffers later -- the song played once per launch. The
cause is a propagation rule, not a state machine: `sync_transport()` acted only
on a *state* change, and the documented rewind (ri_tr_stop's clicks law) changes
no state when the transport is already stopped. The panel's cursor moved; the
engine's did not.

The fix has two halves, and this harness proves the gates see both:

  t242  the propagation decision looks at the panel's stop-click count as well
        as its state, and PLAY/RECORD are the playing side;
  t242  a held STOP rewinds ONCE (the same press-edge rule t241 pins for the
        toggle), its own release re-arms it, and another key's release does not;
  t241  the toggle's own pins survive -- in particular the tempo keys keep
        their repeats, which is what kills a guard widened to the class;
  t71   the keyboard-level dispatch still stops and plays;
  t91   the canvas funnel still eats a space press.

Scored ONLY on the gates' return codes, and every mutant is checked to still
contain the tokens the gates read (a marker may never require a token the
mutant deliberately removes). The harness prints which gate killed each mutant.
"""
import subprocess, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "scripts", "ri_build_host.sh")
TESTS = ("t242_transport_rewind", "t241_transport_toggle_edge",
         "t71_panelui", "t91_canvas_events")

CORE = ("app", "core", "riapp_core.c")
PANEL = ("gui", "panelui.c")

STOP_LATCH = (
    "        } else if (a.arg == RI_KT_STOP) {\n"
    "            if (p->tr_stop == (uint8_t)code)\n"
    "                break;                  "
    "/* one rewind per press, not per repeat */\n"
    "            p->tr_stop = (uint8_t)code;\n"
    "        }\n"
)
STOP_CLEAR = (
    "        if (p->tr_stop == (uint8_t)code)\n"
    "            p->tr_stop = 0;\n"
)

MUTANTS = {
    # m1: the defect -- compare the state only, so a rewind press is dropped
    # and the engine's cursor never leaves the song's end.
    "m1-state-only-sync":
        [("rep", CORE,
          "    if (panel_state == *last_state && panel_clicks == *last_clicks)\n"
          "        return RI_TR_SYNC_NONE;",
          "    if (panel_state == *last_state)\n"
          "        return RI_TR_SYNC_NONE;")],
    # m2: no STOP guard -- a held STOP steps the cursor loop start -> song
    # start at the key repeat rate (a metronome, and a new log storm).
    "m2-no-stop-edge-guard":
        [("rep", PANEL, STOP_LATCH, "        }\n")],
    # m3: the latch never cleared -- STOP works once and is dead until the
    # panel is re-initialised.
    "m3-stop-latch-never-cleared":
        [("rep", PANEL, STOP_CLEAR, "")],
    # m4: ANY release clears the STOP latch -- a held STOP resumes stepping
    # when another key comes up.
    "m4-any-release-clears-stop":
        [("rep", PANEL, STOP_CLEAR,
          "        if (p->tr_stop != 0)\n"
          "            p->tr_stop = 0;\n")],
    # m5: the decision inverted -- PLAY becomes STOP and STOP becomes PLAY.
    "m5-inverted-sync":
        [("rep", CORE,
          "    return playing ? RI_TR_SYNC_PLAY : RI_TR_SYNC_STOP;",
          "    return playing ? RI_TR_SYNC_STOP : RI_TR_SYNC_PLAY;")],
    # m6: the guard widened to the whole transport class -- holding + stops
    # ramping the tempo, which is the behaviour the narrow guard exists to keep.
    "m6-guard-the-class":
        [("rep", PANEL, "        } else if (a.arg == RI_KT_STOP) {",
          "        } else if (1) {")],
}

# A marker must be a token the SURVIVING code still has.
MARKERS = {
    "m1-state-only-sync": ["RI_TR_SYNC_NONE", "RI_TR_SYNC_STOP", "panel_clicks"],
    "m2-no-stop-edge-guard": ["RI_KT_STOPPLAY", "tr_toggle", "RI_KT_STOP"],
    "m3-stop-latch-never-cleared": ["tr_toggle", "RI_RAW_UPFLAG", "RI_KT_STOP"],
    "m4-any-release-clears-stop": ["tr_toggle", "tr_stop", "RI_RAW_UPFLAG"],
    "m5-inverted-sync": ["RI_TR_SYNC_PLAY", "RI_TR_SYNC_STOP", "playing"],
    "m6-guard-the-class": ["RI_KT_STOPPLAY", "tr_toggle", "RI_KT_TEMPO_UP"],
}


def run_gate():
    out, failed = [], []
    for t in TESTS:
        r = subprocess.run(["bash", BUILD, "test", t], cwd=ROOT,
                           capture_output=True, text=True)
        out.append("%s rc=%d %s" % (t, r.returncode,
                                    (r.stdout or "") + (r.stderr or "")))
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
