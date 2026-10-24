#!/usr/bin/env python3
"""Mutation harness for the three-octave keybed map (t239, 2026-10-24).

Scored ONLY on the gates' return codes: a mutant that merely broke a file is
not a kill, so every mutant is checked to still contain the tokens the gates
read (a marker may never require a token the mutant deliberately removes).

The gate is FIVE tests on purpose, and the harness prints which one killed
each mutant so the division of labour is a fact rather than a habit:

  t239  the drawn bed is the mapped bed: every white and black slot the art's
        band lays out has exactly one control on that box, the 37 ids are the
        owner's map (24..36 + 228..251), the pitch rises left to right, and
        the census is closed both ways;
  t238  each key paints the slot it can be hit in, and no key paints another
        key's centre (the rule the drawing calls);
  t221  press/release latching, for all 37 keys;
  t149  the LEVI panel row count -- RI_SLEVI_NCTL covers ids 228..251;
  t92   the standing "any drawing change is deliberate" hash pins.

t239 drives the ART's band constants against the registry and the geometry.
A mutant that makes the DRAWING stop using the paint rule, or adds an art key
with no control, is invisible to t239 BY CONSTRUCTION -- t239 sees the map, not
the painter -- and only the hash pin sees it. Those are m5/m6 below, reported
as "t92 only" rather than scored as a quiet clean sweep.
"""
import subprocess, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "scripts", "ri_build_host.sh")
TESTS = ("t239_keybed_drawn_is_playable", "t238_levi_key_paint", "t221_keybed",
         "t149_levi_perfglide", "t92_draw_hash")

GEO = ("gui", "panelgeo.c")
SECT = ("gui", "sectlevi.c")
HEAD = ("gui", "sectlevi.h")
ART = ("gui", "draw", "art_levi.c")
CEV = ("app", "core", "canvas_events.c")

MUTANTS = {
    # m1: the last white key's control deleted -- the "last seven are silent"
    # shape, now for one key. The art still lays its bed.
    "m1-last-white-unmapped":
        [("rep", GEO,
          "    { SL(251), RI_GEO_RECT, 0, 1708, 510, 55, 80 },  /* white C7 */\n",
          "")],
    # m2: one black key moved 18 Q off its boundary -- a key you can hit in
    # the wrong place, the aim/picture split in the other direction.
    "m2-black-key-misplaced":
        [("rep", GEO,
          "    { SL(228), RI_GEO_RECT, 0, 912, 412, 36, 116 },  /* black C#5 */",
          "    { SL(228), RI_GEO_RECT, 0, 930, 412, 36, 116 },  /* black C#5 */")],
    # m3: an id collision -- the extension black C#5 renumbered onto D5, so
    # one key has two rows and another has none.
    "m3-id-collision":
        [("rep", GEO,
          "    { SL(228), RI_GEO_RECT, 0, 912, 412, 36, 116 },  /* black C#5 */",
          "    { SL(229), RI_GEO_RECT, 0, 912, 412, 36, 116 },  /* black C#5 */")],
    # m4: the note map off by one semitone -- every key plays its neighbour.
    "m4-note-map-off-by-one":
        [("rep", SECT,
          "    return (uint8_t)(RI_SLEVI_MIDDLE_C + n);",
          "    return (uint8_t)(RI_SLEVI_MIDDLE_C + n + 1u);")],
    # m5: the DRAWING stops using the rule's answer and re-stretches the
    # whites over the whole band. t238 stays green (the rule is untouched);
    # the picture goes wrong. t92 only.
    "m5-drawing-ignores-rule":
        [("rep", ART,
          "        cx += PX((bx0 + bx1) / 2) - PX((int)it->cx);\n"
          "        cy += PX((by0 + by1) / 2) - PX((int)it->cy);\n"
          "        hw = PX(bx1 - bx0) / 2;\n"
          "        hh = PX(by1 - by0) / 2;\n",
          "        (void)bx0; (void)bx1; (void)by0; (void)by1;\n"
          "        if (!black) {\n"
          "            cy = (PX(RI_LEVI_KB_BED) + PX(RI_LEVI_KB_Y1)) / 2 + (cy - PX(510));\n"
          "            hh = (PX(RI_LEVI_KB_Y1) - PX(RI_LEVI_KB_BED)) / 2;\n"
          "        }\n")],
    # m6: the art draws a black key back, at the right-hand end, with nothing
    # behind it -- exactly the decoration the owner's ear found. t92 only.
    "m6-art-decoration-key":
        [("rep", ART,
          "        ri_draw_rect(dl, ox + PX(x0 + 1), oy + PX(RI_LEVI_KB_BED),\n"
          "            ox + PX(x0 + (int)RI_LEVI_KB_PITCH - 2), oy + PX(RI_LEVI_BK_Y1),\n"
          "            ri_art_rgb(C_WHITEKEY));\n",
          "        ri_draw_rect(dl, ox + PX(x0 + 1), oy + PX(RI_LEVI_KB_BED),\n"
          "            ox + PX(x0 + (int)RI_LEVI_KB_PITCH - 2), oy + PX(RI_LEVI_BK_Y1),\n"
          "            ri_art_rgb(C_WHITEKEY));\n"
          "        if (k == (int)RI_LEVI_KB_NW - 1)\n"
          "            ri_art_rect(dl, ox + PX(1720), oy + PX(RI_LEVI_BK_Y0),\n"
          "                ox + PX(1750), oy + PX(RI_LEVI_BK_Y1), C_BLACK);\n")],
    # m7: NCTL left at the old 228 -- the extension keys' values fall off the
    # end of the section's array. t149 owns the row count.
    "m7-nctl-not-grown":
        [("rep", HEAD, "#define RI_SLEVI_NCTL 252u", "#define RI_SLEVI_NCTL 228u")],
    # m8: the latch narrowed back to the old octave -- keys 13..36 press but
    # never hold, so they never play. t221 owns both edges for all 37.
    "m8-latch-old-octave":
        [("rep", CEV,
          "    if (section == RI_SEC_LEVI && ri_levi_key_n((uint16_t)idx) >= 0)",
          "    if (section == RI_SEC_LEVI && idx >= RI_SLEVI_KEY0 &&\n"
          "        idx < RI_SLEVI_KEY0 + RI_SLEVI_KEY_LO)")],
}

# A marker must be a token the SURVIVING code still has.
MARKERS = {
    "m1-last-white-unmapped": ["SL(250)", "SL(249)", "RI_GEO_RECT"],
    "m2-black-key-misplaced": ["SL(230)", "SL(231)", "RI_GEO_RECT"],
    "m3-id-collision": ["SL(229)", "SL(231)", "RI_GEO_RECT"],
    "m4-note-map-off-by-one": ["ri_levi_key_note", "RI_SLEVI_MIDDLE_C + n"],
    "m5-drawing-ignores-rule": ["ri_levi_key_paint_q", "RI_LEVI_KB_BED"],
    "m6-art-decoration-key": ["RI_LEVI_KB_NW", "ri_levi_key_paint_q"],
    "m7-nctl-not-grown": ["RI_SLEVI_KEY_EXT0", "RI_SLEVI_KEYS"],
    "m8-latch-old-octave": ["levi_key_latch", "RI_SEC_LEVI"],
}


def run_gate():
    """Run EVERY gate and report which ones failed.

    (Stopping at the first failure made the report lie: the tests after it
    never ran, so every one of them looked like a killer.)"""
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
