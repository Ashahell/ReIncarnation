# MIDI — evidence ledger (Task 13, gate G13)

Spec §13: CAMD backend, note→step, CC→ctl learn, clock/MMC;
hot-unplug survival TC-2.8.4; flood shed at a documented cap (P-19
OPEN value); §17 failures 4 (flood) and 8 (unplug).

## Pure map (`midi_io/midi.c`, host-tested in t1_formats §9–12)

- `cc_to_ctl[128]`, init −1: unmapped CCs fail closed (no MIDI input
  ever moves an unlearned control). `midi_learn` ignores CC ≥ 128
  and ctl > 0xFFFF. Loopback asserted (CC7→0x0A00, CC1→0x0300).
- note→step: `note % 16`; velocity accent `vel ≥ 64`.
- MMC sysex (`F0 7F <dev> 06 <cmd> F7`): 02/03 → play (1), 01 → stop
  (0); pause (09) and anything else → −1, fail closed (the engine has
  no pause transport state; a defect draft mapped it to stop and the
  test rejected it — see green-defects.md).
- Simulated clock: 24 ticks/quarter; 100 bars × 4 × 24 = 9600 ticks
  exact, drift 0 (< 1 tick bound).
- Flood: `RI_MIDI_MAX_PER_BUFFER` = **64** (P-19 OPEN value, executor
  choice — soak-gated). Beyond the cap, oldest-first drop, counted
  (`dropped`); 200 pushes → len 64, dropped 136, head = first
  survivor. Playback timing never stalls on MIDI by construction
  (bounded shift, no alloc).
- Hot-unplug: `midi_backend_unplug()` → status `RI_MIDI_TIMEOUT`
  (2); `midi_transport_alive()` stays 1 (asserted). Pending MIDI
  events are dropped and counted bridge-side; transport continues.

## AROS CAMD backend (`midi_io/camd_backend.c`, AROS-only)

Guarded `#ifndef __AROS__` + `#error`, excluded from the host build
and from `all` (probe_ahi.c precedent, audit-gated). Owns the
cluster handle state mapping (NULL → NO_DEVICE, dead → TIMEOUT),
the unplug entry (marks dead + drops/counts + transport continues),
and the per-buffer pump (≤ 64 events via `midi_flood_push`,
master-clock stamping at the caller). Full device open/pump wiring
is Task-14 app wiring; this TU is the compile-checked discipline.
USB-MIDI scoped to Poseidon `camdusbmidi.class`/Bulk per spec W1
(platform fact, not re-proven here).
