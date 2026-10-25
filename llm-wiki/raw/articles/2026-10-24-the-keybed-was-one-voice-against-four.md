# The keybed was not quiet, it was one voice against four (2026-10-24)

- Source: ReIncarnation session (OpenCode lane), 2026-10-24. Commit `afa8551`, tag `[levi-midi]` (phase P-KEY4).
- Evidence: `tests/unit/t240_keybed_level.c` (RED 6 → PASS), `tests/mutate_keybed_level.py` (8/8); `t148_levi_perfzone` (SINGLE = song path × hand gain); `t172_levi_bitexact` (`perf` case re-pinned, the only corpus case that fires a hand); `engine/dsp/levi.{c,h}`; `scripts/ri_audit.sh`; `docs/evidence/midi/ledger.md` §P-KEY4; probes and dumps in `docs/evidence/midi/pkey4/`.
- Collected: 2026-10-24
- Published: 2026-10-24
- Spec: `docs/superpowers/specs/2026-09-20-reincarnation-spec.md` (§12.10 GUI, keybed; §12.7 Levi voice bank)
- Prior: `raw/articles/2026-10-24-the-keybed-draws-a-keyboard-it-does-not-own.md` (P-KEY3, every drawn key plays); `raw/articles/2026-10-23-the-keybed-painted-slots-it-does-not-own.md` (P-KEY2, paint == hit and velocity 127). This article closes the level question P-KEY2 explicitly left open.

## The report, verbatim

> "all piano key produce sound now, but their volume is lower than the song being played."

The first listening round **on the P-KEY3 build** (`2f6d65f`), where every drawn key played. Coverage was closed; level was not.

## The obvious fix was wrong, and the measurement says why

The tempting move was "the keybed is attenuated somewhere". It is not. A live keybed note and a song lane's note are the **same fire** (`levi_note_vel` → `note_on_core` → `zone_fire` → `alloc_fire` → `levi_trigger`, and `levi_trigger` directly for the song). On the host, with the song's own tick-0 automation applied (`levi.Patch_Level 16`, `mix-levi.Level 90`, master 124), they are **bit-identical**:

| path | peak | RMS |
|---|---|---|
| live note 60, velocity 127 | 0.112997852 | 0.073350373 |
| song note 60, lane 0 | 0.112997852 | 0.073350373 |
| song chord (46,73,34,61) | 0.432491899 | 0.147373536 |

Velocity was already at the note path's maximum (P-KEY2 raised 100 → 127). There was no headroom left in velocity, and no per-note attenuation to remove.

## The song's Levi part is chordal — the bank says so

The Levi bank dump (`pkey4/levi-bank.txt`) is the whole explanation: pattern 1 row 4 carries **notes 46,73,34,61 on four lanes**; patterns 1..3 carry four notes on most rows, pattern 4 carries two. The track selects patterns 1..4 from bar 22 (`pkey4/song-track.txt`), and the tick-0 automation puts the part at `levi.Patch_Level 16` (`pkey4/song-auto.txt`).

Four uncorrelated voices sum as `sqrt(n)` in RMS, so one key is a quarter of the part's **peak** and about half its **RMS**. Against the full host render (`songs/local/zombie-nation/zombie-nation-host-render.wav`, 37.7–49 s), the mix RMS is 0.1456. The single key sat **6.05 dB** below the part it played over — audible, and exactly the owner's report.

## The rule: a hand, not a lane

`RI_LEVI_LIVE_GAIN` (2.0, +6 dB) is stamped on the **player's entry points** — `levi_note_on` / `levi_note_vel`, chord mode included, and the mono / unison re-fire of a held key — and carried on a per-voice `livegain` field. The field is separate from `zgain` on purpose: the P9c zone laws pin `zgain == 1.0` for SINGLE live notes, and folding a level decision into the zone field would rewrite those laws.

Sequenced paths stay at unity: `levi_trigger` (song lanes) and `levi_note_strike` (the device sequencer and arp) are **parts**, not hands. Both render paths (`levi_voice_render` and `levi_voice_render_stereo`) multiply the field; the entry point resets the pending gain after the fire, so it never leaks into a lane the song triggers later.

After, on the same rig:

| path | peak | RMS |
|---|---|---|
| live note 60, velocity 127 | 0.225995705 | **0.146700745** |
| song note 60, lane 0 | 0.112997852 | 0.073350373 |
| song chord (46,73,34,61) | 0.432491899 | 0.147373536 |

One key now has the RMS of the song's Levi part (0.1474) and of the full mix (0.1456), while its peak stays far below the mix peak (0.586). A live four-note chord peaks at 0.86, still under 1.0 — the choice of 2.0 rather than 4.0 is what keeps live chords out of clipping.

## Tests: the law is pinned, not restated

`t240_keybed_level` was written first: RED with **6 failures** on the scaffolded field (the defaults and the song path were already green), then GREEN. It pins:

- the constant is 2.0 (a decision, like `RI_LEVI_KEY_VEL 127`, not a derivation);
- the live note's peak **and** RMS are the gain times the song note;
- both render paths (stereo and mono);
- `levi_note_on` and the mono re-fire of a held key are hands;
- `zgain` stays 1.0 (the zone laws are untouched);
- `levi_note_strike` stays at unity (a part is not a player);
- the pending gain is reset, so a song fire after a live note is at unity.

`t148`'s "Single is the song path, bit for bit" becomes "times the hand gain", compared by an **exact divide** (2.0 is a power of two, so `x / 2.0f` is exact). `t172`'s `perf` case is the only corpus case that fires a hand, and it is the only pin that moved: `0x0077fcf3691f9513` → `0x5a5cbb6c031f9513`. The other twenty cases stayed bit-identical — that is the evidence the gain never reached a lane.

## The mutation harness, 8/8 on return codes

`tests/mutate_keybed_level.py`; the marker rule holds (no marker requires a token its mutant removes). Gates: t240, t148, t238. It prints which gate killed each mutant:

| mutant | killed by |
|---|---|
| m1 the gain back to unity | t240 |
| m2 the gain stamped on the song path | t240, t148 |
| m3 the gain squared | t240, t148 |
| m4 the pending gain not reset | t240 |
| m5 the stereo multiply missing | t240, t148 |
| m6 the mono multiply missing | **t240 only** |
| m7 the gain folded into `zgain` | t240, t148 |
| m8 the device sequencer boosted | **t240 only** |

m6 and m8 are the honest limits: only the level test renders the mono sum and asks what a strike is. `AUDIT 0/0 PASS` after the harness restored every edit.

## Build identity and ear-proof scope

Staged for the ear round: v11 build `afa8551`, **1 221 072 B**, sha256 `fddf39efce908b2d562172da3fe0a43b691f8d661c6fa08b22966992562c8022`, chunked put 199 chunks `sha_ok=True`, launcher `rc=0`, exactly one RIAPP window, `build=afa8551` in the EV log, `TR PLAY`, buffers advancing (2930 → 5875 in ~16 s), `xruns=0`, `overloads=0`.

**Not ear-proven.** Proven: the hand gain is exactly 2.0 on every hand path and both render paths, and nowhere else, and one key now measures at the part's RMS. Whether +6 dB is the level the owner wants over the whole song is the next Dell round's question; the constant is one line to move if the ear says otherwise.
