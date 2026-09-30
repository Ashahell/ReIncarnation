#!/usr/bin/env python3
"""tools/mid2rbs.py — arrange a MIDI file onto the ReIncarnation devices
as an RBS song script (songs & playlists, owner 2026-09-30).

The MIDI is only a note source: an arrangement map (JSON) says which MIDI
tracks play on which device, over which bars, with which transposition,
note range and drum-lane mapping. The converter quantises to the 16-step
grid, folds each device's bars into at most 31 distinct patterns (slot 0
stays silent), writes the song track, and appends a hand-written sound
program (SET/AUTO lines). tools/rbsc then compiles the script through the
real RBNG codec.

usage: python3 tools/mid2rbs.py MAP.json OUT.rbs
needs: mido (pip install mido)

Map keys (paths relative to the map file):
  midi         source MIDI file
  step_ticks   MIDI ticks per 16th step at the song tempo
  first_bar    MIDI bar that becomes song bar 0 (default 0)
  bars         number of song bars
  tempo        song tempo (BPM)
  cprg         credit line written as CPRG (<= 127 bytes)
  sound        RBS fragment appended verbatim (SET / AUTO lines)
  parts        list of parts:
    dev        303a | 303b | 808 | 909 | levi
    tracks     MIDI track indices
    bars       [[from, to], ...] MIDI bars, inclusive (default: all)
    transpose  semitones (default 0)
    min_note / max_note   keep only notes in range (after transposing)
    pick       303 only: "high" | "low" note when several start together
    tie        303 only: hold notes longer than a step with slide (default true)
    accent_vel note velocity at or above which a step is accented (default 128 = never)
    accent_steps  drums / 303: list of steps (0..15) accented in every bar
    drummap    drums: {"MIDI note": "LANE"} or {"note": ["LANE", "X"]} (X = high hit)
    lanes      levi: max lanes (default 6)
"""
import json
import os
import sys

try:
    import mido
except ImportError:
    sys.exit("mid2rbs: needs mido (pip install mido)")

DEVS = {"303a": 0, "303b": 1, "808": 2, "909": 3, "levi": 4}


def notes_of(mid, track):
    """(start_tick, dur_ticks, note, velocity) for one MIDI track."""
    out, on, t = [], {}, 0
    for msg in mid.tracks[track]:
        t += msg.time
        if msg.type == "note_on" and msg.velocity > 0:
            on[(msg.channel, msg.note)] = (t, msg.velocity)
        elif msg.type in ("note_off", "note_on"):
            k = (msg.channel, msg.note)
            if k in on:
                s, v = on.pop(k)
                out.append((s, t - s, msg.note, v))
    return sorted(out)


