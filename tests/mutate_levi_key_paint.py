#!/usr/bin/env python3
"""Mutation harness for the keybed paint rule (t238_levi_key_paint, 2026-10-23).

Scored ONLY on the gates' return codes: a mutant that merely broke a file is
not a kill, so each mutant is checked to still contain the tokens the gates
read. The gate here is TWO tests, and that is deliberate:

  t238  the invariant -- a key paints the slot it can be hit, and no live key
        paints another key's centre;
  t92   the standing "any drawing change is deliberate" hash pins.

t238 drives the RULE (ri_levi_key_paint_q), because the drawing calls it. A
mutant that makes the DRAWING stop calling it is invisible to t238 by
construction -- it would be testing a rule nothing uses -- and only the hash
pin sees it. That is m5/m6 below, and the harness says which gate killed each
so the division of labour is a fact and not a habit.

  m1  the white keys' box stretched over the whole band again -- the exact
      regression, and the shape the owner saw (a 36 Q black key showing 4 Q).
  m2  the paint box widened to the full key pitch instead of the key's own
      width -- the "paints cover the neighbour" shape.
  m3  the box shifted by a few Q -- off-by-small, the kind of change a
      hand-edit to nudge a key makes and a reviewer misses.
  m4  the keybed's fixed velocity lowered back to the old 100 -- the owner's
      raised ruling quietly reverted.
  m5  the DRAWING stops using what the rule answered and re-stretches the
      live whites itself -- the rule still passes every t238 assertion, so
      t238 alone would call this fix good while the keys kept painting wrong.
  m6  the art's bed for the live slots removed -- nothing behind the black
      keys but the panel, which no assertion of t238 touches.
"""
import subprocess, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "scripts", "ri_build_host.sh")
TESTS = ("t238_levi_key_paint", "t92_draw_hash")
RULE = ("gui", "draw", "art_levi.c")
VEL = ("gui", "sectlevi.h")

BOX = ("    *x0 = (int)it->cx - (int)it->w / 2;\n"
       "    *x1 = (int)it->cx + (int)it->w / 2;\n"
       "    *y0 = (int)it->cy - (int)it->h / 2;\n"
       "    *y1 = (int)it->cy + (int)it->h / 2;\n")

PAINT_CALL = ("        if (ri_levi_key_paint_q(it, &bx0, &by0, &bx1, &by1) != 0)\n"
              "            return;\n")

BED = ("            ri_draw_rect(dl, ox + PX(x0 + 1), oy + PX(LEVI_KEY_Y0),\n"
       "                ox + PX(x0 + LEVI_KEY_W - 2), oy + PX(LEVI_BKEY_Y1), wk);\n")

MUTANTS = {
    "m1-white-band-stretched":
        [("rep", RULE, BOX,
         "    *x0 = (int)it->cx - (int)it->w / 2;\n"
         "    *x1 = (int)it->cx + (int)it->w / 2;\n"
         "    if (it->h / 2 > 50) {\n"
         "        *y0 = (int)it->cy - (int)it->h / 2;\n"
         "        *y1 = (int)it->cy + (int)it->h / 2;\n"
         "    } else {\n"
         "        *y0 = LEVI_KEY_Y0;\n"
         "        *y1 = LEVI_KEY_Y1;\n"
         "    }\n")],
    "m2-width-full-pitch":
        [("rep", RULE, BOX,
         "    *x0 = (int)it->cx - (int)it->w;\n"
         "    *x1 = (int)it->cx + (int)it->w;\n"
         "    *y0 = (int)it->cy - (int)it->h / 2;\n"
         "    *y1 = (int)it->cy + (int)it->h / 2;\n")],
    "m3-box-shifted":
        [("rep", RULE, BOX,
         "    *x0 = (int)it->cx - (int)it->w / 2 + 4;\n"
         "    *x1 = (int)it->cx + (int)it->w / 2 + 4;\n"
         "    *y0 = (int)it->cy - (int)it->h / 2;\n"
         "    *y1 = (int)it->cy + (int)it->h / 2;\n")],
    "m4-velocity-reverted":
        [("rep", VEL, "#define RI_LEVI_KEY_VEL 127u", "#define RI_LEVI_KEY_VEL 100u")],
    "m5-live-whites-paint-the-band-themselves":
        [("rep", RULE,
         "        cx += PX((bx0 + bx1) / 2) - PX((int)it->cx);\n"
         "        cy += PX((by0 + by1) / 2) - PX((int)it->cy);\n"
         "        hw = PX(bx1 - bx0) / 2;\n"
         "        hh = PX(by1 - by0) / 2;\n",
         "        (void)bx0; (void)bx1; (void)by0; (void)by1;\n"
         "        if (!black) {\n"
         "            cy = (PX(LEVI_KEY_Y0) + PX(LEVI_KEY_Y1)) / 2 + (cy - PX(510));\n"
         "            hh = (PX(LEVI_KEY_Y1) - PX(LEVI_KEY_Y0)) / 2;\n"
         "        }\n")],
    "m6-art-bed-not-laid":
        [("rep", RULE, BED, "            (void)wk;\n")],
}

