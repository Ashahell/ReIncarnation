# M2 RIAPP proof (2026-10-09, riqemu1 ABIv1 lane, agent anon)

Lane: QEMU `-boot c` DH0, 1280x1024 (screendump verified), spooler port
9295, monitor 4477. RIAPP binary hash
`06bc6bae349676e87808497a52fad3f4a9c179a681716ea0af79376c03fd2515`
(1129808 B, built from HEAD with the per-message transport sync).

## G7 subset in the live app

`m2-riapp.mid.txt` (12 messages) into cluster `riapp` via `MIDISEND`:

- Stop(70) → `TR STOP`; channel-2 Play → `ignored=1`, transport
  untouched; Play(69) → `TR PLAY`; steps → `DSTEP … st=1` ×4;
  CC 38/17 → `push=2`; final Stop → `TR STOP` (`m2-ev7.log).
- Screenshot `m2-proof.png`: MIDI LED lit, transport stopped at BAR
  119 (ran, then stopped per script).

## Transient-edge law (the M2 bug found by this proof)

Transport Play/Stop are momentary commands, but `midi_drain()` ran
after `sync_transport()` and applied a whole drained batch before the
next sync — so a Play followed by Stop in one batch collapsed to the
batch-final state and neither edge reached the engine. The fix calls
`sync_transport()` inside the drain loop whenever a message actually
moves the panel transport state (`app/riapp.c`, 5 lines).

- RED/mutant: the same proof against the batch-final code
  (`m2-transport-collapse-mutant.log`: STOP, ignored, CCs, steps, but NO Play
  and NO final Stop) reproduces the collapse exactly.
- Host-side `/tmp/opencode/play_seq` replicates the 9-message
  sequence through bridge + midimap: `tr 0->1` on Play, `1->0` on
  final Stop.
