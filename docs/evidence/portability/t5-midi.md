# T5 — MIDI split (portability plan §3.5)

- Status: GREEN (host) 2026-09-26. Device re-proof open (riqemu1 lane).
- `platform/aros/midi_camd.c` (new, AROS-only): receiver node+link per
  `open_in` (`RIPAL`, RecvSignal, 512 queue), cached sender for `send`
  (3-byte messages; longer returns 1), `ri_pal_midi_poll()` draining one
  signal batch into the open callback (`mm_Status/Data1/Data2`; `time_us`
  is 0 — no backend clock yet, `ri_time_us` lands in T8), full teardown in
  `close`. Owns the `CamdBase` inline base (one definition per link;
  `midisend.c` now `extern`s it).
- `app/sectproof.c` remote mode on PAL: `open_in("ri.remote")` + per-loop
  `poll()` into `sect_midi_in` (same `ri_midi_msg` delivery + `s_camd_got/
  s_camd_last` diagnostics, `mm_Msg` packing reconstructed); close via PAL.
  Note: the loop no longer waits on the CAMD signal bit (PAL exports no
  signal) — remote drains per event-loop iteration. Re-proof on riqemu1
  (MIDISEND → cluster → RISECT remote, G7 traces) is open; needs the lane.
- `app/midisend.c` script playback on `ri_pal_midi_send` (same script
  format, return codes, `D <ms>` waits). SELFTEST stays on raw CAMD by
  design — it diagnoses the stack itself (links, errors, clusters).
- Host: `platform/host/midi_host.c` — deterministic script-file source
  (`<time_us> <hex...>`, `#` comments; delivers synchronously at open) +
  send capture file; `poll()` no-op. Wired in `MOD_formats`.

## Tests

- `tests/unit/t89_pal_midi.c` (RED: `red-t89.txt`): 3-event delivery
  (bytes + times, comment skipped), miss path, send capture bytes, empty
  send refused.
- GREEN: `PASS pal_midi`.
- Mutant: script time base `10 → 16`: `FAIL times` (killed).
- AROS: `midi_camd.c`, `sectproof.c`, `midisend.c` compile clean (v1 SDK,
  `-Werror`; two real findings fixed: `MidiMsg.mm_Data` is a SysEx pointer
  — status bytes are `mm_Status/Data1/Data2`; missing `CamdBase`
  definition). RISECT + MIDISEND re-link (221568 / 34152 bytes, 0 UND).
- G7 remote proof re-run on riqemu1 through MIDISEND with the same traces:
  OPEN (owner lane).