def in_bars(bar, ranges):
    return ranges is None or any(a <= bar <= b for a, b in ranges)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    mp_path, out_path = sys.argv[1], sys.argv[2]
    base = os.path.dirname(os.path.abspath(mp_path))
    mp = json.load(open(mp_path))
    mid = mido.MidiFile(os.path.join(base, mp["midi"]), clip=True)
    st = int(mp["step_ticks"])
    bar_ticks = st * 16
    first = int(mp.get("first_bar", 0))
    nbars = int(mp["bars"])
    # grid[dev][bar] -> per-device step content
    grid = {d: [None] * nbars for d in DEVS}
    for part in mp["parts"]:
        dev = part["dev"]
        if dev not in DEVS:
            sys.exit("mid2rbs: unknown dev %s" % dev)
        tr = int(part.get("transpose", 0))
        lo, hi = part.get("min_note", 0), part.get("max_note", 127)
        acc_vel = part.get("accent_vel", 128)
        acc_steps = set(part.get("accent_steps", []))
        for trk in part["tracks"]:
            for s, d, n, v in notes_of(mid, trk):
                mbar = s // bar_ticks
                if not in_bars(mbar, part.get("bars")):
                    continue
                bar = mbar - first
                if bar < 0 or bar >= nbars:
                    continue
                step = int(round((s % bar_ticks) / st))
                if step > 15:
                    continue
                dur = max(1, int(round(d / st)))
                if dev in ("808", "909"):
                    m = part["drummap"].get(str(n))
                    if m is None:
                        continue
                    lane, hit = (m, "x") if isinstance(m, str) else (m[0], m[1])
                    cell = grid[dev][bar] = grid[dev][bar] or {"lanes": {}, "acc": set()}
                    row = cell["lanes"].setdefault(lane, ["."] * 16)
                    row[step] = "X" if (hit == "X" or v >= acc_vel) else ("X" if row[step] == "X" else "x")
                    if v >= acc_vel or step in acc_steps:
                        cell["acc"].add(step)
                    continue
                n += tr
                if n < lo or n > hi:
                    continue
                if dev == "levi":
                    cell = grid[dev][bar] = grid[dev][bar] or [[] for _ in range(16)]
                    if n not in cell[step] and len(cell[step]) < part.get("lanes", 6):
                        cell[step].append(n)
                    continue
                cell = grid[dev][bar] = grid[dev][bar] or [None] * 16
                cur = cell[step]
                if cur is not None and cur[0] == "start":
                    better = (n > cur[1]) if part.get("pick", "high") == "high" else (n < cur[1])
                    if not better:
                        continue
                acc = v >= acc_vel or step in acc_steps
                cell[step] = ("start", n, acc, dur)
    lines = ["RBS 1", "# generated by tools/mid2rbs.py from %s" % os.path.basename(mp_path),
             "TEMPO %d" % int(mp["tempo"])]
    if mp.get("cprg"):
        lines.append("CPRG " + mp["cprg"])
    slots = {}
    for dev, idx in DEVS.items():
        uniq, seq = {}, []
        for bar in range(nbars):
            cell = grid[dev][bar]
            if dev in ("303a", "303b") and cell:
                toks = ["-"] * 16
                for k in range(16):
                    if cell[k] and cell[k][0] == "start":
                        _, n, acc, dur = cell[k]
                        hold = min(dur, 16 - k) if next((p for p in mp["parts"] if p["dev"] == dev),
                                                       {}).get("tie", True) else 1
                        for j in range(hold):
                            if j > 0 and cell[k + j] and cell[k + j][0] == "start":
                                break
                            slide = "s" if j < hold - 1 and not (cell[k + j + 1] and cell[k + j + 1][0] == "start") else ""
                            toks[k + j] = "%d%s%s" % (n, "a" if acc and j == 0 else "", slide)
                key = ("303", tuple(toks))
            elif dev in ("808", "909") and cell:
                key = ("drum", tuple(sorted((l, "".join(r)) for l, r in cell["lanes"].items())),
                       "".join("x" if k in cell["acc"] else "." for k in range(16)))
            elif dev == "levi" and cell and any(cell):
                key = ("levi", tuple(tuple(c) for c in cell))
            else:
                key = None
            if key is None:
                seq.append(0)
                continue
            if key not in uniq:
                if len(uniq) >= 31:
                    sys.exit("mid2rbs: %s needs more than 31 patterns" % dev)
                uniq[key] = len(uniq) + 1
                slot = uniq[key]
                if key[0] == "303":
                    lines.append("P303 %d %d %s" % (idx, slot, " ".join(key[1])))
                elif key[0] == "drum":
                    for lane, row in key[1]:
                        lines.append("PDRUM %d %d %s %s" % (idx, slot, lane, row))
                    if "x" in key[2]:
                        lines.append("PACC %d %d %s" % (idx, slot, key[2]))
                else:
                    for k, ns in enumerate(key[1]):
                        if ns:
                            lines.append("PLEVI %d 16 %d %s" % (slot, k, ",".join(str(x) for x in ns)))
            seq.append(uniq[key])
        slots[dev] = seq
        print("mid2rbs: %-4s %2d patterns" % (dev, len(uniq)))
    # Song track: runs of identical 5-tuples.
    run_start, prev = 0, None
    for bar in range(nbars + 1):
        cur = tuple(slots[d][bar] for d in DEVS) if bar < nbars else None
        if cur != prev:
            if prev is not None:
                lines.append("TRACK %d %d %s" % (run_start, bar - 1, " ".join(str(x) for x in prev)))
            run_start, prev = bar, cur
    # Bars past the end: every device on its silent slot 0 (the default).
    if mp.get("sound"):
        lines.append("# ---- sound program: %s" % mp["sound"])
        lines.extend(open(os.path.join(base, mp["sound"])).read().splitlines())
    open(out_path, "w").write("\n".join(lines) + "\n")
    print("mid2rbs: %d bars -> %s" % (nbars, out_path))


if __name__ == "__main__":
    main()