# A marker must be a token the SURVIVING code still has: it may never require
# a token the mutant deliberately removes.
MARKERS = {
    "m1-white-band-stretched": ["ri_levi_key_paint_q", "LEVI_KEY_Y1", "reg_id & 0xFFu"],
    "m2-width-full-pitch": ["ri_levi_key_paint_q", "it->h / 2", "RI_SLEVI_KEYS"],
    "m3-box-shifted": ["ri_levi_key_paint_q", "it->w / 2 + 4", "RI_SLEVI_KEY0"],
    "m4-velocity-reverted": ["RI_LEVI_KEY_VEL", "RI_SLEVI_MIDDLE_C"],
    "m5-live-whites-paint-the-band-themselves": ["ri_levi_key_paint_q", "RI_SLEVI_KEY0 + RI_SLEVI_KEYS"],
    "m6-art-bed-not-laid": ["LEVI_BKEY_Y1", "ri_levi_key_paint_q"],
}


def run_gate():
    """Both gates must pass. Returns (rc, text); rc 0 = the mutant SURVIVED."""
    out = []
    for t in TESTS:
        r = subprocess.run(["bash", BUILD, "test", t], cwd=ROOT,
                           capture_output=True, text=True)
        out.append("%s rc=%d %s" % (t, r.returncode, (r.stdout or "") + (r.stderr or "")))
        if r.returncode != 0:
            return r.returncode, "\n".join(out)
    return 0, "\n".join(out)


def assert_baseline():
    rc, out = run_gate()
    if rc != 0:
        print("BASELINE FAILED -- fix the test first.\n%s" % out[-1500:])
        sys.exit(2)
    print("baseline: both gates green on unmutated code\n")


def main():
    assert_baseline()
    killed = 0
    for name, edits in MUTANTS.items():
        backups = {}
        for kind, rel, old, new in edits:
            path = os.path.join(ROOT, *rel)
            with open(path, "r") as fh:
                text = fh.read()
            if kind == "rep":
                if text.count(old) != 1:
                    print("  EDIT NOT UNIQUE (%s found %d times) -- mutant is not surgical"
                          % (old[:40], text.count(old)))
                    break
                hit = text.replace(old, new)
            else:
                m = re.search(old, text)
                if not m:
                    print("  PATTERN ABSENT (%s)" % old[:40])
                    break
                hit = text[:m.start()] + new + text[m.end():]
            if any(tok not in hit for tok in MARKERS[name]):
                print("  MARKER LOST -- mutant removed a token the test reads")
                break
            backups[os.path.join(ROOT, *rel)] = text
            with open(path, "w") as fh:
                fh.write(hit)
        else:
            rc, out = run_gate()
            survived = "SURVIVED" if rc == 0 else "killed"
            killer = " ".join(t for t in TESTS if ("%s rc=0" % t) not in out)
            print("%-34s %-8s %s" % (name, survived, killer or "-"))
            if rc != 0:
                killed += 1
        for path, text in backups.items():
            with open(path, "w") as fh:
                fh.write(text)
    print("\nscore: %d/%d mutants killed" % (killed, len(MUTANTS)))
    return 0 if killed == len(MUTANTS) else 1


if __name__ == "__main__":
    sys.exit(main())
