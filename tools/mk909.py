#!/usr/bin/env python3
"""tools/mk909.py — clean-room 909 tom/rim/clap layers for classic-01.

Extends reference/packs/classic-01 with the 5 voices the shipped pack
lacks (LT/MT/HT toms, RS rimshot, CP clap). Same conventions as the
shipped layers: stdlib only, seeded LCG noise (deterministic), 44100 Hz
16-bit mono WAV, peak -3 dBFS, d[0] = 0, per-layer recipe .txt, MANIFEST
rows printed to stdout (append by hand after review).

usage: python3 tools/mk909.py   # writes into reference/packs/classic-01/
"""
import math
import os
import struct
import wave

SR = 44100
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "..", "reference", "packs", "classic-01")
PEAK = 10.0 ** (-3.0 / 20.0)  # -3 dBFS, like the shipped layers


class LCG:
    def __init__(self, seed):
        self.s = seed & 0xFFFFFFFF

    def next(self):
        self.s = (1664525 * self.s + 1013904223) & 0xFFFFFFFF
        return self.s / 4294967296.0 * 2.0 - 1.0


def normalize(x):
    peak = max(max(x), -min(x), 1e-9)
    g = PEAK / peak
    return [v * g for v in x]


def write_wav(name, samples):
    samples = normalize(samples)
    samples[0] = 0.0  # click-free start convention
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b"".join(
            struct.pack("<h", max(-32768, min(32767, int(round(v * 32767.0)))))
            for v in samples))


def write_recipe(name, **kw):
    with open(os.path.join(OUT, name + ".txt"), "w") as f:
        for k, v in kw.items():
            f.write("%s: %s\n" % (k, v))


def tom(f0, f1, tau, seconds, seed, extra=""):
    n = int(seconds * SR)
    rng = LCG(seed)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / SR
        # exponential pitch drop f0 -> f1 over ~80 ms, then hold
        k = min(1.0, t / 0.08)
        f = f1 + (f0 - f1) * math.exp(-t / 0.08 * 3.0) if k < 1.0 else f1
        phase += 2.0 * math.pi * f / SR
        body = math.sin(phase) * math.exp(-t / tau)
        click = math.sin(2.0 * math.pi * 2500.0 * t) * math.exp(-t / 0.004) * 0.35
        nz = rng.next() * math.exp(-t / 0.01) * 0.05
        out.append(body + click + nz)
    return out, extra


def rim(hard, seed):
    n = int(0.15 * SR)
    rng = LCG(seed)
    out = []
    for i in range(n):
        t = i / SR
        body = math.sin(2.0 * math.pi * (500.0 if hard else 420.0) * t)
        body *= math.exp(-t / 0.012)
        nz = rng.next() * math.exp(-t / 0.003) * (0.5 if hard else 0.3)
        out.append(body + nz)
    return out


