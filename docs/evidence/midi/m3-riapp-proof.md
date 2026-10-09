# M3 RIAPP proof (2026-10-09, riqemu1 ABIv1 lane, agent anon)

RIAPP binary with the follow applier (1129808+ B, `RIAPP_MIDI_SYNC=1`
in `ENV:`; default stays Internal). Two scripts,
`m3-clock.mid.txt` (transport: FA/clocks/silence/FC/SPP/FB/clocks/FC)
and `m3-tempo.mid.txt` (FA + 240 steady clocks for the lock).

## Transport (`m3-ev-transport.log`)

- Stop → `TR STOP`; FA + first F8 → `intent=1`, `TR PLAY` from song
  start; 3 s silence → dropout `TR STOP` (latched, single, no FC);
  FC → `intent=3` (no-op, already stopped); SPP 10 → `intent=4
  seek=240` (cursor located, silent); FB + F8 → `intent=2`,
  `TR PLAY` from tick 240; final FC → `TR STOP`.
- Screenshot `m2-proof.png` class (transport + LEDs) applies.

## Tempo (`m3-ev-tempo.log`, `m3-tempo.png`)

- `MIDI follow=60bpm live-only @…` on lock (repeated on jitter
  relocks — honest flapping under script-playback jitter, each a new
  lock). Measured ~60 BPM for ~40 ms script clocks.
- Panel TEMPO reads 60 (was the 140 song default): the control shows
  measured while following; knob + TAP ignore edits (host-pinned in
  t182, secttr `tempo_lock`).
- Transport running at the followed tempo (BAR advancing, play lit).

## M3b: the lane found five defects the host tests could not

The 5-minute Dell loopback (`RIAPP.M3`, ~62 BPM scripted master) and a
seek proof on riqemu1 exposed what the host harness had papered over:
the tempo servo's reference, the panel's position math and the SPP
conversion were each wrong in a way only a real clock shows. Each got
RED + mutation proof; the numbers below are from the lane.

1. **`ri_core_meters` projected 16ths at a fixed 120 BPM**
   (`m3-ev-tempo.log`: `@66432` after 7500 clocks — exactly 2× the
   ~33 000 ticks 60 BPM owes). The Song Position ran at the wrong speed
   for every tempo but 120. Fixed to the session tempo (t95).
2. **The servo measured against the panel's projection**, not the
   engine: a per-frame, wrong-rate number in a different instant from
   the clock expectation. Its error railed the ±2 BPM trim for the
   whole take. Now `s_core.session.cursor_ticks` (t182 local-stop law).
3. **A local Stop did not freeze the expectation** — the trim railed
   against a parked engine (t182).
4. **SPP landed a quarter of the distance it named.** The intent is
   sixteenths; the follower emitted `beats * 24` (a beat counted as a
   sixteenth), so Continue after a seek played 4× too early. Found
   because the first seek script wrote `F2 00 28` — SPP is `F2 <lsb>
   <msb>`, so that was position 5120, which located bar 320 of a
   151-bar song and immediately ran the take off the end
   (`m3-ev-seek-first.log`). Law now `beats * 4` sixteenths, converted
   to ticks by the panel's own PPQ (t125/t126/t181/t182).
5. **The Song Position never rebased at the playback edge**, so a take
   that started late appeared minutes into the song (`m3-locate.png`
   first reading: BAR 22 for a take located at bar 10). `ri_panel_live`
   now rebases the cumulative audio clock (t71).

Also added: a locate **clamps** to the panel's `song_bars` (t182), and
the applier's cursor is the ENGINE's tick cursor — the shell stores it
into the session before the transport sync starts the take, so a
Continue after a seek plays from where the master put it.

## Locate proof (`m3-ev-locate.log`, `m3-locate.png`)

Script `m3-seek2.mid.txt`: Stop, SPP 40 beats (`F2 28 00`), Continue,
240 steady clocks, Stop.

```
intent=4 seek=160        SPP 40 beats = 160 sixteenths
locate=3840              = 3840 engine ticks = bar 10
intent=2 / TR PLAY       Continue fires on the next F8
follow=60bpm @5627       the ENGINE is at bar 10 + ~4 beats
intent=3 / TR STOP
```

Screenshot mid-take: **BAR 11** (located bar 10 + ~2 s at 62 BPM),
**TEMPO 62** measured, Play lit, MIDI LED lit. Before the fixes the
same take read BAR 22 with the trim railed.

## Dell (ABIv11)

`RIAPP.M3` staged in Dell `RAM:` (1117184 B) — that was the pre-M3b
binary; the M3b build (1117936 B) is what the owner proof runs now.
Owner 5-minute Live proof (Song mode, USB-MIDI) pending — lane ready
when the owner is.