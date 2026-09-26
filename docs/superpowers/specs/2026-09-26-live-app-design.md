# Live app (§12.11 G9) — design note

**Date:** 2026-09-26. **Status:** E0 design, implements spec §4 (LOCKED) + §5 + §8 + §13 + §17 #5.
**Scope:** one AROS application in which the panel plays sound in real time, driven by the transport.

## 1. Live-session state machine

States are the transport states (`RI_TR_STOPPED / PLAYING / RECORD`):

- STOPPED: render outputs silence, engine cursor held, player phase held, automation carry held. First stopped Stop moves the tick cursor per the C4 law (E1 p. 145): before loop start goes to song start, else loop start; second goes to song start.
- PLAYING: each device buffer advances ticks and samples, emits player + automation + control events, renders.
- RECORD: same as PLAYING, plus knob moves also call `ri_auto_touch` on the GUI-side back lane (G9.5). Stop or Record-off ends the pass (`ri_auto_pass_end`) then `ri_auto_pub_request`.

Discontinuities (play start, seek, loop wrap, song end): **punch-out-all, then chase** — `ri_auto_punch_out_all` (keeps touched), then `ri_auto_chase` at the new tick, then `ri_auto_carry_reindex`. Loop wrap is a discontinuity (owner DECIDED 2026-09-26).

## 2. Control-plane message format

Fixed-capacity SPSC ring, power-of-two capacity 256:

```c
struct RIControlMsg { uint16_t key; uint8_t val; uint8_t flags; };
```

- Single writer (GUI) / single reader (render). No alloc, no locks, no IO.
- Key space is exactly the lane keys: `ri_auto_allowed()` gates enqueue. A refused key is counted (`refused`), never stored.
- Overflow keeps the newest value per key (coalesce): a full queue with the same key updates in place; a full queue with a new key drops the oldest, stores the newest, counts one `dropped`. Overflow is never silent (`dropped` + GUI xrun-style indicator).
- Drained at buffer start into a caller-provided `RIEvent` array as `RI_EV_AUTOMATION` (`device` from the key block, `value` = key, `flags` = val) at the buffer's first sample. The same path serves knob moves, MIDI CC (`midimap` → registry → key) and automation playback. One path, no second setter route.

## 3. Sample-rate policy (E0, ledger)

Run the whole session at the negotiated device rate. The engine takes `sr`; the tempo map carries the same rate (`map.sr = session sr`). Offline export stays 48 kHz. Live == offline is proved **at the same rate** (host test renders the fixture at 48 kHz both ways; a 44.1 kHz row pins rate-genericity). Cost-if-wrong: resampling artefacts or drift; revisit needs a resampler + rate field in RBNG.

## 4. Xrun / degraded behaviour (§17 #5)

- Render Task `Wait()`s on the AHI PlayerFunc `Signal()`; on wake renders exactly the device buffer. Measured render time per buffer is recorded; overruns count `AUQA_XRUN_COUNT`, latch a GUI indicator, never hang. The master clock never rewinds (missed bus renders silence for that buffer per §17 #1).
- AHI missing → null backend + the exact `RI_AUDIO_NULL_MSG`. Missing 909 pack → 909 renders silence + GUI notice (never silent).
- Clean stop releases device, signal, task (the WBS 2.7 close-path lesson: no leaked AllocAudio).

## 5. What the GUI shows

Transport position from the real transport tick cursor (no stand-in clock — closes G6b). Meters from a render-published snapshot (small struct copied at buffer end: section peaks, FX peaks, comp GR, sample count, tick cursor, xrun count). The GUI never reads engine internals. Playback moves on-screen controls by chasing displayed values from the published lane at the playhead (read-only). "Lane full" shows the sticky `RI_AUTO_FLAG_FULL`, never silent.