def clap(seed, density):
    n = int(0.30 * SR)
    rng = LCG(seed)
    # RBJ bandpass ~1600 Hz Q~2, direct form (clean-room standard math)
    f0, q = 1600.0, 2.0
    w0 = 2.0 * math.pi * f0 / SR
    alpha = math.sin(w0) / (2.0 * q)
    b0, b1, b2 = alpha, 0.0, -alpha
    a0, a1, a2 = 1.0 + alpha, -2.0 * math.cos(w0), 1.0 - alpha
    x1 = x2 = y1 = y2 = 0.0
    # burst onsets at 0/12/24 ms + density-scaled tail
    bursts = [0.0, 0.012, 0.024]
    out = []
    for i in range(n):
        t = i / SR
        env = 0.0
        for bt in bursts:
            if t >= bt:
                env += math.exp(-(t - bt) / 0.004)
        env += density * math.exp(-t / 0.09)
        x0 = rng.next() * env
        y0 = (b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0
        x2, x1, y2, y1 = x1, x0, y1, y0
        out.append(y0 * 0.8)
    return out


MANIFEST = []


def emit(layer, voice, lo, hi, samples, recipe, manf_map):
    write_wav(layer, samples)
    write_recipe(layer, **recipe)
    MANIFEST.append("%s src=synth;date=2026-09-27;equip=host-python3;lic=CC0;holder=RI;proc=%s;fmt=44100/16;norm=-3dBFS;loop=none;map=%s" % (layer, layer, manf_map))
    print("%s %d %d %d reference/packs/classic-01/%s.wav" % (layer, voice, lo, hi, layer))


def main():
    base_seed = 0x9091
    # Toms: LOW/MID/HI follow the BD/SD tune-split convention.
    for name, vid, f0, f1, tau in (("LT", 6, 120.0, 55.0, 0.35),
                                   ("MT", 7, 160.0, 80.0, 0.30),
                                   ("HT", 8, 210.0, 105.0, 0.25)):
        for ln, mult, lo, hi in (("LOW", 0.94, 0, 42), ("MID", 1.0, 43, 84),
                                 ("HI", 1.06, 85, 127)):
            lid = "%s-%s" % (name, ln)
            smp, _ = tom(f0 * mult, f1 * mult, tau, 0.9,
                         base_seed + vid * 16 + lo)
            emit(lid, vid, lo, hi, smp,
                 {"layer": "%s (voice %s, tune %d-%d)" % (lid, name.lower(), lo, hi),
                  "source": "synthesized from scratch (this script)",
                  "recipe": "sine %.0fHz->%.0fHz tau %.2fs + 2.5kHz click blip(tau 4ms, mix 0.35); d[0]=0 by sine phase 0" % (f0 * mult, f1 * mult, tau),
                  "rate/depth": "44100/16 master, peak -3dBFS, no dither",
                  "seed": "LCG 0x9091+%d (%s)" % (vid * 16 + lo, name.lower()),
                  "license": "CC0, holder ReIncarnation project"},
                 "%s:%d-%d" % (name.lower(), lo, hi))
    # Rimshot A/B: hard/soft variants.
    for ln, hard, lo, hi in (("A", True, 0, 63), ("B", False, 64, 127)):
        lid = "RS-%s" % ln
        emit(lid, 9, lo, hi, rim(hard, base_seed + 9 * 16 + lo),
             {"layer": "%s (voice rs, tune %d-%d)" % (lid, lo, hi),
              "source": "synthesized from scratch (this script)",
              "recipe": "sine %dHz tau 12ms + noise click(tau 3ms, mix %.1f); d[0]=0" % (500 if hard else 420, 0.5 if hard else 0.3),
              "rate/depth": "44100/16 master, peak -3dBFS, no dither",
              "seed": "LCG 0x9091+%d (rs)" % (9 * 16 + lo),
              "license": "CC0, holder ReIncarnation project"},
             "rs:%d-%d" % (lo, hi))
    # Clap A/B: burst density variants.
    for ln, density, lo, hi in (("A", 1.0, 0, 63), ("B", 0.7, 64, 127)):
        lid = "CP-%s" % ln
        emit(lid, 10, lo, hi, clap(base_seed + 10 * 16 + lo, density),
             {"layer": "%s (voice cp, tune %d-%d)" % (lid, lo, hi),
              "source": "synthesized from scratch (this script)",
              "recipe": "noise bursts at 0/12/24ms + tail, RBJ bandpass 1600Hz Q2, density %.1f; d[0]=0" % density,
              "rate/depth": "44100/16 master, peak -3dBFS, no dither",
              "seed": "LCG 0x9091+%d (cp)" % (10 * 16 + lo),
              "license": "CC0, holder ReIncarnation project"},
             "cp:%d-%d" % (lo, hi))
    print("--- MANIFEST rows to append ---")
    for row in MANIFEST:
        print(row)


if __name__ == "__main__":
    main()
