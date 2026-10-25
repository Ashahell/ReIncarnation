#!/usr/bin/env python3
"""Mutation harness for the keybed's live-performance gain (t240, 2026-10-24).

P-KEY4: the keybed plays one voice against the song's chordal Levi part, and
at the same per-voice level one key is 6 dB below the part it plays over.
The fix is a hand gain (RI_LEVI_LIVE_GAIN 2.0) stamped on live note fires
only. This harness proves the gates see the parts of that rule that matter:

  t240  the gain is 2.0; a hand's peak and RMS are the gain times the song
        note; the mono sum carries it; levi_note_on and the mono re-fire are
        hands; the device sequencer and the song path stay at unity; the
        pending gain never leaks into a later fire;
  t148  the zone laws survive: SINGLE is still the song path times the hand
        gain, bit for bit, and zgain stays the zone field;
  t238  the keybed still sends velocity 127 -- the gain is not a velocity
        rewrite, and t238's model of the key is unchanged.

Scored ONLY on the gates' return codes, and every mutant is checked to still
contain the tokens the gates read (a marker may never require a token the
mutant deliberately removes). The harness prints which gate killed each
mutant so the division of labour is a fact rather than a habit.
"""
import subprocess, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "scripts", "ri_build_host.sh")
TESTS = ("t240_keybed_level", "t148_levi_perfzone", "t238_levi_key_paint")

HEAD = ("engine", "dsp", "levi.h")
LEVI = ("engine", "dsp", "levi.c")

STEREO_GAIN = (
    "        if (v->livegain != 1.0f)                   "
    "/* live-performance gain (P-KEY4) */\n"
    "            amp *= v->livegain;\n"
    "    }\n"
    "    if (!(outL > -1e20f && outL < 1e20f) || "
    "!(outR > -1e20f && outR < 1e20f)) {\n"
)
MONO_GAIN = (
    "        if (v->livegain != 1.0f)                   "
    "/* live-performance gain (P-KEY4) */\n"
    "            amp *= v->livegain;\n"
    "    }\n"
    "    if (!(out > -1e20f && out < 1e20f)) {\n"
)

MUTANTS = {
    # m1: the decision reverted -- hands are back at the sequenced level.
    "m1-live-gain-unity":
        [("rep", HEAD, "#define RI_LEVI_LIVE_GAIN 2.0f",
          "#define RI_LEVI_LIVE_GAIN 1.0f")],
    # m2: the gain stamped on the song path too -- every lane doubles.
    "m2-gain-in-song-path":
        [("rep", LEVI,
          "    v->livegain = s->p_livegain; /* live-performance gain (P-KEY4); "
          "1.0f sequenced */",
          "    v->livegain = RI_LEVI_LIVE_GAIN;")],
    # m3: the hand gain applied twice (a squared boost).
    "m3-gain-squared":
        [("rep", LEVI,
          "    s->pvel = vel > 127u ? 127u : vel;\n"
          "    s->p_livegain = RI_LEVI_LIVE_GAIN;   /* a hand, not a lane (P-KEY4) */",
          "    s->pvel = vel > 127u ? 127u : vel;\n"
          "    s->p_livegain = RI_LEVI_LIVE_GAIN * RI_LEVI_LIVE_GAIN;   "
          "/* a hand, not a lane (P-KEY4) */")],
    # m4: the pending hand gain is not reset -- it rides into the next fire,
    # which is the song's lane when the player stops playing.
    "m4-pending-not-reset":
        [("rep", LEVI,
          "        n = note_on_core(s, note, 1);\n"
          "    s->p_livegain = 1.0f;\n"
          "    return n;\n"
          "}\n\n"
          "int levi_note_rel_vel",
          "        n = note_on_core(s, note, 1);\n"
          "    return n;\n"
          "}\n\n"
          "int levi_note_rel_vel")],
    # m5: the stereo render drops the hand gain (voices still carry it).
    "m5-stereo-multiply-missing":
        [("rep", LEVI, STEREO_GAIN,
          "    }\n"
          "    if (!(outL > -1e20f && outL < 1e20f) || "
          "!(outR > -1e20f && outR < 1e20f)) {\n")],
    # m6: the mono render drops it -- the console's mono sum goes quiet.
    "m6-mono-multiply-missing":
        [("rep", LEVI, MONO_GAIN,
          "    }\n"
          "    if (!(out > -1e20f && out < 1e20f)) {\n")],
    # m7: the hand gain folded into the zone field -- t148's zone laws
    # (zgain == 1.0 in SINGLE) must not be rewritten to carry it.
    "m7-gain-folded-into-zgain":
        [("rep", LEVI,
          "    v->zgain = s->p_zgain;      /* zone layer gain (P9c); "
          "1.0f on the song path */\n"
          "    v->livegain = s->p_livegain;",
          "    v->zgain = s->p_zgain * s->p_livegain;      /* zone layer gain (P9c); "
          "1.0f on the song path */\n"
          "    v->livegain = 1.0f;")],
    # m8: the device sequencer treated as a hand -- the boundary t240 pins
    # (a part is not a player) is broken.
    "m8-strike-boosted":
        [("rep", LEVI,
          "int levi_note_strike(struct RILeviSet *s, uint8_t note) {\n"
          "    if (!s || note > 127u)\n"
          "        return -1;\n"
          "    return note_on_core(s, note, 0);",
          "int levi_note_strike(struct RILeviSet *s, uint8_t note) {\n"
          "    int n;\n"
          "    if (!s || note > 127u)\n"
          "        return -1;\n"
          "    s->p_livegain = RI_LEVI_LIVE_GAIN;\n"
          "    n = note_on_core(s, note, 0);\n"
          "    s->p_livegain = 1.0f;\n"
          "    return n;")],
}

# A marker must be a token the SURVIVING code still has.
MARKERS = {
    "m1-live-gain-unity": ["RI_LEVI_LIVE_GAIN", "p_livegain"],
    "m2-gain-in-song-path": ["levi_trigger", "p_livegain", "RI_LEVI_LIVE_GAIN"],
    "m3-gain-squared": ["levi_note_vel", "RI_LEVI_LIVE_GAIN", "p_livegain"],
    "m4-pending-not-reset": ["levi_note_vel", "p_livegain", "levi_note_rel_vel"],
    "m5-stereo-multiply-missing": ["outL", "outR", "livegain", "RI_LEVI_LIVE_GAIN"],
    "m6-mono-multiply-missing": ["levi_voice_render_sum", "livegain", "RI_LEVI_LIVE_GAIN"],
    "m7-gain-folded-into-zgain": ["zgain", "p_zgain", "RI_LEVI_LIVE_GAIN"],
    "m8-strike-boosted": ["levi_note_strike", "note_on_core", "RI_LEVI_LIVE_GAIN"],
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
