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

## Dell (ABIv11)

`RIAPP.M3` staged in Dell `RAM:` (1117184 B). Owner 5-minute Live
proof (Song mode, USB-MIDI) pending — lane ready when the owner is.
